# Daemons detach themselves; tetrisctl owns their lifecycle

_Status: accepted and implemented, except for `tetrisd`'s control socket. The
first version of `tetrisctl` — `start`, `status`, `stop`, `restart` by pidfile
and signal — and `lib/libcoredaemon` both exist; the control socket remains the
second step described below._

`tetrisd` and `tetrislogd` perform their own double-fork at start-up, publish
a locked pidfile, and are started, inspected and stopped through `tetrisctl`.
The shell's `dspawn`, `dcheck` and `dkill` revert to what they were built to
be — generic tools for daemonising arbitrary programs — and stop being the
lifecycle manager for the game daemons.

The reason is a blind spot that cannot be patched from outside. `dspawn`
writes its registry entry and *then* calls `execvp`, so it never learns
whether the program it launched actually started. A `tetrislogd` that loses
the race for its log-file lock exits immediately and leaves a registry entry
that `dcheck` renders as "down", with no cause recorded anywhere. Every
attempted fix — capturing the daemon's stderr to a file, checking `/proc`
before listing — treats a symptom of a spawner that cannot observe its target.

Detaching inside the daemon closes it, because the process that knows whether
boot succeeded is the one doing the booting. The parent holds a readiness pipe
and does not exit until the child either reports success or dies; a failed
`tetrisd` therefore prints its reason to the terminal and exits non-zero,
which is both visible to a person and testable in a script. The shell's
existing `daemon_spawn` looks like it already does this, and does not: its
pipe is closed on success *and* on death, so the parent cannot tell them apart
and always exits `EXIT_SUCCESS`. That is a quiet-pipe — it exists so the
shell's next prompt is drawn after the daemon's output — and the difference is
one byte written on success, which is why `libcoredaemon` carries its own
implementation rather than linking the shell's.

Reusing the shell's copy was considered and rejected on those grounds: making
it report failure changes semantics that `dspawn` and `dplant` depend on, it
would mean editing the very file this decision reverts, and it points a
dependency from the daemons at the shell's internals, inverting the layering
every other component follows.

## The pidfile is the instance guard

Each daemon writes `<name>.pid` after detaching and holds an exclusive `flock`
on it for as long as it runs. One mechanism answers three questions: which pid
to signal, whether a second instance may start, and whether a pidfile left
behind is stale — a lock that can be acquired means its writer is gone, with
no `/proc` probe and no race.

It also separates two things `tetrislogd` had conflated. Its single-instance
guard was an `flock` on the log file itself, so deleting `tmp/` took away the
sink and the guard in one stroke, and the sink-reclaim path had to restore
both. With the guard on the pidfile, `sink_open` no longer needs to lock at
all, and reclaiming the sink goes back to being only about the sink.

## Stopping is asymmetric on purpose

`tetrisctl` stops `tetrislogd` with `SIGTERM` and confirms it by blocking on
the pidfile lock: when the lock comes free, the logger has finished draining,
reclaimed its sink and written its exit line. This gives the "blocks until
teardown completes" guarantee `docs/use_cases.md` asks for without a protocol,
and keeps `libhtttp` out of the daemon whose defended property
([ADR-0005](./0005-logger-keeps-no-internal-queue.md)) is that it is the
smaller thing to get right.

`tetrisd` gets the control socket that `use_cases.md` specifies, because it
buys something the signal path cannot: an admin channel on its own thread that
still answers while the public port is flooded. That lands as a second step;
the first version of `tetrisctl` drives both daemons by pidfile and signal, so
nothing about stopping them waits on a protocol being built.

## Consequences

- `lib/libcoredaemon` is a new library carrying both sides: detach, readiness
  and pidfile-claim for the daemons, pidfile-read and wait-for-exit for
  `tetrisctl`. The two sides agree on the file format by construction.
- The fork lives in `main.c` only, never behind `server_start` or
  `logd_start`, or the in-process test suites would begin forking.
- Each daemon reopens its own stderr onto a configured path after detaching.
  This is not cosmetic: `tetrisd`'s stderr is the last-resort copy of records
  the logger could not take, and `tetrislogd`'s is where Degraded records go.
- Teardown order is the reverse of launch order and is declared in
  `.tetrishrc`, not compiled into `tetrisctl`. Stopping the logger first would
  push `tetrisd`'s entire shutdown into its error file instead of the log.
- `make reset` must stop the daemons before wiping runtime state. It currently
  does the opposite, and that wipe under a running logger is what the
  sink-reclaim work was written to survive. Reclaim stays — a log file can
  still be rotated or removed by hand — but it stops being a patch for a
  self-inflicted wound.
- `CLAUDE.md`'s statement that daemons are launched from inside the shell via
  `dspawn` no longer holds; `.tetrishrc` invokes the binaries directly.
- A second instance now fails on the pidfile lock rather than on the sink
  lock, so the boot-order argument in `src/tetrislogd/README.md` — claim the
  sink before binding the socket — needs restating against the new guard.
