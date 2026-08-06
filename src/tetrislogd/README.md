# tetrislogd

The standalone logger daemon for tetriSH. Receives log records from `tetrisd` over an `AF_UNIX` datagram socket, validates them, and writes them to a log file. It detaches itself and publishes a locked pidfile; `tetrisctl` starts, inspects and stops it through that file.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Signals](#signals)
- [Configuration](#configuration)
- [Log Format](#log-format)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)

---

## Features

- One process, one thread, one loop — no internal queue, no shared state, no lock order ([ADR-0005](../../docs/adr/0005-logger-keeps-no-internal-queue.md))
- Survives `tetrisd` restarts; a producer that goes away simply stops sending
- Detaches itself and holds a locked pidfile, so a second instance refuses to start rather than interleaving lines ([ADR-0007](../../docs/adr/0007-daemons-detach-themselves.md))
- Reports its own boot over a readiness pipe, so a launch that failed exits non-zero with the reason on the terminal
- Keeps running with the sink unavailable — records go to stderr and are counted Degraded until an idle-tick retry gets the file back
- `fdatasync` on the idle tick, never per record, so disk latency is not the ceiling on log throughput
- Malformed datagrams are Rejected and counted, never written — a bad record cannot corrupt a line operators read

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

The logger links `libcoreipc` and `libcoredaemon` — no OpenSSL, no certificates.

---

## Build

```bash
make -C src/tetrislogd
```

This builds `lib/libcoreipc` in place and links it into `src/tetrislogd/tetrislogd`.

| Command | Description |
|---|---|
| `make -C src/tetrislogd` | Build the daemon (`make all`) |
| `make -C src/tetrislogd libs` | Build `libcoreipc` only |
| `make -C src/tetrislogd run` | Build and launch |
| `make -C src/tetrislogd test` | Build and run every suite |
| `make -C src/tetrislogd clean` | Remove object files and test binaries |
| `make -C src/tetrislogd fclean` | Remove object files and the binary |
| `make -C src/tetrislogd re` | Full rebuild (`fclean` + `all`) |

---

## Run

**Run it from the project root.** Paths in `.tetrishrc` are root-relative — not relative to the rc file or the binary.

```bash
make -C src/tetrislogd run
```

Or, the way `.tetrishrc` does it, through the lifecycle manager:

```bash
tetrisctl start tetrislogd
tetrisctl status
tetrisctl stop tetrislogd
```

The binary daemonises itself, so running it by name returns to the prompt once the logger is actually up. That return is meaningful: the command exits `0` only after the daemon has bound its socket and opened its sink, and non-zero — with the reason printed — if it did not.

**Start the logger before `tetrisd`.** It binds the socket the game server sends to; started second, it unlinks and rebinds that path, and `tetrisd` keeps sending into the old inode until it retries. `tetrisctl` takes that order from `TETRISCTL_DAEMONS` in `.tetrishrc` and stops in the reverse.

---

## Signals

| Signal | Effect |
|---|---|
| `SIGTERM`, `SIGINT` | Drain what is left in the socket, report final counters, release the sink and exit |
| `SIGHUP` | Reopen the sink at the same path (rotation); config is **not** re-read |
| `SIGUSR1` | Write the counter line to the log — the stand-in for a control channel |
| `SIGPIPE` | Ignored — a closed stderr must not kill the degraded path |

Signals set a flag and write one byte down the self-pipe; the loop asks `sig_take` once per wake-up, so repeated signals between iterations coalesce.

---

## Configuration

Every setting comes from `.tetrishrc`, resolved as `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variable. A missing rc file is not an error — the defaults are a working setup.

| Key | Default | Meaning |
|---|---|---|
| `TETRISLOGD_SOCK` | `tmp/tetrisd/tetrislogd.sock` | Datagram socket to bind; must equal `TETRISD_LOG_IPC` |
| `TETRISLOGD_FILE` | `tmp/tetrislogd/tetrislogd.log` | Log file to write; parent directories are created |
| `TETRISLOGD_PID` | `tmp/tetrislogd/tetrislogd.pid` | Pidfile to claim and hold; the single-instance guard, and what `tetrisctl` signals |
| `TETRISLOGD_ERR` | `tmp/tetrislogd/tetrislogd.err` | Where stderr goes after boot — Degraded records land here |

Config is cold: paths are resolved once at boot and change only by restarting the daemon. An unknown `TETRISLOGD_*` key fails the boot rather than being ignored, so a setting documented but never wired up cannot silently do nothing.

---

## Log Format

One record is one line, formatted by `logrecord_format_line`:

```text
<timestamp_ms> <level>  <component>[<pid>]: <message>
1743085512034 info    tetrisd[8412]: room S-01 started with 1 player
```

Levels are `debug`, `info`, `warning`, `error`. The logger writes its own records the same way, under the component `tetrislogd`, and they go straight to the sink — a logger that logged to itself over IPC would deadlock against its own receive buffer.

Counters are reported whenever the sink changes hands or the daemon is asked to account for itself — `boot`, `rotated`, `dump`, `sink recovered`, `sink replaced`, `exit`:

```text
dump: 14812 written, 3 rejected, 0 degraded
```

---

## Architecture

### The three fates of a record

The words are not interchangeable, and only two of them are counted here:

| Fate | Meaning | Counted by |
|---|---|---|
| **Dropped** | `tetrisd`'s ring buffer was full; the record never left `tetrisd` | `tetrisd` (`ring_dropped_count`) |
| **Rejected** | Arrived here but failed `logrecord_validate`; discarded | `tetrislogd` |
| **Degraded** | Valid, but the sink was unavailable; written to stderr | `tetrislogd` |

A Degraded record went to stderr rather than to the sink. Once the daemon has detached, stderr is the file named by `TETRISLOGD_ERR`; the daemon reopens it onto that path itself, after boot succeeds, which is why a boot *failure* still reaches the terminal instead. That makes a degraded record recoverable, not retained: the error file lives in the `tmp/` that `make reset` wipes, and nothing reclaims that descriptor the way the sink reclaims its own. The counter is the guarantee — it says how many took that route. `written` and `degraded` together are every valid record the logger handled; `rejected` is the malformed remainder.

### The loop

```text
poll(socket, self-pipe, idle_ms)
     │
     ├── self-pipe ready   selfpipe_drain, then act on sig_take: STOP, HUP, DUMP
     │
     ├── socket ready      recv → logd_accept, repeated until EAGAIN
     │                     so a burst is handled in one pass, not one per poll
     │
     └── timeout           sink_sync, then reclaim the sink if it needs it
```

The kernel's socket receive buffer is the only queue in the design. A full buffer returns `EAGAIN` to `tetrisd`'s shipper, which still holds a copy of the record — backpressure that reaches the sender is strictly better than a drop that does not. The rejected alternative, a receiver thread draining into a ring, moves only *who loses*; see [ADR-0005](../../docs/adr/0005-logger-keeps-no-internal-queue.md).

One call to `logd_run_once` is exactly one poll iteration. That is the seam the tests drive: send a datagram or raise a signal, call it once, assert on the file — no thread, no fork.

### Reclaiming the sink

The sink can be lost two ways, and only one of them announces itself.

A sink that is **closed** — the reopen failed, the disk was full — makes every write fail, so records degrade to stderr and the idle tick retries until the file comes back.

A sink whose file was **deleted or replaced** fails at nothing. An open descriptor outlives the unlink that took its name away: `write` still returns success, the counters still climb, and the log file simply does not exist. `make reset` does exactly this — it wipes `tmp/` under a running logger — and the symptom is a `tetrislogd.log` with no boot line in it, because the boot line went into an inode nothing can open.

So the sink remembers which file it holds (`dev`, `ino` at open time) and `sink_is_stale` compares that against whatever the path names now. The idle tick reopens on a mismatch and logs `sink replaced`, whose counters say how much went into the file that is gone. `logd_stop` reclaims too, before writing its `exit` line — a shutdown can arrive before any idle tick, and the exit line is the one an operator goes looking for.

Reclaim is now only about the sink. It used to have to restore the `flock` as well, because the sink carried the single-instance guard: `flock` is per inode, so between the unlink and the reclaim the guard was on a file nobody could open and a second logger could start. With the guard on the pidfile that second job is gone, and reclaim stays for the reason it was always worth having — a log file can be rotated or removed by hand.

### Boot order

`main.c` claims the pidfile *before* calling `logd_start`, and that ordering is the guard. `unixsock_dgram_bind` unlinks its path unconditionally, so binding the socket is the point at which a second instance would damage the first; losing the pidfile race happens before the bind is ever reached, while the running logger's socket is still intact. Reversing those two lines is silent: both instances start, and the older one goes deaf.

The claim also comes *after* `cd_detach`, because the pid written has to be the detached process's and the lock has to be held by the process that will still be there to hold it. Both the fork and the claim live in `main.c` alone — behind `logd_start` they would make every in-process test suite fork.

---

## Project Structure

```text
src/tetrislogd/
├── include/tetrislogd.h  Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            Detach, claim the pidfile, then loop until stopped
│   ├── cfg.c             .tetrishrc parsing, rc resolution, mkdir -p
│   ├── sink.c            The log file: open, write, sync, rotate, reclaim
│   ├── logd.c            Bring-up, the poll loop, record acceptance, counters
│   └── signals.c         Handlers: a flag and one byte down the self-pipe
├── tests/                harness.c builds each suite's throwaway socket and file
├── scripts/run_tests.sh
└── Makefile              → src/tetrislogd/tetrislogd
```

---

## Testing

Four suites, each driving a real daemon in-process over a throwaway socket:

```bash
make -C src/tetrislogd test
make -C src/tetrislogd test FILTER=sink    # only suites matching "sink"
```

Suites lower `idle_ms` from its `TL_IDLE_MS` default so the timeout path — `fdatasync` and the sink-reopen retry — runs without a one-second wait per assertion.

Valgrind is expected to be clean:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrislogd/tests/bin/test_logd
```

