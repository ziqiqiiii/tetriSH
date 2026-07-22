# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`src/tetrish/` has its own CLAUDE.md covering the shell's pipeline, test conventions, and 42-school code style — read that one when working inside the shell.

## Project

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the CoreStack Challenge (50.003 × 50.005) at SUTD.

Implementation status:

| Component | Status |
|---|---|
| `src/tetrish` (shell) | implemented — REPL, builtins, `.tetrishrc`, `bin/` system programs |
| `src/tetrisu` (client) | partial — notcurses intro/menu/audio; no gameplay or networking yet |
| `lib/libtetrisbrain` | implemented — all six modules + tests |
| `lib/libmacminidb` | implemented — in-memory store, WAL, catalogues + tests |
| `lib/libtetrissh` | implemented — handshake, session framing + tests |
| `lib/libcoreipc` | planning only — README is the agreed scope, no code |
| `lib/libhtttp` | planning only — README is a design proposal, no code |
| `tetrisd`, `tetrislogd`, `tetrisctl` | not started — no `src/` directories yet |

## Build & Test

The root `Makefile` is an umbrella: it installs dependencies, recurses into every
`lib/lib*/` that has a Makefile, builds the shell, then builds whichever daemon
directories exist (matched via `wildcard`, so unbuilt components are skipped
rather than erroring).

```bash
make              # deps + libs + shell + daemons
make run          # build, then launch the shell (sources .tetrishrc)
make stack        # build, then launch available daemons headless
make deps         # check/install dependencies for this OS
make check-deps   # verify dependencies without changing the system
make clean / fclean / re
make reset        # fclean + wipe daemon runtime state (tmp/, archive/, bin/)
```

Set `AUTO_INSTALL_DEPS=0` to make the dependency step check-only (CI). Root
dependencies are the toolchain, pkg-config, OpenSSL, readline, and ncurses;
`tetrisu` additionally needs notcurses (required) and SDL2 + SDL2_mixer
(optional — audio compiles out via `-DTETRISU_ENABLE_AUDIO=0`).

Each library is also **self-contained** — it owns a Makefile that builds its
archive in place and runs its own tests:

```bash
make -C lib/libtetrisbrain                        # -> lib/libtetrisbrain/libtetrisbrain.a
make -C lib/libtetrisbrain test                   # formatted unit test output
make -C lib/libtetrisbrain test FILTER=abilities  # run a single suite
make -C lib/libtetrisbrain clean|fclean|re
```

Every library exposes the same five targets (`all`, `test`, `clean`, `fclean`,
`re`) and the same `FILTER=` convention. To link one into other code, add the
archive and its header path:

```bash
gcc ... lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include ...
```

The networked binaries will each link the archives they need plus OpenSSL
(`-lssl -lcrypto`); `libmacminidb` also needs `-lpthread`.

## Architecture

Three layers above the kernel:

```
HTTTP (application protocol)
Secure session (cert auth, RSA-OAEP key exchange, AES-256 frames)
TCP (POSIX sockets)
```

**Binaries:**

- `tetrish` — interactive shell (REPL, builtins, `.tetrishrc`, daemon lifecycle via `dspawn`/`dcheck`/`dkill`). Builds as `src/tetrish/macmini_shell`; its system programs land in `src/tetrish/bin/` and are symlinked into `./bin` by the root `bin-link` target.
- `tetrisd` — concurrent game server; server-authoritative; manages rooms, game logic, clients
- `tetrislogd` — separate logger process; receives log records over IPC; survives `tetrisd` restarts
- `tetrisctl` — admin CLI; talks to `tetrisd` over a local-only control-plane IPC channel (not the public TCP port)
- `tetrisu` — terminal client; renders board, handles input + network simultaneously

Room chat, narration, and the marketplace (buy/equip/profile/leaderboard) are
served by `tetrisd` itself over the same authenticated session — there are no
separate social-layer daemons.

Daemons are launched from inside the shell via `dspawn` (see `.tetrishrc`), never
from the root Makefile. `.tetrishrc` keeps the launch lines commented out until
each binary lands; launch order is logger → game server.

**Libraries (statically linked):** each is a self-contained directory with its
own `Makefile`, `src/`, `include/`, and `tests/`, building into `lib/libXXX/libXXX.a`.

- `libtetrisbrain/` — pure game logic (no I/O, no networking); linked into `tetrisd` and optionally `tetrisu` for client-side prediction
- `libmacminidb/` — in-memory NoSQL store ("NoSQLite") for player/character/theme state, with an append-only log and crash recovery
- `libtetrissh/` — secure session handshake and encrypted framing; linked into both `tetrisd` and `tetrisu`
- `libcoreipc/` — IPC primitives (ring buffer, `AF_UNIX` helpers, POSIX message queues); no internal dependencies, built first
- `libhtttp/` — HTTTP parser and serialiser; linked into both `tetrisd` and `tetrisu`

**Self-contained library layout** (every `libXXX/` follows this):

```
libXXX/
├── Makefile          # make -C lib/libXXX [test|clean|fclean|re]; builds libXXX.a
├── include/XXX.h     # public header — consumers add -I lib/libXXX/include
├── src/*.c           # implementation
├── tests/test_*.c    # unit tests, each with its own main()
├── scripts/run_tests.sh
├── obj/              # generated objects
└── libXXX.a          # generated archive
```

## libtetrisbrain API (tetrisbrain.h)

Header at `lib/libtetrisbrain/include/tetrisbrain.h`; each module is one `.c` under `src/`:

| File | Functions |
|---|---|
| `board.c` | `board_init`, `board_get`, `board_set`, `board_in_bounds`, `board_inject_garbage`, `board_copy` |
| `pieces.c` | `piece_spawn`, `piece_is_valid`, `piece_move`, `piece_rotate`, `piece_stamp` |
| `gravity.c` | `gravity_tick`, `piece_soft_drop`, `piece_hard_drop` |
| `lineclear.c` | `board_clear_lines` (returns lines cleared 0–4) |
| `scoring.c` | `score_on_clear`, `level_from_lines`, `gravity_interval_ms` |
| `abilities.c` | `board_cut_top`, `board_cut_bottom`, `board_apply_gravity`, `board_invert`, `board_fill_rows`, `board_clear_cells`, `board_delete_columns` |

`t_brain_result` return codes: `BRAIN_OK`, `BRAIN_BLOCKED`, `BRAIN_LOCKED`, `BRAIN_GAME_OVER`, `BRAIN_CLEARED`.

Out-of-bounds reads via `board_get` return `CELL_FILLED` (solid wall), so collision checks work uniformly without range guards in every caller.

## libmacminidb API (macminidb.h)

Opens with `db_open(data_dir, config_dir, &db)` — both paths come from the
caller, never hard-coded. Every public call takes exactly one lock (a rwlock
over the in-memory index) and returns a `t_db_result`: `DB_OK`, `DB_NOT_FOUND`,
`DB_EXISTS`, `DB_BAD_CREDS`, `DB_INSUFFICIENT`, `DB_NOT_OWNED`, `DB_IO_ERROR`,
`DB_FULL`, `DB_INVALID`.

Surface: `db_signup` / `db_login` / `db_get_player`, `db_buy_character` /
`db_buy_theme` / `db_equip_character` / `db_equip_theme`,
`db_player_owns_character` / `db_player_owns_theme`, `db_record_game`,
`db_leaderboard` / `db_rank`, `db_get_character` / `db_get_theme`.

Indexes: hash map (`username → player`) for point lookups, skip list
(`(score, id) → player`) for the leaderboard. Writes append to a
Last-Writer-Wins log replayed on boot; a background flusher thread `fdatasync`s
it every second, so no blocking syscall happens under the store's lock. Static
character and theme catalogues load once from `config/*.cfg` at open.

## libtetrissh API (tetrissh.h)

`t_session` plus `session_send` / `session_recv` / `session_close`, with a role
enum (`TETRISSH_ROLE_CLIENT` / `TETRISSH_ROLE_SERVER`). `session_recv` returns
one complete decrypted message. `src/common.c` is the frozen course-provided
crypto helper — the Makefile compiles it separately with relaxed flags.

## Key Design Constraints

- `common.c`/`common.h` (PA2 crypto primitives, at `lib/libtetrissh/src/common.c` and `include/libs/common.h`) are **not modified** — all crypto goes through them.
- No TLS, no `SSL_*` API — handshake is implemented manually in `libtetrissh`.
- `libtetrisbrain` has **no I/O, no side effects** — pure logic only.
- `libcoreipc` must not log, `printf`, or `exit()` — it *is* the log path and must never recurse into itself. Errno-style returns only.
- No hard-coded paths anywhere; all paths come from `.tetrishrc` or are passed in by the caller.
- `tetrislogd` and `tetrisd` communicate over IPC with a non-blocking ring buffer — log records are dropped (not blocked) under pressure; the drop counter is exposed via `tetrisctl dropped-logs`.
- No mutex held across a blocking syscall. Lock acquisition order must be documented and strictly followed to prevent deadlocks.
- Frame size cap: 64 KiB. HTTTP messages exceeding this → `413 Payload Too Large`.
- All components compile clean under `-Wall -Wextra -Werror`; test binaries are expected to pass `valgrind --leak-check=full --error-exitcode=1`.

## HTTTP Protocol

Custom HTTP-like protocol. Only `STATE` is server-originated (pushed); all other methods are client-initiated request/response. The `Player-Id` header is required on every authenticated request. See README.md for the full grammar and method table.

## Docs

- `README.md` — full protocol grammar, method table, `.tetrishrc` keys, IPC design
- `docs/use_cases.md`, `docs/game-economics.md`, `docs/themes.md` — gameplay and economy specs
- `docs/class_diagrams/class-diagrams.md` — system and per-library class diagrams
- `docs/cleaning/{code_style,makefile_style,readme_style}.md` — style guides these files are expected to follow
