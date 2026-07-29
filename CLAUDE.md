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
| `lib/libtetrisbrain` | implemented — all nine modules + tests |
| `lib/libmacminidb` | implemented — in-memory store, WAL, catalogues + tests |
| `lib/libtetrissh` | implemented — handshake, session framing + tests |
| `lib/libcoreipc` | implemented — log records, ring buffer, `AF_UNIX`, mqueue + tests |
| `lib/libhtttp` | implemented — parser, serialiser, validation, dispatch + tests |
| `lib/libstatusbody` | implemented — body codecs for state, rooms, profile, leaderboard + tests |
| `lib/libtetrisroom` | implemented — room/slot/lobby domain + tests |
| `src/tetrisd`, `src/tetrislogd` | scaffolded — Makefile, header, and empty `main.c`; no logic yet |
| `tetrisctl` | not started — no `src/` directory yet |

## Build & Test

The root `Makefile` is an umbrella: it installs dependencies, recurses into every
`lib/lib*/` that has a Makefile, builds the shell, then builds whichever daemon
directories exist (matched via `wildcard`, so unbuilt components are skipped
rather than erroring).

```bash
make              # deps + libs + shell + daemons
make test         # build, then run every library and component suite
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
- `libtetrisroom/` — pure lobby/room/slot domain — seating, ownership succession, start verdicts, room listing
- `libmacminidb/` — in-memory NoSQL store ("NoSQLite") for player/character/theme state, with an append-only log and crash recovery
- `libtetrissh/` — secure session handshake and encrypted framing; linked into both `tetrisd` and `tetrisu`
- `libcoreipc/` — IPC primitives (log records, ring buffer, `AF_UNIX` helpers, self-pipe, POSIX message queues); no internal dependencies, built first
- `libhtttp/` — HTTTP parser and serialiser; linked into both `tetrisd` and `tetrisu`
- `libstatusbody/` — HTTTP message-body codec — encodes the bodies `tetrisd` sends, decodes the ones `tetrisu` receives

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
| `bag.c` | `piece_bag_init`, `piece_bag_next` (7-bag randomiser) |
| `charge.c` | `charge_state_init`, `charge_on_clear`, `ability_cost`, `charge_can_afford`, `charge_deduct`, `charge_transfer` |
| `effects.c` | `effect_state_init`, `effect_apply`, `effect_clear`, `effect_on_piece_lock`, plus the `effect_*` predicates (`rotation_blocked`, `fastdrop_blocked`, `controls_inverted`, `thwack_active`, `fry_rows`) |

`t_brain_result` return codes: `BRAIN_OK`, `BRAIN_BLOCKED`, `BRAIN_LOCKED`, `BRAIN_GAME_OVER`, `BRAIN_CLEARED`.

Out-of-bounds reads via `board_get` return `CELL_FILLED` (solid wall), so collision checks work uniformly without range guards in every caller.

## libmacminidb API (macminidb.h)

Opens with `db_open(data_dir, config_dir, &db)` — both paths come from the
caller, never hard-coded. Every public call takes exactly one lock (a rwlock
over the in-memory index) and returns a `t_db_result`: `DB_OK`, `DB_NOT_FOUND`,
`DB_EXISTS`, `DB_BAD_CREDS`, `DB_INSUFFICIENT`, `DB_NOT_OWNED`, `DB_IO_ERROR`,
`DB_FULL`, `DB_INVALID`.

The two ownership probes (`db_player_owns_character` / `db_player_owns_theme`)
are the exception: they answer a predicate rather than reporting an outcome, so
they return a `t_db_bool` — `DB_TRUE`, `DB_FALSE`, `DB_UNKNOWN` (bad handle).
`DB_FALSE` means "does not own it", which is a successful read, not a failure —
so never test these against `DB_OK`. The catalogue getters `db_get_character` /
`db_get_theme` return a borrowed pointer or `NULL`.

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

## libtetrisroom API (tetrisroom.h)

Pure domain library — no I/O, no networking, same discipline as `libtetrisbrain`.
Three layers: `t_membership` (a seated player), `t_slot` (a seat), `t_room`
(mode, slots, status), and `t_lobby` (a set of rooms).

Verdict enums report *why* an operation was refused rather than a bare failure:
`t_join_verdict` — `JOIN_ACCEPTED`, `JOIN_FULL`, `JOIN_IN_GAME`;
`t_start_verdict` — `START_ACCEPTED`, `START_NOT_OWNER`, `START_TOO_FEW_PLAYERS`,
`START_ALREADY_STARTED`. Room status is `ROOM_WAITING`, `ROOM_READY`,
`ROOM_IN_GAME`, `ROOM_FINISHED`.

`room_seat` and `room_release` take a `probe` callback (`bool (*)(void *ctx,
t_player_id)`) so liveness is asked of the caller — the library never touches a
socket itself. `room_release` picks a successor via `room_select_successor` when
the owner leaves.

## libstatusbody API (statusbody.h)

The HTTTP message-body codec shared by both ends: `tetrisd` encodes, `tetrisu`
decodes. Four body types, each an encode/decode pair in its own `.c` —
`sb_state_*` (`state.c`), `sb_rooms_*` (`rooms.c`), `sb_profile_*`
(`profile.c`), `sb_leaderboard_*` (`leaderboard.c`).

Encoders return the body length in bytes, decoders return `0`; both return `-1`
with `errno` set — `EINVAL` for NULL args or a too-small buffer, `EBADMSG` for
malformed input. Callers pass a buffer and its capacity; the library allocates
nothing.

## Key Design Constraints

- `common.c`/`common.h` (PA2 crypto primitives, at `lib/libtetrissh/src/common.c` and `include/libs/common.h`) are **not modified** — all crypto goes through them.
- No TLS, no `SSL_*` API — handshake is implemented manually in `libtetrissh`.
- `libtetrisbrain` and `libtetrisroom` have **no I/O, no side effects** — pure logic only. Where a room decision needs external facts (is a player still connected?), the caller supplies a probe callback.
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
- `docs/diagrams/class_and_sequence_diagrams/cd_sd_uc*.md` — per-use-case class, sequence, domain, and solution diagrams
- `docs/diagrams/{component_diagrams,use_case_diagrams}/` — component and use-case diagrams
- `docs/bugs/*.md` — post-mortem notes on design defects: what broke, the fix, and the lesson
- `skills/{code_style,makefile_style,readme_style}.md` — style guides these files are expected to follow; see `skills/README.md`
