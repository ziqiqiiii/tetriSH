# tetrisd

The concurrent, server-authoritative game server for tetriSH. Owns accounts, the lobby, and every live game, and is the only process that decides what happened.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Signals](#signals)
- [Configuration](#configuration)
- [Routes](#routes)
- [Bodies](#bodies)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [References](#references)

---

## Features

- Server-authoritative Single mode end to end: signup, login, lobby, room, live game
- One reactor thread owns every established connection; a bounded pool runs the blocking handshake; one ticker thread per in-game room
- Cert-authenticated encrypted sessions via `libtetrissh` — refuses to boot without a certificate
- Server-pushed `STATE` snapshots on a fixed tick, coalesced through a one-slot mailbox
- Per-connection input rate limiting (token bucket), answering `429` with `Retry-After`
- Non-blocking log shipping to `tetrislogd`; records are dropped and counted, never waited on

---

## Prerequisites

Install the shared build dependencies and mint a development certificate from the repository root:

```bash
make -C ../.. deps
make -C ../.. certs
```

`make certs` is once per clone — it produces the dev CA and server certificate that `TETRISD_CERT_PATH` and `TETRISD_KEY_PATH` point at.

---

## Build

```bash
make -C src/tetrisd
```

This builds the seven CoreStack archives in place and links them with OpenSSL, pthreads, and librt into `src/tetrisd/tetrisd`.

| Command | Description |
|---|---|
| `make -C src/tetrisd` | Build the daemon (`make all`) |
| `make -C src/tetrisd libs` | Build the library archives only |
| `make -C src/tetrisd run` | Build, `cd` to the project root, and launch |
| `make -C src/tetrisd test` | Build and run every suite |
| `make -C src/tetrisd clean` | Remove object files |
| `make -C src/tetrisd fclean` | Remove object files and binaries |
| `make -C src/tetrisd re` | Full rebuild (`fclean` + `all`) |

---

## Run

**Run it from the project root.** Paths in `.tetrishrc` are root-relative — not relative to the rc file or the binary — so launching from elsewhere fails the certificate check. `$TETRISHRC` changes only which file is *read*, never what its paths mean.

```bash
make -C src/tetrisd run
```

Or, the way `.tetrishrc` does it, through the lifecycle manager:

```bash
tetrisctl start tetrisd
tetrisctl status
tetrisctl stop tetrisd
```

`tetrisd` daemonises itself: `main.c` double-forks, claims the pidfile named by `TETRISD_PID_PATH`, and reports itself ready only once the listener is up. Running the binary by name therefore returns to the prompt when the server is actually serving — and exits non-zero, with the reason printed, when it is not. It refuses to boot when `cert_path` or `key_path` cannot be read *or parsed* — both are loaded once at boot and held for the server's lifetime — so it can never silently serve players unauthenticated, and no connection waits on the disk to be authenticated. A `SIGHUP` does not reload them; the listener they authenticate is already open.

The fork lives in `main.c` alone and never behind `server_start`, or the integration suite — which runs a real server in-process on port 0 — would begin forking.

---

## Signals

| Signal | Effect |
|---|---|
| `SIGTERM`, `SIGINT` | Leave the loop, stop the handshake pool, join every ticker, end every client, close the store |
| `SIGHUP` | Re-read `.tetrishrc`; applies the log level and tick period |
| `SIGUSR1` | Dump server state to the log: settings, connections, rooms, live games, log drop count |
| `SIGPIPE` | Ignored — a vanished client kills its own connection, not the server |

---

## Configuration

Every path and setting comes from `.tetrishrc`, resolved as `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variables. Each is an ordinary `export` line, so the shell exports it into the daemon's environment *and* `tetrisd` parses the same file itself. `tetrisctl` hands the file it resolved on as `argv[1]`, so both ends always read the same one.

| Key | Default | Meaning |
|---|---|---|
| `TETRISD_PORT` | `4242` | TCP port; `0` lets the kernel choose (tests) |
| `TETRISD_DATA_DIR` | `tmp/tetrisd` | Where the player store keeps its append-only log |
| `TETRISD_CONFIG_DIR` | `lib/libmacminidb/config` | Read-only character and theme catalogues |
| `TETRISD_CERT_PATH` | `certs/server.crt` | Server certificate (required) |
| `TETRISD_KEY_PATH` | `certs/server.key` | Server private key (required) |
| `TETRISD_CA_PATH` | `certs/ca.crt` | CA clients verify the server against |
| `TETRISD_LOG_IPC` | `tmp/tetrisd/tetrislogd.sock` | Datagram socket `tetrislogd` binds |
| `TETRISD_LOG_LEVEL` | `info` | `debug`, `info`, `warning`, `error` |
| `TETRISD_PID_PATH` | `tmp/tetrisd/tetrisd.pid` | Pidfile claimed and `flock`ed for the process's lifetime; the single-instance guard, and what `tetrisctl` signals |
| `TETRISD_ERR_PATH` | `tmp/tetrisd/tetrisd.err` | Where stderr goes once boot has succeeded — the last-resort copy of records the logger could not take |
| `TETRISD_MAX_CLIENTS` | `64` | Simultaneous connections; beyond it, accepts are refused |
| `TETRISD_TICK_MS` | `12` | Room ticker period |
| `TETRISD_BR_SLOTS` | `4` | Slots in a Battle Royale room (2–16) |
| `TETRISD_INPUT_BURST` | `120` | Per-connection input tokens held at once |
| `TETRISD_INPUT_RATE` | `60` | Input tokens refilled per second |
| `TETRISD_HANDSHAKE_WORKERS` | `4` | Threads running the blocking handshake (1–64). Sized against the connection *arrival* rate, not the client count — a handshake is brief and one-off |
| `TETRISD_HANDSHAKE_TIMEOUT_MS` | `5000` | Budget for one handshake, from the moment a worker starts it (100–60000). The other half of the guarantee above: the pool is what makes a slow peer everyone's problem, and this is what stops it |

---

## Routes

For now, the server serves Single mode. Every route below `LOGIN` requires a `Player-Id` header matching the player bound to the connection; a mismatch is `401`, never a guess.

| Method | Path | Success | Refusals |
|---|---|---|---|
| `SIGNUP` | `/account` | `201` | • `409` name taken <br>• `400` bad body |
| `LOGIN` | `/session` | `200` + `Player-Id` | •  `401` bad credentials <br>• `409` connection already bound <br>• `503` previous connection would not release the player |
| `LIST` | `/rooms` | `200` room rows | •  `401` |
| `JOIN` | `/rooms` | `201` room created <br>• caller is owner |  • `400` unknown mode <br>• `409` `lobby-full` / `already-in-room` |
| `JOIN` | `/room/<name>` | `200` seated |  • `404` no such room <br>• `409` `full` / `in-game` |
| `LEAVE` | `/room/<name>` | `200` |  • `404` not in that room |
| `START` | `/room/<name>` | `200` |  • `403` `not-owner` <br>• `409` `too-few-players` / `already-started` |
| `MOVE` | `/room/<name>/player/<pid>` | `200` |  • `409` `input-blocked` <br>• `403` not your board <br>• `400` bad body <br>• `429` too fast |
| `ROTATE` | `/room/<name>/player/<pid>` | `200` | as `MOVE` |
| `DROP` | `/room/<name>/player/<pid>` | `200` | as `MOVE` |
| `STATE` | `/room/<name>/player/<pid>` | **server-pushed** | — |

Rooms are never named by clients: the lobby assigns `S-01`, `D-02`, `BR-03`, so creation addresses the collection (`JOIN /rooms`) and every later call addresses the room by its assigned name. Finishing a game clears every slot, so the room empties and returns to the lobby — the next game means a fresh room, not a seat kept.

A refusal carries its verdict as a `reason <verdict>` body line: a player who cannot tell a full room from one already in game cannot act on the answer. A malformed message is `400`, an unknown method `501`. `413` answers a message that parses as oversized; a *frame* beyond the 64 KiB session cap cannot be decrypted at all, so `libtetrissh` drops that connection before HTTTP sees it.

---

## Bodies

Requests and responses use the same plaintext `key value` lines the status bodies use; input commands are a single upper-case word.

```text
SIGNUP /account          username amber\npassword hunter2\n
LOGIN /session           username amber\npassword hunter2\n
JOIN /rooms              mode single|double|br\n
MOVE .../player/<pid>    LEFT | RIGHT
ROTATE .../player/<pid>  CW | CCW
DROP .../player/<pid>    SOFT | HARD
```

A request carrying a body must declare `Content-Type: application/tetris-command` or it is `400`. Responses are `application/tetris-status` and `STATE` pushes `application/tetris-state` — kept distinct so a message's direction is legible from its type alone.

`LIST` answers with a `libstatusbody` room-row body; `STATE` carries a `libstatusbody` snapshot. Passwords are salted and SHA-256 hashed here — the store never sees one.

---

## Architecture

### Threading model

```text
reactor        epoll_wait(listener, wake pipe, every client)
               reads, opens frames, dispatches, seals, writes - owns all of it
handshake pool TETRISD_HANDSHAKE_WORKERS threads, each blocked in one handshake
per room       ticker thread   gravity on a fixed tick, then STATE snapshots
logging        shipper thread  drains the ring, datagrams records to tetrislogd
```

One thread owns the lobby, every room, every game, the registry and every outbox. The seam is the handshake, the one call that genuinely blocks; why it sits exactly there is [ADR-0008](../../docs/adr/0008-tetrisd-is-event-driven.md). Two rules the code cannot state for itself:

- **No client is freed inside the event loop.** `epoll_event.data.ptr` carries the `t_client *`, and a batch can hold several events for one client, so `client_kill` only unlinks and parks it on the zombie list; the drain after the batch is the sole `free()` site. Breaking this is a use-after-free no test would catch.
- **A handshake has a total budget**, `TETRISD_HANDSHAKE_TIMEOUT_MS`, enforced by the reactor over and above the matching `SO_RCVTIMEO`/`SO_SNDTIMEO`. The socket option bounds one `recv`, and `libtetrissh` loops until it has its bytes — so a peer dribbling a byte per timeout would hold a pool worker forever, and enough of them stop every login server-wide. The clock starts when a worker picks the connection up, never at accept: charging queue time would make whoever queues behind an attacker inherit the attacker's leftovers.

Each client owns a bounded response FIFO plus a one-slot `STATE` mailbox. Overflowing the FIFO closes the connection — a client that cannot keep up must not grow the server's memory — while snapshots overwrite, so a stalled peer loses intermediate frames and never delays a room's ticker. The reactor pops, seals, and writes; a short write leaves a cursor in the send buffer and arms `EPOLLOUT`, since `session_send`'s retry loop is not available to a thread that drives every other connection too.

Gravity accumulates real elapsed time against each player's own `gravity_interval_ms(level)`, so a descheduled ticker catches up rather than slowing the game down. Inputs mark the game dirty under the room's mutex; the ticker alone encodes and pushes, so inputs and gravity produce one `STATE` stream instead of two racing ones. A ticker enqueues and pokes the wake pipe — it never touches a socket — so the reactor is what turns a snapshot into bytes.

### Lock order

```text
lobby_mutex  >  room->mutex  >  registry rwlock  >  outbox mutex
```

Strictly descending, never re-entered upward. No `db_*`, `session_*`, or IPC send happens under any lock — the outbox push is the sole exception, and it cannot block. Database writes happen after the room mutex is released.

Every lock here exists only because room tickers are still threads. Step 4 of ADR-0008 replaces them with one `timerfd` and step 5 deletes the order entirely; until then it is load-bearing and is not to be relaxed a lock at a time. One rule sits underneath it and is easy to break without breaking the order: a room is guarded by *its own* mutex everywhere, including while the lobby destroys it. Found by `-fsanitize=thread`, not by lock-order review.

### Client lifetime

The registry's rwlock keeps a room ticker from enqueueing into a client that is going away: the ticker holds the read lock across its push, unlinking takes the write lock. It says nothing about memory, which the reactor alone owns. Teardown runs on the loop, in one order:

1. drop the descriptor from the epoll set
2. forfeit — leaving, topping out, and a dropped connection are the same event
3. take the write lock, unlink from the registry, release it
4. `shutdown(fd)`, close the outbox, park the client on the zombie list — the drain after the batch closes the socket and frees it

An empty room returns to the lobby. A player holds at most one connection, so a `LOGIN` for an already-connected player closes the older one — synchronously, in the handler, because the loop owns both. The old connection has forfeited its room and unlinked itself before the new one binds, so the wait once needed between two client threads is gone rather than ported.

### Logging

Every significant event becomes a `libcoreipc` log record pushed into a ring buffer. The shipper thread drains it and datagrams each record to `tetrislogd`, falling back to stderr and retrying while the logger is unreachable. Under pressure records are **dropped and counted**, never waited on — `ring_dropped_count()` is the number `tetrisctl dropped-logs` reports.

---

## Project Structure

```text
src/tetrisd/
├── include/tetrisd.h     Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            Thin shim over server_start / server_stop
│   ├── config.c          .tetrishrc parsing and validation
│   ├── logger.c          Ring buffer, shipper thread, stderr fallback
│   ├── listener.c        Listening socket: open and accept
│   ├── clock.c           Wall-clock and monotonic milliseconds
│   ├── server.c          Bring-up, the reactor thread, shutdown, SIGHUP reload
│   ├── reactor.c         The event loop: dispatch, accept, sweep, wind-down
│   ├── signals.c         Handlers: a flag and one byte down the self-pipe
│   ├── dump.c            SIGUSR1 state dump
│   ├── registry.c        Client registry, the lifetime guard
│   ├── client.c          Client lifetime: spawn, adopt, kill, reap
│   ├── clientio.c        Frame in, frame out: the reactor's data path
│   ├── handshake_pool.c  Bounded worker pool for the blocking handshake
│   ├── bytes.c           Growable byte buffer with a cursor
│   ├── outbox.c          Bounded FIFO + latest-STATE mailbox
│   ├── dispatch.c        Frame → route → response, status mapping
│   ├── handlers_account.c  SIGNUP, LOGIN, password hashing
│   ├── handlers_lobby.c    LIST, JOIN, LEAVE, START
│   ├── handlers_input.c    MOVE, ROTATE, DROP
│   ├── game.c            The per-player game aggregate over libtetrisbrain
│   └── room.c            Room runtime, ticker, STATE push, forfeit
├── tests/                harness.c is the headless client; test_*.c the suites
├── scripts/run_tests.sh
└── Makefile              → src/tetrisd/tetrisd
```

`t_game` is the aggregate `libtetrisbrain` deliberately does not own — board, piece, bag, score, charge, and effects, with the ordering rules between them enforced in one place.

---

## Testing

Eight suites, each driving a real server in-process on port `0`:

```bash
make -C src/tetrisd test
make -C src/tetrisd test FILTER=game    # only suites matching "game"
```

Valgrind proves teardown:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrisd/tests/bin/test_shutdown
```

It says nothing about data races, so the suite is also rebuilt under ThreadSanitizer. Fewer threads remain than before, but the ones that do — the handshake pool and the room tickers — are exactly the ones that reach across into state the reactor owns, so this stays non-negotiable:

```bash
make -C lib/<each> re FLAGS="<its flags minus -Werror> -fsanitize=thread -g -O1"
make -C src/tetrisd test FLAGS="-Wall -Wextra -fsanitize=thread -g -O1" \
     LDLIBS="-lssl -lcrypto -lpthread -lrt -fsanitize=thread"
```

Both are expected to be clean.

---

## References

- [HTTP Server from scratch in C](https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa)
- [Concurrent Servers Design](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)
