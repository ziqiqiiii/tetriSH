# tetrisctl

The admin CLI for tetriSH. Starts, inspects and stops the game daemons through
the locked pidfile each of them publishes.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Configuration](#configuration)
- [Exit Codes](#exit-codes)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)

---

## Features

- `start` reports whether the daemon it launched actually came up — the thing `dspawn` could never tell you ([ADR-0007](../../docs/adr/0007-daemons-detach-themselves.md))
- `status` answers from the pidfile *lock*, so a pidfile left behind by a crash reads as stopped rather than as a pid worth signalling
- `stop` blocks until teardown has finished, with no protocol on either side
- Launch order comes from `.tetrishrc`, and teardown is that order reversed
- No control socket yet — the whole CLI is pidfiles and signals, which is why it works before `tetrisd` grows an admin channel

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

`tetrisctl` links `libcoredaemon` and nothing else — no OpenSSL, no
certificates, no protocol library.

---

## Build

```bash
make -C src/tetrisctl
```

| Target | Description |
|---|---|
| `make -C src/tetrisctl` | Build `src/tetrisctl/tetrisctl` |
| `make -C src/tetrisctl test` | Build and run both suites |
| `make -C src/tetrisctl test FILTER=<pattern>` | Run only suites matching `<pattern>` |
| `make -C src/tetrisctl clean` | Remove object files and test binaries |
| `make -C src/tetrisctl fclean` | Remove object files and the binary |
| `make -C src/tetrisctl re` | Full rebuild (`fclean` + `all`) |

---

## Run

**Run it from the project root.** Paths in `.tetrishrc` are root-relative — not
relative to the rc file or the binary — and the daemons are resolved through
`PATH`, which the shell points at `./bin`.

```bash
tetrisctl start              # every daemon, in launch order
tetrisctl start tetrislogd   # just one
tetrisctl status
tetrisctl stop               # reverse of launch order
tetrisctl restart
```

```text
$ tetrisctl status
tetrislogd   running   pid 40213    tmp/tetrislogd/tetrislogd.pid
tetrisd      stopped   -            tmp/tetrisd/tetrisd.pid
```

`-f <rc>` points it at a different start-up file; otherwise it resolves
`$TETRISHRC`, then `./.tetrishrc`. Whichever it lands on is handed to each
daemon as `argv[1]`, so both ends read their settings out of the same file.

---

## Configuration

| Key | Default | Meaning |
|---|---|---|
| `TETRISCTL_DAEMONS` | `"tetrislogd tetrisd"` | Which daemons to manage, in launch order. Teardown is this reversed. |

The pidfile of each daemon is read from the key that daemon publishes it
under — `TETRISLOGD_PID` and `TETRISD_PID_PATH`. `tetrisctl` reads keys it does
not own on purpose: the pidfile is named once, by the process that holds it,
and naming it a second time here is exactly how the two ends would drift apart.

An unrecognised *key* in `.tetrishrc` is skipped, because the file is a shell
start-up script full of lines that are none of this program's business. An
unrecognised *daemon* in `TETRISCTL_DAEMONS` is not: it names something to be
started and stopped, and quietly leaving it out would produce a stack that is
half up under a zero exit code.

---

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Every selected daemon reached the asked-for state |
| `1` | Something did not: a daemon failed to boot, a stop timed out, a pidfile could not be read, or the roster named something unmanaged |

`status` exits `0` for a daemon that is simply stopped — that is a successful
report. It exits `1` only when a question could not be answered.

---

## Architecture

### Start waits, and that is the feature

`d_start` forks, execs the daemon binary, and waits for that child. The child
is the daemon's *own* launching process: the binary daemonises itself, so the
child survives exactly as long as the boot does and exits with the verdict its
readiness pipe carried. Waiting for it is therefore not a delay, it is the
answer.

`dspawn` could not do this. It wrote its registry entry and *then* called
`execvp`, so it never learned whether the program it launched started — a
`tetrislogd` that lost a race exited immediately and left an entry `dcheck`
rendered as "down", with no cause recorded anywhere.

### Stop asks the lock, not the process table

`d_stop` sends `SIGTERM` and then blocks on the pidfile lock. The lock comes
free as the daemon's last act — after it has drained what it had, reclaimed
its sink and written its exit line — so when `stop` returns, the daemon is
gone, not merely signalled. That is the "blocks until teardown completes"
guarantee `docs/use_cases.md` asks for, bought without a protocol and without
`libhtttp` in the process whose defended property
([ADR-0005](../../docs/adr/0005-logger-keeps-no-internal-queue.md)) is that it
is the smaller thing to get right.

The same lock answers `status`. A pidfile is readable long after its writer is
gone, so trusting the file would mean signalling a pid some unrelated process
now owns; a lock cannot outlive its holder.

### Order lives in the file

`TETRISCTL_DAEMONS` is the only place launch order is written down, and
teardown is that list reversed. Stopping the logger first would push
`tetrisd`'s entire shutdown — every disconnect, every room torn down — into
its error file instead of the log, which is exactly the record an operator
reaches for after a shutdown goes wrong.

Resolution is deferred until the whole start-up file has been read, because a
file written for people may well declare the roster in a "daemons" section
below the settings it refers to.

### What is deliberately not here

The control socket `use_cases.md` specifies for `tetrisd` (`STATUS /admin`,
`SHUTDOWN /admin`, `kick`, `rooms`, `players`, `dropped-logs`) is a second
step. It buys something the signal path cannot — an admin channel on its own
thread that still answers while the public port is flooded — but nothing about
starting and stopping the daemons should wait on a protocol being built.

---

## Project Structure

```text
src/tetrisctl/
├── include/tetrisctl.h   Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            Argument shim over cfg_load and the cmd_* functions
│   ├── cfg.c             .tetrishrc parsing, the roster, pidfile resolution
│   ├── daemon.c          One daemon: probe it, launch it, signal it, wait
│   └── cmd.c             start / status / stop / restart across the roster
├── tests/
│   ├── fakedaemon.c      A real self-detaching daemon for the suites to drive
│   ├── test_cfg.c
│   └── test_lifecycle.c
├── scripts/run_tests.sh
└── Makefile              → src/tetrisctl/tetrisctl
```

---

## Testing

```bash
make -C src/tetrisctl test
make -C src/tetrisctl test FILTER=lifecycle
```

| Suite | Proves |
|---|---|
| `test_cfg` | Defaults name both daemons in launch order; the roster comes from `.tetrishrc`; pidfiles are read from each daemon's own prefix; key order in the file does not matter; an unmanaged daemon name fails the load while an unknown key is skipped; the environment beats the file; the shipped `.tetrishrc` resolves to the same paths the compiled fallbacks do |
| `test_lifecycle` | Start reports a daemon that came up *and* one that died booting; a second start is a no-op on the same pid; stop blocks until the process is gone; stopping what is not running is success; teardown runs in the reverse of launch order; a crash-left pidfile reads as stopped |

`test_lifecycle` forks and execs a real self-detaching daemon
(`tests/fakedaemon.c`) rather than a stub, because what is under test is the
handover between two processes. The Makefile installs that binary twice under
`tests/bin/`, named after each managed daemon, since `tetrisctl` resolves what
to run through `PATH`.

Valgrind is expected to be clean:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrisctl/tests/bin/test_cfg
```
