# libcoreipc stub implementation — working plan (temp, not committed... except this
# revision, which the user asked to be committed as a record of the mac-compat decision)

Workflow per increment: opencode (deepseek-v4-flash-free) writes the file -> Claude
builds/tests/valgrinds/reviews -> Claude reports -> wait for user go-ahead -> commit + push
-> next increment.

Command template:
```
opencode run -m opencode/deepseek-v4-flash-free --dir lib/libcoreipc --auto "<prompt>"
```

Verify template, in order, stop at first failure:
```
make -C lib/libcoreipc re
make -C lib/libcoreipc test FILTER=<module>
valgrind --leak-check=full --error-exitcode=1 <test binary>
make -C lib/libcoreipc test        # full regression
```

Fixed constraints every increment must satisfy (from CLAUDE.md): no printf/logging/exit(),
errno-style returns only, C11 -Wall -Wextra -Werror clean, no hard-coded paths, valgrind
clean, no mutex held across a blocking syscall.

## Cross-platform decision (2026-07-30, agreed with teammate)

`coreipc.h` and this Makefile currently make the *entire* library mac-incompatible, not
just mq_helpers.c: `coreipc.h` unconditionally `#include <mqueue.h>` (doesn't exist on
Darwin/macOS at all -- no kernel support, not just a missing flag), and the Makefile
unconditionally links `-lrt` (doesn't exist on macOS either). Both get fixed as part of
increment 5, below -- which also retroactively makes increments 1-4 natively
mac-buildable (no more Docker needed just to compile; still using Docker for valgrind
verification since it's unreliable on modern macOS regardless, per root README:64).

`mq_helpers.c` itself gets a real redesign, not just a portability patch: implemented
in-process on top of `rb_*` (ring_buffer.c, increment 2) + `pthread_mutex_t` +
`pthread_cond_t`, instead of wrapping real POSIX message queue syscalls. One
implementation, no platform `#ifdef`s, identical behavior on Linux/macOS/WSL2.

Tradeoff, discussed and accepted: this only works for openers within the *same process*.
Two separate processes calling `mqh_open` with the same name won't see each other's
messages -- only threads within one process share a queue. Confirmed via
docs/use_cases.md + README.md:472 (room_t.mutex) that tetrisd is single-process,
multi-threaded (rooms are mutex-guarded structs, not separate processes), and the
`libcoreipc/README.md`-documented "Room -> room garbage" channel that uses mqh_* is
intra-process already -- so this costs nothing today. If a future design ever needs two
separate binaries to share a named queue, that would need re-visiting (real kernel mq
on Linux + some other cross-process primitive on macOS). Not needed by anything in the
current architecture.

Upside, not just a tradeoff: no kernel-imposed queue-count/size limits to tune
(`/proc/sys/fs/mqueue/*` on Linux defaults are fairly restrictive), and no syscall
overhead per send/recv since both ends are already in the same address space -- likely
*faster* than real POSIX mq would have been for this specific channel.

This also means `tests/test_mq_helpers.c`'s `mqueue_available()` gate (currently
`access("/dev/mqueue", F_OK)`, which is always false on macOS, so the test permanently
self-skips there) needs to change as part of increment 5 -- otherwise the new
implementation can never actually get exercised by the real test suite on mac, only by
a throwaway scratch harness. This is a deliberate, disclosed exception to the "never
touch tests/" rule below, scoped to exactly that one gate function.

## Per-file rule (still holds for increments 1-4)

Edit only the one target src/*.c file -- coreipc.h, tests/*, Makefile stay untouched.
(Increment 5 is the documented exception above.)

## Increment 1 — fd_signal.c [DONE, commit 39da4fc]
Functions: us_set_nonblock, us_close_unlink, sp_pipe, sp_notify, sp_drain
Test: tests/test_fd_signal.c (FILTER=fd_signal)
Notes: O_NONBLOCK must be added without clobbering existing flags (F_GETFL then OR).
sp_pipe sets O_NONBLOCK|O_CLOEXEC on both ends via portable pipe()+fcntl() (no pipe2 --
unavailable on macOS, needs _GNU_SOURCE on Linux which this build doesn't define).
sp_notify must be signal-handler-safe (write() only, no malloc). sp_drain reads until
EAGAIN or EOF (n==0) -- coalesces N wakeups to one, doesn't spin if the write end closes.

## Increment 2 — ring_buffer.c
Functions: rb_init, rb_push, rb_pop, rb_drain, rb_drops, rb_destroy
Test: tests/test_ring_buffer.c + tests/test_ring_buffer_mt.c (FILTER=ring_buffer)
Notes: capacity+1 slots (full/empty disambiguation via one wasted slot), mutex around
push/pop/drain, atomic drop counter (increment on push-into-full, no lock needed for
rb_drops read). MT test: 4 producers x 10k, 1 consumer, popped+dropped==pushed, no dupes.
This becomes the backing store for the new mq_helpers.c in increment 5, so its mutex
discipline and full/empty accounting need to be solid before increment 5 starts.

## Increment 3 — unix_dgram.c
Functions: us_dgram_bind, us_dgram_open, us_dgram_send_nb, us_dgram_recv
Test: tests/test_unix_dgram.c (FILTER=unix_dgram)
Depends on: us_set_nonblock (increment 1, already implemented by this point).
Notes: bind unlinks stale socket file first, chmod after bind, non-blocking. send uses
MSG_DONTWAIT|MSG_NOSIGNAL; EAGAIN/ECONNREFUSED both mean dropped (return -1, not a crash).
recv truncates past buflen, EAGAIN when empty.

## Increment 4 — unix_stream.c
Functions: us_stream_listen, us_stream_accept, us_stream_connect, us_send_all, us_recv_all
Test: tests/test_unix_stream.c (FILTER=unix_stream)
Depends on: us_set_nonblock (increment 1).
Notes: listen mode arg matters (0600 for ctl_socket = entire authz model for tetrisctl,
per README -- do not default/ignore it). accept retries on EINTR. send_all loops over
short writes with MSG_NOSIGNAL. recv_all: peer closing early during read == EPIPE, not a
short-read return.

## Increment 5 — portability fix + mq_helpers.c (redesigned)

Two parts, done together:

**5a. Portability fix** (coreipc.h + Makefile):
- coreipc.h: drop the unconditional `#include <mqueue.h>`. Define our own `mqd_t`
  (plain `int`, matching what the tests already assume via `(mqd_t)-1` sentinel
  comparisons) so no platform ifdef is needed anywhere, on any file.
- Makefile: strip `-lrt` on Darwin, mirroring the root Makefile's existing `UNAME_S`
  detection pattern. `-lpthread` stays (needed on both, and now genuinely used by
  mq_helpers.c's cond var).
- Re-verify increments 1-4 still build + pass + valgrind-clean after this change, on
  both the Linux container *and* natively on this Mac now that the header no longer
  blocks compilation there.

**5b. mq_helpers.c** (in-process redesign, see decision above):
Functions: mqh_open, mqh_send_nb, mqh_recv_nb, mqh_recv_timed, mqh_close, mqh_unlink
Test: tests/test_mq_helpers.c (FILTER=mq_helpers) -- mqueue_available() gate rewritten
(no longer checks /dev/mqueue; the new backend doesn't need it) so the suite actually
runs on macOS instead of permanently skipping there.
Notes:
- Small in-process table of open queues (name -> entry), each entry owning an `rb_*`
  ring buffer sized to (maxmsg, msgsize) plus its own mutex/cond var.
- mqh_open: EINVAL if name doesn't start with '/'. Find-or-create the named entry.
- mqh_send_nb / mqh_recv_nb: rb_push / rb_pop on the entry's ring buffer -- EAGAIN on
  full/empty is already exactly what rb_* gives, no separate counting needed.
- mqh_recv_nb: EMSGSIZE (not truncate) when the caller's buffer is smaller than msgsize.
- mqh_recv_timed: pthread_cond_timedwait with an absolute deadline, woken by
  mqh_send_nb signaling the cond var after a successful push -- must actually park
  (test asserts a time floor), not spin-poll. Never called with an external lock held
  (README constraint) -- and must not introduce one of its own that could be held
  across the wait.
- mqh_unlink: remove the table entry, rb_destroy it. Second unlink on the same name ->
  ENOENT (test asserts this).

## Verification going forward

Now that the portability fix (5a) lands, verify every increment (including
re-verifying 1-4) both:
- natively on this Mac (compile + run the real test suite directly, no Docker), and
- inside the tetrish-libcoreipc-ci Linux container (also stands in for WSL2, which runs
  a real Linux kernel, not a translation layer).
valgrind stays container-only (unreliable on modern macOS per root README:64).

## Status
- [x] Increment 1: fd_signal.c (commit 39da4fc, pushed)
- [ ] Increment 2: ring_buffer.c
- [ ] Increment 3: unix_dgram.c
- [ ] Increment 4: unix_stream.c
- [ ] Increment 5: portability fix (coreipc.h + Makefile) + mq_helpers.c redesign
