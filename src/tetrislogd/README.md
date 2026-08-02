# tetrislogd

The standalone logger daemon for tetriSH. Receives log records from `tetrisd` over an `AF_UNIX` datagram socket, validates them, and writes them to one exclusively-locked file.

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
- Exclusive `flock` on the sink, so a second instance refuses to start rather than interleaving lines
- Keeps running with the sink unavailable — records go to stderr and are counted Degraded until an idle-tick retry gets the file back
- `fdatasync` on the idle tick, never per record, so disk latency is not the ceiling on log throughput
- Malformed datagrams are Rejected and counted, never written — a bad record cannot corrupt a line operators read

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

The logger links `libcoreipc` and nothing else — no OpenSSL, no certificates.

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

Or from inside the shell, once the root `bin-link` target has published `bin/tetrislogd`:

```bash
dspawn tetrislogd -- tetrislogd
```

The `--` is not optional: a bare `dspawn tetrislogd` runs dspawn's placeholder loop instead of this binary.

**Start the logger before `tetrisd`.** It binds the socket the game server sends to; started second, it unlinks and rebinds that path, and `tetrisd` keeps sending into the old inode until it retries. `tetrislogd` does not daemonise — `dspawn` already did that before exec'ing it.

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

Both settings come from `.tetrishrc`, resolved as `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variable. A missing rc file is not an error — the defaults are a working setup.

| Key | Default | Meaning |
|---|---|---|
| `TETRISLOGD_SOCK` | `tmp/tetrisd/tetrislogd.sock` | Datagram socket to bind; must equal `TETRISD_LOG_IPC` |
| `TETRISLOGD_FILE` | `tmp/tetrislogd/tetrislogd.log` | Log file to write; parent directories are created |

Config is cold: paths are resolved once at boot and change only by restarting the daemon. An unknown `TETRISLOGD_*` key fails the boot rather than being ignored, so a setting documented but never wired up cannot silently do nothing.

---

## Log Format

One record is one line, formatted by `lr_format_line`:

```text
<timestamp_ms> <level>  <component>[<pid>]: <message>
1743085512034 info    tetrisd[8412]: room S-01 started with 1 player
```

Levels are `debug`, `info`, `warning`, `error`. The logger writes its own records the same way, under the component `tetrislogd`, and they go straight to the sink — a logger that logged to itself over IPC would deadlock against its own receive buffer.

Counters are reported at four moments — `boot`, `rotated`, `dump`, `exit`:

```text
dump: 14812 written, 3 rejected, 0 degraded
```

---

## Architecture

### The three fates of a record

The words are not interchangeable, and only two of them are counted here:

| Fate | Meaning | Counted by |
|---|---|---|
| **Dropped** | `tetrisd`'s ring buffer was full; the record never left `tetrisd` | `tetrisd` (`rb_drops`) |
| **Rejected** | Arrived here but failed `lr_validate`; discarded | `tetrislogd` |
| **Degraded** | Valid, but the sink was unavailable; written to stderr | `tetrislogd` |

A Degraded record went to stderr rather than to the sink. Whether that is loss depends on who started the daemon: run from a terminal it is on screen, but `dspawn` redirects stderr to `/dev/null` (`daemon_spawn.c`), so under the shell it is gone and the counter is the only trace it existed. `written` and `degraded` together are every valid record the logger handled; `rejected` is the malformed remainder.

### The loop

```text
poll(socket, self-pipe, idle_ms)
     │
     ├── self-pipe ready   sp_drain, then act on sig_take: STOP, HUP, DUMP
     │
     ├── socket ready      recv → logd_accept, repeated until EAGAIN
     │                     so a burst is handled in one pass, not one per poll
     │
     └── timeout           sink_sync, and retry a closed sink
```

The kernel's socket receive buffer is the only queue in the design. A full buffer returns `EAGAIN` to `tetrisd`'s shipper, which still holds a copy of the record — backpressure that reaches the sender is strictly better than a drop that does not. The rejected alternative, a receiver thread draining into a ring, moves only *who loses*; see [ADR-0005](../../docs/adr/0005-logger-keeps-no-internal-queue.md).

One call to `logd_run_once` is exactly one poll iteration. That is the seam the tests drive: send a datagram or raise a signal, call it once, assert on the file — no thread, no fork.

### Boot order

`logd_start` claims the sink *before* binding the socket. `us_dgram_bind` unlinks its path unconditionally, so a second instance has to lose the `flock` race and exit while the running logger's socket is still intact. Reversing these two lines is silent: both instances start, and the older one goes deaf.

---

## Project Structure

```text
src/tetrislogd/
├── include/tetrislogd.h  Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            Thin shim over logd_start / logd_run_once / logd_stop
│   ├── cfg.c             .tetrishrc parsing, rc resolution, mkdir -p
│   ├── sink.c            The log file: open, exclusive lock, write, sync, rotate
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

