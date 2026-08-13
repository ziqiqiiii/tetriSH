# tetrisctl

The admin CLI for tetriSH. Starts, inspects and stops the game daemons through the locked pidfile each of them publishes.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Commands](#commands)
- [Configuration](#configuration)
- [Exit Codes](#exit-codes)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)

---

## Features

- `start` waits for the daemon's own readiness verdict, so it reports whether the daemon actually came up
- `status` answers from the pidfile *lock*, so a pidfile left by a crash reads as stopped rather than as a pid worth signalling
- `stop` blocks until teardown has finished, with no protocol on either side
- Launch order comes from `.tetrishrc`; teardown is that order reversed
- Links `libcoredaemon` and nothing else — no OpenSSL, no certificates, no protocol library

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

---

## Build

```bash
make -C src/tetrisctl
```

| Target | Description |
|---|---|
| `make -C src/tetrisctl` | Build `src/tetrisctl/tetrisctl` |
| `make -C src/tetrisctl libs` | Build `libcoredaemon` only |
| `make -C src/tetrisctl run` | Build, then print `status` |
| `make -C src/tetrisctl test` | Build and run both suites |
| `make -C src/tetrisctl test FILTER=<pattern>` | Run only suites matching `<pattern>` |
| `make -C src/tetrisctl clean` | Remove object files and test binaries |
| `make -C src/tetrisctl fclean` | Remove object files and the binary |
| `make -C src/tetrisctl re` | Full rebuild (`fclean` + `all`) |

---

## Run

**Run it from the project root.** Paths in `.tetrishrc` are root-relative, not relative to the rc file or the binary, and daemons are resolved through `PATH`, which the shell points at `./bin`.

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

---

## Commands

```text
tetrisctl [-f <rc>] start|status|stop|restart [daemon]
```

| Command | Effect |
|---|---|
| `start [daemon]` | Launch in roster order; a daemon already running is reported and stepped over |
| `status [daemon]` | Print one line per daemon — name, state, pid, pidfile |
| `stop [daemon]` | `SIGTERM`, then block on the pidfile lock until teardown finishes |
| `restart [daemon]` | `stop` then `start`; the stop completes before the start begins |

`-f <rc>` names the start-up file; otherwise it resolves `$TETRISHRC`, then
`./.tetrishrc`. Whichever it lands on is handed to each daemon as `argv[1]`, so
both ends read their settings out of the same file.

---

## Configuration

| Key | Required | Meaning |
|---|---|---|
| `TETRISCTL_DAEMONS` | yes | Daemons to manage, in launch order; teardown is this reversed |
| `TETRISLOGD_PID_PATH` | if `tetrislogd` is listed | Where `tetrislogd` publishes its pidfile |
| `TETRISD_PID_PATH` | if `tetrisd` is listed | Where `tetrisd` publishes its pidfile |

The file is read first, then any matching environment variable overlays it.

Nothing is defaulted. A default roster would put launch order back inside this binary; a default path would name a pidfile the daemon already names, free to drift. So the two pidfile keys are read, not owned — only *which key* goes with *which name* is compiled in.

An unrecognised *key* is skipped, since the file is a shell start-up script full of other programs' lines. Everything else is refused loudly: an unmanaged daemon name, a listed daemon whose pidfile key is unset, or a missing roster would each leave a stack half up under a zero exit code.

---

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Every selected daemon reached the asked-for state |
| `1` | A daemon failed to boot, a stop timed out, a pidfile could not be read, or the roster named something unmanaged |

`status` exits `0` for a daemon that is simply stopped — that is a successful
report. It exits `1` only when a question could not be answered.

---

## Architecture

Every question this program asks is asked of one thing — the lock on the pidfile:

```text
 start ──▶ waitpid on the launching child   ┌──────────────────┐
 status ─▶ daemon_pid_probe   LOCK_SH|LOCK_NB ─▶│ flock(pidfile)   │ held ⇢ running
 stop ───▶ SIGTERM, then daemon_pid_wait ──────▶│ the daemon holds │ free ⇢ stopped
                          polls until free  │ it exclusively   │
                                            └──────────────────┘
```

A lock cannot outlive its holder, so a pidfile left by a crash reads as stopped rather than as a pid worth signalling.

### Start waits, and that is the feature

`managed_start` forks, execs the daemon binary, and waits for that child. The child is the daemon's *own* launching process: the binary daemonises itself, so the child lives exactly as long as the boot does and exits with the verdict its readiness pipe carried. Waiting for it is not a delay, it is the answer.

The shell's `dspawn` could not do this. It wrote its registry entry and *then* called `execvp`, so it never learned whether the program it launched started — a `tetrislogd` that lost a race exited immediately and left an entry `dcheck` rendered as "down", with no cause recorded anywhere.

### Stop asks the lock, not the process table

`managed_stop` sends `SIGTERM`, then blocks on the pidfile lock. The lock comes free as the daemon's last act — after it has drained what it had, reclaimed its sink and written its exit line — so when `stop` returns the daemon is gone, not merely signalled. That is the "blocks until teardown completes" guarantee `docs/use_cases.md` asks for, bought without a protocol on either side.

The same lock answers `status`. A pidfile is readable long after its writer is gone, so trusting the file would mean signalling a pid some unrelated process now owns.

### Order lives in the file

`TETRISCTL_DAEMONS` is the only place launch order is written down, and teardown
is that list reversed:

```text
 start   tetrislogd ──▶ tetrisd    sink is up before anything can log to it
                  ▲          │
                  └──────────┘     log records over IPC
 stop    tetrislogd ◀── tetrisd    the logger outlives the shutdown it records
```

Reversed, `tetrisd`'s entire shutdown — every disconnect, every room torn down —
would land in its error file instead of the log, which is exactly the record an operator reaches for after a shutdown goes wrong.

Resolution is deferred until the whole file has been read, because a file written for people may declare the roster in a "daemons" section below the settings it refers to.

---

## Project Structure

```text
src/tetrisctl/
├── include/tetrisctl.h   Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            Argument shim over config_load and the *_command functions
│   ├── config.c             .tetrishrc parsing, the roster, pidfile resolution
│   ├── managed.c          One daemon: probe it, launch it, signal it, wait
│   └── commands.c             start / status / stop / restart across the roster
├── tests/
│   ├── fakedaemon.c      A real self-detaching daemon for the suites to drive
│   ├── test_config.c
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
| `test_config` | • Roster comes from `.tetrishrc`<br>• Pidfiles read from each daemon's own prefix <br>• Key order does not matter <br>• Unmanaged daemon name fails the load, unknown key is skipped <br>• Environment beats the file <br>• The shipped `.tetrishrc` resolves |
| `test_lifecycle` | • Start reports a daemon that came up *and* one that died booting <br>• A second start is a no-op on the same pid <br>• Stop blocks until the process is gone <br>• Stopping what is not running is success <br>• Teardown reverses launch order <br>• A crash-left pidfile reads as stopped |

`test_lifecycle` execs a real self-detaching daemon (`tests/fakedaemon.c`), not a stub, because the two-process handover is what is under test. The Makefile installs it under `tests/bin/` once per managed daemon name, since `tetrisctl` resolves what to run through `PATH`.

Valgrind is expected to be clean:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrisctl/tests/bin/test_config
```
