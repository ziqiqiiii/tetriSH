# tetriSH

A terminal-based Battle Royale Tetris system written in C. Combining a custom Unix shell, concurrent daemon processes, authenticated encrypted networking, and a bespoke application-layer protocol (HTTTP).

Part of the **CoreStack Challenge** (50.003 × 50.005), Singapore University of Technology and Design.

---

## Table of Contents

- [Status](#status)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Play](#play)
- [Run](#run)
- [Binaries](#binaries)
- [Libraries](#libraries)
- [Architecture](#architecture)
- [Protocol: HTTTP](#protocol-htttp)
- [Configuration: .tetrishrc](#configuration-tetrishrc)
- [Design Constraints](#design-constraints)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Documentation](#documentation)

---

## Status

The sections below describe the target design; this table says what exists today.

| Component | Status |
|---|---|
| `src/tetrish` | Implemented — REPL, builtins, `.tetrishrc`, system programs under `bin/` |
| `src/tetrisu` | Partial — notcurses intro, menu, and audio; no gameplay or networking |
| `src/tetrisd` | Implemented — Single mode end to end: accounts, lobby, rooms, live games, `STATE` push; integration tested (Linux only — reactor is `epoll`, no macOS build) |
| `src/tetrislogd` | Implemented — receives, validates, writes, rotates on `SIGHUP`; 35 tests across four suites, valgrind-clean |
| `tetrisctl` | Partial — `start`/`status`/`stop`/`restart` by pidfile and signal; the control socket is a later step |
| `lib/libtetrisbrain` | Implemented — nine modules, unit tested |
| `lib/libtetrisroom` | Implemented — room/slot/lobby domain, unit tested |
| `lib/libmacminidb` | Implemented — in-memory store, WAL, catalogues, unit tested |
| `lib/libtetrissh` | Implemented — handshake and encrypted framing, unit tested |
| `lib/libcoreipc` | Implemented — log records, ring buffer, `AF_UNIX`, mqueue, unit tested (mqueue module is Linux only — Darwin has no POSIX `mqueue.h`) |
| `lib/libhtttp` | Implemented — parser, serialiser, validation, dispatch, unit tested |
| `lib/libstatusbody` | Implemented — state, rooms, profile, leaderboard codecs, unit tested |

---

## Prerequisites

| Dependency | Needed for | Missing means |
|---|---|---|
| GCC 15 + binutils 2.44+, `make`, `pkg-config` | everything | error — GCC 15 emits `.base64`, which an older GNU as rejects |
| OpenSSL, Readline, ncurses | shell, daemons | error |
| notcurses 3.0.5+ | `tetrisu` | error — built from source where unpackaged |
| SDL2, SDL2_mixer | `tetrisu` audio | compiles out via `-DTETRISU_ENABLE_AUDIO=0` |
| Container engine — Docker on Linux, colima + the `docker` CLI on macOS | [`make play-image`](#play) | warning; `make deps` adds you to the `docker` group, `REQUIRE_DOCKER=1` makes it fatal |
| Valgrind | memory-safety runs | warning; `REQUIRE_VALGRIND=1` makes it fatal |

```bash
make deps                # check and install anything missing
make check-deps          # check only; never modifies the system
make deps-info           # show detected OS/WSL and dependency policy
make -C src/tetrisu deps # tetrisu render/audio deps only
```

Install covers Linux (apt, dnf/yum, pacman, zypper, apk) and macOS (Homebrew + Xcode Command Line Tools), and may request sudo; `AUTO_INSTALL_DEPS=0` keeps it check-only, as in CI.

**macOS cannot build the tree** — `lib/libcoreipc` includes `<mqueue.h>` and `tetrisd`'s reactor is `epoll` plus `timerfd`, none of which Darwin ships. Run [`make play-image`](#play) there, and the server and valgrind on Linux or WSL.

---

## Build

Clone the repository and build everything from the repo root:

```bash
git clone <repo-url>
cd MacMini_tetriSH
make
```

This runs `make deps`, builds every library under `lib/`, builds the shell, then builds whichever daemon components exist. Components are matched by their `Makefile`, so the ones that have not landed yet are skipped rather than failing the build.

| Target | Description |
|---|---|
| `make` / `make all` | Install missing dependencies, then build libraries, shell, and daemons |
| `make libs` | Build every self-contained library under `lib/` |
| `make shell` | Build `tetrish` only |
| `make daemons` | Build the daemon and client components that exist |
| `make bin-link` | Symlink every built binary into `./bin` |
| `make run` | Build, then launch the shell (sources `.tetrishrc`) |
| `make certs` | Generate the development CA and server certificate `tetrisd` boots with |
| `make stack` | Build, then launch the available daemons headless for integration tests |
| `make test` | Build, then run every available component test suite |
| `make clean` | Remove object files from every component |
| `make fclean` | Remove object files, binaries, and `./bin` |
| `make reset` | Stop any running daemons, then `fclean` plus their runtime state (`tmp/`, `archive/`, `bin/`) |
| `make re` | `fclean` + `all` |

Each library is also self-contained — it owns its `Makefile` and builds and tests on its own:

```bash
make -C lib/libtetrisbrain           # build lib/libtetrisbrain/libtetrisbrain.a
make -C lib/libtetrisbrain test      # run its unit tests (formatted output)
make -C lib/libtetrisbrain clean     # remove its objects + test binaries
```

To compile your own code against a library, link the archive and add its include path:

```bash
gcc my_program.c lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include -o my_program
```

Networked binaries additionally link OpenSSL (`-lssl -lcrypto`); `libmacminidb` requires `-lpthread`.

---

## Play

One command from a fresh clone to a running client:

```bash
make play          # client built on this host
make play-image    # client built in a container instead
```

Either installs what is missing, compiles it, opens a kitty window and starts the game in it; every step checks before it acts, so re-running is how you restart. They differ only in **where the client is built**:

| | `make play` | `make play-image` |
|---|---|---|
| Toolchain, notcurses, SDL2 | installed on this host | carried by the image |
| Host needs a compiler | yes | no |
| Container engine needed | no | yes |
| Works on macOS | no — `make` stops at `mqueue.h` | yes |

The container never draws: `tetrisu`'s board is Kitty-graphics-protocol escape sequences, bytes on the pty `docker run -t` allocates, so the host's terminal renders them and one Linux image serves Linux, macOS and WSL alike.

| Environment | Terminal | Installed by `make play` |
|---|---|---|
| Linux desktop | kitty | Yes — apt, dnf, pacman, zypper, apk |
| macOS | kitty | Yes — `brew install --cask kitty` |
| WSL with WSLg | kitty | Yes, as a native WSLg window |
| WSL without WSLg | WezTerm on Windows | No — prints the `winget` command to run |
| SSH into a Linux VM | whichever terminal you connected from | No — that machine has no display |

No packaging removes that last row: with no display there is no window to open, so the client runs in the terminal you are already in and only its protocol support matters. **kitty**, **Ghostty** and **WezTerm** speak it; Windows Terminal gets the cell board. `TETRISU_RENDERER=cell` pins that path anywhere — renderer tiers in [`src/tetrisu/README.md`](src/tetrisu/README.md).

**Both play on the shared tetriSH server**, and neither starts one here. Its address lives in [`scripts/play.sh`](scripts/play.sh) as `DEFAULT_HOST` — named once there, beside the CA it has to be paired with — and `TETRISH_HOST=` or `HOST=` overrides it.

```bash
make play HOST=tetrish.dev             # against another server, checked first
make play-image REBUILD=1              # rebuild the client image first
bash scripts/play.sh --local           # start a server here and play on it
bash scripts/play.sh --client-only     # against a local server already up
bash scripts/play.sh --container       # what make play-image runs
bash scripts/play.sh --stop            # take the local server down (Linux)
```

The server's address is typed into **SERVER ID** on the sign-in screen, which wins over the environment; the certificate needs no configuring, `certs/demo-ca.crt` being committed and bind-mounted into the container read-only rather than baked in.

**Only one CA is trusted per run**, picked from the host `play.sh` launched against — `certs/demo-ca.crt` for the shared server or one named with `HOST=`, the scratch `certs/ca.crt` for a `--local` one. A *different* SERVER ID than that host fails with `certificate signature failure`: reachable server, other CA. Name it up front; concatenating both CAs is not a workaround, since `load_cert_file` in the frozen [`common.c`](lib/libtetrissh/src/common.c) reads one certificate with `PEM_read_X509` and ignores the rest.

macOS runs the client and never the server, so the server steps are skipped there.

One concern per script: [`scripts/container.sh`](scripts/container.sh) the engine, image and run; [`scripts/terminal.sh`](scripts/terminal.sh) which terminal draws and whether one can be opened; [`scripts/play.sh`](scripts/play.sh) the order.

---

## Run

**1. Build and launch the shell:**
```bash
make run
```

`make run` symlinks every built binary into `./bin`, which the shell prepends to `$PATH`, then sources `.tetrishrc`. To launch the shell without rebuilding, run `./src/tetrish/macmini_shell`.

**2. Launch the daemons from inside the shell:**
```
tetrish$ tetrisctl start
```

`.tetrishrc` already ends with that line, so the daemons come up before the first prompt. Each binary double-forks itself and reports over a readiness pipe, so `tetrisctl start` returns only once they are actually up — and non-zero, with the reason on the terminal, if one is not. Which daemons run and in what order is `TETRISCTL_DAEMONS` in `.tetrishrc`; launch order is logger first, then game server.

**3. Inspect and stop running daemons:**
```
tetrish$ tetrisctl status
tetrish$ tetrisctl stop            # reverse of launch order
```

`stop` blocks until each daemon has finished tearing down.

**4. Connect a client (in a separate terminal):**
```bash
./src/tetrisu/bin/tetrisu
```

**5. Query and shut down the server:**
```bash
./bin/tetrisctl status
./bin/tetrisctl stop
```

Steps 4 and 5 depend on components that are not finished yet — see [Status](#status). `tetrisctl`'s richer admin queries (`kick`, `rooms`, `players`, `dropped-logs`) need `tetrisd`'s control socket, which has not landed.

---

## Binaries

| Binary | Role |
|---|---|
| `tetrish` | Interactive shell — reads `.tetrishrc`, launches daemons, entry point for the user |
| `tetrisd` | Concurrent game server — accepts clients, manages rooms, runs game logic, serves chat and the marketplace |
| `tetrislogd` | Dedicated logger daemon — receives log records over IPC and writes them to disk |
| `tetrisctl` | Admin CLI — issues control commands to a running `tetrisd` over a local-only IPC channel |
| `tetrisu` | Terminal game client — connects, completes the secure handshake, renders the board, reads input |

### tetrish

`tetrish` is the entry shell, built as `src/tetrish/macmini_shell`. It implements the full REPL — `fork()` + `execvp()`, pipes, redirections, `$VAR` expansion, signal handling, and builtins — executes `.tetrishrc` on startup, and ships standalone system programs into `src/tetrish/bin/`, which `make bin-link` symlinks into `./bin`. Its `dspawn`, `dcheck` and `dkill` daemonise arbitrary programs; the game daemons daemonise themselves and are managed by `tetrisctl`. See [`src/tetrish/README.md`](src/tetrish/README.md).

### tetrisd

`tetrisd` is the server-authoritative game daemon. It binds the TCP port from `.tetrishrc`, accepts concurrent clients, establishes a secure session before any HTTTP traffic, maintains rooms and runs game logic, and broadcasts `STATE`. It handles `SIGTERM` (graceful shutdown) and `SIGHUP` (reload config); it ignores `SIGPIPE`, so a client that vanishes mid-send kills only its own connection. All log records are forwarded to `tetrislogd` over a non-blocking ring buffer, and a separate local-only channel exposes the control plane to `tetrisctl`. See [`src/tetrisd/README.md`](src/tetrisd/README.md).

### tetrislogd

`tetrislogd` is a separate process, not a thread inside `tetrisd`, so it survives game-server restarts. A single-threaded loop receives records on the socket from `.tetrishrc` and appends them to a log file it holds an exclusive `flock` on; it keeps no internal queue. It counts Rejected and Degraded records — not Dropped, which is `tetrisd`'s ring counter — and handles `SIGTERM`/`SIGINT` (drain, report, exit), `SIGHUP` (reopen the log file for rotation) and `SIGUSR1` (report counters). See [`src/tetrislogd/README.md`](src/tetrislogd/README.md).

### tetrisctl

`tetrisctl` owns both daemons' lifecycle. Its first version drives them by pidfile and signal — `start` forks and execs the binary and reports what its readiness pipe said, `status` reads the pidfile lock, `stop` sends `SIGTERM` and blocks until that lock comes free — so nothing about starting and stopping the stack waits on a protocol being built. A local-only control channel to `tetrisd`, carrying `status`, `kick`, `rooms`, `players` and `dropped-logs` over HTTTP rather than the public TCP port, is the second step. See [`src/tetrisctl/README.md`](src/tetrisctl/README.md).

### tetrisu

`tetrisu` is the notcurses terminal client. It currently renders an image home screen, a skippable splash intro, and a bunny-selector menu, with optional SDL2_mixer audio that degrades to silence when unavailable. Networking, board rendering, and non-blocking input are still to land. See [`src/tetrisu/README.md`](src/tetrisu/README.md).

---

## Libraries

Every library is a self-contained directory with its own `Makefile`, `include/`, `src/`, and `tests/`, building `libXXX.a` in place. All are statically linked into the binaries that use them.

| Library | Role | Linked into |
|---|---|---|
| `libtetrisbrain` | Pure game logic: board, pieces, gravity, line clear, scoring, abilities, bag, charge, effects | `tetrisd`, optionally `tetrisu` |
| `libtetrisroom` | Pure lobby domain: seating, ownership succession, start verdicts, room listing | `tetrisd` |
| `libmacminidb` | In-memory player/character/theme store with an append-only log and crash recovery | `tetrisd` |
| `libtetrissh` | Secure session: cert auth, RSA-wrapped AES key exchange, encrypted framing | `tetrisd`, `tetrisu` |
| `libcoreipc` | IPC primitives: log records, ring buffer, `AF_UNIX` helpers, self-pipe, POSIX message queues | `tetrisd`, `tetrislogd` |
| `libcoredaemon` | Detach, readiness pipe and pidfile claim for the daemons; pidfile read, probe and wait-for-exit for the CLI | `tetrisd`, `tetrislogd`, `tetrisctl` |
| `libhtttp` | HTTTP parser, serialiser, validation, and method dispatch | `tetrisd`, `tetrisu` |
| `libstatusbody` | HTTTP message-body codec — `tetrisd` encodes, `tetrisu` decodes | `tetrisd`, `tetrisu` |

---

## Architecture

The system has exactly three layers above the kernel:

```
+---------------------------------------------+
|  Application: HTTTP messages                |
|  (HyperText Tetris Transfer Protocol)       |
+---------------------------------------------+
|  Secure session                             |
|  (cert auth, RSA-wrapped AES, framed)       |
+---------------------------------------------+
|  Transport: TCP via POSIX sockets           |
+---------------------------------------------+
```

TCP reliability, ordering, and congestion control come from the kernel; tetriSH implements the two layers above it.

### Secure session

`libtetrissh` runs on both ends, so client and server handshake behaviour cannot drift. The client sends a fresh 32-byte nonce; the server replies with its PEM X.509 certificate and an RSA-PSS/SHA-256 signature over that nonce; the client verifies both against its CA, then wraps a fresh AES-256 key with RSA-OAEP/SHA-256. Failure at any step wipes secrets and closes the connection before a single HTTTP byte moves.

Every post-handshake frame is AES-256-GCM:

```text
frame_len[4] || nonce[12] || tag[16] || ciphertext
```

`frame_len` counts the bytes after the length field. Per-direction monotonic sequence counters are authenticated as AAD, so replayed or reordered frames fail tag verification. Cryptographic primitives come exclusively from the frozen `common.c` — no TLS, no `SSL_*` API, no reverse proxy.

### Processes and IPC

`tetrisd` forwards every log record to `tetrislogd` through `libcoreipc`'s non-blocking ring buffer: game-critical threads enqueue, a shipper thread drains, and records are dropped rather than blocked when the buffer is full. The drop counter will be observable via `tetrisctl dropped-logs` once `tetrisd`'s control socket lands; the counters `tetrislogd` owns surface in the log file itself at boot, on rotation, on `SIGUSR1` and at shutdown.

### Battle Royale

When a player clears N ≥ 2 lines in one move, N − 1 garbage rows are inserted at the bottom of a randomly selected other player's board in a different room. The targeting room is chosen at line-clear time, transfer is server-side managed via IPC rather than cross-room function calls, and injection is synchronised under the receiving room's mutex.

---

## Protocol: HTTTP

HTTTP (HyperText Tetris Transfer Protocol) is the application-layer protocol. Its wire format is fixed.

```
REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
RESPONSE      ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE   ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF
```

| Method | Path | Purpose |
|---|---|---|
| `JOIN` | `/room/<id>` | Join or create a room |
| `LEAVE` | `/room/<id>` | Leave a room |
| `START` | `/room/<id>` | Begin the game (room owner only) |
| `MOVE` | `/room/<id>/player/<pid>` | Body: `LEFT` or `RIGHT` |
| `ROTATE` | `/room/<id>/player/<pid>` | Body: `CW` or `CCW` |
| `DROP` | `/room/<id>/player/<pid>` | Body: `SOFT` or `HARD` |
| `STATE` | `/room/<id>` | Server-originated — pushed broadcast of board state |

`STATE` is the only server-originated message; every other method is client-initiated request/response. Clients must read pushed `STATE` frames unprompted while interleaving with their own request-response cycles.

Status codes: `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`, `500`.

Required headers:

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server `STATE` broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

The full grammar and method table live in [`lib/libhtttp/README.md`](lib/libhtttp/README.md); body formats in [`lib/libstatusbody/README.md`](lib/libstatusbody/README.md).

---

## Configuration: .tetrishrc

`.tetrishrc` is the shell start-up file, executed one command per line; blank lines and lines starting with `#` are ignored. The shell reads the project-local `.tetrishrc` first, then `$HOME/.tetrishrc`, and creates an empty project file if neither exists. Set `$TETRISHRC` to override the path.

Its role at startup is to declare the daemons and launch them in dependency order:

```
export TETRISCTL_DAEMONS="tetrislogd tetrisd"   # logger first, so it captures everything
tetrisctl start                                 # blocks until both are actually up
```

`TETRISCTL_DAEMONS` is the only place launch order is written down; `tetrisctl stop` reverses it.

Daemon settings live in the same file as `export` lines, so each one is both an ordinary shell command — inherited by anything the shell launches — and a line the daemon parses out of the file itself at boot. `tetrisd` re-reads them on `SIGHUP`:

```
export TETRISD_PORT=4242                             # TCP port
export TETRISD_DATA_DIR=tmp/tetrisd                  # player store
export TETRISD_CONFIG_DIR=lib/libmacminidb/config    # item catalogues
export TETRISD_CERT_PATH=certs/server.crt            # server certificate
export TETRISD_KEY_PATH=certs/server.key             # server private key
export TETRISD_CA_PATH=certs/ca.crt                  # CA clients verify against
export TETRISD_LOG_IPC=tmp/tetrisd/tetrislogd.sock   # tetrisd -> tetrislogd
export TETRISLOGD_SOCKET_PATH=tmp/tetrisd/tetrislogd.sock   # same socket, logger side
export TETRISLOGD_LOG_PATH=tmp/tetrislogd/tetrislogd.log # where records are written
export TETRISD_LOG_LEVEL=info                        # debug|info|warning|error
export TETRISD_MAX_CLIENTS=64                        # connection limit
export TETRISD_TICK_MS=12                            # room ticker period
export TETRISD_BR_SLOTS=4                            # Battle Royale room slots
```

Certificates come from `make certs`, which writes a development CA and server certificate into the git-ignored `certs/`; `tetrisd` refuses to boot without them. All paths are relative to the project root. No hard-coded paths exist in the source.

---

## Design Constraints

These hold across every component:

- `common.c` / `common.h` (the PA2 crypto primitives) are **never modified** — all crypto goes through them
- No TLS and no `SSL_*` API — the handshake is implemented manually in `libtetrissh`
- `libtetrisbrain` and `libtetrisroom` do no I/O and have no side effects; where a room decision needs an external fact (is a player still connected?), the caller supplies a probe callback
- `libcoreipc` must not log, `printf`, or `exit()` — it *is* the log path and must never recurse into itself
- No hard-coded paths anywhere; every path comes from `.tetrishrc` or is passed in by the caller
- No mutex is held across a blocking syscall, and lock acquisition order is documented and strictly followed
- Frame size is capped at 64 KiB; HTTTP messages exceeding it are rejected with `413 Payload Too Large`
- Everything compiles clean under `-Wall -Wextra -Werror`, and test binaries pass `valgrind --leak-check=full --error-exitcode=1`

---

## Project Structure

```
MacMini_tetriSH/
├── bin/                           Symlinks to every built binary (make bin-link)
├── lib/                           All self-contained libraries live here
│   └── libtetrisbrain/            Pattern every lib/libXXX/ follows
│       ├── Makefile               make -C lib/libXXX [test|clean|fclean|re]
│       ├── include/XXX.h          Public header (-I lib/libXXX/include)
│       ├── src/*.c                Implementation, one module per file
│       ├── tests/test_*.c         Unit tests, each with its own main()
│       ├── scripts/run_tests.sh   Formatted test runner
│       ├── obj/                   Generated objects
│       └── libXXX.a               Generated archive
├── src/
│   ├── tetrish/                   Shell → macmini_shell, system programs → bin/
│   ├── tetrisu/                   notcurses client → src/tetrisu/bin/tetrisu
│   ├── tetrisd/                   Game server (Single mode end to end)
│   └── tetrislogd/                Logger daemon (scaffolded)
├── docs/
│   ├── use_cases.md               Gameplay use cases
│   ├── game-economics.md          Points, pricing, rewards
│   ├── themes.md                  Theme catalogue
│   ├── test_plan.md               Cross-component test plan
│   ├── diagrams/                  Class, sequence, component, and solution diagrams
│   └── bugs/                      Post-mortem notes on design defects
├── .claude/skills/                Code, Makefile, and README style guides
├── scripts/                       Dependency, certificate, and launch helpers
│   ├── play.sh                    One command from a fresh clone to a client
│   ├── container.sh               Engine, client image, and how the client runs
│   └── terminal.sh                Which terminal draws, and whether one can open
├── .tetrishrc                     Shell start-up file — launches the daemons
├── Dockerfile                     Client image — builds tetrisu, never draws
├── Makefile                       Umbrella; recurses into every component
└── README.md
```

`src/tetrisctl/` follows the same pattern; the umbrella `Makefile` picks it up by wildcard.

---

## Testing

`make test` from the root builds everything, then runs every available suite. Each component also runs on its own:

```bash
make -C lib/libtetrisbrain test
make -C lib/libmacminidb test
make -C src/tetrisu test
```

The shell splits its suites into Unity unit tests and shell-script integration tests:

```bash
make -C src/tetrish unit
make -C src/tetrish integration
make -C src/tetrish test          # both
```

Every suite takes a `FILTER` substring to run a subset:

```bash
make -C lib/libtetrisbrain test FILTER=abilities   # one library suite
make -C src/tetrish unit FILTER=lexer              # one shell suite
```

Run memory-safety checks on Linux or WSL; valgrind is unreliable on current macOS.

---

## Documentation

| Path | Contents |
|---|---|
| `lib/*/README.md`, `src/*/README.md` | Each component's own scope, API, and build |
| [`docs/use_cases.md`](docs/use_cases.md) | Gameplay use cases |
| [`docs/game-economics.md`](docs/game-economics.md) | Points, pricing, rewards |
| [`docs/themes.md`](docs/themes.md) | Theme catalogue |
| [`docs/test_plan.md`](docs/test_plan.md) | Cross-component test plan |
| [`docs/diagrams/`](docs/diagrams/) | Class, sequence, domain, component, and use-case diagrams |
| [`docs/bugs/`](docs/bugs/) | Post-mortems: what broke, the fix, the lesson |
| [`.claude/skills/`](.claude/skills/) | Style guides for code, Makefiles, and READMEs |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
