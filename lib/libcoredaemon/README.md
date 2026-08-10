# libcoredaemon

tetriSH daemonisation library — detaching, and finding what has detached

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
make -C lib/libcoredaemon
```

This produces `lib/libcoredaemon/libcoredaemon.a`. The library has no internal
dependencies and links nothing beyond libc.

| Target | Description |
|---|---|
| `make` / `make all` | Build `libcoredaemon.a` |
| `make test` | Build and run every suite under `tests/` |
| `make test FILTER=<pattern>` | Run only suites whose path contains `<pattern>` |
| `make clean` | Remove object files and test binaries |
| `make fclean` | Remove objects, test binaries, and the archive |
| `make re` | `fclean` + `all` |

Link the archive and add the public include directory:

```bash
cc ... -I lib/libcoredaemon/include lib/libcoredaemon/libcoredaemon.a
```

`tetrisd` and `tetrislogd` link it for the daemon side; `tetrisctl` links it
for the other side of the same pidfile.

---

## Testing

Each test has its own `main()` and links `libcoredaemon.a`:

```bash
make -C lib/libcoredaemon test
make -C lib/libcoredaemon test FILTER=pidfile
```

| Suite | Proves |
|---|---|
| `test_detach` | A daemon that reports ready exits its parent `0`; one that dies during boot exits it non-zero; the detached process is not a session leader; `daemon_stderr_redirect` moves the stream and refuses an unusable path |
| `test_pidfile` | Claim writes the pid and creates parents; a second process loses the lock with `EWOULDBLOCK`; release hands it on; a probe reads a live holder's pid, and reads a crash-left pidfile as *nothing running*; `daemon_pid_wait` times out on a live holder and returns once it is gone |
| `test_paths` | `daemon_mkdir_p` builds a whole chain and is idempotent; a regular file in the way is `ENOTDIR`, not success; `daemon_mkdir_parent` ignores the leaf |

The single-instance guard cannot be tested from one process — `flock` is held
per open file description, so a process re-locking its own pidfile succeeds and
proves nothing. `tests/harness.c` therefore forks a real second process that
claims the file for real, which is the shape of a second daemon losing the race.

---

## API Reference

All declarations live in `include/coredaemon.h`.

### Detaching — daemon side

| Function | Description |
|---|---|
| `int daemon_detach(int *ready_fd)` | Double-fork into the background. Returns `0` in the daemon only; the originating process blocks inside this call and exits `0` or `1` according to what happens next. |
| `void daemon_ready(int ready_fd)` | Report a successful boot, releasing the process that launched us. |
| `int daemon_stderr_redirect(const char *path)` | Point `stderr` at a file, once there is nothing left to fail. |

### The pidfile — daemon side

| Function | Description |
|---|---|
| `void daemon_pid_blank(t_pidfile *pf)` | Put a zeroed pidfile into the "nothing is held" state. Call before anything that can fail. |
| `int daemon_pid_claim(t_pidfile *pf, const char *path)` | Take the lock, then write the pid. `-1` with `errno == EWOULDBLOCK` means another instance is running. |
| `void daemon_pid_release(t_pidfile *pf)` | Release the lock. Safe on a pidfile that was never claimed. |

### The pidfile — `tetrisctl` side

| Function | Description |
|---|---|
| `int daemon_pid_read(const char *path, pid_t *out)` | What the file says, regardless of who holds it. |
| `int daemon_pid_probe(const char *path, pid_t *out)` | `1` running (with the pid), `0` not running, `-1` on error. |
| `int daemon_pid_wait(const char *path, int timeout_ms)` | Block until the lock comes free. `-1` with `errno == ETIMEDOUT` on timeout; a negative timeout waits indefinitely. |

### Paths

| Function | Description |
|---|---|
| `int daemon_mkdir_p(const char *path)` | Create a directory and every missing directory above it. |
| `int daemon_mkdir_parent(const char *path)` | Create the directories a file path needs, ignoring the file itself. |

---

## Architecture

### One byte, not one close

`daemon_detach` hands back the write end of a pipe the launching process is
blocked reading. Writing one byte to it means "I am up"; closing it without
writing means "I died". That single byte is the whole reason this library
exists rather than reusing the shell's `daemon_spawn`, whose pipe is closed on
success *and* on death, so its parent cannot tell them apart and always exits
`EXIT_SUCCESS`.

The practical consequence is that `tetrisctl start` learns whether the boot it
asked for happened, and a failed launch is testable in a script.

### stderr stays on the terminal until boot is over

`daemon_detach` takes stdin and stdout to `/dev/null` and leaves `stderr` alone.
Everything between `daemon_detach` and `daemon_ready` is boot, and a boot that fails
has to say why somewhere a person is looking. `daemon_stderr_redirect` moves the
stream to its configured file afterwards — which is not cosmetic either:
`tetrisd`'s stderr is the last-resort copy of records the logger could not
take, and `tetrislogd`'s is where Degraded records go.

### The lock is the guard, not the file

A pidfile is `flock`ed for as long as its daemon runs, and one mechanism
answers three questions: which pid to signal, whether a second instance may
start, and whether a pidfile left behind is stale. A lock that *can* be
acquired means its writer is gone — no `/proc` probe, and no window between
checking whether a process exists and signalling it.

Two ordering rules follow, and both matter:

- **Claim after `daemon_detach`, never before.** The pid written has to be the
  detached process's, and the lock has to be held by the process that will
  still be here to hold it.
- **`daemon_detach` belongs in `main()` alone.** Putting it behind a `*_start()`
  function would make every in-process test suite fork the moment it booted a
  daemon.

`daemon_pid_release` leaves the file on disk. A pidfile whose lock is free already
reads as stale, so unlinking buys nothing, costs the operator the last pid of a
daemon that has gone, and races a second instance that has just claimed the
same path.

### No logging, ever

This library is on the path a daemon uses to report that it cannot start. A
library printing over that would bury the one message worth reading, so every
function here returns errno-style and says nothing.

### The working directory is left alone

The customary `chdir("/")` is deliberately not done. Every path in
`.tetrishrc` is relative to where the daemon was launched, so moving the
working directory would resolve all of them somewhere else.

---

## Project Structure

```
libcoredaemon/
├── Makefile
├── README.md
├── include/
│   └── coredaemon.h        # public header
├── src/
│   ├── detach.c            # daemon_detach, daemon_ready, daemon_stderr_redirect
│   ├── pidfile.c           # daemon_pid_blank, daemon_pid_claim, daemon_pid_release
│   ├── probe.c             # daemon_pid_read, daemon_pid_probe, daemon_pid_wait
│   └── paths.c             # daemon_mkdir_p, daemon_mkdir_parent
├── tests/
│   ├── harness.[ch]        # temp dirs, and a real second process to race
│   ├── test_detach.c
│   ├── test_pidfile.c
│   └── test_paths.c
└── scripts/
    └── run_tests.sh
```
