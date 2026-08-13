# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`src/tetrish/` has its own CLAUDE.md covering the shell's pipeline, test conventions, and 42-school code style — read that one when working inside the shell.

## Project

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the CoreStack Challenge (50.003 × 50.005) at SUTD.

Implementation status:

| Component | Status |
|---|---|
| `src/tetrish` (shell) | implemented — REPL, builtins, `.tetrishrc`, `bin/` system programs |
| `src/tetrisu` (client) | partial — notcurses intro/menu/audio, Solo, Settings, Leaderboard, Marketplace, and the multiplayer mode/lobby/create-room/waiting-room screens. The session layer is implemented and covered end to end against a real `tetrisd` (`src/tetrisu/tests/integration/test_net_solo.sh`): connect, `SIGNUP`/`LOGIN`, `JOIN`/`START`, every gameplay action, and `STATE` decoded into the Solo view model. Solo runs through `solo_authority.c`, which is either the server or the local rules, and the sign-in screen hands it a live session when `TETRISU_NET` is set. `solo_authority.c` holds the server's clock paused for the length of the client's 3-2-1 (`src/tetrisu/tests/integration/test_solo_authority.sh`), and `net_client.c` mutes stdout/stderr across the handshake, because the frozen `common.c` prints the certificate report onto the screen notcurses owns. The Leaderboard screen reads the real ranking. Settings and the Marketplace are server-authoritative too (`src/tetrisu/tests/integration/test_net_store.sh`): the catalogue and its prices come from `LIST /store`, the wallet, rank, inventory and loadout from `PROFILE`, and buying and equipping are `BUY` and `EQUIP` — the client sends an item id and nothing else. Artwork is keyed by catalogue id in `catalogue_art.c`, because that is the only field both ends agree on. The waiting room's chat is real: `[C]` composes, `net_chat.c` holds the received feed in a drop-oldest ring, and the panel draws what the server sent rather than what was typed — the sender's own line comes back down the socket with everyone else's (`src/tetrisu/tests/integration/test_net_chat.sh`). The Double and Battle Royale match screens are real and playable on the Solo pipeline (`multiplayer_match.c`, `multiplayer_match_mode.c`, and a bitmap plus a compatibility renderer), both served by `tetrisd` end to end. The arena is drawn as `MP_ARENA_BANDS` horizontal strips per half, cut on card-row boundaries: it was one plane per half guarded by a signature folded over every card on that side, so one rival moving re-drew and re-encoded forty-nine boards, which at the arena's cadence is most of both halves most of the time — the same banding that has made Double's boards smooth since the input-latency work. A Battle Royale's rivals arrive in the frame's **arena** section — cards filed by seat, **one nibble per cell** (0 empty, 1 garbage, 2 upward a piece in the seven types' own order), on a clock slower than the board's. It was one *bit* per cell until every block in the mode drew grey: a silhouette cannot tell a piece somebody placed from a row somebody else sent them, so the client painted every filled cell one colour. A nibble is four times a bit and still a quarter of a full board, and it took `BODY_ARENA_LINE_MAX` from 91 to 241 — which failed `tetrisd`'s `_Static_assert` on `TETRISD_BODY_MAX_BYTES` at build time, exactly as that assertion is written to — and the client draws what it is sent: the roster, the head count, each rival's knockouts and placing, and who is attacking whom (`src/tetrisu/tests/integration/test_net_arena.sh`). W/A/S/D declares a targeting mode through `TARGET` and the arena marks the rivals it singles out — the two edges that are about *this* player (attacking you, singled out by you) get a second ring `MP_MATCH_CARD_MARK_PX` wide outside the ordinary one, because at thumbnail size a 3px outline in a different colour is not something anybody picks out of a screen of ninety-eight cards; an eliminated player keeps watching with their placing on the HUD. The cards are ordered by what the player needs to see first — attackers, then the ones their mode singled out, then the living, then the dead — and the layout degrades through three tiers rather than refusing a terminal one row short. The local board is blitted as `MP_MATCH_BOARD_BANDS` horizontal strips so a moving piece only re-encodes the strips it touches — a terminal bitmap has no partial update, and re-encoding the whole board was the entire input latency. A notification card is a bitmap over those bitmaps and notcurses annihilates the cells of the sprixel it covers, so `refresh_notifications` unions the cards' cell rectangles — before destroying the outgoing planes and after making the incoming ones, because a card leaving damages the cells it vacates too — and the match re-emits only the regions that rectangle overlaps (`region_restage`). It asked for a full rebuild until now, which in a Battle Royale is 170–237 ms of every board going away and coming back on **every knockout**, that card being a K.O.; restaging *all* the regions instead was measured at 182–242 ms and is no answer, because in that mode the regions are the screen (`docs/bugs/a_knockout_rebuilt_the_whole_screen.md`). `TETRISU_MATCH_TRACE` names a file to get one line per frame — `still`/`idle`/`incremental`/`REBUILD` with microseconds, plus `note refused <guard>` when a guard turned the bitmap renderer away, because every one of those returns before the first trace call and a silently-falling-back match used to leave no file at all, which reads as "tracing was off". Cards are only rebuilt when their content signature changes, and a repaint is only flagged to the screens when their **layout** signature changes: a fade step redraws the same rectangle at a different opacity and damages nothing under it, so flagging one made every frame of every fade a full-screen bitmap rebuild on every screen that consumes the flag — which is what a volume keypress looked like. There is deliberately no pause in a match. **Bots**: `B` in the waiting room adds one (owner only, up to `BOT_FARM_HAND_MAX` (4) **and never more than the room has free seats** — a Double room is two seats with the player in one, so a second `B` there could only fork a process that would be refused at the door), `K` kicks **the bot the roster pointer is on**, and the seat list marks them. Up/down move a pointer (`roster_cursor`) and the window only follows it; they used to scroll the window with nothing pointed at, which is why `K` could only ever mean "the one added last" — the parent forks a child and the child walks the server's pool for the first free `BOT_nn`, so which account it holds is not decidable in the parent. A second pipe answers that: the child writes its claimed account and closes, the parent reads it non-blocking on the roster's own cadence (`bot_farm_collect_names`), and `bot_farm_drop_named` kills by name. Its ends run opposite to the deadman's, so it is the *parent's read* end that is `FD_CLOEXEC`. Only this client's own children can be kicked, so a person, or another client's bot, is refused rather than silently redirected. `F1` is the stress tool beside it and not a feature: it fills every free seat up to `BOT_FARM_MAX` (50) on the same `add_bot` at the other ceiling, stops on the first refusal, and reports how many started — a function key with nothing advertising it, because no letter a player might reach for should fork fifty processes, and because what a room of fifty boards costs was a question the client could not ask. `TETRISD_BOT_ACCOUNTS` is 64 for it: at 32 the second half of one such room would be refused *after* each bot's JOIN, which reads as a broken bot rather than a pool that ran out. A bot is a child process running `bin/tetrisu-bot`, an ordinary client with no notcurses in it — it claims one of the server's `BOT_nn` accounts, joins, and plays through `bot_brain.c`. Everything a bot can fail at happens *after* the fork has already succeeded — no account pool on the server, a refused `JOIN`, a room that filled first — so `B` answers BOT ADDED before anything can go wrong, and the only correction available is the child exiting: `reap_lost_bots` runs `bot_farm_reap_exited` on the roster's own refresh cadence and answers `ROOM_FEEDBACK_BOT_LOST`, naming the bot log where the reason is. Nothing called that reaper before, so a bot that was refused stayed in the farm as a zombie, held a place against the limit of four, and left BOT ADDED standing on a screen whose roster never grew — which is exactly what a server built without `bot_pool.c` looks like from the client. Kicking therefore needs no protocol at all: it is a signal to a child, and the seat is released by the disconnect path. `bot_proc.c` redirects the child's stdout/stderr before `exec` (the terminal it would inherit is the one notcurses owns) and holds a deadman pipe so an orphaned bot exits on its own — the pipe's **write** end is `FD_CLOEXEC` and that is the whole of why it works, because a child otherwise inherits the write end of its own pipe (so its read end can never reach EOF) *and* the write ends of every bot forked before it. Five orphans were found still playing on the deployed server, one of them 73 minutes after its client had gone. It finds the bot binary three ways in order — `TETRISU_BOT_BIN`, the running executable (`/proc/self/exe`, `_NSGetExecutablePath` on Darwin), then `argv[0]` resolved as a path or through `PATH` — because asking only the first of those is a Linux-only question and the client runs on macOS too; `tests/test_bot_proc.c` covers the two answers no other test could, every other bots test having to set the override. The evaluator is **Dellacherie's six features** (landing height, eroded piece cells, row transitions, column transitions, holes, well sums) — it replaced a four-feature set whose bumpiness term was worth more than its hole term (`BOT_W_BUMP * 2 > BOT_W_HOLE`), so the bot paid itself to bury a hole in every notch and topped out in 49–99 pieces. The deepest well is charged nothing for the attacking tiers, because Dellacherie is a survival evaluator that prices the Tetris well as damage; that one exemption is what took `normal` from 270 rows of garbage per 3000 pieces to 371. Three tiers, and a tier is a **tempo** as much as a price: `easy` takes every clear and throws one piece in four away, `normal` prices a clear by what it *sends*, and `ultra` is not built. `bot_piece_pace_ms` gives each piece a jittered budget (`BOT_PACE_*_MS`, 1.9 s / 1.4 s / 1.0 s, shaved `BOT_PACE_LEVEL_PCT` a level down to a `BOT_PACE_FLOOR_PCT` floor) measured from the moment the bot notices the piece, and `run_match` spends the remainder pumping the socket. The floor is 65% and not 45% because at 45% easy's floor (765 ms) was faster than ultra's base, so a long game inverted the tiers at exactly the point the game got hard; the bases are bounded above too, at 1923 ms, by the +30% jitter against the 2.5 s a tier test refuses to go past. Without it nothing paced a bot at all — it planned, moved and hard-dropped at the speed of the socket, measured at 6–13 pieces a second each against a person's 1–2, and three of them buried a Battle Royale player who did nothing under 17 rows in 22 seconds. `TETRISU_BOT_LEVEL` picks the tier; Settings is where it belongs and is outstanding (`src/tetrisu/tests/integration/test_bots.sh`) |
| `lib/libtetrisbrain` | implemented — all nine modules + tests |
| `lib/libmacminidb` | implemented — in-memory store, WAL, catalogues + tests. `DB_RESERVED_PREFIX` (`BOT_`) marks an account the store owns rather than a person: `db_signup` refuses it, `db_signup_reserved` is the only way to make one, and it is kept off the leaderboard at the three *writes* (signup, recovery, `db_record_game`) rather than filtered in the two readers — so `whatever is in the list is rankable` stays true and `db_leaderboard`/`db_rank` cannot drift apart. A Player carries three running numbers that answer three different questions and must not be conflated: `leaderboard_score` is the best single game (what the board ranks on, moved only by being beaten), `lifetime_points` is every point ever scored (what the wallet's rate is charged against), `wallet_points` is what is left to spend |
| `lib/libtetrissh` | implemented — handshake, session framing + tests |
| `lib/libcoreipc` | implemented — log records, ring buffer, `AF_UNIX` dgram/stream, self-pipe, mqueue + tests (7 of 7 suites pass) |
| `lib/libcoredaemon` | implemented — detach + readiness pipe, pidfile claim/probe/wait + tests (3 of 3 suites pass, valgrind-clean) |
| `lib/libhtttp` | implemented — parser, serialiser, validation, dispatch + tests |
| `lib/libstatusbody` | implemented — body codecs for state, rooms, room, chat, profile, leaderboard, catalogue + tests (8 of 8 suites pass) |
| `lib/libtetrisroom` | implemented — room/slot/lobby domain + tests (7 of 7 suites pass). Two statuses beyond the obvious four: `ROOM_SELECTING` is the character-select window a committed room holds open before it deals, and `room_rematch` is the other way a match can end — the game stops and the room does not |
| `src/tetrisd` | implemented — Single mode end to end: config, logging, listener, epoll reactor, handshake pool, auth, lobby, one gravity `timerfd`, `STATE` push, signals (incl. `SIGUSR1` state dump), input rate limiting, hold, pause/resume, restart, a held line-clear phase (the completed rows stay on the board for `clear_duration_ms` and reach the client as `phase clearing` + rows + offset), Guideline lock delay (a landed piece keeps `LOCKDOWN_DELAY_MS` and 15 move/rotate resets; hard drop is exempt, soft drop into the floor is refused), and the whole Gaiden ability catalogue + tests (22 of 22 suites pass). A game that reaches game-over or is forfeited is recorded once through `award_game` in `room.c`, which credits the wallet at `TETRISD_POINTS_PER_WALLET_POINT` (100) game points each — as the difference between what the player's `lifetime_points` were worth before the game and after, so a game worth less than the rate carries its remainder rather than rounding to nothing. The same call ranks the player on their **best single game**, never on that total. Room chat and system narration are served here too, as one feed with two authors: `CHAT /room/<name>` posts a line, `narrate.c` writes the server's own (`joined the room`, `set as owner`, `left the room`, the successor after an owner leaves), and both are pushed as a server-originated `CHAT`. The feed rides a **third outbox lane** — a small drop-oldest ring that never closes a client — because a room narrating a Battle Royale's knockouts would otherwise fill the response FIFO and kill a slow connection. Narration is emitted only from `room.c`, the one module holding both halves of a Room, and no history is kept: a late joiner has missed what was said. Steps 1–5 of the event-driven migration are done — the migration is complete. Double (step 6) is built: a room with an opponent in it reaches a match one way whichever route asked for it, because the owner's `START` opens a `SELECTING` window rather than dealing. A seat arrives **ready** (`slot_occupy`), since joining and holding the connection through `room_seat`'s probe is already the answer readiness asks; `room_set_ready` still moves it either way, and it only decides which way it starts. That retired the other route into a match — the last seat to declare ready used to open the window too, which only worked while a seat arrived not ready and declaring was deliberate; with every seat ready from the moment a room fills, that branch would have opened the window on whatever the next `READY` happened to be, including one *withdrawing* it. A room left holding nothing but `BOT_` accounts is ended: `server_rooms_evict_abandoned` runs once per reactor batch, armed by a departure rather than asked every batch, and kills those clients so their seats are released by the one path every dropped connection takes and `room_close` takes the room on the last one out. The window clears every seat's declared character, so "locked in" is a fact about this match and not a leftover from the last; it ends the moment every seat has named a fighter or `TETRISD_MATCH_SELECT_MS` runs out, and the room deals itself. `room_rematch` ends a match without ending the room, so the two players come back to the seats they never left. All sixteen abilities are served, the eleven that need a Target included — `src/tetrisd/tests/test_ability_matrix.c` asserts one consequence specific to each. Battle Royale is built: a room of 4–99 seats that only its owner may start, a select window that survives a departure, and an **arena** of one-bit board masks on its own cadence (`TETRISD_BR_ARENA_MS`) so ninety-eight rivals cost what a thumbnail costs rather than what a board does. Garbage leaves one hole per row, drawn from the game's own seeded LCG with only the previous column excluded — it was `garbage_seq % BOARD_WIDTH` off a counter, which moves the hole every row (the stated requirement, and what its test asserted) by marching it one column right, so a Battle Royale's worth of rows drew a diagonal across the board (`docs/bugs/the_garbage_holes_marched_in_a_line.md`). A placing is taken the moment a player goes out — everybody eliminated on one tick shares it and the next skips the numbers they took — and a knockout is credited to whoever's garbage last *landed*, narrated on the chat lane. `TARGET /room/<name>` declares one of four targeting modes, and the mode decides how many rivals an attack reaches as well as which: Attackers (everyone landing rows on you) and KOs (every stack level with the tallest) hit **all** of what they matched, while Randoms and Badges hit **one**, drawn from a seed of the room's own — both of those match a crowd, so spraying either would put one clear onto most of the room. Every Target is queued the whole amount, never a share. A mode that matched nobody falls back to one drawn rival, so declaring Attackers before anybody has attacked never hits the room — the client's legend reads `(ANY)` rather than `(0)` in that state, because `(0)` describes an empty match set as an empty outcome and was read as targeting having stopped working. The same set answers abilities, except for the four that need a Target but transform their caster (Mirror, Pals, Vampire, Copy) — those land once (`hits_every_target` in the ability table). `make stress --mode br --players 99` seats a full room and plays it: 7440 arena pushes in 60 s, no session lost |
| `src/tetrislogd` | implemented — sink + reclaim, dgram receive, counters, signals, self-detach + pidfile; 4 suites pass, valgrind-clean |
| `src/tetrisctl` | partial — `start`/`status`/`stop`/`restart` by pidfile and signal, plus the four read-only Control-channel verbs (3 unit suites pass, valgrind-clean). `status`, `rooms`, `players` and `dropped-logs` ask a running `tetrisd` over `TETRISD_CONTROL_PATH` — an `AF_UNIX` socket, mode `0600`, plaintext HTTTP behind the same 4-byte length prefix, no session and no certificates, because authorisation is the ability to open the socket and nothing sent there names a Player. Which daemons have a channel is compiled in beside the pidfile keys (`control_key` in `g_known`), since it is knowledge about the programs rather than about a deployment: `tetrisd` serves one, `tetrislogd` does not, and no configuration can give the logger a channel it never implemented. A missing `TETRISD_CONTROL_PATH` is deliberately not fatal at load time — a pidfile is how every verb works and a channel is how four of them work, so a signal-managed stack still starts and stops and only the admin verbs refuse. `status` prints the pidfile rows first and Health after, never the reverse: the lock is the question that needs no running server, so an unreachable channel exits non-zero without retracting rows already printed. Every exchange is bounded by `TETRISCTL_CONTROL_MS` (3 s), because a daemon that accepts the connection and then says nothing would otherwise cost the operator their terminal rather than three seconds. `KICK` and `SHUTDOWN` (UC-23/24) are not built; `stop` remains `SIGTERM` plus the pidfile lock, which is what makes a returning `stop` mean gone rather than signalled |

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
make play         # set up everything and launch a client against a server
```

Set `AUTO_INSTALL_DEPS=0` to make the dependency step check-only (CI). Root
dependencies are the toolchain, pkg-config, OpenSSL, readline, and ncurses;
`tetrisu` additionally needs notcurses (required) and SDL2 + SDL2_mixer
(optional — audio compiles out via `-DTETRISU_ENABLE_AUDIO=0`).

**macOS cannot run the server**, and it is not a packaging gap: `tetrisd` is
built directly on `epoll_create1`/`epoll_ctl`/`epoll_wait` plus `timerfd`, and
`libcoreipc`'s mqueue module on POSIX `mq_open`/`mq_send`/`mq_receive` — Darwin
has none of the three, so neither compiles there. `tetrisu` does build there and
wants the host's own terminal, because the board is Kitty-protocol bitmaps a
container cannot hand to a Mac. So a macOS checkout builds and runs `tetrisu`
only (`make play`) and connects to a `tetrisd` running elsewhere —
`scripts/play.sh --host ADDR` (`make play-local HOST=...`) checks that server is
reachable and verifies it against the committed demo CA before launching the
client. There is no local server path on macOS.

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
- `tetrisctl` — admin CLI; owns both daemons' lifecycle (`start`/`status`/`stop`/`restart`) through their locked pidfiles, and asks a running `tetrisd` for Health, rooms, connections and the Dropped counter over its local-only Control channel (`TETRISD_CONTROL_PATH`, not the public TCP port). `KICK` and `SHUTDOWN` on that channel are a later step
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

The **Control channel** is served too, in the reactor's own epoll set rather
than on a thread of its own: `STATUS`, `ROOMS`, `PLAYERS` and `DROPPED` on
`/admin`, over an `AF_UNIX` socket at `TETRISD_CONTROL_PATH` (mode `0600`, four
connections, plaintext HTTTP behind the 4-byte length prefix). That placement is
the point — every one of those answers is read straight out of the lobby, the
registry or the logger, and the reactor already owns all three, so serving them
needs no lock and no handoff. A listener thread would have had to hand each
request over the wake fd and wait for it, which is a rendezvous bought to reach
state the answering thread was already sitting on. Authorisation is the socket's
mode: nothing arriving there names a Player, so there is no session and no
`Player-Id`. Health is assembled once by `server_health_assemble` and rendered
twice — `SIGUSR1` as a log line, `STATUS` as a body — so the two can never
disagree; the tick it reports is the **configured** one and says so on the wire,
because nothing measures an observed rate. `ROOMS` shares `server_rooms_list`
with `LIST /rooms` rather than building its own rows, so the lobby's view and the
Administrator's cannot drift. `KICK` and `SHUTDOWN` are specified and answer
`404`. The control fd is set to `-1` before anything in boot can fail, for the
reason `logger_blank` exists: teardown is shared with the start-up failure path,
and a zeroed fd is 0, so the tolerant `destroy` would have closed stdin and
unlinked a path it never bound.

M1 serves Single mode: `SIGNUP`, `LOGIN`, `LIST` (`/rooms` and `/store`),
`JOIN` (`/rooms` creates, `/room/<name>` joins), `LEAVE`, `START`, `MOVE`,
`ROTATE`, `DROP`, `LEADERBOARD`, `PROFILE`, `BUY`, `EQUIP`, plus pushed
`STATE`. `TETRISD_BOT_ACCOUNTS` (64) accounts named `BOT_01` upward are created at boot for the bots a player fills a room with — 64 rather than 32 so the client's `F1` can fill one room with fifty of them; there is no claim table, because the registry already knows who is connected — a reserved account that is already connected answers `409` instead of displacing, which is the one rule inverted from a person's. The marketplace is the store's: prices come from `config/*.cfg` — Halloween is the starter every account is granted, Mirurun costs nothing and is bought with an empty wallet exactly as the free "Design and AI" theme is, and the other two characters are 10 —
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

## Traps

Non-obvious rules that have each already cost a bug.

- `db_player_owns_character` / `db_player_owns_theme` answer a predicate and
  return `t_db_bool` — `DB_TRUE`, `DB_FALSE`, `DB_UNKNOWN`. `DB_FALSE` is a
  successful read meaning "does not own it", so comparing these against `DB_OK`
  turns every non-owner into an error.
- Audio is optional, and a warning-level dependency never reaches the install
  path: `check_deps.sh` warns about missing SDL2 and exits `0`, so audio gets
  its own step in `src/tetrisu/scripts/deps.sh` through `install_deps.sh audio`.
  `AUDIO_STAMP` (`obj/.audio-config`) puts `-DTETRISU_ENABLE_AUDIO` in the
  dependency graph, because make compares timestamps rather than flags and SDL2
  arriving after a silent build would otherwise stay silent until an `fclean`.
  The container engine gets its own step in `scripts/deps.sh` for the same
  reason; `WANT_ENGINE=0` turns it off and `make play` sets it, since a native
  build should not install Docker as a side effect.
- **`--network host` is decided by the daemon, not by `uname`.** On WSL the CLI
  is in the distro and Docker Desktop's daemon is in another VM, so the "host"
  namespace is not this distro's and a containerised client dialling `127.0.0.1`
  never reaches a local `tetrisd`. `engine_is_desktop` in
  `scripts/container.sh` asks `docker info`, and that case drops host networking
  and rewrites loopback to the distro's own address (`wsl_distro_address`) —
  never `host.docker.internal`, which is the *Windows* host and reaches this
  distro only if WSL's localhost forwarding relays it. A distro-local
  `docker.io` really does share the namespace and keeps the Linux answer.
- **Sound does not ride the pty the way the board does.** The board is escape
  sequences the host terminal renders, so the container needs no display; audio
  is SDL2 opening a device, and a container has none. `audio_args` in
  `scripts/container.sh` bind-mounts the host's PulseAudio socket
  (`/mnt/wslg/PulseServer` under WSLg, else `$XDG_RUNTIME_DIR/pulse/native`)
  plus the cookie when one exists. `--device /dev/snd` is deliberately not a
  fallback — it takes exclusive access to the card. macOS finds no socket and
  stays silent.

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
