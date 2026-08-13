# tetrisd

The server-authoritative game daemon for tetriSH. Accepts encrypted client sessions over TCP, owns the lobby, the rooms and every game board, and pushes each player their own `STATE` on a gravity tick. It detaches itself and publishes a locked pidfile; `tetrisctl` starts, inspects and stops it through that file.

---

## Table of Contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Protocol](#protocol)
- [Signals](#signals)
- [Logging](#logging)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [References](#references)

---

## Features

- One reactor thread owns every connection, room, game and outbox — no locks over game state; a bounded pool runs the one blocking call under a deadline the reactor enforces
- Server-authoritative on the board and off it: a client sends inputs and item ids, never board state, prices or inventory
- One gravity `timerfd` for the whole server; elapsed comes from the clock, so a late tick catches games up rather than running them slow
- One connection per player — a second `LOGIN` displaces the first; disconnecting forfeits, so an abandoned game is still recorded
- Inputs rate limited per connection (token bucket, `429` with `Retry-After`); passwords salted and SHA-256 hashed here, so plaintext never reaches the store
- Chat and system narration are one feed on their own outbox lane: best-effort, drop-oldest, never a reason to close a connection
- Detaches itself, holds a locked pidfile, reports its boot over a readiness pipe

| Mode | Status |
|---|---|
| Single | End to end — hold, pause/resume, restart, the self-affecting abilities |
| Double | End to end — select window, countdown, rival board in every snapshot, garbage, every ability, `won`/`lost` recorded once, `room_rematch` without a lobby trip ([plan](double-mode-plan.md)) |
| Battle Royale | Designed, unbuilt ([plan](battle_royale_plan.md)) |

### Match flow

Both seats `READY`, or the owner's `START`, opens a `SELECTING` window of `TETRISD_MATCH_SELECT_MS` — it does not deal. Opening it clears every seat's declared character, so a fighter belongs to *this match* and an account-wide `EQUIP` mid-match cannot change it; `READY` carries an optional `character <id>`, checked with `db_player_owns_character` against `DB_TRUE`, and with none named the account's equipped character stands (every Single game). The room deals once every seat has named one or the clock runs out, then holds the boards for `TETRISD_MATCH_COUNTDOWN_MS`.

### Garbage and targeting

Garbage crosses at one place — `settle_garbage` in `room.c`, the only module holding both halves of a Room. `libtetrisbrain` decides how many rows a clear is worth (`garbage_lines_from_clear`, N−1) and the seating decides who owes them, answered by `server_room_target_of`. It runs after every game advances and before any snapshot, so the frame showing a clear shows the `pending` it caused. Garbage never crosses rooms.

**Rows land at the Target's next piece lock, never on arrival** — injecting under a falling piece can produce a board `piece_is_valid` rejects. The drain sits after the clear resolves and before `spawn_next`, so the rows are part of the board the next piece is validated against and a failed spawn is a top-out. Targeted abilities wait on the same lock: an effect counted in pieces would otherwise be a piece short before it began.

| Lands on | Abilities | When |
|---|---|---|
| The caster | Mirror, Pals, Vampire, Copy | At once — they read the Target but write the caster, so nothing needs deferring |
| The Target | Dark, Bomb, Inversion, Pentaris, Sirtet, Paralysis, Nue | Queued to the Target's next piece lock |

- **Mirror** is checked when an ability is *aimed*, not when it lands, since a queued effect can be several behind by then. Reflecting is not refusing — the sender still paid, and it still happens, to them
- **Fry** puts rows on the sender's own floor to burn at their next lock, then sends them whole (not through N−1) to the Target, on the ability lane so Pals cannot absorb them
- **Pals** makes incoming garbage take rows *off* the floor, ability garbage excepted — which is why `t_game` counts `pending_garbage` and `pending_ability_garbage` separately
- **Dark and Pals** are the two effects `libtetrisbrain` refuses to time: `t_game` carries `dark_pieces` / `pals_pieces` and `age_server_effects` runs them down on the holder's locks. Dark's count is armed where it *lands*, so the delivering lock does not spend one
- **Dark** is the one effect the server cannot carry out — it rides the wire as a count and `tetrisu` blanks the drawn grid, not the board, so hidden rows still collide

Bomb's scatter and garbage's hole column walk from the game's own counters, `libtetrisbrain` owning no RNG by contract — unpredictable to a player without being unreproducible to a test.

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

`tetrisd` links eight CoreStack archives — `libcoreipc`, `libcoredaemon`, `libhtttp`, `libtetrisbrain`, `libmacminidb`, `libtetrissh`, `libtetrisroom`, `libstatusbody` — plus `-lssl -lcrypto`, `-lpthread` and `-lrt`. Certificates are a boot requirement, not an option; `make certs` from the root mints development ones.

---

## Build

```bash
make -C src/tetrisd
```

| Target | Description |
|---|---|
| `all` | Build every archive under `LIB_NAMES` in place, link `src/tetrisd/tetrisd` |
| `libs` | Build the CoreStack archives only |
| `run` | Build and launch from the project root |
| `test` | Build and run every suite; `FILTER=` selects |
| `clean` / `fclean` / `re` | Objects and test binaries / and the binary / full rebuild |

Cleanup never recurses into `lib/` — the archives are shared and owned by their own directories.

---

## Run

**Run it from the project root**: paths in `.tetrishrc` are root-relative, not relative to the rc file or the binary. `make -C src/tetrisd run` does this; `.tetrishrc` does it through the lifecycle manager:

```bash
tetrisctl start tetrislogd
tetrisctl start tetrisd
tetrisctl status
tetrisctl stop tetrisd
```

The binary daemonises itself, returning to the prompt with `0` only once the port is bound and the store is open, non-zero with the reason printed if not. **Start `tetrislogd` first**, stop it last — it binds the socket `tetrisd` ships records to, and stopping it first pushes `tetrisd`'s whole shutdown into its error file. `TETRISCTL_DAEMONS` in `.tetrishrc` is the only place that order is written down.

---

## Protocol

HTTTP over an authenticated, encrypted session. `Player-Id` is required on every authenticated request and checked against the player bound at `LOGIN`, so a forged header buys nothing. Rooms are named by the lobby (`S-01`, `D-02`, `BR-03`), never by clients, which is why creation addresses the collection. Bodies are `key value` lines, one key per line.

| Method | Path | Effect |
|---|---|---|
| `SIGNUP` | `/account` | Register a player; `201` with its id, `409` when the name is taken |
| `LOGIN` | `/session` | Bind the connection to a player, displacing any older one |
| `LIST` | `/rooms` | Every occupied room, in-game ones included |
| `LIST` | `/room/<name>` | Room state and ordered occupied seats; visible only to a seated player |
| `LIST` | `/store` | The character and theme catalogues, with this server's prices |
| `LEADERBOARD` | `/leaderboard` | Top ten by recorded score; registering is what puts a player on it |
| `PROFILE` | `/player/<pid>` | Wallet, score, rank, owned items, loadout; `403` for another player |
| `BUY` | `/store/character/<cid>`, `/store/theme/<tid>` | Spend the wallet; answers the profile, `403` `insufficient-funds`, `409` `inventory-full` |
| `EQUIP` | `/player/<pid>/character/<cid>`, `/player/<pid>/theme/<tid>` | Set the loadout; answers the profile, `403` `not-owned` |
| `JOIN` | `/rooms`, `/room/<name>` | Create a room in the body's `mode` and own it (`201`), or take a slot in one (`200`) |
| `LEAVE` | `/room/<name>` | Give up the slot, forfeiting a game in progress |
| `READY` | `/room/<name>` | Body `ready <0\|1>`, optionally `character <id>`; `403` when not owned. Pressed twice per match by design — once to commit, once inside the select window to lock a fighter in |
| `START` | `/room/<name>` | Owner begins; `403` for a non-owner. Single deals on the spot, anything with an opponent opens the select window |
| `MOVE` / `ROTATE` / `DROP` | `/room/<name>/player/<pid>` | Body `LEFT\|RIGHT`, `CW\|CCW`, `SOFT\|HARD` |
| `HOLD` | `/room/<name>/player/<pid>` | No body — swap with the hold slot, once per piece |
| `PAUSE` / `RESTART` | `/room/<name>/player/<pid>` | Body `PAUSE\|RESUME` / no body, dealing a fresh game; **Single only** |
| `ABILITY` | `/room/<name>/player/<pid>` | Body `level <1-4>`, optionally `column <0-9>` to aim Sol |
| `CHAT` | `/room/<name>` | Body `text <line>`; broadcast including the sender. `429`, `404`, `403` `muted`, `400` `bad-text` |
| `STATE` | `/room/<name>/player/<pid>` | **Server-originated** — one player's board on tick, with the countdown and, at match end, that player's result |
| `CHAT` | `/room/<name>` | **Server-originated** — one line of the room's feed, pushed to every seat |

Statuses in use: `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`, `500`, `501`. A refusal the domain has a reason for carries it — `full`, `in-game`, `not-owner`, `too-few-players`, `already-started`, `already-in-room`, `lobby-full`, `input-blocked`, `not-single`, `no-target`, `no-charge`, `ability-blocked`, `ability-unavailable`, `ability-invalid`, `insufficient-funds`, `inventory-full`, `not-owned`, `muted`, `bad-text` — so a player can tell a full room from one already playing.

Input is refused `409` whenever nothing is falling: through the match countdown, and through a clear. A piece that touches down keeps `LOCKDOWN_DELAY_MS` (500 ms), refreshed by every accepted `MOVE` or `ROTATE` up to `LOCKDOWN_MAX_RESETS` (15) times and refilled whenever it falls past its lowest row — the Guideline's Extended Placement. `DROP HARD` is exempt and locks at once; `DROP SOFT` on the floor is refused and keeps the delay ([post-mortem](../../docs/bugs/the_piece_locked_the_moment_it_landed.md)). A lock that completes rows holds them for `clear_duration_ms(level)` — still filled, nothing scored, no piece dealt — and every tick of that hold is a `STATE` carrying `phase clearing` ([post-mortem](../../docs/bugs/the_line_clear_never_reached_the_client.md)).

### Abilities

An `ABILITY` body names a **level**, never an ability: which four a level selects from is decided by the character, read from the match or the account rather than the request, so a client cannot use one it has not equipped. Charge is `libtetrisbrain`'s — two cleared lines bank one, levels 1–4 cost 2/4/6/8, deducted only once an activation is accepted, so a refusal costs nothing. **Single mode has no Target** ([`docs/CONTEXT.md`](../../docs/CONTEXT.md)), so the twelve abilities landing on somebody else are refused `reason no-target` rather than redirected. Four are self-affecting — every character has one:

| Character | Level | Ability | What it does to its own board |
|---|---|---|---|
| Halloween | 1 | Fry | Fills the bottom three rows; they burn off at the next lock |
| Mirurun | 1 | Mirurun | Removes the bottom four rows |
| Princess | 1 | Sol | Clears three adjacent columns, aimed by the body's `column` |
| Wolf-man | 1 | Cut | Removes the top four rows |
| Wolf-man | 4 | Thwack | For the next four pieces, blocks cascade after a clear |

Every transform is tried on a copy and kept only when the falling piece survives, so an ability that would bury the piece is refused whole rather than half-applied.

### Marketplace

The equipped character decides which abilities exist, so ownership is a gameplay fact and `tetrisd` decides all of it: prices from `config/characters.cfg` and `config/themes.cfg`, balance from the store, `db_buy_*` / `db_equip_*` enforcing affordability and ownership atomically. `BUY` and `EQUIP` answer with the **updated profile**, so acting and re-reading are one round trip; buying something already owned is a `200` no-op carrying that same profile. Catalogue ids are never reused or renumbered — they live in players' owned lists, and theme id `5` stays a gap.

The wallet is filled by playing: a game that reaches game-over — or is forfeited by leaving, topping out or losing the connection — is recorded once by `db_record_game` at `TETRISD_POINTS_PER_WALLET_POINT` (100) game points per wallet point, credited as the difference in what `lifetime_points` is worth before and after, so a small game leaves its remainder on the account instead of rounding to nothing ([`docs/game-economics.md`](../../docs/game-economics.md)). Restarting records nothing. The same call ranks the player on their **best single game**, never on that total.

---

## Signals

| Signal | Effect |
|---|---|
| `SIGTERM`, `SIGINT` | Stop the loop, forfeit and close every connection, release the pidfile |
| `SIGHUP` | Re-read `.tetrishrc` — only `TETRISD_LOG_LEVEL` and `TETRISD_TICK_MS` can change |
| `SIGUSR1` | Dump port, uptime, clients, rooms and slots to the log — the stand-in for a control channel |
| `SIGPIPE` | Ignored — a client vanishing mid-send kills its own connection, not the server |

Handlers set a flag and write one byte down the self-pipe; the reactor asks the `signals_take_*` calls once per wake-up, so repeated signals coalesce. A reload cannot move the port, the certificates or the data directory — those name already-open resources. Retiming gravity is a `timerfd_settime` on the thread owning the timer, leaving `last_tick` alone so accumulated gravity survives.

---

## Logging

Every line about one connection is filed under a **connection id** — `conn N`, counted from 1 per run and never reused, because the kernel hands a closed descriptor's number straight back:

```text
conn 3 accepted
conn 3 handshake ok
conn 3 (anonymous) SIGNUP 201
conn 3 amber LOGIN 200
conn 3 amber MOVE 200          # debug
conn 3 amber disconnected
```

The id is claimed once the registry has taken the client, and the accept line is written *before* the handshake pool is signalled — the other order let `handshake failed` precede `accepted`. The one refusal with no id is the client limit, refused before a connection exists to name.

The **Access line** is one record for one complete HTTTP exchange, written once the answer is known so a request and its response are a single line rather than two interleaved halves. Every exchange has one, including those refused before any handler ran; an unparseable frame reports its method as `-`. The actor is the username once `LOGIN` has bound one and `(anonymous)` before that. `MOVE`, `ROTATE`, `DROP` and `HOLD` log at **debug** and everything else at **info**, those four being the only methods sent at key-repeat rate. Records reach `tetrislogd` through the ring buffer and the shipper thread, never the reactor; one the ring had no room for is **Dropped** and counted, reported by `SIGUSR1` and kept distinct from `tetrislogd`'s Rejected and Degraded ([`docs/CONTEXT.md`](../../docs/CONTEXT.md)).

---

## Configuration

Every setting comes from `.tetrishrc` as `export TETRISD_*=<value>` lines, resolved `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variable. **The keys are documented inline in `.tetrishrc`**, which is their home.

```text
TETRISD_PORT                  0-65535 (0 binds a kernel-assigned port)
TETRISD_TICK_MS               1-1000                     # re-read on SIGHUP
TETRISD_LOG_LEVEL             debug|info|warning|error   # re-read on SIGHUP
TETRISD_MAX_CLIENTS           up to 4096
TETRISD_INPUT_BURST/RATE      1-10000
TETRISD_HANDSHAKE_WORKERS     up to 64
TETRISD_HANDSHAKE_TIMEOUT_MS  100-60000
```

An unknown `TETRISD_*` key or an out-of-range value fails the boot, so a setting documented but never wired up cannot silently do nothing. A missing rc file is fine — the defaults work — but missing certificates are fatal. `TETRISD_LOG_IPC` must equal `TETRISLOGD_SOCKET_PATH`: one socket, named twice.

---

## Architecture

### One owner

> `tetrisd` has exactly one owner of all mutable game state.

The reactor thread reads the socket, opens the frame, dispatches, seals the answer and writes it — and it alone touches the lobby, every room, every game, the registry and every outbox. There is no lock order because there are no locks over game state; the four-level order this replaced (`lobby_mutex > room->mutex > registry rwlock > outbox mutex`) is gone with them. Two locks survive and guard none of it: the handshake pool's, and `libmacminidb`'s internal — wanting a third means the work is on the wrong thread. `tetrisd` is not single-threaded but single-owner: the handshake workers, each owning one un-established connection, and the log shipper may not touch game state.

### The loop

```text
epoll_wait(listener, wake pipe, timer, every client)
     │
     ├── listener      accept until EAGAIN, spawn a client, submit it to the pool
     ├── wake pipe     drain, then: signals (STOP, HUP, USR1), finished handshakes
     ├── timer         read the clock once, tick every playing room, arm the sweep
     └── client        EPOLLOUT → flush; EPOLLIN/HUP/ERR → read, dispatch, reply
     │
     ▼
   sweep              write out what the tick queued; kill clients that overflowed
     │
     ▼
   client_reap        the only free() site for a client
```

The wait is unbounded unless a handshake is in flight, when it ends at that deadline; gravity needs none, the tick timer being a descriptor in the same set. `epoll_event.data.ptr` carries three kinds of object, so every watched object begins with a `t_event_tag` the reactor reads first — recovering a client from the kernel without an fd map to keep in step.

**Client lifetime.** What makes `data.ptr` safe: **no client is freed inside the event loop.** A batch can carry several events for one client, so `client_kill` unlinks it onto the zombie list, later events skip anything marked `dead`, and `client_reap` is the only `free()`. The registry is a directory rather than a lifetime guard — every live connection addressable by player id, which is how a tick reaches the right outbox and how `LOGIN` finds the connection it displaces.

**Handshake pool.** `session_handshake_server` is the one genuinely blocking call, so it runs off the loop. A worker owns its client outright until it hands the session back; `client_adopt` sets `watched` on the far side of that handoff and the sweep skips anything not yet `watched`, or two threads would share one socket. `TETRISD_HANDSHAKE_TIMEOUT_MS` budgets the whole handshake, not one read: `SO_RCVTIMEO` bounds a single `recv` and `libtetrissh` loops, so a peer dribbling a byte per timeout would otherwise hold a worker forever.

### Outbox

Three lanes, because three kinds of message fail differently.

| Lane | Shape | On overflow |
|---|---|---|
| Responses | Bounded FIFO | Close the client — it cannot keep up, and buffering more lets it exhaust the server |
| `STATE` | One-slot mailbox | Overwrite; a snapshot supersedes the one before it |
| `CHAT` | Small ring | Drop the oldest, **never** close the client (UC-09 E1) |

Chat cannot share the response FIFO: a Battle Royale room narrating knockouts would fill it and kill a connection whose only fault was being slow, which is also why `registry_enqueue_chat` is separate from `registry_enqueue` — chat must not reach the `shutdown` the response path takes. Draining order is responses, chat, `STATE`, the snapshot being the one thing worth deferring. Three lanes means **no total order** between them, which is why every chat line carries its own `seq`.

### The room feed

Player chat and system narration are one feed with two authors — same body, same lane, same per-room counter — so a client draws one ordered list. `narrate.c` owns both: `room_chat_broadcast` stamps and delivers, `room_narrate` is the same call with the server as author. Narration is emitted from `room.c` alone, the only module holding both halves of a Room and already receiving every event; emitting from a handler would let the roster and the feed disagree. The server keeps **no history**, which is what makes closing a room a no-op for the feed.

Two orderings are load-bearing and asserted in `tests/test_chat.c`: a refused join narrates **nothing**, and a status reaches the client before the narration about it — not by call order but because responses drain ahead of chat.

### A Room is two objects

The domain `t_room` that `libtetrisroom` owns, and the runtime beside it — per-slot games, dirty flags, whether it is `ticking`. They share a lobby index and a lifetime, so `room.c` alone opens and closes them together and nothing outside it reaches through `->room`: handlers ask `server_room_seat` / `_start` / `_input` / `_describe`. The halves drifting apart once evicted a player seconds after they created the room ([post-mortem](../../docs/bugs/room_runtime_outlived_its_room.md)).

A Slot is written down twice: the room holds the seat, and the client holds a `t_room_binding` naming it, which goes stale on its own when a finished game clears every slot. So `room.c` owns that too — seating writes it, `server_room_unbind` is the only clear, and everyone else asks `server_room_resolve(srv, cli, name)`, which answers which room the binding names *now* or `NULL`, and **writes nothing**. A predicate that repaired what it was asked about would make call order load-bearing; staleness is repaired at one deliberate site, `JOIN`.

### Boot order

`main.c` detaches *before* claiming the pidfile — the pid written has to be the detached process's — and both the fork and the claim live there alone; behind `server_start` they would make every in-process suite fork. Boot owns the terminal: everything up to `daemon_ready` reports on stderr and exits non-zero, and stderr moves to `TETRISD_ERR_PATH` only once nothing is left to fail.

---

## Project Structure

```text
src/tetrisd/
├── include/tetrisd.h      Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c             Detach, claim the pidfile, start, wait, stop
│   ├── server.c           Bring-up, teardown, SIGHUP reload
│   ├── reactor.c          The event loop, sweep, teardown
│   ├── client.c           Spawn, adopt, kill, reap — the client lifetime rules
│   ├── clientio.c         Socket reads, frame boundaries, sealed writes
│   ├── outbox.c           Response FIFO, STATE mailbox, chat ring — three lanes
│   ├── registry.c         Live connections, addressable by player id
│   ├── handshake_pool.c   Bounded workers for the one blocking call
│   ├── dispatch.c         Frame → route → status; body field helpers
│   ├── request_target.c   Who may act on whose board, and the input budget
│   ├── handlers_*.c       account, lobby, input, game, profile, store, chat
│   ├── ability_ctrl.c     The Gaiden catalogue, targeting, and what it costs
│   ├── room.c             Both halves of a Room; ticking and STATE push
│   ├── narrate.c          The room feed — broadcast, and the server's own lines
│   └── …                  game (t_game), config, logger, listener, buffer,
│                          clock, signals, dump
├── tests/                 harness.c drives a real server over a real session
└── Makefile               → src/tetrisd/tetrisd
```

---

## Testing

Integration suites boot a real server in-process on port `0` through `server_start` and talk to it with a headless `libtetrissh` client, over throwaway certificates and a throwaway data directory:

```bash
make -C src/tetrisd test
make -C src/tetrisd test FILTER=game    # only suites matching "game"
valgrind --leak-check=full --error-exitcode=1 src/tetrisd/tests/bin/test_game
```

`main.o` is excluded from the test link so each suite provides its own `main()` — which is also why the double-fork may never move behind `server_start`. Valgrind is expected to be clean.

---

## References

- [HTTP Server from scratch in C](https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa)
- [Concurrent Servers Design](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)
- [Building a Multiplayer FPS](https://codersblock.org/multiplayer-fps/part1/)
- [Reactive Programming](https://medium.com/@anju.elias_67491/reactive-programming-a58693a08c27)
- [Garuna War — a single-threaded C++ UDP game server](https://github.com/eubrunomiguel/garuna)
