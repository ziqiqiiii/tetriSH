# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`src/tetrish/` has its own CLAUDE.md covering the shell's pipeline, test conventions, and 42-school code style — read that one when working inside the shell.

## Project

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the CoreStack Challenge (50.003 × 50.005) at SUTD.

Implementation status:

| Component | Status |
|---|---|
| `src/tetrish` (shell) | implemented — REPL, builtins, `.tetrishrc`, `bin/` system programs |
| `src/tetrisu` (client) | partial — notcurses intro/menu/audio, Solo, Settings, Leaderboard, Marketplace, and the multiplayer mode/lobby/create-room/waiting-room screens. The session layer is implemented and covered end to end against a real `tetrisd` (`src/tetrisu/tests/integration/test_net_solo.sh`): connect, `SIGNUP`/`LOGIN`, `JOIN`/`START`, every gameplay action, and `STATE` decoded into the Solo view model. Solo runs through `solo_authority.c`, which is either the server or the local rules, and the sign-in screen hands it a live session when `TETRISU_NET` is set. `solo_authority.c` holds the server's clock paused for the length of the client's 3-2-1 (`src/tetrisu/tests/integration/test_solo_authority.sh`), and `net_client.c` mutes stdout/stderr across the handshake, because the frozen `common.c` prints the certificate report onto the screen notcurses owns. The Leaderboard screen reads the real ranking. Settings and the Marketplace are server-authoritative too (`src/tetrisu/tests/integration/test_net_store.sh`): the catalogue and its prices come from `LIST /store`, the wallet, rank, inventory and loadout from `PROFILE`, and buying and equipping are `BUY` and `EQUIP` — the client sends an item id and nothing else. Artwork is keyed by catalogue id in `catalogue_art.c`, because that is the only field both ends agree on. The waiting room's chat is real: `[C]` composes, `net_chat.c` holds the received feed in a drop-oldest ring, and the panel draws what the server sent rather than what was typed — the sender's own line comes back down the socket with everyone else's (`src/tetrisu/tests/integration/test_net_chat.sh`). The Double and Battle Royale match screens are real and playable on the Solo pipeline (`multiplayer_match.c`, `multiplayer_match_mode.c`, and a bitmap plus a compatibility renderer), with Double served by `tetrisd` end to end and Battle Royale's rivals still modelled in-process (step 7). The local board is blitted as `MP_MATCH_BOARD_BANDS` horizontal strips so a moving piece only re-encodes the strips it touches — a terminal bitmap has no partial update, and re-encoding the whole board was the entire input latency. There is deliberately no pause in a match |
| `lib/libtetrisbrain` | implemented — all nine modules + tests |
| `lib/libmacminidb` | implemented — in-memory store, WAL, catalogues + tests. A Player carries three running numbers that answer three different questions and must not be conflated: `leaderboard_score` is the best single game (what the board ranks on, moved only by being beaten), `lifetime_points` is every point ever scored (what the wallet's rate is charged against), `wallet_points` is what is left to spend |
| `lib/libtetrissh` | implemented — handshake, session framing + tests |
| `lib/libcoreipc` | implemented — log records, ring buffer, `AF_UNIX` dgram/stream, self-pipe, mqueue + tests (7 of 7 suites pass) |
| `lib/libcoredaemon` | implemented — detach + readiness pipe, pidfile claim/probe/wait + tests (3 of 3 suites pass, valgrind-clean) |
| `lib/libhtttp` | implemented — parser, serialiser, validation, dispatch + tests |
| `lib/libstatusbody` | implemented — body codecs for state, rooms, room, chat, profile, leaderboard, catalogue + tests (8 of 8 suites pass) |
| `lib/libtetrisroom` | implemented — room/slot/lobby domain + tests (7 of 7 suites pass). Two statuses beyond the obvious four: `ROOM_SELECTING` is the character-select window a committed room holds open before it deals, and `room_rematch` is the other way a match can end — the game stops and the room does not |
| `src/tetrisd` | implemented — Single mode end to end: config, logging, listener, epoll reactor, handshake pool, auth, lobby, one gravity `timerfd`, `STATE` push, signals (incl. `SIGUSR1` state dump), input rate limiting, hold, pause/resume, restart, a held line-clear phase (the completed rows stay on the board for `clear_duration_ms` and reach the client as `phase clearing` + rows + offset), Guideline lock delay (a landed piece keeps `LOCKDOWN_DELAY_MS` and 15 move/rotate resets; hard drop is exempt, soft drop into the floor is refused), and the whole Gaiden ability catalogue + tests (22 of 22 suites pass). A game that reaches game-over or is forfeited is recorded once through `award_game` in `room.c`, which credits the wallet at `TETRISD_POINTS_PER_WALLET_POINT` (100) game points each — as the difference between what the player's `lifetime_points` were worth before the game and after, so a game worth less than the rate carries its remainder rather than rounding to nothing. The same call ranks the player on their **best single game**, never on that total. Room chat and system narration are served here too, as one feed with two authors: `CHAT /room/<name>` posts a line, `narrate.c` writes the server's own (`joined the room`, `set as owner`, `left the room`, the successor after an owner leaves), and both are pushed as a server-originated `CHAT`. The feed rides a **third outbox lane** — a small drop-oldest ring that never closes a client — because a room narrating a Battle Royale's knockouts would otherwise fill the response FIFO and kill a slow connection. Narration is emitted only from `room.c`, the one module holding both halves of a Room, and no history is kept: a late joiner has missed what was said. Steps 1–5 of the event-driven migration are done — the migration is complete. Double (step 6) is built: a room with an opponent in it reaches a match one way whichever route asked for it, because both the last readiness and the owner's `START` open a `SELECTING` window rather than dealing. The window clears every seat's declared character, so "locked in" is a fact about this match and not a leftover from the last; it ends the moment every seat has named a fighter or `TETRISD_MATCH_SELECT_MS` runs out, and the room deals itself. `room_rematch` ends a match without ending the room, so the two players come back to the seats they never left. All sixteen abilities are served, the eleven that need a Target included — `src/tetrisd/tests/test_ability_matrix.c` asserts one consequence specific to each. Battle Royale (step 7) is designed but unbuilt |
| `src/tetrislogd` | implemented — sink + reclaim, dgram receive, counters, signals, self-detach + pidfile; 4 suites pass, valgrind-clean |
| `src/tetrisctl` | partial — `start`/`status`/`stop`/`restart` by pidfile and signal + tests (2 of 2 suites pass, valgrind-clean); the control socket is a later step |

## Build & Test

The root `Makefile` is an umbrella: it installs dependencies, recurses into every
`lib/lib*/` that has a Makefile, builds the shell, then builds whichever daemon
directories exist (matched via `wildcard`, so unbuilt components are skipped
rather than erroring).

```bash
make              # deps + libs + shell + daemons
make test         # build, then run every library and component suite
make stress       # put a fleet of players on one tetrisd and report the cost
make run          # build, then launch the shell (sources .tetrishrc)
make stack        # build, then launch available daemons headless
make deps         # check/install dependencies for this OS
make check-deps   # verify dependencies without changing the system
make clean / fclean / re
make reset        # stop running daemons, then fclean + wipe their runtime state (tmp/, archive/, bin/)
make play         # install + compile on this host, then play in kitty
make play-image   # the same, built in a container instead (REBUILD=1 forces)
```

Set `AUTO_INSTALL_DEPS=0` to make the dependency step check-only (CI). Root
dependencies are the toolchain, pkg-config, OpenSSL, readline, and ncurses;
`tetrisu` additionally needs notcurses (required) and SDL2 + SDL2_mixer
(optional — audio compiles out via `-DTETRISU_ENABLE_AUDIO=0`).

A container engine is a root dependency too, because `make play` runs the
client in one — Docker on Linux (and membership of the `docker` group, which
`make deps` adds), colima plus the `docker` CLI on macOS. It is needed to play
and never to compile, so it is warned about rather than enforced, the same
treatment Valgrind gets; `REQUIRE_DOCKER=1` makes it fatal. It gets its own
step in `scripts/deps.sh` rather than riding `install_deps.sh`, because a
warning-level dependency never reaches the install path: the required-deps
check passes without an engine, so nothing would ever install one.

`WANT_ENGINE=0` turns that step off, and `make play` sets it: the native path
builds and runs the client here, so pulling in a container runtime — and the
group membership that makes one usable — would be installing Docker as a side
effect of a build that never opens a container. `play.sh` also passes
`DEPS_READY=1` into `make -C src/tetrisu` for the same reason, because that
Makefile's `check-dependencies` otherwise recurses back into the root `deps`
with the caller's flags stripped.

**macOS cannot run the server**, and it is not a packaging gap: `tetrisd` is
built directly on `epoll_create1`/`epoll_ctl`/`epoll_wait` plus `timerfd`, and
`libcoreipc`'s mqueue module on POSIX `mq_open`/`mq_send`/`mq_receive` — Darwin
has none of the three, so neither compiles there. A macOS checkout therefore
runs the client only and connects to a `tetrisd` running elsewhere; `make play`
drops the server steps there rather than refusing.

The **client** is built on the host by `make play` and in a container by `make
play-image`; both end in a kitty window, and they differ only in where the
compiler lives. The container is what makes one Linux image serve macOS too,
and is the only path that works there: the board is Kitty-graphics-protocol
escape sequences,
which are bytes on the pty `docker run -t` allocates, so the container needs no
display and the *host's* terminal draws them. Nothing hands a container's
framebuffer to a Mac, because there is no framebuffer — an earlier version of
this file claimed the opposite and it was wrong. What a container cannot supply
is the **window**: a machine with no display (a Linux VM over SSH) has none to
open, so the client runs in the terminal already attached and only that
terminal's protocol support matters. kitty, Ghostty and WezTerm speak it;
Windows Terminal does not and gets the cell renderer.

The client trusts **exactly one CA per run**, and `scripts/play.sh` picks it
from the host it was launched against: the local scratch `certs/ca.crt` for a
server started here, the committed `certs/demo-ca.crt` for one named with
`--host`. Typing a different address into SERVER ID than the one it launched
against therefore fails with `certificate signature failure` — reachable
server, wrong CA — so a remote server is named up front (`make play
HOST=...`). Concatenating both CAs into one file is not a fix: `load_cert_file`
in the frozen `common.c` reads a single certificate with `PEM_read_X509` and
ignores the rest.

Three scripts own this, one concern each — `scripts/container.sh` the engine,
image and run; `scripts/terminal.sh` which terminal draws and whether one can
be opened; `scripts/play.sh` the order. The image is deps-only plus a build of
`tetrisu`; `certs/` and `src/tetrisu/assets/` are bind-mounted read-only rather
than baked in, credentials because they are credentials and the artwork because
it is 183 MB already on disk. `bash scripts/play.sh --native` is the escape
hatch back to a host build.

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

- `tetrish` — interactive shell (REPL, builtins, `.tetrishrc`). Builds as `src/tetrish/macmini_shell`; its system programs land in `src/tetrish/bin/` and are symlinked into `./bin` by the root `bin-link` target. Its `dspawn`/`dcheck`/`dkill` are generic tools for daemonising arbitrary programs and are *not* the lifecycle manager for the game daemons.
- `tetrisd` — concurrent game server; server-authoritative; manages rooms, game logic, clients
- `tetrislogd` — separate logger process; receives log records over IPC; survives `tetrisd` restarts
- `tetrisctl` — admin CLI; owns both daemons' lifecycle (`start`/`status`/`stop`/`restart`) through their locked pidfiles. A local-only control-plane IPC channel to `tetrisd` (not the public TCP port) is a later step
- `tetrisu` — terminal client; renders board, handles input + network simultaneously

Room chat, narration, and the marketplace (buy/equip/profile/leaderboard) are
served by `tetrisd` itself over the same authenticated session — there are no
separate social-layer daemons.

Both daemons perform their own double-fork at start-up and publish a locked
pidfile; the fork lives in each one's `main.c` only, never behind
`server_start`/`logd_start`, or the in-process test suites would begin forking.
They are launched by `tetrisctl start` from inside the shell (see
`.tetrishrc`), never from the root Makefile. `TETRISCTL_DAEMONS` in
`.tetrishrc` is the only place launch order is written down — logger → game
server — and teardown is that order reversed, because stopping the logger first
would push `tetrisd`'s whole shutdown into its error file instead of the log.

**Libraries (statically linked):** each is a self-contained directory with its
own `Makefile`, `src/`, `include/`, and `tests/`, building into `lib/libXXX/libXXX.a`.

- `libtetrisbrain/` — pure game logic (no I/O, no networking); linked into `tetrisd` and optionally `tetrisu` for client-side prediction
- `libtetrisroom/` — pure lobby/room/slot domain — seating, ownership succession, start verdicts, room listing
- `libmacminidb/` — in-memory NoSQL store ("NoSQLite") for player/character/theme state, with an append-only log and crash recovery
- `libtetrissh/` — secure session handshake and encrypted framing; linked into both `tetrisd` and `tetrisu`
- `libcoreipc/` — IPC primitives (log records, ring buffer, `AF_UNIX` helpers, self-pipe, POSIX message queues); no internal dependencies, built first
- `libcoredaemon/` — both sides of daemonising: detach, readiness pipe and pidfile claim for the daemons; pidfile read, probe and wait-for-exit for `tetrisctl`. Never logs — it is the path a daemon uses to report that it cannot start
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
| `lockdown.c` | `lockdown_init`, `lockdown_grounded`, `lockdown_on_fall`, `lockdown_on_shift`, `lockdown_tick` (Guideline Extended Placement: 500 ms, 15 resets) |
| `lineclear.c` | `board_clear_lines` (returns lines cleared 0–4) |
| `scoring.c` | `score_on_clear`, `level_from_lines`, `gravity_interval_ms`, `clear_duration_ms` |
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
`db_leaderboard` / `db_rank`, `db_get_character` / `db_get_theme`,
`db_characters` / `db_themes`, `db_username_valid`. The `db_characters` /
`db_themes` pair enumerates a whole catalogue, which probing ids cannot do —
ids carry gaps, so a `NULL` is not the end of the roster.

`db_username_valid` is the charset rule, asked without a handle: printable
ASCII, no space. It is the *format's* rule and not a policy — every body that
names a player is a line of space-separated fields, and the leaderboard's
decoder reads its rows with a whitespace-delimited scan, so a single player
called `amber lee` shifted every field of that row and had the whole
leaderboard rejected as malformed for everybody. `db_signup` applies it, and
`recovery_run` drops any row that predates it rather than recovering it (its
id is still spent, so the number is never reissued). `tetrisu` keeps its own
copy of the rule in `auth_form.c` — it cannot link this archive, and a
refusal in the sign-up form beats a `400` after the fact.

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

Beneath the two blocking calls sits a pure frame codec: `session_frame_seal` /
`session_frame_open` (`frame.c`) do the AES-256-GCM work over caller-owned byte
arrays, perform no I/O, and ignore `sess->fd`; `session_send` / `session_recv`
are thin wrappers that add the socket and the 4-byte length prefix. That split
is what lets the reactor own the socket.

The server's certificate and private key are loaded once into an opaque
`t_tetrissh_credentials` (`session_credentials_load` / `session_credentials_free`),
which `session_handshake_server` takes instead of two paths — so the handshake
never touches the disk (step 2 of the event-driven migration). The object is immutable after loading
and safe to share across concurrent handshakes.

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
decodes. Four body types, each an encode/decode pair in its own `.c` — `body_state_*` (`state.c`), `body_rooms_*` (`rooms.c`), `body_profile_*` (`profile.c`), `body_leaderboard_*` (`leaderboard.c`).

Encoders return the body length in bytes, decoders return `0`; both return `-1`
with `errno` set — `EINVAL` for NULL args or a too-small buffer, `EBADMSG` for
malformed input. Callers pass a buffer and its capacity; the library allocates
nothing.

## tetrisd (src/tetrisd/include/tetrisd.h)

Entry seam is `server_start(cfg, &srv)` / `server_stop(srv)`; `main.c` is a
thin shim over it, and every test drives a real server in-process on port 0.
Config comes from `.tetrishrc` as `export TETRISD_*=...` lines (`argv[1]` →
`$TETRISHRC` → `./.tetrishrc`, then environment), re-read on SIGHUP; missing
certificates are a fatal boot error (`make certs` mints dev ones).

Threads **as built today**: one reactor thread in `epoll_wait` owns the
listener, the wake pipe and every established connection — it reads, opens
frames, dispatches, seals and writes, and it owns the lobby, the rooms, the
games, the registry and every outbox. Beside it: a `TETRISD_HANDSHAKE_WORKERS`
pool (default 4) running the blocking handshake under a per-handshake budget
(`TETRISD_HANDSHAKE_TIMEOUT_MS`, default 5 s) that the reactor enforces, and
one log shipper thread. Gravity is one `timerfd` in the reactor's epoll set,
not a thread per room.

The reactor's lifetime rule replaces the registry rwlock as the thing that
keeps `epoll_event.data.ptr` valid: **no client is freed inside the event
loop.** `client_kill` unlinks it and parks it on the zombie list, and
`client_reap` — run once after every event in a batch — is the only `free()`
site for a client.

There is no lock order, because there are no locks over game state:

> `tetrisd` has exactly one owner of all mutable game state.

The four-level order this replaced (`lobby_mutex > room->mutex > registry
rwlock > outbox mutex`) is gone with the locks in it. Two locks survive and
neither guards game state: the handshake pool's own mutex, and whatever
`libmacminidb` holds internally. Wanting a third is a sign the work is on the
wrong thread.

Steps 1–5 of the event-driven migration are
implemented. Steps 6 and 7 are Double mode and Battle Royale.

M1 serves Single mode: `SIGNUP`, `LOGIN`, `LIST` (`/rooms` and `/store`),
`JOIN` (`/rooms` creates, `/room/<name>` joins), `LEAVE`, `START`, `MOVE`,
`ROTATE`, `DROP`, `LEADERBOARD`, `PROFILE`, `BUY`, `EQUIP`, plus pushed
`STATE`. The marketplace is the store's: prices come from `config/*.cfg`,
`db_buy_*` / `db_equip_*` enforce affordability and ownership atomically, and
`BUY`/`EQUIP` answer with the updated profile so a client never draws a wallet
it has not been told. Catalogue ids are never renumbered — they live in
players' owned lists — so they carry gaps and are never a position. A player holds at most one connection — a second `LOGIN` displaces the
first. Inputs are rate limited per connection, answering `429` with
`Retry-After`. `t_game` in `game.c` is the game aggregate `libtetrisbrain` does not
own. Routes, bodies, and status mapping are in `src/tetrisd/README.md`.

A Room is two objects sharing a lobby index — the domain `t_room` and the
runtime beside it (games, dirty flags, `ticking`) — and `room.c` is the only
module that holds either. `server_room_open` and the static `room_close` are
the only callers of `lobby_create_room` / `lobby_destroy_room`, closing a room
blanks its runtime in the same call, and nothing outside `room.c` reaches
through `->room`. Handlers ask the Room (`server_room_seat`,
`server_room_start`, `server_room_input`, `server_room_describe`) rather than
the domain object; the two halves drifting apart is what evicted a player from
a room seconds after they created it
(`docs/bugs/room_runtime_outlived_its_room.md`).

The client's half of a Slot is `t_room_binding` on `t_client`, and `room.c`
owns that too: seating writes it, `server_room_unbind` is the only clear, and
`server_room_resolve(srv, cli, name)` is how anyone asks which room it names
now — it returns the room or `NULL` and writes nothing, so no caller has to
validate before it indexes. A stale binding is thrown away at one deliberate
site, `JOIN`; every other route refuses and leaves it.

## Key Design Constraints

- `common.c`/`common.h` (PA2 crypto primitives, at `lib/libtetrissh/src/common.c` and `include/libs/common.h`) are **not modified** — all crypto goes through them.
- No TLS, no `SSL_*` API — handshake is implemented manually in `libtetrissh`.
- `libtetrisbrain` and `libtetrisroom` have **no I/O, no side effects** — pure logic only. Where a room decision needs external facts (is a player still connected?), the caller supplies a probe callback.
- `libcoreipc` must not log, `printf`, or `exit()` — it *is* the log path and must never recurse into itself. Errno-style returns only.
- No hard-coded paths anywhere; all paths come from `.tetrishrc` or are passed in by the caller.
- The single-instance guard is the `flock` on each daemon's pidfile, and nothing else. It is claimed *after* the double-fork (the pid written must be the detached process's) and *before* anything a second instance could damage — for `tetrislogd` that means before `unixsock_dgram_bind`, which unlinks its socket path unconditionally.
- A daemon keeps `stderr` on the terminal until its boot has succeeded, then moves it to its configured error file. Boot failures have to reach the person who typed the command; after boot, `stderr` is `tetrisd`'s last-resort copy of records the logger could not take and `tetrislogd`'s home for Degraded records.
- `tetrisd` reaches `tetrislogd` through a non-blocking ring buffer on the *producer* side — log records are dropped (not blocked) when it is full, and that Dropped counter is what `tetrisctl dropped-logs` reports. `tetrislogd` itself keeps no queue and counts two different things: Rejected (malformed on arrival) and Degraded (valid, sink unavailable, written to stderr). The three words are not interchangeable — see `docs/CONTEXT.md`.
- No mutex held across a blocking syscall. Lock acquisition order must be documented and strictly followed to prevent deadlocks. In `tetrisd` this constraint has been retired rather than satisfied — the event-driven migration removed the shared state instead of ordering access to it — but it still binds every lock elsewhere in the project.
- Cross-player effects (garbage, offensive abilities) are queued against a **Target** and applied at that player's next piece lock, never on arrival. A player's own inputs still apply immediately. Injecting garbage under an active piece can produce a board `piece_is_valid` would reject, so the safe point is a game rule, not an optimisation. Battle Royale is one Room of 4–99 slots; garbage never crosses rooms.
- Frame size cap: 64 KiB. HTTTP messages exceeding this → `413 Payload Too Large`.
- All components compile clean under `-Wall -Wextra -Werror`; test binaries are expected to pass `valgrind --leak-check=full --error-exitcode=1`.

## HTTTP Protocol

Custom HTTP-like protocol. Two methods are server-originated (pushed): `STATE`, and `CHAT` when the server is delivering a room's feed rather than receiving a line for it. `CHAT` is the only method that travels in both directions — up as a command a player typed (`application/tetris-command`), down as a line of the feed (`application/tetris-chat`) — which is why `htttp_validate` accepts either type for it and one type for everything else. All other methods are client-initiated request/response. The `Player-Id` header is required on every authenticated request. `lib/libhtttp/README.md` carries the grammar and method table; `lib/libstatusbody/README.md` documents the body formats.

## Docs

- `README.md` — project identity and context only; the detail lives in the per-component READMEs below
- `lib/*/README.md`, `src/*/README.md` — each component's own scope, API, and build; `libhtttp` carries the protocol grammar and method table
- `.tetrishrc` — the shell start-up file; its keys are documented inline as comments
- `docs/CONTEXT.md` — the shared glossary; domain terms only, no implementation. Check a term here before inventing one
- `docs/use_cases.md`, `docs/game-economics.md`, `docs/themes.md` — gameplay and economy specs. `themes.md` is the source of truth for ability text; `use_cases.md` carries a second table of the same abilities as server-enforced effects, kept in step with it
- `docs/diagrams/class_and_sequence_diagrams/cd_sd_uc*.md` — per-use-case class, sequence, domain, and solution diagrams
- `docs/diagrams/{component_diagrams,use_case_diagrams}/` — component and use-case diagrams
- `docs/bugs/*.md` — post-mortem notes on design defects: what broke, the fix, and the lesson
- `docs/naming.md` — naming conventions and the one-time rename that reached them. All three stages are applied, so the prefix map in §2 is the live namespace: check it before inventing a prefix. §4.3 and §5.4 record the two decisions `daemon_` forced, and §5.6 the one rename deliberately left undone
- `.claude/skills/{c-style,makefile-style,readme-style}/` — the style guides these files are expected to follow, as auto-invoked skills. `c-style` discloses component layout to `LAYOUT.md` and test conventions to `TESTING.md`; `makefile-style` discloses dependency-script rules to `DEPS_SCRIPTS.md`
