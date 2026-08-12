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
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [References](#references)

---

## Features

- One reactor thread in `epoll_wait` owns every connection, the lobby, the rooms, the games and every outbox — no locks over game state; beside it, a bounded pool runs the one genuinely blocking call under a deadline the reactor enforces
- Server-authoritative off the board too: the marketplace catalogue, prices, wallet, inventory and equipped loadout are the store's, and a client sends an item id and nothing else
- One gravity `timerfd` for the whole server, not a ticker thread per room; elapsed comes from the clock, so a late or coalesced tick catches games up rather than running them slow
- Server-authoritative: the client sends inputs, never board state, and the subject of every input rides in the request path
- A player holds at most one connection — a second `LOGIN` displaces the first; disconnecting forfeits the game in progress, so an abandoned game is still recorded
- Inputs are rate limited per connection with a token bucket, answering `429` with `Retry-After`; passwords are salted and SHA-256 hashed here, so the plaintext never reaches the store
- Room chat and system narration are one feed on their own outbox lane: best-effort, drop-oldest, and never a reason to close a connection
- Detaches itself, holds a locked pidfile, and reports its boot over a readiness pipe

Single mode is served end to end, including hold, pause/resume, restart and the self-affecting half of the Gaiden ability catalogue.

Double is complete: both seats declare readiness with `READY`, which commits the room and opens a `SELECTING` window of `TETRISD_MATCH_SELECT_MS` rather than dealing — the owner's `START` opens the same window, so a match is reached one way whichever route asked for it. Opening it clears every seat's declared character, so a seat is locked in exactly when it names one and that fact belongs to this match; the room deals itself the moment every seat has, or when the clock runs out on whatever they have equipped. The boards are then dealt and held for `TETRISD_MATCH_COUNTDOWN_MS`, every snapshot carries the other player's board beside its own — with their meter and their fighter, so a client can draw the rival's column without guessing at it, a clear on one board becomes garbage on the other, every ability in the catalogue resolves — including the eleven that need a Target — and the match ends when one player is left standing rather than when the last one stops, the survivor recorded `won` and the player who topped out `lost`, each exactly once. A finished match does not finish the room: `room_rematch` withdraws readiness and leaves both players in the seats they never left, so the next match is one `READY` away rather than a trip through the lobby to find each other again. Battle Royale is built on the same match: a room of 4-99 seats that only its owner may start, a select window that survives a departure, and an **arena** of one-bit board masks on a cadence of its own (`TETRISD_BR_ARENA_MS`) so ninety-eight rivals cost what a thumbnail costs rather than what a board does. A placing is taken the moment a player goes out - everybody eliminated on one tick shares it, and the next skips the numbers they took - and a knockout is credited to whoever's garbage last *landed*, narrated on the chat lane. `TARGET /room/<name>` declares one of four targeting modes; see `settle_garbage` above for which of them reach one rival and which reach all of them.

Garbage crosses at one place, `settle_garbage` in `room.c`, because it is the only module holding both halves of a Room: how many rows a clear is worth is `libtetrisbrain`'s (`garbage_lines_from_clear`, N−1) and who owes them to whom is the seating's. It runs after every game is advanced and before any snapshot is taken, so the frame that shows a clear is the frame that shows the `pending` count it caused. `server_room_targets_of` answers *which* seats — plural, because two of Battle Royale's four targeting modes hit everyone they matched rather than one drawn from them. Attackers and KOs fan out; Randoms and Badges draw one, because both match a crowd. The fan-out is over the *matched* set only: a mode that matched nobody falls back to every live opponent, and that fallback is drawn from, since it means "no preference applies". Every Target is queued the whole amount. `server_room_target_of` survives as the single-seat answer the pair-shaped tests ask for.

The character a match is played with is declared rather than inferred. `READY` carries an optional `character <id>`; `server_room_set_ready` stores it against the seat, `deal_games` copies it onto the game, and `ability_handler` resolves `(character, level)` against *that* — falling back to the account's equipped character when none was named, which is every Single game. Declaring rather than reading the account is what makes the choice per match: `EQUIP` is an account-wide change, and making one in order to play one game is the wrong scope, so a purchase or an equip made mid-match cannot change which four abilities a level selects from. Ownership is checked with `db_player_owns_character`, which answers a `t_db_bool` and is therefore tested against `DB_TRUE` — `DB_FALSE` is a successful read meaning "does not own it".

**Rows land at the Target's next piece lock, never on arrival.** That is a game rule and not a scheduling convenience: injecting garbage raises the stack under whatever is falling and can produce a board `piece_is_valid` would reject, and there is no correct thing to do with a piece already in the air on a board that is no longer legal. The drain sits after the clear resolves — so a player never takes rows in the middle of watching their own go — and before `spawn_next`, so the rows are part of the board the next piece is validated against. A spawn that then fails is a top-out, which is the right outcome of being buried. Targeted abilities wait on the same lock and for a second reason of their own: a status effect counted in pieces that took hold mid-piece would be a piece short before it began.

### Abilities that need a Target

The catalogue's eleven targeted abilities divide on which board *changes*, not on which board is read:

| Lands on | Abilities | When |
|---|---|---|
| The caster | Mirror, Pals, Vampire, Copy | At once, like the self-affecting four |
| The Target | Dark, Bomb, Inversion, Pentaris, Sirtet, Paralysis, Nue | Queued, applied at the Target's next piece lock |

Vampire and Copy *read* the Target but write the caster, and a read cannot leave anybody's piece inside their stack — so they need no deferral. They still need a Target to exist: there is nothing to steal, nothing to copy, and no incoming garbage in a room of one, which is what `needs_target` means for them and for Mirror and Pals. Copy keeps the "try it on a copy of the board and keep it only if the falling piece survives" discipline `apply_self` uses, because it is the one ability that replaces the caster's board out of somebody else's.

**Mirror is checked when an ability is aimed, not when it lands.** It steals *the next ability activated against* its holder, and a queued effect can be several effects behind by the time it arrives — checking at the landing would steal the wrong one. Reflecting is not refusing: the sender still paid, and it still happens, to them.

Two of the eleven need something beyond a queue entry:

- **Fry** is two halves. The rows go onto the sender's *own* floor and burn at their next lock — and are then sent to the Target, whole rather than through `garbage_lines_from_clear`'s N−1, because they are not a clear being converted but rows a player deliberately buried themselves under in order to hand over. They ride the ability lane, so Pals does not absorb them.
- **Pals** turns the ordinary kind upside down: incoming garbage takes rows *off* the floor instead of putting them on. Ability garbage is excluded by the ability text, which is why `t_game` counts `pending_garbage` and `pending_ability_garbage` separately — one counter could not tell Pentaris from a tetris, so Pals would absorb both or neither.

**Dark and Pals are the two effects `libtetrisbrain` refuses to time.** Its comment says the server decides when they end, so `t_game` carries `dark_pieces` and `pals_pieces` and `age_server_effects` runs them down on the holder's own locks. Without that, "for a limited time" is forever and one Dark ends the game. Dark's count is armed where it *lands* rather than where it was sent, so the lock that delivers it does not spend one of its pieces.

**Dark is also the one ability a server cannot carry out.** Every other effect is a rule about what a player may *do*, enforced by refusing the input; Dark is a rule about what they may *see*. So it rides the wire as a count and `tetrisu` blanks the cells outside a window under the falling piece — applied to the drawn grid and not to the board, so the hidden rows still collide. The player is blinded, not helped.

Bomb's scatter and garbage's hole column both walk from the game's own counters rather than from a random source. `libtetrisbrain` owns no RNG by contract, and a scatter that moves with how long a game has run is unpredictable to a player without being unreproducible to a test.

---

## Prerequisites

Install the shared build dependencies from the repository root:

```bash
make -C ../.. deps
```

`tetrisd` links eight CoreStack archives — `libcoreipc`, `libcoredaemon`, `libhtttp`, `libtetrisbrain`, `libmacminidb`, `libtetrissh`, `libtetrisroom`, `libstatusbody` — plus OpenSSL (`-lssl -lcrypto`), `-lpthread` and `-lrt`.

Certificates are a boot requirement, not an option; `make certs` from the root mints development ones.

---

## Build

```bash
make -C src/tetrisd
```

This builds every archive under `LIB_NAMES` in place and links `src/tetrisd/tetrisd`.

| Target | Description |
|---|---|
| `make -C src/tetrisd` | Build the daemon (`make all`) |
| `make -C src/tetrisd libs` | Build the CoreStack archives only |
| `make -C src/tetrisd run` | Build and launch from the project root |
| `make -C src/tetrisd test` | Build and run every suite |
| `make -C src/tetrisd clean` | Remove object files and test binaries |
| `make -C src/tetrisd fclean` | Remove object files and the binary |
| `make -C src/tetrisd re` | Full rebuild (`fclean` + `all`) |

Cleanup never recurses into `lib/` — the archives are shared with the other daemons and owned by their own directories.

---

## Run

**Run it from the project root.** Paths in `.tetrishrc` are root-relative — not relative to the rc file or the binary. `make -C src/tetrisd run` does this; the way `.tetrishrc` does it is through the lifecycle manager:

```bash
tetrisctl start tetrislogd
tetrisctl start tetrisd
tetrisctl status
tetrisctl stop tetrisd
```

The binary daemonises itself, so running it by name returns to the prompt once the server is actually listening. It exits `0` only after the port is bound and the store is open, and non-zero — with the reason printed — if it did not.

**Start `tetrislogd` first**, and stop it last: it binds the socket `tetrisd` ships records to, and stopping it first would push `tetrisd`'s whole shutdown into its error file. `TETRISCTL_DAEMONS` in `.tetrishrc` is the only place that order is written down.

---

## Protocol

HTTTP over an authenticated, encrypted session. `Player-Id` is required on every authenticated request and is checked against the player bound to the connection at `LOGIN` — a forged header buys nothing.

| Method | Path | Effect |
|---|---|---|
| `SIGNUP` | `/account` | Register a player; `201` with its id, `409` when the name is taken |
| `LOGIN` | `/session` | Bind the connection to a player, displacing any older one |
| `LIST` | `/rooms` | Every occupied room, in-game ones included |
| `LIST` | `/room/<name>` | Detailed room state and ordered occupied seats; visible only to a player seated in that room |
| `LIST` | `/store` | The character and theme catalogues, with the prices this server charges |
| `LEADERBOARD` | `/leaderboard` | Top ten by recorded score, rank ascending; registering is what puts a player on it, so a fresh account ranks last with nought |
| `PROFILE` | `/player/<pid>` | Wallet, score, rank, owned items and the equipped loadout; `403` for another player |
| `BUY` | `/store/character/<cid>`, `/store/theme/<tid>` | Spend the wallet; answers the updated profile, `403` `insufficient-funds`, `409` `inventory-full` |
| `EQUIP` | `/player/<pid>/character/<cid>`, `/player/<pid>/theme/<tid>` | Set the loadout; answers the updated profile, `403` `not-owned` |
| `JOIN` | `/rooms` | Create a room in the body's `mode` and own it; `201` |
| `JOIN` | `/room/<name>` | Take a slot in an existing room; `200` |
| `LEAVE` | `/room/<name>` | Give up the slot, forfeiting a game in progress |
| `READY` | `/room/<name>` | Body `ready <0\|1>`, optionally `character <id>` — declare or withdraw readiness and name the fighter for this match; answers the room. `403` when the character is not owned. Pressed twice per match by design: once to commit, which opens the select window, and once inside it to lock a fighter in |
| `START` | `/room/<name>` | Owner begins the game; `403` for a non-owner. Single is dealt on the spot; anywhere there is an opponent the request opens the same select window the last readiness does, and the answer names the status rather than asserting `in-game`. Once dealt, the boards are held for `TETRISD_MATCH_COUNTDOWN_MS`, during which every input is refused `409` |
| `MOVE` | `/room/<name>/player/<pid>` | Body `LEFT` or `RIGHT` |
| `ROTATE` | `/room/<name>/player/<pid>` | Body `CW` or `CCW` |
| `DROP` | `/room/<name>/player/<pid>` | Body `SOFT` or `HARD` |
| `HOLD` | `/room/<name>/player/<pid>` | No body — swap the falling piece with the hold slot, once per piece |
| `PAUSE` | `/room/<name>/player/<pid>` | Body `PAUSE` or `RESUME`; **Single only** |
| `RESTART` | `/room/<name>/player/<pid>` | No body — deal a fresh game, discarding the one in progress; **Single only** |
| `ABILITY` | `/room/<name>/player/<pid>` | Body `level <1-4>`, optionally `column <0-9>` to aim Sol |
| `CHAT` | `/room/<name>` | Body `text <line>`; broadcast to the room including the sender. `429` rate-limited, `404` not seated there, `403` `muted`, `400` `bad-text` |
| `TARGET` | `/room/<name>` | Body `mode random\|ko\|attackers\|badges`; **Battle Royale only**. Every mode narrows the live opponents and the room still draws from what is left, so a player chooses a kind of rival and never a person. `400` on a word that is not a mode, `404` not seated there, `409` `not-battle-royale` or `not-playing`, `429` rate-limited on the chat bucket — it is a key pressed a few times a match, and spending an input token on it would cost a piece movement |
| `STATE` | `/room/<name>/player/<pid>` | **Server-originated** — one player's board, pushed on tick, carrying the room's countdown and, when the match ends, that player's result |
| `CHAT` | `/room/<name>` | **Server-originated** — one line of the room's feed, pushed to every seat |

A piece that touches down does not lock on the spot either. It keeps
`LOCKDOWN_DELAY_MS` (500 ms), and every accepted `MOVE` or `ROTATE` buys that
half-second back, up to `LOCKDOWN_MAX_RESETS` (15) times — refilled whenever
the piece falls past the lowest row it has occupied. That is the Guideline's
Extended Placement lock down, and it is what makes it possible to slide a
piece into a gap instead of only dropping it onto one. `DROP HARD` is exempt:
it means "and I am done with it", and locks at once. `DROP SOFT` is only
gravity in a hurry, so on the floor it is refused (`409 input-blocked`) and
the piece keeps its delay
([`docs/bugs/`](../../docs/bugs/the_piece_locked_the_moment_it_landed.md)).

A lock that completes rows does not clear them on the spot. They are held for
`clear_duration_ms(level)` — still filled in the board, nothing scored yet, no
piece dealt — and every tick of that hold is a `STATE` carrying `phase
clearing`, the rows, and how far through the server is. Input is refused for
its length, because the piece has locked and the next one does not exist yet.
Before this the clear happened between two ticks and no client could ever see
it ([`docs/bugs/`](../../docs/bugs/the_line_clear_never_reached_the_client.md)).

Rooms are named by the lobby (`S-01`, `D-02`, `BR-03`), never by clients, which is why creation addresses the collection rather than a name. Request and response bodies are the same `key value` line format the status bodies use, one key per line.

Statuses in use: `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`, `500`, `501`.

A refusal the domain has a reason for carries it — `reason full`, `in-game`, `not-owner`, `too-few-players`, `already-started`, `already-in-room`, `lobby-full`, `input-blocked`, `not-single`, `no-target`, `no-charge`, `ability-blocked`, `ability-unavailable`, `ability-invalid`, `insufficient-funds`, `inventory-full`, `not-owned`, `muted`, `bad-text`. A bare status would leave a player unable to tell a full room from one already playing, or "you cannot afford that" from "that ability has nothing here to act on".

### Abilities

An `ABILITY` body names a **level**, never an ability. Which four abilities a level selects from is decided by the character the player has equipped, and `tetrisd` reads that out of the store rather than taking it from the request — it is a fact about the account. Level 1 is Fry for Halloween and Cut for Wolf-man; a client that could name the ability could use one it has not equipped.

Charge is `libtetrisbrain`'s: two cleared lines bank one, and levels 1–4 cost 2/4/6/8. It is deducted only once an activation has been accepted, so a refusal costs nothing. The client's own charge counter is never read — it arrives in `STATE` and goes nowhere.

**Single mode has no Target** ([`docs/CONTEXT.md`](../../docs/CONTEXT.md)), so the twelve abilities that land on somebody else are refused with `reason no-target` rather than redirected at the player who asked for one. Four are self-affecting and served end to end — every character has one:

| Character | Level | Ability | What it does to its own board |
|---|---|---|---|
| Halloween | 1 | Fry | Fills the bottom three rows; they burn off at the next lock |
| Mirurun | 1 | Mirurun | Removes the bottom four rows |
| Princess | 1 | Sol | Clears three adjacent columns, aimed by the body's `column` |
| Wolf-man | 1 | Cut | Removes the top four rows |
| Wolf-man | 4 | Thwack | For the next four pieces, blocks cascade after a clear |

Every transform is tried on a copy and kept only when the falling piece survives it, so an ability that would leave the piece inside the stack is refused whole rather than half-applied.

### Marketplace

Which four abilities an `ABILITY` level selects from is decided by the equipped character, so who owns what is a gameplay fact, not a cosmetic one. `tetrisd` therefore decides all of it: the price comes from `config/characters.cfg` and `config/themes.cfg`, the balance from the store, and `db_buy_*` / `db_equip_*` enforce affordability and ownership atomically under their own write lock. A client sends an id and nothing else — it cannot name a price, assert what it owns, or equip a character it never bought.

`BUY` and `EQUIP` answer with the **updated profile** rather than a bare status, so acting on the account and re-reading it are one round trip and no client ever draws a wallet it has not been told. Buying something already owned is a no-op that answers `200` with that same profile — the owned list in it is what tells the screen the tile is theirs.

Catalogue ids are never reused or renumbered: they are written into every player's owned lists, so renumbering would repoint what somebody already bought at a different item. Theme id `5` is the gap left by a cut theme and stays a gap, which is why `LIST /store` carries each id explicitly rather than implying it from position.

The wallet is filled by playing. A game that reaches game-over — or is forfeited by leaving, topping out, or losing the connection — is recorded once with `db_record_game`, which credits `TETRISD_POINTS_PER_WALLET_POINT` (100) game points per wallet point. The credit is the difference between what the player's `lifetime_points` were worth before the game and what they are worth after, not that game's score divided on its own, so a game worth less than the rate leaves its remainder on the account instead of rounding to nothing (`docs/game-economics.md`). A game abandoned by restarting is not recorded at all.

The same call ranks the player, and it ranks them on their **best single game**, not on that total — `leaderboard_score` moves only when a game beats it. The two must not be confused: charging the wallet against the best score would stop paying the moment a player stopped setting records, and ranking on the total would rank whoever played most.

---

## Signals

| Signal | Effect |
|---|---|
| `SIGTERM`, `SIGINT` | Stop the loop, forfeit and close every connection, release the pidfile |
| `SIGHUP` | Re-read `.tetrishrc` — only `TETRISD_LOG_LEVEL` and `TETRISD_TICK_MS` can change |
| `SIGUSR1` | Dump port, uptime, clients, rooms and slots to the log — the stand-in for a control channel |
| `SIGPIPE` | Ignored — a client vanishing mid-send must kill its own connection, not the server |

Handlers set a flag and write one byte down the self-pipe; the reactor asks the `signals_take_*` calls once per wake-up, so repeated signals between iterations coalesce.

A reload cannot move the port, the certificates or the data directory: those name resources that are already open. Retiming gravity is a `timerfd_settime` call on the thread that owns the timer, and it leaves `last_tick` alone so the reload does not discard accumulated gravity.

---

## Configuration

Every setting comes from `.tetrishrc` as `export TETRISD_*=<value>` lines, resolved as `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variable. **The keys are documented inline in `.tetrishrc`**, which is their home; what follows is only what that file does not state.

An unknown `TETRISD_*` key or an out-of-range value fails the boot rather than being ignored, so a setting documented but never wired up cannot silently do nothing. A missing rc file *is* fine — the defaults are a working setup — but missing certificates are fatal.

```text
TETRISD_PORT                  0-65535 (0 binds a kernel-assigned port)
TETRISD_TICK_MS               1-1000            # re-read on SIGHUP
TETRISD_LOG_LEVEL             debug|info|warning|error  # re-read on SIGHUP
TETRISD_MAX_CLIENTS           up to 4096
TETRISD_INPUT_BURST/RATE      1-10000
TETRISD_HANDSHAKE_WORKERS     up to 64
TETRISD_HANDSHAKE_TIMEOUT_MS  100-60000
```

`TETRISD_LOG_IPC` must equal `TETRISLOGD_SOCKET_PATH` — one socket, named twice.

---

## Architecture

### One owner

> `tetrisd` has exactly one owner of all mutable game state.

The reactor thread reads the socket, opens the frame, dispatches the request, seals the answer and writes it — and it alone touches the lobby, every room, every game, the registry and every outbox. There is no lock order because there are no locks over game state; the four-level order this replaced (`lobby_mutex > room->mutex > registry rwlock > outbox mutex`) is gone with the locks in it. Two locks survive and neither guards game state: the handshake pool's own mutex, and whatever `libmacminidb` holds internally. Wanting a third is a sign the work is on the wrong thread.

`tetrisd` is not single-threaded; it is single-owner. Two other kinds of thread exist and neither may touch game state: the handshake workers, each owning one un-established connection until it hands it back, and the log shipper draining the ring buffer to `tetrislogd`.

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

The wait is unbounded unless a handshake is in flight, in which case it ends at that handshake's deadline. Gravity needs no timeout: the tick timer is a descriptor in the same set. `epoll_event.data.ptr` carries three kinds of object, so every watched object begins with a `t_event_tag` the reactor reads before using the pointer as anything — which recovers a client from the kernel without an fd-to-client map to keep in step.

### Client lifetime

The rule that makes `data.ptr` safe replaced the registry rwlock: **no client is freed inside the event loop.** A batch can carry several events for the same client, so `client_kill` only unlinks it and parks it on the zombie list, every later event in the batch skips a client marked `dead`, and `client_reap` is the only `free()` site. The registry is now a directory rather than a lifetime guard — every live connection addressable by player id, which is how a tick reaches the right outbox and how `LOGIN` finds the connection it displaces.

### Handshake pool

`session_handshake_server` is the one genuinely blocking call left, so it runs off the loop. A worker owns its client outright until it hands the established session back; `client_adopt` sets `watched` on the far side of that handoff, and the sweep skips anything not yet `watched` — dropping that check would put two threads on one socket.

`TETRISD_HANDSHAKE_TIMEOUT_MS` is a budget for the whole handshake, not for one read: `SO_RCVTIMEO` bounds a single `recv` and `libtetrissh` loops until it has the bytes it asked for, so a peer dribbling one byte per timeout would otherwise hold a worker indefinitely.

### Outbox

Three lanes, because three kinds of message fail differently.

A bounded FIFO of responses: a response belongs to a request the client is waiting on, so overflowing it closes the client — it cannot keep up, and buffering more would let it exhaust the server. A one-slot mailbox holding the latest `STATE`: a snapshot supersedes the one before it, so it overwrites and a stalled client loses intermediate frames but never holds up the tick that produced them. A small ring of `CHAT`: best-effort by specification (UC-09 E1), so it drops its oldest line and **never closes the client**.

Chat cannot share the response FIFO. A Battle Royale room narrating its knockouts would fill it, and the next genuine response would kill a connection whose only fault was being slow — the opposite of what that FIFO's rule is for. `registry_enqueue_chat` is a separate call from `registry_enqueue` for the same reason: chat must not be able to reach the `shutdown` the response path takes on overflow.

Draining order is responses, then chat, then `STATE`. A snapshot is regenerated on the following tick, so it is the one thing worth deferring. Three lanes means there is **no total order** between the three; chat is ordered within itself and against nothing else, which is why every line carries its own `seq`.

That split has nothing to do with threading, which is why it outlived the writer thread, the condition variable and the mutex unchanged.

### The room feed

Player chat and system narration are one feed with two authors — the same body, the same lane, numbered by the same per-room counter — so a client draws one ordered list instead of merging two. `narrate.c` owns both: `room_chat_broadcast` stamps and delivers, `room_narrate` is the same call with the server as the author.

Narration is emitted from `room.c` and nowhere else, because that is the only module holding both halves of a Room and it already receives every event — `room_seat`'s verdict, `room_release`'s `t_release_result` with its successor. Emitting from a handler would let the roster and the feed disagree, which is [the bug that already happened once](../../docs/bugs/room_runtime_outlived_its_room.md) in a different guise.

Two orderings are load-bearing and asserted in `tests/test_chat.c`. A refused join narrates **nothing**. And a status reaches the client before the narration about it — not by arranging the calls, but because responses drain ahead of chat, so the guarantee survives someone rearranging them.

The server keeps **no history**: a player who joins late has missed what was said. That is what makes closing a room a no-op for the feed, and it is why `tetrisu`'s own ring is the only backlog that exists.

### A Room is two objects

The domain `t_room` that `libtetrisroom` owns, and the runtime beside it — the per-slot games, which are dirty, and whether it is `ticking`. They share a lobby index and therefore a lifetime, so `room.c` alone opens and closes them together, and nothing outside it reaches through `->room`: handlers ask `server_room_seat` / `_start` / `_input` / `_describe` rather than the domain object. The two halves drifting apart once evicted a player from a room seconds after they created it ([post-mortem](../../docs/bugs/room_runtime_outlived_its_room.md)).

### A Slot is written down twice

The room holds the seat; the client holds a `t_room_binding` — room name, room index, slot — saying which seat it is. That copy goes stale on its own, because a finished game clears every slot, so `room.c` owns it too: seating writes it, `server_room_unbind` is the only clear, and everyone else asks `server_room_resolve(srv, cli, name)`, which answers which room the binding names *now* — the index still carries that name, the player is still a member — or `NULL`, and **writes nothing**.

A predicate that repairs what it was asked about makes call order load-bearing: validate before you index, or you index on a binding nobody checked. A resolve that hands back the room you were going to look up next cannot be called in the wrong order. Staleness is repaired at one deliberate site — `JOIN`, the request that wants a clean binding; `LEAVE`, `START` and the input routes refuse and leave it alone.

### Boot order

`main.c` detaches *before* claiming the pidfile — the pid written has to be the detached process's — and both the fork and the claim live there alone; behind `server_start` they would make every in-process suite fork.

Boot owns the terminal: everything up to `daemon_ready` reports on stderr and exits non-zero, so the operator who typed the command sees the failure. stderr moves to `TETRISD_ERR_PATH` only once nothing is left to fail — redirecting earlier would hide boot failures.

---

## Project Structure

```text
src/tetrisd/
├── include/tetrisd.h      Every type and prototype; src/*.c include only this
├── src/
│   ├── main.c             Detach, claim the pidfile, start, wait, stop
│   ├── server.c           server_start / server_stop, bring-up, SIGHUP reload
│   ├── reactor.c          The event loop, sweep, teardown
│   ├── client.c           Spawn, adopt, kill, reap — the client lifetime rules
│   ├── clientio.c         Socket reads, frame boundaries, sealed writes
│   ├── outbox.c           Response FIFO, STATE mailbox, chat ring — three lanes
│   ├── registry.c         Live connections, addressable by player id
│   ├── handshake_pool.c   Bounded workers for the one blocking call
│   ├── dispatch.c         Frame → route → status; body field helpers
│   ├── request_target.c   Who may act on whose board, and the input budget
│   ├── handlers_*.c       account (SIGNUP, LOGIN), lobby, input, game,
│   │                      profile (PROFILE), store (LIST /store, BUY, EQUIP),
│   │                      chat (CHAT)
│   ├── ability_ctrl.c     The Gaiden catalogue, targeting, and what it costs
│   ├── room.c             Both halves of a Room; ticking and STATE push
│   ├── narrate.c          The room feed — broadcast, and the server's own lines
│   ├── game.c             t_game — the aggregate libtetrisbrain does not own
│   └── …                  config, logger, listener, buffer, clock, signals, dump
├── tests/                 harness.c drives a real server over a real session
├── scripts/run_tests.sh
└── Makefile               → src/tetrisd/tetrisd
```

---

## Testing

Fourteen suites. Integration suites boot a real server in-process on port `0` through `server_start` and talk to it with a headless `libtetrissh` client, over throwaway certificates and a throwaway data directory:

```bash
make -C src/tetrisd test
make -C src/tetrisd test FILTER=game    # only suites matching "game"
```

`main.o` is excluded from the test link so each suite provides its own `main()` — which is also why the double-fork may never move behind `server_start`.

Valgrind is expected to be clean:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrisd/tests/bin/test_game
```

---

## References

- [HTTP Server from scratch in C](https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa)
- [Concurrent Servers Design](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)
- [Building a Multiplayer FPS](https://codersblock.org/multiplayer-fps/part1/)
- [Reactive Programming](https://medium.com/@anju.elias_67491/reactive-programming-a58693a08c27)
- [Garuna War — a single-threaded C++ UDP game server](https://github.com/eubrunomiguel/garuna)
