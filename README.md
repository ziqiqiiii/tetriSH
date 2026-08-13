# tetriSH

> A terminal-based Battle Royale Tetris system written in C. Combining a custom Unix shell, concurrent daemon processes, authenticated encrypted networking, and a bespoke application-layer protocol (HTTTP).

Part of the **CoreStack Challenge** (50.003 × 50.005), Singapore University of Technology and Design.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Modes](#modes)
- [Run the Server Yourself](#run-the-server-yourself)
- [Make Targets](#make-targets)
- [Testing](#testing)
- [Documentation](#documentation)

---

## Prerequisites

| Platform | Client | Server |
|---|---|---|
| Linux | native or container | yes |
| WSL | native or container | yes |
| macOS | container only — `make play-image` | no — Darwin ships no `epoll`, `timerfd` or POSIX `mqueue` |

Everything the native path needs is installed for you by the build; a
containerised client needs only a container engine.

| Dependency | Needed for | Missing means |
|---|---|---|
| GCC 15 + binutils 2.44+, `make`, `pkg-config` | everything | error — GCC 15 emits `.base64`, which an older GNU as rejects |
| OpenSSL, Readline, ncurses | shell, daemons | error |
| notcurses 3.0.5+ | `tetrisu` | error — built from source where unpackaged |
| SDL2, SDL2_mixer | `tetrisu` audio | warning; compiles out |
| Container engine — Docker on Linux, colima + `docker` CLI on macOS | `make play-image` | warning; `REQUIRE_DOCKER=1` makes it fatal |
| Valgrind | memory-safety runs | warning; `REQUIRE_VALGRIND=1` makes it fatal |

```bash
make deps                # check and install anything missing (may request sudo)
make check-deps          # check only; never modifies the system
make deps-info           # show detected OS/WSL and dependency policy
```

Install covers Linux (apt, dnf/yum, pacman, zypper, apk) and macOS (Homebrew +
Xcode CLT); `AUTO_INSTALL_DEPS=0` keeps it check-only, as in CI.

---

## Quick Start

Clone, then one command from a fresh clone to a running client on the shared
tetriSH server:

```bash
git clone https://github.com/ziqiqiiii/MacMini_tetriSH.git
cd MacMini_tetriSH
make play          # client built on this host
make play-image    # client built in a container instead
```

Either installs what is missing, compiles it, opens a kitty window and starts the game in it. Every step checks before it acts, so re-running is how you restart.

| | `make play` | `make play-image` |
|---|---|---|
| Toolchain, notcurses, SDL2 | installed on this host | carried by the image |
| Host needs a compiler | yes | no |
| Container engine needed | no | yes |
| Works on macOS | no | yes |

---

## Modes

| To play | Run |
|---|---|
| On the shared server (default) | `make play` |
| On another server | `make play HOST=tetrish.dev` |
| On a server started here | `make play PLAY_ARGS=--local` |
| With no server at all — Solo on local rules | `make play PLAY_ARGS=--offline` |
| Against a local server already up | `bash scripts/play.sh --client-only` |
| In a container, image rebuilt first | `make play-image REBUILD=1` |

Stop a local server with `bash scripts/play.sh --stop`; `bash scripts/play.sh --help` lists every flag. The shared server's address is `DEFAULT_HOST` in [`scripts/play.sh`](scripts/play.sh), overridden by `TETRISH_HOST=` or `HOST=`.

---

## Run the Server Yourself

[Quick Start](#quick-start) is the short route to a client. Run the stack by
hand when working on the server (Linux or WSL only).

**1. Build and launch the shell:**
```bash
make run
```

This symlinks every built binary into `./bin`, which the shell prepends to
`$PATH`, then sources `.tetrishrc`.

**2. Launch the daemons from inside the shell:**
```
tetrish$ tetrisctl start
```

`.tetrishrc` already ends with that line, so the daemons come up before the
first prompt. `tetrisctl start` returns only once they are up — and non-zero,
with the reason on the terminal, if one is not.

**3. Connect a client (in a separate terminal):**
```bash
./src/tetrisu/bin/tetrisu
```

On the sign-in screen, **SERVER ID** is `localhost` or `127.0.0.1` (the default), then **CHECK SERVER** before **LOGIN** or **SIGN UP**.

**4. Inspect and stop:**
```
tetrish$ tetrisctl status
tetrish$ tetrisctl stop            # reverse of launch order, blocks until down
```

---

## Make Targets

Components are matched by their `Makefile`, so the ones that have not landed yet are skipped rather than failing the build.

| Target | Description |
|---|---|
| `make` / `make all` | Install missing dependencies, then build libraries, shell, and daemons |
| `make libs` / `make shell` / `make daemons` | Build one group only |
| `make bin-link` | Symlink every built binary into `./bin` |
| `make run` | Build, then launch the shell (sources `.tetrishrc`) |
| `make certs` | Generate the development CA and server certificate `tetrisd` boots with |
| `make stack` | Build, then launch the available daemons headless for integration tests |
| `make test` | Build, then run every available component test suite |
| `make clean` / `make fclean` | Remove objects; `fclean` also removes binaries and `./bin` |
| `make reset` | Stop the daemons, then `fclean` plus their runtime state (`tmp/`, `archive/`, `bin/`) — the player store survives |
| `make del-db` | Stop the daemons, then delete the player store (`TETRISD_DATA_DIR/players.log`) |
| `make re` | `fclean` + `all` |

---

## Testing

`make test` from the root builds everything, then runs every available suite.
Each component also runs on its own, the shell splits unit from integration, and
every suite takes a `FILTER` substring:

```bash
make -C lib/libtetrisbrain test FILTER=abilities
make -C src/tetrish unit FILTER=lexer
make -C src/tetrish integration
```

Everything compiles clean under `-Wall -Wextra -Werror`, and test binaries pass
`valgrind --leak-check=full --error-exitcode=1`. Run memory-safety checks on
Linux or WSL; valgrind is unreliable on current macOS.

---

## Documentation

| Path | Contents |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | Layers, binaries, libraries, HTTTP, `.tetrishrc`, project structure |
| `lib/*/README.md`, `src/*/README.md` | Each component's own scope, API, and build |
| [`docs/CONTEXT.md`](docs/CONTEXT.md) | The shared glossary — check a term here before inventing one |
| [`docs/use_cases.md`](docs/use_cases.md) | Gameplay use cases, and abilities as server-enforced effects |
| [`docs/game-economics.md`](docs/game-economics.md) | Points, pricing, rewards |
| [`docs/themes.md`](docs/themes.md) | Theme catalogue and the source of truth for ability text |
| [`docs/naming.md`](docs/naming.md) | Naming conventions; §2 is the live prefix namespace |
| [`docs/test_plan.md`](docs/test_plan.md) | Cross-component test plan |
| [`docs/diagrams/`](docs/diagrams/) | Class, sequence, domain, component, and use-case diagrams |
| [`docs/adr/`](docs/adr/) | Architecture decision records |
| [`docs/bugs/`](docs/bugs/) | Post-mortems: what broke, the fix, the lesson |
| [`docs/superpowers/`](docs/superpowers/) | Completed specs and plans, kept as a record |
| [`.claude/skills/`](.claude/skills/) | Style guides for code, Makefiles, and READMEs |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
