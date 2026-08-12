# tetriSH

> A terminal-based Battle Royale Tetris system written in C — combining a custom
> Unix shell, concurrent daemon processes, authenticated encrypted networking,
> and a bespoke application-layer protocol (HTTTP).

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
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Documentation](#documentation)

---

## Status

| Component | Status |
|---|---|
| `lib/*` | Implemented — all eight libraries, unit tested, valgrind-clean |
| `src/tetrish` | Implemented — REPL, builtins, `.tetrishrc`, system programs under `bin/` |
| `src/tetrisd` | Implemented — Single and Double end to end; Battle Royale designed and unbuilt (Linux only) |
| `src/tetrislogd` | Implemented — receives, validates, writes, rotates on `SIGHUP` |
| `src/tetrisctl` | Partial — `start`/`status`/`stop`/`restart` by pidfile and signal; control socket is a later step |
| `src/tetrisu` | Playable — Solo, Double, Battle Royale; Battle Royale's rivals still modelled in-process |

---

## Prerequisites

| Dependency | Needed for | Missing means |
|---|---|---|
| GCC 15 + binutils 2.44+, `make`, `pkg-config` | everything | error — GCC 15 emits `.base64`, which an older GNU as rejects |
| OpenSSL, Readline, ncurses | shell, daemons | error |
| notcurses 3.0.5+ | `tetrisu` | error — built from source where unpackaged |
| SDL2, SDL2_mixer | `tetrisu` audio | warning; compiles out via `-DTETRISU_ENABLE_AUDIO=0` |
| Container engine — Docker on Linux, colima + `docker` CLI on macOS | [`make play-image`](#play) | warning; `REQUIRE_DOCKER=1` makes it fatal |
| Valgrind | memory-safety runs | warning; `REQUIRE_VALGRIND=1` makes it fatal |

```bash
make deps                # check and install anything missing (may request sudo)
make check-deps          # check only; never modifies the system
make deps-info           # show detected OS/WSL and dependency policy
make -C src/tetrisu deps # tetrisu render/audio deps only
```

Install covers Linux (apt, dnf/yum, pacman, zypper, apk) and macOS (Homebrew + Xcode CLT); `AUTO_INSTALL_DEPS=0` keeps it check-only, as in CI.

**macOS cannot build the tree** — `lib/libcoreipc` includes `<mqueue.h>` and `tetrisd`'s reactor is `epoll` plus `timerfd`, none of which Darwin ships. Run [`make play-image`](#play) there, and the server and valgrind on Linux or WSL.

---

## Build

Clone the repository and build everything from the repo root:

```bash
git clone <repo-url>
cd MacMini_tetriSH
make
```

Components are matched by their `Makefile`, so the ones that have not landed yet are skipped rather than failing the build.

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

Every library builds, tests, and links on its own:

```bash
make -C lib/libtetrisbrain test
gcc my_program.c lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include -o my_program
```

Networked binaries additionally link OpenSSL (`-lssl -lcrypto`); `libmacminidb` requires `-lpthread`.

---

## Play

One command from a fresh clone to a running client, on the shared tetriSH server:

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

The container never draws — the board is Kitty-graphics escape sequences on the pty, rendered by the host's terminal, so one Linux image serves Linux, macOS and WSL alike. Sound is the exception: [`scripts/container.sh`](scripts/container.sh) bind-mounts the host's PulseAudio socket, and macOS finds none and plays silent.

| Environment | Terminal | Installed by `make play` |
|---|---|---|
| Linux desktop | kitty | Yes — apt, dnf, pacman, zypper, apk |
| macOS | kitty | Yes — `brew install --cask kitty` |
| WSL with WSLg | kitty | Yes, as a native WSLg window |
| WSL without WSLg | WezTerm on Windows | No — prints the `winget` command to run |
| SSH into a Linux VM | whichever terminal you connected from | No — that machine has no display |

With no display there is no window to open, so the client runs in the terminal you are already in and only its protocol support matters: **kitty**, **Ghostty** and **WezTerm** speak it, Windows Terminal gets the cell board, and `TETRISU_RENDERER=cell` pins that path anywhere. Renderer tiers in [`src/tetrisu/README.md`](src/tetrisu/README.md).

```bash
make play HOST=tetrish.dev             # against another server, checked first
make play-image REBUILD=1              # rebuild the client image first
bash scripts/play.sh --local           # start a server here and play on it
bash scripts/play.sh --client-only     # against a local server already up
bash scripts/play.sh --stop            # take the local server down (Linux)
```

The shared server's address is `DEFAULT_HOST` in [`scripts/play.sh`](scripts/play.sh), overridden by `TETRISH_HOST=` or `HOST=`.

`--local` plus `--container` is the one combination where *whose* Docker daemon it is matters. Under Docker Desktop on WSL the daemon lives in another VM, so `--network host` is not this distro's namespace and `127.0.0.1` reaches nothing; [`scripts/container.sh`](scripts/container.sh) detects that from `docker info` and dials the distro's own address instead. A `docker.io` installed inside WSL shares the namespace and needs none of it.

**Only one CA is trusted per run**, paired with the host launched against — `certs/demo-ca.crt` for a named server, the scratch `certs/ca.crt` for `--local`. Typing a *different* address into **SERVER ID** on the sign-in screen fails with `certificate signature failure`; concatenating both CAs is not a workaround, since `load_cert_file` in the frozen [`common.c`](lib/libtetrissh/src/common.c) reads one certificate and ignores the rest.

One concern per script: [`scripts/container.sh`](scripts/container.sh) the engine, image and run; [`scripts/terminal.sh`](scripts/terminal.sh) which terminal draws; [`scripts/play.sh`](scripts/play.sh) the order.

---

## Run

[`make play`](#play) is the short route to a client. The steps below run the stack by hand, which is what you want when working on the server.

**1. Build and launch the shell:**
```bash
make run
```

This symlinks every built binary into `./bin`, which the shell prepends to `$PATH`, then sources `.tetrishrc`. To launch the shell without rebuilding, run `./src/tetrish/macmini_shell`.

**2. Launch the daemons from inside the shell:**
```
tetrish$ tetrisctl start
```

`.tetrishrc` already ends with that line, so the daemons come up before the first prompt. Each binary double-forks and reports over a readiness pipe, so `tetrisctl start` returns only once they are up — and non-zero, with the reason on the terminal, if one is not.

**3. Connect a client (in a separate terminal):**
```bash
./src/tetrisu/bin/tetrisu
```

**4. Inspect and stop:**
```
tetrish$ tetrisctl status
tetrish$ tetrisctl stop            # reverse of launch order, blocks until down
```

`tetrisctl`'s richer admin queries (`kick`, `rooms`, `players`, `dropped-logs`) need `tetrisd`'s control socket — see [Status](#status).

---

## Binaries

| Binary | Role | Detail |
|---|---|---|
| `tetrish` | Interactive shell — REPL, `.tetrishrc`, system programs into `bin/`; its `dspawn`/`dcheck`/`dkill` daemonise arbitrary programs and are not the game daemons' manager | [README](src/tetrish/README.md) |
| `tetrisd` | Server-authoritative game server — secure session before any HTTTP byte, rooms, game logic, chat, marketplace, `STATE` broadcast | [README](src/tetrisd/README.md) |
| `tetrislogd` | Dedicated logger — a separate process, not a thread, so it survives game-server restarts; holds an exclusive `flock` on the log file | [README](src/tetrislogd/README.md) |
| `tetrisctl` | Admin CLI — owns both daemons' lifecycle by pidfile and signal | [README](src/tetrisctl/README.md) |
| `tetrisu` | Terminal client — connects, handshakes, renders the board, reads input; everything with a number in it comes from the server | [README](src/tetrisu/README.md) |

---

## Libraries

Every library is a self-contained directory with its own `Makefile`, `include/`, `src/`, and `tests/`, building `libXXX.a` in place for static linking.

| Library | Role | Linked into |
|---|---|---|
| `libtetrisbrain` | Pure game logic: board, pieces, gravity, line clear, scoring, abilities, bag, charge, effects | `tetrisd`, optionally `tetrisu` |
| `libtetrisroom` | Pure lobby domain: seating, ownership succession, start verdicts, room listing | `tetrisd` |
| `libmacminidb` | In-memory player/character/theme store with an append-only log and crash recovery | `tetrisd` |
| `libtetrissh` | Secure session: cert auth, RSA-wrapped AES key exchange, encrypted framing | `tetrisd`, `tetrisu` |
| `libcoreipc` | IPC primitives: log records, ring buffer, `AF_UNIX` helpers, self-pipe, POSIX message queues | `tetrisd`, `tetrislogd` |
| `libcoredaemon` | Detach, readiness pipe and pidfile claim for the daemons; pidfile read and probe for the CLI | `tetrisd`, `tetrislogd`, `tetrisctl` |
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

- **Secure session** — `libtetrissh` runs on both ends, so handshake behaviour cannot drift: client nonce, server certificate plus RSA-PSS/SHA-256 signature over it, then an AES-256 key wrapped with RSA-OAEP/SHA-256. Every frame after it is `frame_len[4] || nonce[12] || tag[16] || ciphertext` under AES-256-GCM, with per-direction sequence counters as AAD so replays fail tag verification.
- **One owner of game state** — `tetrisd` is event-driven: one reactor thread in `epoll_wait` owns the listener, every connection, the lobby, the rooms, the games and every outbox, and gravity is one `timerfd` rather than a thread per room. Beside it run only a handshake worker pool and a log shipper, which are what the two surviving locks guard.
- **Processes and IPC** — `tetrisd` ships log records to `tetrislogd` through `libcoreipc`'s non-blocking ring buffer, dropped rather than blocked when full. The three counters are not interchangeable: **Dropped** is `tetrisd`'s ring, **Rejected** is malformed on arrival, **Degraded** is valid with the sink unavailable.
- **Battle Royale** — one room of 4–99 slots, where clearing N ≥ 2 lines queues N − 1 garbage rows against another player in that same room, landing at that player's next piece lock. Designed and unbuilt on the server; `tetrisu` models the rivals in-process meanwhile.

Design constraints binding every component are the Invariants section of [`CLAUDE.md`](CLAUDE.md); rationale lives in [`docs/`](docs/).

---

## Protocol: HTTTP

HTTTP (HyperText Tetris Transfer Protocol) is the application-layer protocol. Its wire format is fixed.

```
REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
RESPONSE      ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE   ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF
```

Methods: `SIGNUP`, `LOGIN`, `LIST`, `JOIN`, `LEAVE`, `START`, `READY`, `CHAT`, `MOVE`, `ROTATE`, `DROP`, `ABILITY`, `STATE`, `BUY`, `EQUIP`, `PROFILE`, `LEADERBOARD`, plus the `tetrisctl` admin set.

Status codes: `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`, `500`.

Required headers:

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server `STATE` broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

`STATE` is always server-originated and `CHAT` travels both ways — up as a command, down as a room's feed — so clients must read pushed frames unprompted while interleaving their own request-response cycles. The full grammar and method table live in [`lib/libhtttp/README.md`](lib/libhtttp/README.md); body formats in [`lib/libstatusbody/README.md`](lib/libstatusbody/README.md).

---

## Configuration: .tetrishrc

`.tetrishrc` is the shell start-up file, executed one command per line; blank lines and `#` lines are ignored. The shell reads the project-local file first, then `$HOME/.tetrishrc`, and creates an empty project file if neither exists. Set `$TETRISHRC` to override the path.

Its role at startup is to declare the daemons and launch them in dependency order:

```
export TETRISCTL_DAEMONS="tetrislogd tetrisd"   # logger first, so it captures everything
tetrisctl start                                 # blocks until both are actually up
```

`TETRISCTL_DAEMONS` is the only place launch order is written down; `tetrisctl stop` reverses it.

Daemon settings are `export` lines in the same file, so each is both an ordinary shell command and a line the daemon parses at boot; `tetrisd` re-reads them on `SIGHUP`:

```
export TETRISD_PORT=4242                             # TCP port
export TETRISD_DATA_DIR=tmp/tetrisd                  # player store
export TETRISD_CONFIG_DIR=lib/libmacminidb/config    # item catalogues
export TETRISD_CERT_PATH=certs/server.crt            # server certificate
export TETRISD_LOG_IPC=tmp/tetrisd/tetrislogd.sock   # tetrisd -> tetrislogd
export TETRISLOGD_LOG_PATH=tmp/tetrislogd/tetrislogd.log  # where records are written
export TETRISD_MAX_CLIENTS=64                        # connection limit
```

Every key is documented inline in [`.tetrishrc`](.tetrishrc) itself — that file, not this one, is the list. All paths are relative to the project root, and `tetrisd` refuses to boot without the certificates `make certs` writes into the git-ignored `certs/`.

---

## Project Structure

```
MacMini_tetriSH/
├── bin/                Symlinks to every built binary (make bin-link)
├── lib/                Self-contained libraries → lib/libXXX/libXXX.a
│   └── libXXX/         Makefile, include/XXX.h, src/, tests/, scripts/run_tests.sh
├── src/
│   ├── tetrish/        Shell → macmini_shell, system programs → bin/
│   ├── tetrisu/        notcurses client → src/tetrisu/bin/tetrisu
│   ├── tetrisd/        Game server → tetrisd
│   ├── tetrislogd/     Logger daemon → tetrislogd
│   └── tetrisctl/      Admin CLI → tetrisctl
├── docs/               Glossary, specs, diagrams, and bug post-mortems
├── scripts/            Dependency, certificate, and launch helpers
│   ├── play.sh         One command from a fresh clone to a client
│   ├── container.sh    Engine, client image, and how the client runs
│   └── terminal.sh     Which terminal draws, and whether one can open
├── .claude/skills/     Code, Makefile, and README style guides
├── .tetrishrc          Shell start-up file — launches the daemons
├── Dockerfile          Client image — builds tetrisu, never draws
└── Makefile            Umbrella; recurses into every component
```

---

## Testing

`make test` from the root builds everything, then runs every available suite. Each component also runs on its own, the shell splits unit from integration, and every suite takes a `FILTER` substring:

```bash
make -C lib/libtetrisbrain test
make -C lib/libtetrisbrain test FILTER=abilities
make -C src/tetrish unit FILTER=lexer
make -C src/tetrish integration
```

Everything compiles clean under `-Wall -Wextra -Werror`, and test binaries pass `valgrind --leak-check=full --error-exitcode=1`. Run memory-safety checks on Linux or WSL; valgrind is unreliable on current macOS.

---

## Documentation

| Path | Contents |
|---|---|
| `lib/*/README.md`, `src/*/README.md` | Each component's own scope, API, and build |
| [`docs/CONTEXT.md`](docs/CONTEXT.md) | The shared glossary — check a term here before inventing one |
| [`docs/use_cases.md`](docs/use_cases.md) | Gameplay use cases, and abilities as server-enforced effects |
| [`docs/game-economics.md`](docs/game-economics.md) | Points, pricing, rewards |
| [`docs/themes.md`](docs/themes.md) | Theme catalogue and the source of truth for ability text |
| [`docs/naming.md`](docs/naming.md) | Naming conventions; §2 is the live prefix namespace |
| [`docs/test_plan.md`](docs/test_plan.md) | Cross-component test plan |
| [`docs/diagrams/`](docs/diagrams/) | Class, sequence, domain, component, and use-case diagrams |
| [`docs/bugs/`](docs/bugs/) | Post-mortems: what broke, the fix, the lesson |
| [`docs/superpowers/`](docs/superpowers/) | Completed specs and plans, kept as a record |
| [`.claude/skills/`](.claude/skills/) | Style guides for code, Makefiles, and READMEs |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
