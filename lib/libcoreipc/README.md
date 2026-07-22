# libcoreipc — Scope & Implementation Plan

> Status: **planning** — no code yet. This document is the agreed scope for the
> library before implementation starts (sprint S1, week 4). Owner: Sanjan
> (see `AGENTS.md`).

`libcoreipc` is the corestack IPC library: thin wrappers around the three IPC
mechanisms every tetriSH daemon uses. It has **no internal dependencies** and
is built first; `libtetrissh` consumes it, and it is linked into `tetrisd`,
`tetrislogd`, `tetrisctl` and `tetrisu`.

Sources of truth: `docs/corestack.md` §8, `docs/requirements.md` (NFR-CO4/CO5,
sprint table S1), `docs/checkoff-prep.md` (ring buffer question),
`docs/architecture.md` (logging pipeline, Battle Royale garbage flow).

---

## Scope

Three modules plus an atomic drop counter — "thin wrappers", not a framework:

| Module | Provides | First consumer |
|---|---|---|
| `src/ring_buffer.c` | In-process MPSC bounded ring for log records; non-blocking push; atomic drop counter | `tetrisd` logshipper pipeline |
| `src/unix_socket.c` | `AF_UNIX` helpers: DGRAM bind/send (non-blocking), STREAM listen/connect, send/recv-all loops | `tetrislogd`, `tetrisctl` control plane, `libtetrissh` |
| `src/mq_helpers.c` | POSIX message queue wrappers with `O_NONBLOCK` drop semantics | Battle Royale garbage (`/tetris-garbage`, sprint S7) |

### Library rules (inherited from the project)

- **No hard-coded paths.** Socket paths and queue names always come from the
  caller (ultimately `.tetrishrc`).
- **No logging, no printf, no exit()** inside the library — this *is* the log
  path; it must never recurse into itself. Errno-style returns only.
- Compiles clean under `-Wall -Wextra -Werror`; all test binaries pass
  `valgrind --leak-check=full --error-exitcode=1` (NFR-R4).

---

## Layout (clone of libtetrisbrain conventions)

```
lib/libcoreipc/
├── Makefile                 # NAME := libcoreipc.a, HEADER := include/coreipc.h
├── README.md                # this file
├── include/coreipc.h        # single public header
├── src/
│   ├── ring_buffer.c
│   ├── unix_socket.c
│   └── mq_helpers.c
├── tests/
│   ├── test_ring_buffer.c
│   ├── test_ring_buffer_mt.c
│   ├── test_unix_socket.c
│   └── test_mq_helpers.c
└── scripts/run_tests.sh     # copied from libtetrisbrain
```

Makefile deltas vs libtetrisbrain: test link line needs `-lpthread` (mt test)
and `-lrt` (POSIX mq on older glibc).

---

## Module 1 — ring_buffer (build first)

MPSC bounded ring of **fixed-size records copied by value** — any thread
pushes, only the logshipper thread pops. By-value, not pointers: pointer
queues create ownership/lifetime questions across threads.

```c
typedef struct s_ring_buffer
{
    unsigned char        *slots;        /* capacity * record_size */
    size_t               record_size;
    size_t               capacity;
    size_t               head;          /* consumer reads here  */
    size_t               tail;          /* producers write here */
    pthread_mutex_t      mutex;         /* v1 — see decision below */
    atomic_uint_fast64_t drops;
}   t_ring_buffer;

int      rb_init(t_ring_buffer *rb, size_t record_size, size_t capacity);
int      rb_push(t_ring_buffer *rb, const void *record);  /* 0, or -1 = full → drops++ */
int      rb_pop(t_ring_buffer *rb, void *out);            /* 0, or -1 = empty          */
size_t   rb_drain(t_ring_buffer *rb, void *out, size_t max_records);
uint64_t rb_drops(const t_ring_buffer *rb);               /* atomic load, no lock      */
void     rb_destroy(t_ring_buffer *rb);
```

Semantics locked in by the checkoff prep doc:

- `rb_push` is O(1), **never blocks, never retries**. Full ⇒ return `-1` and
  `atomic_fetch_add(&drops, 1)` (NFR-CO5 observable-drop guarantee).
- Full check is `(tail + 1) % capacity == head` — this sacrifices one slot.
  Allocate `capacity + 1` slots internally so the caller-visible capacity is
  exact.
- `rb_drops` must be readable without the lock (plain atomic load) — that is
  the `tetrisctl dropped-logs` path.

**Decision — "lock-free" vs mutex.** NFR-CO5 says "lock-free", but the README
lock table lists `log_buffer.mutex`. **v1 ships mutex-protected push/pop with
the atomic drop counter** — the critical section is a `memcpy` of ≤ 256 B and
is trivially helgrind-clean. The API is frozen so internals can change; if
time remains in S8, upgrade to a real lock-free MPMC (Vyukov bounded queue
with per-slot sequence numbers is the reference design). Amend NFR-CO5 wording
to "non-blocking push" to match whichever ships.

---

## Module 2 — unix_socket

Thin `us_`-prefixed wrappers so callers never hand-roll `sockaddr_un`:

```c
int us_dgram_bind(const char *path, mode_t mode);  /* unlink stale → socket → bind → chmod */
int us_dgram_open(const char *path);               /* connect()ed DGRAM sender             */
int us_dgram_send_nb(int fd, const void *buf, size_t len);
        /* MSG_DONTWAIT; 0 ok; -1 = would-block / no receiver → CALLER counts the drop */
int us_stream_listen(const char *path, int backlog, mode_t mode);
int us_stream_connect(const char *path);
int us_send_all(int fd, const void *buf, size_t len);   /* short-write loop, MSG_NOSIGNAL */
int us_recv_all(int fd, void *buf, size_t len);
int us_set_nonblock(int fd);
```

- The `mode` parameter on bind/listen is the control-plane security boundary
  (`0600` for `ctl_socket`) — explicit, not an afterthought.
- `us_dgram_send_nb` owns **no** drop counter — `tetrisd` counts `logd_drops`,
  rooms count `garbage_drops`; the wrapper stays policy-free.
- `us_send_all` uses `MSG_NOSIGNAL` so the library never depends on the caller
  having ignored `SIGPIPE`.
- All functions return fd/0 on success or -1 with `errno` preserved.

---

## Module 3 — mq_helpers (needed last — Battle Royale is S7)

`mqh_`-prefixed to avoid colliding with the real POSIX `mq_*` names:

```c
mqd_t   mqh_open(const char *name, long maxmsg, long msgsize, mode_t mode);
        /* O_CREAT | O_RDWR | O_NONBLOCK, attr built from args */
int     mqh_send_nb(mqd_t q, const void *msg, size_t len);  /* 0, or -1 = full → caller drops */
ssize_t mqh_recv_nb(mqd_t q, void *buf, size_t buflen);     /* >0 len, or -1 = empty */
int     mqh_close(mqd_t q);
int     mqh_unlink(const char *name);
```

- Queue names (e.g. `/tetris-garbage`) come from callers via `.tetrishrc`,
  never defaulted here.
- Receive buffers must be ≥ `mq_msgsize` (else `EMSGSIZE`); payload is always
  `game_event_t`, so callers size queues with `msgsize = sizeof(game_event_t)`.

---

## Tests

Each test has its own `main()`, links `libcoreipc.a`, and runs via
`make -C lib/libcoreipc test [FILTER=ring_buffer]`.

| Test | Proves |
|---|---|
| `test_ring_buffer.c` | FIFO order, wraparound past capacity, full ⇒ -1 and `rb_drops` increments, pop-empty ⇒ -1, drain batching |
| `test_ring_buffer_mt.c` | 4 producers × 10k stamped records + 1 consumer: `popped + dropped == pushed`, no duplicate/corrupt records. S8 helgrind target |
| `test_unix_socket.c` | DGRAM roundtrip under a `mkdtemp()` path; send to unbound path ⇒ -1 (`ECONNREFUSED`); re-bind over stale socket file; STREAM roundtrip of a 100 KiB buffer (forces short writes) |
| `test_mq_helpers.c` | `maxmsg=4`: four sends succeed, fifth ⇒ -1/`EAGAIN` without blocking, drain in order, unlink. Skip-with-notice if `/dev/mqueue` unavailable |

---

## Milestones

1. **Scaffold** — copy Makefile/runner from libtetrisbrain, rename; empty
   header compiles into an archive.
2. **ring_buffer** + both tests — unblocks the logshipper design and the
   checkoff question.
3. **unix_socket** + test — unblocks `tetrislogd`, `tetrisctl`, and
   `libtetrissh` socket usage.
4. **mq_helpers** + test — can slip without blocking anything until S7.
5. **Hardening + docs** — valgrind on all test bins, helgrind on the mt test,
   fill the root README's IPC-Design placeholders with the now-real mechanisms.

---

## Doc inconsistencies to resolve alongside this library

1. **NFR-CO5 "lock-free" vs `log_buffer.mutex`** in the root README lock
   table — align both with the v1-mutex story above.
2. **`tetrislogd`'s link line** (`docs/requirements.md` binary table) lists no
   corestack libs, but `docs/corestack.md` §8 says all daemons use the Unix
   socket helpers. Recommendation: add `libcoreipc` to `tetrislogd` —
   `us_dgram_bind()` is exactly its receive path, and duplicating it defeats
   the library's purpose.
