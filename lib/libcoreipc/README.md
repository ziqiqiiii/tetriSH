# libcoreipc

tetriSH IPC (Inter-process communication) library

---

## Table of Contents

- [Build](#build)
- [Testing](#testing)
- [API Reference](#api-reference)
- [Architecture](#architecture)
- [Project Structure](#project-structure)


---

## Build

```bash
make -C lib/libcoreipc
```

This produces `lib/libcoreipc/libcoreipc.a`. The library has no internal
dependencies and is built first.

| Target | Description |
|---|---|
| `make` / `make all` | Build `libcoreipc.a` |
| `make test` | Build and run every suite under `tests/` |
| `make test FILTER=<pattern>` | Run only suites whose path contains `<pattern>` |
| `make clean` | Remove object files and test binaries |
| `make fclean` | Remove objects, test binaries, and the archive |
| `make re` | `fclean` + `all` |

Link the archive and add the public include directory:

```bash
cc ... -I lib/libcoreipc/include lib/libcoreipc/libcoreipc.a -lpthread -lrt
```

`tetrisd`, `tetrislogd`, `tetrisctl`, and `tetrisu` link it directly.

---

## Testing

Each test has its own `main()` and links `libcoreipc.a`:

```bash
make -C lib/libcoreipc test
make -C lib/libcoreipc test FILTER=ring_buffer
```

| Suite | Proves |
|---|---|
| `test_ring_buffer` | FIFO order, wraparound, full ⇒ `-1` and `ring_dropped_count` increments, pop-empty ⇒ `-1`, drain batching |
| `test_ring_buffer_mt` | 4 producers × 10k records, 1 consumer: `popped + dropped == pushed`, no duplicates |
| `test_unix_dgram` | Round trip under `mkdtemp()`, unbound path ⇒ `ECONNREFUSED`, re-bind over a stale file |
| `test_unix_stream` | 100 KiB round trip (forcing short writes), `0600` on the bound path, early close ⇒ `EPIPE` |
| `test_fd_signal` | Flags preserved, socket file removed, notify from a real handler, N notifies coalesce to one |
| `test_msgqueue` | With `maxmsg=4`: four sends succeed, the fifth ⇒ `EAGAIN` without blocking, drain in order |

---

## API Reference

All declarations live in `include/coreipc.h`. 

### Ring buffer


| Function | Description |
|---|---|
| `ring_init(rb, record_size, capacity)` | Allocate `capacity + 1` slots and initialise the mutex and drop counter |
| `ring_push(rb, record)` | Copy one record in; O(1), or drop it and increment the counter when full |
| `ring_pop(rb, out)` | Remove the oldest record; `-1` when empty |
| `ring_drain(rb, out, max_records)` | Remove up to `max_records` records under one lock; returns the count |
| `ring_dropped_count(rb)` | Read the monotonic drop total with an atomic load, taking no lock |
| `ring_destroy(rb)` | Free the slots and destroy the mutex; safe on a zeroed struct |

### Unix domain sockets — datagram

| Function | Description |
|---|---|
| `unixsock_dgram_bind(path, mode)` | Unlink any stale socket file, bind, `chmod`, and set non-blocking |
| `unixsock_dgram_open(path)` | Open a `connect()`ed sender aimed at a bound path |
| `unixsock_dgram_send_nonblock(fd, buf, len)` | Send one datagram with `MSG_DONTWAIT \| MSG_NOSIGNAL`; `EAGAIN` and `ECONNREFUSED` both mean dropped |
| `unixsock_dgram_recv(fd, buf, buflen)` | Receive one datagram, truncating past `buflen`; `EAGAIN` when nothing is queued |


### Unix domain sockets — stream

| Function | Description |
|---|---|
| `unixsock_stream_listen(path, backlog, mode)` | • Unlink any stale socket file, bind, `chmod`, and listen. <br>• The `mode` argument is the control plane's entire authorisation model — `0600` on `ctl_socket` is what restricts `tetrisctl` to the operator, since that channel carries no `Player-Id` and no session auth. |
| `unixsock_stream_accept(listen_fd)` | Accept one connection, retrying internally on `EINTR` |
| `unixsock_stream_connect(path)` | Connect to a listening socket path |
| `unixsock_send_all(fd, buf, len)` | Send every byte, looping over short writes with `MSG_NOSIGNAL` |
| `unixsock_recv_all(fd, buf, len)` | Receive exactly `len` bytes; a peer closing early is `EPIPE`, not a short read |


### fd and signal plumbing

| Function | Description |
|---|---|
| `unixsock_set_nonblock(fd)` | Add `O_NONBLOCK` without clearing flags the caller set |
| `unixsock_close_unlink(fd, path)` | Close a bound socket and remove its filesystem entry |
| `selfpipe_open(fds)` | Create a self-pipe at `fds[SELFPIPE_READ]` / `fds[SELFPIPE_WRITE]`, both `O_NONBLOCK \| O_CLOEXEC` |
| `selfpipe_notify(write_fd)` | Write one byte to wake the event loop; callable from a signal handler |
| `selfpipe_drain(read_fd)` | Read every pending wakeup byte after `poll()` reports the read end |

Coalescing is intended: N signals between two loop iterations are one wakeup.

### POSIX message queues


| Function | Description |
|---|---|
| `msgqueue_open(name, maxmsg, msgsize, mode)` | Open or create with `O_CREAT \| O_RDWR \| O_NONBLOCK` |
| `msgqueue_send_nonblock(q, msg, len)` | Send one message at priority 0; `EAGAIN` when full |
| `msgqueue_recv_nonblock(q, buf, buflen)` | Receive one message; `EAGAIN` when empty |
| `msgqueue_recv_timed(q, buf, buflen, timeout_ms)` | Receive one message, waiting up to `timeout_ms`; `ETIMEDOUT` when none arrives |
| `msgqueue_close(q)` | Close a descriptor without removing the queue |
| `msgqueue_unlink(name)` | Remove the queue name from the system |

Receive buffers must be at least `mq_msgsize` or the call fails with `EMSGSIZE` rather than truncating. No lock may be held across `msgqueue_recv_timed`; callers holding a room mutex use `msgqueue_recv_nonblock`.

---

## Architecture

Five channels are built from these primitives:

| Channel | Primitive | Serves |
|---|---|---|
| Game threads → logshipper thread | `ring_*` | The non-blocking guarantee for log records |
| `tetrisd` → `tetrislogd` | `unixsock_dgram_*` | Log forwarding across processes |
| `tetrisctl` ↔ `tetrisd` | `unixsock_stream_*`, `unixsock_send_all`, `unixsock_recv_all` | The control plane |
| Room → room garbage | `msgqueue_*` + `ring_*` | Battle Royale garbage transfer |
| Signal handler → event loop | `selfpipe_*` | Graceful shutdown, log rotation, state dump |

```
game thread ──ring_push()──> ring buffer ──ring_drain()──> logshipper thread
                                │                              │
                          full → drop++                   unixsock_dgram_send_nonblock()
                                │                              │
                     tetrisctl dropped-logs                    ▼
                                                          tetrislogd
                                                       (unixsock_dgram_recv)
```


---

## Project Structure

```
lib/libcoreipc/
├── Makefile                    make -C lib/libcoreipc [test|clean|fclean|re]
├── include/
│   └── coreipc.h               Public header — consumers add -I lib/libcoreipc/include
├── src/
│   ├── ring_buffer.c           MPSC ring, non-blocking push, atomic drops
│   ├── unix_dgram.c            AF_UNIX datagram bind, send, receive
│   ├── unix_stream.c           AF_UNIX stream listen, accept, connect, send/recv-all
│   ├── fd_signal.c             Non-blocking fds, socket teardown, self-pipe
│   └── msgqueue.c            POSIX message queue wrappers
├── tests/                      One test_<module>.c per source module
├── scripts/
│   └── run_tests.sh            Formatted test runner
├── obj/                        Generated objects (git-ignored)
└── libcoreipc.a                Generated archive
```



