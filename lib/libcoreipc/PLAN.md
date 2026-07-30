# libcoreipc implementation plan

Workflow per increment: opencode writes the file -> build + test -> report results ->
wait for user approval -> commit + push -> next increment.

Verify template (stop at first failure):
```
make -C lib/libcoreipc re
make -C lib/libcoreipc test FILTER=<module>
make -C lib/libcoreipc test        # full regression
```

Fixed constraints every increment must satisfy (from CLAUDE.md): no printf/logging/exit(),
errno-style returns only, C11 -Wall -Wextra -Werror clean, no hard-coded paths, valgrind
clean, no mutex held across a blocking syscall.

## Cross-platform decision

`coreipc.h` and the Makefile currently make the *entire* library mac-incompatible:
`coreipc.h` unconditionally `#include <mqueue.h>` (doesn't exist on macOS at all), and
the Makefile unconditionally links `-lrt` (doesn't exist on macOS either). Both get
fixed as part of increment 5 — which also retroactively makes increments 1-4 natively
mac-buildable.

Key macOS-aware implementation choices:
- `sp_pipe`: uses portable `pipe()` + `fcntl()` (no `pipe2` — unavailable on macOS,
  needs `_GNU_SOURCE` on Linux which this build doesn't define)
- Socket sends in increments 3-4: `MSG_NOSIGNAL` guarded via `#ifdef __linux__` with
  `SO_NOSIGPIPE` setsockopt fallback on macOS

`mq_helpers.c` itself gets a real redesign, not just a portability patch: implemented
in-process on top of `rb_*` (ring_buffer.c) + `pthread_mutex_t` + `pthread_cond_t`,
instead of wrapping real kernel POSIX message queue syscalls. One implementation,
no `#ifdef`s, identical behavior on Linux/macOS/WSL2.

Tradeoff, discussed and accepted: this only works for queues within the *same process*.
Two separate processes calling `mqh_open` with the same name won't see each other's
messages — only threads within one process share a queue. Confirmed via
docs/use_cases.md + README.md:472 (`room_t.mutex`) that `tetrisd` is single-process,
multi-threaded (rooms are mutex-guarded structs, not separate processes), and the
"Room → room garbage" channel that uses `mqh_*` is intra-process already — so this
costs nothing today. If a future design ever needs two separate binaries to share a
named queue it would need re-visiting, but nothing in the current architecture requires
this.

Upside: no kernel-imposed queue-count/size limits (`/proc/sys/fs/mqueue/*` defaults on
Linux are fairly restrictive), and no syscall overhead per send/recv since both ends
are already in the same address space — likely faster than kernel mq for this channel.

## Per-increment rule

Edit only the target `src/*.c` file — `coreipc.h`, `tests/*`, `Makefile` stay untouched
until increment 5 (which is the documented exception that fixes portability and rewrites
the mqueue test gate).

## Dependency overview

```
          ┌─────────────┐
          │  fd_signal   │  (increment 1 — provides us_set_nonblock,
          └──────┬──────┘     us_close_unlink, sp_pipe, sp_notify, sp_drain)
                 │
    ┌────────────┼────────────┐
    ▼            ▼            ▼
┌──────────┐ ┌──────────┐ ┌────────────┐
│unix_dgram│ │unix_stream│ │ring_buffer │
│(incr 3)  │ │(incr 4)   │ │(incr 2)   │
└──────────┘ └──────────┘ └──────┬─────┘
                                 │
                          ┌──────▼──────┐
                          │  mq_helpers  │  (increment 5 — in-process redesign
                          └─────────────┘   using ring_buffer as backing store)
```

---

## Increment 1 — fd_signal.c

**Functions:** us_set_nonblock, us_close_unlink, sp_pipe, sp_notify, sp_drain

**Tests unlocked:** 6 of 7 in `test_fd_signal` (all except
`test_close_unlink_removes_socket_file` which additionally needs `us_dgram_bind`
from increment 3). **Also unblocks** `us_set_nonblock` / `us_close_unlink` needed
by dgram and stream.

| Function | Notes |
|---|---|
| `us_set_nonblock` | `fcntl(F_GETFL)` then `F_SETFL` with `O_NONBLOCK` OR'd in — preserves existing flags |
| `us_close_unlink` | `close(fd)`; `unlink(path)` treating ENOENT as success; reports first failure with its errno |
| `sp_pipe` | `pipe()` + `fcntl(F_SETFL, O_NONBLOCK)` + `fcntl(F_SETFD, FD_CLOEXEC)` on both ends. No `pipe2` |
| `sp_notify` | `saved = errno`; `write(fd, "\1", 1)`; `errno = saved`. Async-signal-safe |
| `sp_drain` | Loop `read()` until EAGAIN (empty, return 0) or real error. Retry EINTR |

---

## Increment 2 — ring_buffer.c [DONE]

**Functions:** rb_init, rb_push, rb_pop, rb_drain, rb_drops, rb_destroy

**Tests:** 10 passed (8 in `test_ring_buffer` + 2 in `test_ring_buffer_mt`).

| Function | Notes |
|---|---|
| `rb_init` | Validate args (EINVAL if 0), `calloc(capacity + 1, record_size)` — +1 sentinel distinguishes full from empty, `pthread_mutex_init`, `atomic_init(&rb->drops, 0)` |
| `rb_push` | Lock; `(tail+1) % (capacity+1) == head` → full → unlock, `atomic_fetch_add(&drops, 1)`, -1. Else memcpy, advance tail. Multi-producer safe |
| `rb_pop` | Lock; `head == tail` → empty → unlock, -1. Else memcpy, advance head. Single-consumer only |
| `rb_drain` | Lock once; batch pop up to `max_records` under one lock; return count. More efficient than calling rb_pop N times |
| `rb_drops` | `atomic_load` — no lock taken. NULL rb → 0. Used by `tetrisctl dropped-logs`, must never contend with producers |
| `rb_destroy` | `free(slots)`, `pthread_mutex_destroy`, `memset` zero. Safe on zeroed struct (slots==NULL → no-op) |

This is the backing store for the redesigned `mq_helpers.c` in increment 5, so the
mutex discipline and full/empty accounting are verified before that increment starts.

---

## Increment 3 — unix_dgram.c

**Functions:** us_dgram_bind, us_dgram_open, us_dgram_send_nb, us_dgram_recv

**Tests:** 7 in `test_unix_dgram`. **Also unlocks** the remaining 1 fd_signal test
(`test_close_unlink_removes_socket_file` needs both `us_dgram_bind` and
`us_close_unlink`).

**Depends on:** `us_set_nonblock` (increment 1) — called internally by bind and open.

| Function | Notes |
|---|---|
| `us_dgram_bind` | `socket(AF_UNIX, SOCK_DGRAM, 0)`; `unlink` stale (ENOENT ignored); fill `sockaddr_un` (length check → `ENAMETOOLONG`); `bind`; `chmod(mode)`; `us_set_nonblock` |
| `us_dgram_open` | Socket + fill `sockaddr_un` + `connect` + `us_set_nonblock`. Doesn't require receiver to exist yet (ECONNREFUSED surfaces on send) |
| `us_dgram_send_nb` | `send(fd, buf, len, MSG_DONTWAIT \| MSG_NOSIGNAL)`. Short sends impossible on DGRAM → EMSGSIZE. EAGAIN/ECONNREFUSED both mean dropped. `MSG_NOSIGNAL` via `#ifdef __linux__` |
| `us_dgram_recv` | `recv(fd, buf, buflen, 0)`. Retry EINTR. Truncates past buflen (datagram semantics). EAGAIN when empty |

---

## Increment 4 — unix_stream.c

**Functions:** us_stream_listen, us_stream_accept, us_stream_connect, us_send_all,
us_recv_all

**Tests:** 6 in `test_unix_stream`.

**Depends on:** `us_set_nonblock` and `us_close_unlink` (increment 1).

| Function | Notes |
|---|---|
| `us_stream_listen` | Socket + unlink stale + bind + `chmod(mode)` (mode is the authz model for `tetrisctl`, 0600 on ctl_socket — do not default/ignore it) + `listen(backlog)` |
| `us_stream_accept` | Loop `accept(listen_fd, NULL, NULL)` while EINTR |
| `us_stream_connect` | Socket + fill `sockaddr_un` + `connect`. ENOENT = no socket, ECONNREFUSED = no listener |
| `us_send_all` | Loop `send(fd, p, remaining, MSG_NOSIGNAL)`; advance pointer by returned count; retry EINTR. `MSG_NOSIGNAL` via `#ifdef __linux__` |
| `us_recv_all` | Loop `recv(fd, p, remaining, 0)`; retry EINTR. 0 return → peer closed early → `errno = EPIPE`, return -1 |

---

## Increment 5 — portability fix + mq_helpers.c (redesigned)

Two parts, done together:

**5a. Portability fix** (coreipc.h + Makefile):
- `coreipc.h`: drop `#include <mqueue.h>`. Define `mqd_t` as plain `int` (tests already
  use `(mqd_t)-1` sentinel comparisons, so this is type-compatible without preprocessor
  branches)
- Makefile: strip `-lrt` on Darwin via `UNAME_S` detection (mirroring the root Makefile's
  existing pattern). `-lpthread` stays (needed on both, and now genuinely used by
  mq_helpers.c's cond var)
- Re-verify increments 1-4 still build + pass on both Linux and native macOS

**5b. mq_helpers.c** — in-process on top of `rb_*` + `pthread_mutex_t` + `pthread_cond_t`:
- Small in-process table of open queues (name → entry), each entry owning an `rb_*`
  ring buffer sized to `(maxmsg, msgsize)` plus its own mutex/cond var
- No kernel mqueue calls — identical behavior on Linux/macOS/WSL2
- No `#ifdef` needed anywhere in the source

| Function | Notes |
|---|---|
| `mqh_open` | EINVAL if name doesn't start with `/`. Find-or-create the named entry in the table, init its ring buffer and cond var |
| `mqh_send_nb` | `rb_push` on entry's ring buffer — EAGAIN on full is already what `rb_*` returns, no separate counting. Signal cond var to wake waiters |
| `mqh_recv_nb` | `rb_pop`. EMSGSIZE if caller's buffer is smaller than msgsize (test asserts non-truncation) |
| `mqh_recv_timed` | `pthread_cond_timedwait` with absolute `CLOCK_REALTIME` deadline, woken by `mqh_send_nb` signaling after a successful push. Must actually park (test asserts a time floor ≥ 150ms), not spin-poll. Never hold an external lock across this wait |
| `mqh_close` | Table entry release + `rb_destroy` |
| `mqh_unlink` | Remove table entry + `rb_destroy`. Second unlink on same name → ENOENT (test asserts this) |

**Test change:** `test_mq_helpers.c`'s `mqueue_available()` gate switches from
`access("/dev/mqueue", F_OK)` to always-true (the new backend doesn't need kernel
support), so the suite runs on macOS for the first time instead of permanently skipping.

---

## Status

- [ ] Increment 1: fd_signal.c
- [x] Increment 2: ring_buffer.c (commit 395ea9f, pushed)
- [ ] Increment 3: unix_dgram.c
- [ ] Increment 4: unix_stream.c
- [ ] Increment 5: portability fix (coreipc.h + Makefile) + mq_helpers.c redesign
