# tetriSH — Use Case Descriptions

---

## Table of Contents

- [Actors](#actors)
- [Persistence vs Runtime](#persistence-vs-runtime)
- [Request & Return Status (per use case)](#request--return-status-per-use-case)
- [Authentication](#authentication)
  - [UC-01 — Register Account](#uc-01--register-account)
  - [UC-02 — Log In](#uc-02--log-in)
- [Multiplayer Lobby & Rooms](#multiplayer-lobby--rooms)
  - [UC-03 — Browse Open Rooms](#uc-03--browse-open-rooms)
  - [UC-04 — Create Room](#uc-04--create-room)
  - [UC-05 — Join Room from List](#uc-05--join-room-from-list)
  - [UC-06 — Join Room by Room ID](#uc-06--join-room-by-room-id)
  - [UC-07 — Leave Room](#uc-07--leave-room)
  - [UC-08 — Start Game](#uc-08--start-game)
  - [UC-08b — Attempt to Start Game as Non-Owner (Denied)](#uc-08b--attempt-to-start-game-as-non-owner-denied)
  - [UC-09 — Chat in Room](#uc-09--chat-in-room)
- [Gameplay](#gameplay)
  - [UC-10 — Play Single-Player Game](#uc-10--play-single-player-game)
  - [UC-11 — Play Double (2-Player) Game](#uc-11--play-double-2-player-game)
  - [UC-12 — Play Battle Royale Game](#uc-12--play-battle-royale-game)
  - [UC-13 — Control Falling Piece](#uc-13--control-falling-piece-included-by-uc-101112)
  - [UC-20 — Activate Gaiden Ability](#uc-20--activate-gaiden-ability-extends-uc-11uc-12)
- [Marketplace](#marketplace)
  - [UC-14 — Buy Character](#uc-14--buy-character)
  - [UC-15 — Buy Theme](#uc-15--buy-theme)
  - [UC-16 — Deduct Wallet Points](#uc-16--deduct-wallet-points-included-by-uc-14uc-15)
  - [UC-17 — Set Default Character](#uc-17--set-default-character)
  - [UC-18 — Set Default Theme](#uc-18--set-default-theme)
- [Profile, Settings & Leaderboard](#profile-settings--leaderboard)
  - [UC-19 — View Settings / Profile](#uc-19--view-settings--profile)
  - [UC-21 — View Leaderboard](#uc-21--view-leaderboard)
- [Summary of Relationships](#summary-of-relationships)
- [DB Mapping Summary (`libmacminidb`)](#db-mapping-summary-libmacminidb)
- [Open Questions to Resolve (design notes)](#open-questions-to-resolve-design-notes)

---

## Actors

| Actor | Description |
|---|---|
| **Guest** | An unauthenticated user. Can only register or log in. |
| **Player** | An authenticated user. Primary actor for all gameplay, marketplace, settings, and lobby use cases. |
| **Room Owner** | A specialization of Player who created a room; gains room-control use cases (Start Game). |
| **Game Server (tetrisd)** | Supporting actor. Server-authoritative game loop; validates moves, pushes board STATE. Holds all room/lobby/live-game state in memory. |
| **Market Daemon (marketd)** | Supporting actor. Mediates purchases and equips; calls the persistence layer for wallet/inventory changes. |
| **Chat Daemon (chatd)** | Supporting actor. Owns room chat and system narration (runtime only, never persisted). |
| **Persistence — libmacminidb (NoSQLite)** | Supporting component embedded in the server. In-memory player index backed by an append-only Last-Writer-Wins log. **Persists only player, character, and theme state.** Returns `t_db_result` codes the server maps to HTTTP status. |


## Persistence vs Runtime

Two distinct state systems back these use cases, and their status/result codes must not be conflated:

- **Persisted state — `libmacminidb`.** Only **player, character, and theme** documents are durable.
  - *Covers:* signup, login, buy, equip, post-game record, profile, leaderboard.
  - These use cases call the DB, which returns a `t_db_result` (`DB_OK`, `DB_EXISTS`, `DB_BAD_CREDS`, `DB_INSUFFICIENT`, `DB_NOT_OWNED`, `DB_NOT_FOUND`, …). The server translates that result into an HTTTP status.
- **Runtime state — tetrisd / chatd (in memory).** Rooms, the lobby directory, slot occupancy, ready/started status, live boards, and chat are **never persisted**.
  - Their `200 / 201 / 403 / 409` codes are HTTTP protocol-level responses from the game/chat server, not `t_db_result` codes.
  - If the server restarts, this state is gone by design.

| Group | Use cases | Backing store |
|---|---|---|
| **Persisted (DB)** | • UC-01, UC-02<br>• UC-14, UC-15, UC-16<br>• UC-17, UC-18<br>• UC-19, UC-21<br>• the `record_game` step of UC-10/11/12<br>• UC-20 reads catalogue/ownership | `libmacminidb` |
| **Runtime only** | • UC-03, UC-04, UC-05, UC-06<br>• UC-07, UC-08, UC-08b<br>• UC-09, UC-13<br>• the live-play loop of UC-10/11/12 | tetrisd / chatd memory |

---

## Request & Return Status (per use case)

Every use case's wire request and the status codes it can return. Three transports are in play; the **Transport** column says which:

- **HTTTP → tetrisd** 
    - the fixed game protocol (`METHOD PATH HTTTP/1.0`) over the authenticated TCP session.
- **HTTTP → chatd**
    - chatd links the same `libhtttp`, so `CHAT` / `ABILITY` use the same wire format and status codes.
- **marketd IPC**
    - the tetrisu↔marketd **Unix `SOCK_STREAM`, length-prefixed** request/response channel (store browse, buy, equip, profile, leaderboard). 
    - Not HTTTP; the DB result still maps to the same status numbers for consistency.

| UC | Request (wire) | Transport | Success | Error statuses |
|---|---|---|---|---|
| UC-01 Register | `SIGNUP /account` body `{username,password}` | HTTTP → account svc | `201` | `409` taken · `400` malformed · `500` |
| UC-02 Log In | `LOGIN /session` body `{username,password}` | HTTTP → account svc | `200` (+`Player-Id`) | `401` bad creds/unknown · `400` · `500` |
| UC-02a Connect | crypto handshake (nonce → cert → RSA-OAEP AES key) | `libtetrissh` session | session up | handshake fail → connection dropped |
| UC-03 Browse Rooms | `LIST /rooms` | HTTTP → tetrisd | `200` (room list) | `500` |
| UC-03a Refresh | `LIST /rooms` | HTTTP → tetrisd | `200` | `500` |
| UC-04 Create Room | `JOIN /room/<id>` body `Mode:` (new id) | HTTTP → tetrisd | `201` (owner) | `400` bad mode · `409` id exists · `500` |
| UC-05 Join from List | `JOIN /room/<id>` | HTTTP → tetrisd | `200` | `404` no room · `409` full · `409` in-game |
| UC-06 Join by ID | `JOIN /room/<id>` | HTTTP → tetrisd | `200` | `404` no room · `409` full* · `409` in-game* |
| UC-07 Leave Room | `LEAVE /room/<id>` | HTTTP → tetrisd | `200` | `404` not in room |
| UC-08 Start Game | `START /room/<id>` (owner) | HTTTP → tetrisd | `200` | `403` not owner · `409` too few/started |
| UC-08b Non-owner Start | `START /room/<id>` (non-owner) | HTTTP → tetrisd | — | `403` not owner |
| UC-09 Chat | `CHAT /room/<id>` body text | HTTTP → chatd | `200` | `429` rate-limited · `403` muted · `404` |
| UC-10 Single Player | play via UC-13; server `db_record_game` on game-over | HTTTP → tetrisd | `200` per input | `409` invalid move |
| UC-11 Double | UC-13 inputs + UC-20 ability; `STATE` pushed; server `db_record_game` **per player** on game-over | HTTTP → tetrisd | `200` per input | `409` invalid move |
| UC-12 Battle Royale | UC-13 inputs + UC-20 ability; `STATE` pushed; server `db_record_game` **per participant** on game-over | HTTTP → tetrisd | `200` per input | `409` invalid move |
| UC-13 Control Piece | `MOVE`/`ROTATE`/`DROP /room/<id>/player/<pid>` body `LEFT\|RIGHT` / `CW\|CCW` / `SOFT\|HARD` | HTTTP → tetrisd | `200` accepted | `409` INVALID_MOVE (+authoritative pos) · `400` bad body |
| — `STATE /room/<id>` | server-originated broadcast (no client status) | HTTTP ← tetrisd | pushed | — |
| UC-14 Buy Character | `BUY character <cid>` | marketd IPC | `200` (bought / owned no-op) | `403` insufficient · `409` inventory full · `404` no item |
| UC-15 Buy Theme | `BUY theme <tid>` | marketd IPC | `200` (bought / owned no-op) | `403` insufficient · `409` inventory full · `404` no item |
| UC-16 Deduct Points | — internal to `db_buy_*` | — | — | — |
| UC-17 Set Default Character | `EQUIP character <cid>` | marketd IPC | `200` | `403` not owned · `404` |
| UC-18 Set Default Theme | `EQUIP theme <tid>` | marketd IPC | `200` | `403` not owned · `404` |
| UC-19 View Settings | `PROFILE` (+ rank) | marketd IPC | `200` (player doc + rank) | `500` |
| UC-20 Activate Ability | `ABILITY /room/<id>/player/<pid>` body `{ability}` | HTTTP → chatd/tetrisd | `200` applied | `403` not owned · `409` on cooldown |
| UC-21 View Leaderboard | `LEADERBOARD` (top-N) | marketd IPC | `200` (top entries) | `500` |

---

## Authentication

### UC-01 — Register Account

| Field | Content |
|---|---|
| **ID** | UC-01 |
| **Primary Actor** | Guest |
| **Goal** | Create a new player account so the Guest can log in and play. |
| **Preconditions** | The Sign Up page is displayed. The Guest is not authenticated. |
| **Postconditions (success)** | A new account exists with the chosen username; the Guest is routed back to the Login page. |
| **Trigger** | Guest presses **SIGN UP** on the Login page (or is on the Sign Up page). |
| **DB Mapping** | `db_signup(username, password_hashed, salt, &id)` → `DB_OK`=`201 Created`, `DB_EXISTS`=`409 Conflict` (username taken). Server hashes the password with a per-user salt **before** the call; the DB stores only the hash + salt. |

**Main Success Scenario**
1. Guest enters a username.
2. Guest enters a password.
3. Guest re-enters the password.
4. Guest presses **SIGN UP**.
5. System validates that both passwords match (client/server) and hashes the password with a freshly generated per-user salt.
6. System calls `db_signup(username, password_hashed, salt, &id)`.
7. On `DB_OK`, the record is created (wallet = 0 pts, starter character/theme = item `1`, no other owned items) and the Guest is routed back to the Login page.

**Extensions / Alternate Flows**
- **5a. Passwords do not match:** System shows an error; return to step 3.
- **5b. Empty required field:** System highlights the field; Guest completes it.
- **6a. Username already taken (`DB_EXISTS` → 409):** System shows "username taken"; return to step 1.

**Exceptions**
- **E1. Server unreachable:** System shows a connection error; account is not created.

**Related Use Cases** — none.

---

### UC-02 — Log In

| Field | Content |
|---|---|
| **ID** | UC-02 |
| **Primary Actor** | Guest |
| **Goal** | Authenticate against a server and open a session as a Player. |
| **Preconditions** | The Login page is displayed. A valid account exists. |
| **Postconditions (success)** | An authenticated session is established; the Home page is displayed as a Player. |
| **Trigger** | Guest presses **LOGIN**. |
| **DB Mapping** | `db_login(username, password_hashed, &player)` → `DB_OK`=`200 OK`, `DB_BAD_CREDS`=`401 Unauthorized`, `DB_NOT_FOUND`=`401` (do not reveal whether the username exists). Server hashes the entered password with the account's stored salt **before** the call; the DB compares hashes only. |

**Main Success Scenario**
1. Guest enters username.
2. Guest enters password.
3. Guest enters the **Server ID** of the server to connect to.
4. Guest presses **LOGIN**.
5. System connects to the specified server (`«include»` **UC-02a Connect to Server**) and performs the authenticated handshake.
6. System hashes the entered password with the account salt and calls `db_login(...)` to verify credentials.
7. On `DB_OK`, System opens a session and displays the Home page (Single Player / Multiplayer / Marketplace / Leaderboard / Setting).

**Extensions / Alternate Flows**
- **3a. No account yet:** Guest presses **SIGN UP** → UC-01 Register Account.
- **6a. Invalid credentials (`DB_BAD_CREDS` / `DB_NOT_FOUND` → 401):** System shows "invalid username or password" (identical message either way); return to step 1.

**Exceptions**
- **E1. Server ID unreachable / wrong (404 / timeout):** System shows a connection error; no session is opened.

**Related Use Cases** — `«include»` UC-02a Connect to Server.

---

## Multiplayer Lobby & Rooms

### UC-03 — Browse Open Rooms

| Field | Content |
|---|---|
| **ID** | UC-03 |
| **Primary Actor** | Player |
| **Goal** | See the list of open rooms in order to choose one to join. |
| **Preconditions** | Player is authenticated and has entered the Multiplayer Lobby. |
| **Postconditions (success)** | The current list of open rooms (ID, Mode, Players, State, Owner) is displayed; Player's own username, leaderboard score, and ranking are shown in the header. |
| **Trigger** | Player selects **Multiplayer** on the Home page. |

**Main Success Scenario**
1. Player enters the Lobby.
2. System requests the current room directory from the Game Server.
3. System renders each room row: ID, Mode (D / BR), Players (e.g. 1/2, 8/8), State (WAITING / IN-GAME), Owner.
4. System renders the header with the Player's username, leaderboard score, and ranking.

**Extensions / Alternate Flows**
- **2a. No open rooms:** System shows an empty list; Player may Create Room (UC-04) or Join by Room ID (UC-06).
- **3a. Player presses `[R]` Refresh:** → UC-03a Refresh Room List (re-runs steps 2–4).
- **3b. Player presses `[B]` Back:** System returns to the Home page.

**Exceptions**
- **E1. Server unreachable:** System shows a stale-list warning or connection error.

**Related Use Cases** — `«include»` UC-03a Refresh Room List; leads to UC-04, UC-05, UC-06.

---

### UC-04 — Create Room

| Field | Content |
|---|---|
| **ID** | UC-04 |
| **Primary Actor** | Player (becomes Room Owner) |
| **Goal** | Create a new game room in a chosen mode and become its owner. |
| **Preconditions** | Player is in the Lobby. |
| **Postconditions (success)** | A new room exists (server assigns a Room ID); Player is the Room Owner and is placed in the room's Waiting Room. |
| **Trigger** | Player presses `[C]` Create Room. |

**Main Success Scenario**
1. System opens the **Create Room** modal.
2. System presents the mode options: `[1] Double` (2 players, default) and `[2] Battle Royale` (4–99 players).
3. Player selects a mode with `[↑/↓]`.
4. Player presses `[ENTER]` to create (`«include»` **UC-04a Select Game Mode**).
5. System sends the create/JOIN request; the Game Server creates the room, assigns a Room ID, marks the Player as Owner, and returns `201 Created`.
6. System closes the modal and shows the Waiting Room with the Player in slot 1 (Owner), status "Waiting for opponents".

**Extensions / Alternate Flows**
- **3a. Player presses `[ESC]` Cancel:** Modal closes; return to Lobby, no room created.
- **2a. No mode selected:** Default (Double) is used.

**Exceptions**
- **E1. Server rejects creation (500 / capacity):** System shows an error; return to Lobby.

**Related Use Cases** — `«include»` UC-04a Select Game Mode; leads to UC-08 Start Game, UC-07 Leave Room.

---

### UC-05 — Join Room from List

| Field | Content |
|---|---|
| **ID** | UC-05 |
| **Primary Actor** | Player |
| **Goal** | Join an existing open room selected from the lobby list. |
| **Preconditions** | Player is in the Lobby; at least one room is in state WAITING with a free slot. |
| **Postconditions (success)** | Player occupies a slot in the room and is placed in its Waiting Room. |
| **Trigger** | Player selects a room and presses `[ENTER]` Join. |

**Main Success Scenario**
1. Player highlights a room row using `[↑/↓]`.
2. Player presses `[ENTER]` to join.
3. System sends `JOIN /room/<id>` to the Game Server.
4. Server assigns the Player an open slot and returns `200 OK` (joining an existing room, no resource created).
5. System displays the Waiting Room; Player's name appears in the next free slot with status "ready".

**Extensions / Alternate Flows**
- **4a. Room is full (409):** System shows "room full"; return to Lobby (UC-03).
- **4b. Room already IN-GAME (409):** Join is refused; System suggests other open rooms; return to Lobby.
- **4c. Room no longer exists (404):** System refreshes the list; return to Lobby.

**Exceptions**
- **E1. Server unreachable:** System shows a connection error; Player stays in Lobby.

**Related Use Cases** — analogous to UC-06 Join Room by Room ID.

---

### UC-06 — Join Room by Room ID

| Field | Content |
|---|---|
| **ID** | UC-06 |
| **Primary Actor** | Player |
| **Goal** | Join a specific room directly by typing its Room ID (e.g. shared by a friend). |
| **Preconditions** | Player is in the Lobby and knows a valid Room ID. |
| **Postconditions (success)** | Player occupies a slot in the target room and is placed in its Waiting Room. |
| **Trigger** | Player types a Room ID in the **Join By Room ID** panel and presses `[ENTER]`. |

**Main Success Scenario**
1. Player types a Room ID (e.g. `duel-42`) into the entry field.
2. Player presses `[ENTER]` Join.
3. System sends `JOIN /room/<id>` to the Game Server.
4. Server validates the ID and assigns an open slot, returning `200 OK` (joining an existing room, no resource created).
5. System displays the Waiting Room with the Player in a slot.

**Extensions / Alternate Flows**
- **4a. Unknown Room ID (404):** System shows "no such room"; return to step 1.
- **4b. Room full (409):** System shows "room full"; return to step 1.
- **4c. Room IN-GAME (409):** Join refused; System shows a "game already in progress" message; return to step 1.

**Exceptions**
- **E1. Server unreachable:** Connection error; Player stays in Lobby.

**Related Use Cases** — analogous to UC-05.

---

### UC-07 — Leave Room

| Field | Content |
|---|---|
| **ID** | UC-07 |
| **Primary Actor** | Player |
| **Goal** | Leave a waiting room and return to the Lobby. |
| **Preconditions** | Player is in a Waiting Room. |
| **Postconditions (success)** | Player's slot is freed; Player is back in the Lobby. If the Owner leaves and others remain, ownership passes to the next player in slot order before the Owner's slot is freed; if no one remains, the room is destroyed. |
| **Trigger** | Player presses `[L]` Leave. |

**Main Success Scenario**
1. Player presses `[L]` Leave.
2. System sends `LEAVE /room/<id>`.
3. Server frees the Player's slot and updates the room for remaining players.
4. System returns the Player to the Lobby.

**Extensions / Alternate Flows**
- **3a. Leaving Player is the Owner and others remain:** Server (i) reassigns ownership to the **next player in slot order**, (ii) broadcasts the room update (new owner) to all remaining members, then (iii) frees the old Owner's slot and returns them to the Lobby. The transfer happens before the slot is freed so the room is never ownerless.
- **3b. Last player leaves:** Server destroys the room (ROOM_DESTROYED); chat room is torn down.

**Exceptions**
- **E1. Server unreachable:** System still returns Player to Lobby locally; session reconciles on reconnect.

**Related Use Cases** — none.

---

### UC-08 — Start Game

| Field | Content |
|---|---|
| **ID** | UC-08 |
| **Primary Actor** | Room Owner |
| **Goal** | Begin the match for everyone in the room. |
| **Preconditions** | Actor is the Room Owner; room has enough ready players (Double: 2/2; Battle Royale: ≥ 4). |
| **Postconditions (success)** | The room transitions to IN-GAME; all members enter the corresponding gameplay screen. |
| **Trigger** | Owner presses `[S]` Start. |

**Main Success Scenario**
1. Room reaches the required player count; status shows "Ready to start".
2. Owner presses `[S]` Start.
3. System sends `START /room/<id>`.
4. Server verifies the requester is the Owner and the room meets minimum players.
5. Server transitions the room to IN-GAME and pushes the initial `STATE`.
6. All members' clients switch to the gameplay screen (Double → UC-11, Battle Royale → UC-12).

**Extensions / Alternate Flows**
- **4a. Not enough players (Battle Royale < 4):** Server refuses; status stays "Waiting for opponents — Need at least 4 players to start".
- **4b. Requester is not the Owner (403):** Start is refused → see UC-08b.

**Exceptions**
- **E1. A player disconnects during start:** Server aborts start; room returns to WAITING.

**Related Use Cases** — precedes UC-11 / UC-12; alternate actor path UC-08b.

---

### UC-08b — Attempt to Start Game as Non-Owner (Denied)

| Field | Content |
|---|---|
| **ID** | UC-08b |
| **Primary Actor** | Player (non-owner member of the room) |
| **Goal** | A non-owner attempts to start the match; the server must reject the attempt because starting is an owner-only privilege. |
| **Preconditions** | Player is a member of a Waiting Room but is **not** the Room Owner. |
| **Postconditions (success)** | No state change: the room stays in WAITING; the requesting Player is informed that only the Owner can start; the rejection is logged. |
| **Trigger** | A non-owner Player presses `[S]` Start. |

**Main Success Scenario** *(success here = the attempt is correctly denied)*
1. Non-owner Player presses `[S]` Start.
2. Client sends `START /room/<id>`.
3. Server checks the requester's identity against the room's Owner.
4. Server determines the requester is not the Owner and returns `403 Forbidden`.
5. Server leaves the room unchanged (still WAITING) and logs the rejected attempt at warning level.
6. Client shows a "Only the room owner can start the game" message; the Player remains in the Waiting Room.

**Extensions / Alternate Flows**
- **3a. Requester is actually the Owner:** This is not this use case → proceed as UC-08.

**Exceptions**
- **E1. Server unreachable:** Client shows a connection error; no start occurs.

**Related Use Cases** — alternate actor path of UC-08 Start Game (same trigger, non-owner actor).

---

### UC-09 — Chat in Room

| Field | Content |
|---|---|
| **ID** | UC-09 |
| **Primary Actor** | Player |
| **Goal** | Exchange text messages with others in the same room. |
| **Preconditions** | Player is in a Waiting Room (or in-game room with chat enabled). |
| **Postconditions (success)** | The message is broadcast to all room members; it appears in the room chat. |
| **Trigger** | Player presses `[C]` Chat and submits a message. |

**Main Success Scenario**
1. Player presses `[C]` Chat.
2. Player types a message and submits it.
3. Client sends a `CHAT` message to the Chat Daemon (chatd) over the authenticated session.
4. chatd applies the per-session rate limit (token bucket) and broadcasts the message to the room.
5. All members (including sender) see the message; system events may also be narrated (e.g. joins, KOs).

**Extensions / Alternate Flows**
- **4a. Rate limit exceeded (429):** Message is dropped; sender sees a "slow down" notice.
- **4b. Player is muted (admin action):** Message is suppressed.

**Exceptions**
- **E1. chatd unavailable:** Chat is unavailable; gameplay is unaffected (social layer is decoupled from the game loop).

**Related Use Cases** — none.

---

## Gameplay

### UC-10 — Play Single-Player Game

| Field | Content |
|---|---|
| **ID** | UC-10 |
| **Primary Actor** | Player |
| **Goal** | Play a solo Tetris game to earn points and score. |
| **Preconditions** | Player is authenticated; a default character and theme are set. |
| **Postconditions (success)** | Final score is recorded; points earned are credited to the wallet; leaderboard is updated if a personal best is beaten. |
| **Trigger** | Player selects **Single Player** on the Home page. |
| **DB Mapping** | Live play is **runtime only** (tetrisd memory, no DB). Only the post-game step persists: `db_record_game(id, score_delta, points_delta, won)`. |

**Main Success Scenario**
1. System starts a single-player session and renders the board, the piece queue (next pieces), the hold column, the Player's default character portrait, and the live score.
2. System spawns falling pieces on a gravity timer.
3. Player controls pieces (`MOVE` left/right, `ROTATE` cw/ccw, `DROP` soft/hard) — `«include»` **UC-13 Control Falling Piece**.
4. System clears completed lines and updates the score.
5. Loop steps 2–4 until the board tops out (game over).
6. System shows the final score, then makes the **one** persisted call of this use case: `db_record_game(id, score_delta, points_delta, won=false)`, which updates `leaderboard_score`, credits `wallet_points`, and increments `games_played` (the only write that touches the skip list). Leaderboard reflects the new score immediately.

**Extensions / Alternate Flows**
- **3a. Invalid move (collision, `409` runtime):** Server rejects; board keeps the authoritative position.
- **5a. Player quits mid-game:** Session ends **without** calling `db_record_game` — an abandoned game is not scored, so no points, score, or games_played change. (Only a game that reaches game-over is recorded.)

**Exceptions**
- **E1. Server disconnect:** Game pauses/ends; session state reconciled on reconnect.

**Related Use Cases** — `«include»` UC-13 Control Falling Piece.

---

### UC-11 — Play Double (2-Player) Game

| Field | Content |
|---|---|
| **ID** | UC-11 |
| **Primary Actor** | Player (×2) |
| **Goal** | Compete head-to-head against one opponent. |
| **Preconditions** | A Double room with 2 players has been started (UC-08). |
| **Postconditions (success)** | Winner/loser determined; points credited; leaderboard updated. |
| **Trigger** | The room's Start Game (UC-08) completes for a Double room. |
| **DB Mapping** | Live play is runtime only (tetrisd memory). Post-game, **each** player is persisted with `db_record_game(id, score_delta, points_delta, won)` — `won=true` for the winner, `false` for the loser. |

**Main Success Scenario**
1. System renders the split screen: own **Board** with piece queue and hold column on the left, **Opponent Board** on the right, and both usernames with live point totals below.
2. Both Players control their pieces concurrently (`MOVE`/`ROTATE`/`DROP`).
3. Server pushes each Player's `STATE` and the opponent's board updates in real time.
4. Line clears update each Player's score; garbage/abilities may be applied per rules.
5. Game ends when one Player tops out or a win condition is met; the match **reaches game-over**, so System calls `db_record_game(...)` once per player (winner `won=true`, loser `won=false`), crediting points and updating the leaderboard.

**Extensions / Alternate Flows**
- **2a. A Player activates an equipped ability:** → UC-20 Activate Gaiden Ability (`«extend»`).
- **5a. A Player quits/disconnects mid-game:** The **quitter is not recorded** (`db_record_game` is not called for them — an abandoned game is not scored). The remaining Player wins by default; because that is a completed result for them, `db_record_game` **is** called for the winner.

**Exceptions**
- **E1. Server disconnect (whole match aborted):** No game-over is reached, so `db_record_game` is called for **no one**; handled per reconnection policy.

**Related Use Cases** — `«include»` UC-13; `«extend»` UC-20.

---

### UC-12 — Play Battle Royale Game

| Field | Content |
|---|---|
| **ID** | UC-12 |
| **Primary Actor** | Player (4–99) |
| **Goal** | Compete against many players; survive as garbage is traded across boards. |
| **Preconditions** | A Battle Royale room with ≥ 4 players has been started (UC-08). |
| **Postconditions (success)** | Ranking/last-standing determined; points credited; leaderboard updated. |
| **Trigger** | The room's Start Game (UC-08) completes for a Battle Royale room. |
| **DB Mapping** | Live play (boards, garbage IPC across rooms) is runtime only. Post-game, **each** participant is persisted with `db_record_game(id, score_delta, points_delta, won)` — `won=true` only for the last-standing player. |

**Main Success Scenario**
1. System renders own **Board** (center) with piece queue, hold column, and live **Scores**, surrounded by grids showing other players' boards.
2. Players control pieces concurrently.
3. When a Player clears N lines (N ≥ 2) in one move, N−1 rows become garbage inserted at the bottom of a random other player's board in a different room (server-managed via IPC).
4. Server pushes `STATE` updates for all visible boards.
5. Players are eliminated as they top out; play continues until a winner/last-standing remains.
6. At game-over, System records the final ranking and calls `db_record_game(...)` once per **participant who was still in the game at game-over** (last-standing `won=true`, others `won=false`), crediting points (line clears / KOs / win) and updating the leaderboard.

**Extensions / Alternate Flows**
- **2a. Player activates an equipped ability:** → UC-20 Activate Gaiden Ability (`«extend»`).
- **5a. Player is KO'd:** Their board is marked eliminated; they wait out the remainder until a winner is decided, and are recorded at game-over with their finishing rank (`won=false`).
- **5b. Player quits/disconnects mid-game:** The quitter is **not recorded** (`db_record_game` is not called for them). Remaining players play on; each is recorded normally at game-over.

**Exceptions**
- **E1. Server disconnect (whole match aborted):** No game-over reached → `db_record_game` called for no one.

**Related Use Cases** — `«include»` UC-13; `«extend»` UC-20.

---

### UC-13 — Control Falling Piece *(included by UC-10/11/12)*

| Field | Content |
|---|---|
| **ID** | UC-13 |
| **Primary Actor** | Player |
| **Goal** | Move, rotate, and drop the active piece; the server validates each action. |
| **Preconditions** | A game is IN-GAME with an active falling piece. |
| **Postconditions (success)** | The piece is placed at a server-validated position; lines may clear. |
| **Trigger** | Player issues a movement input. |

**Main Success Scenario**
1. Player issues `MOVE LEFT/RIGHT`, `ROTATE CW/CCW`, or `DROP SOFT/HARD`.
2. Server validates the move against the authoritative board.
3. Server applies the move and pushes updated `STATE`.

**Extensions / Alternate Flows**
- **2a. Illegal move (collision):** Server responds `409 INVALID_MOVE` with the authoritative position; client corrects to it.

**Related Use Cases** — included by UC-10, UC-11, UC-12.

---

### UC-20 — Activate Gaiden Ability *(extends UC-11/UC-12)*

| Field | Content |
|---|---|
| **ID** | UC-20 |
| **Primary Actor** | Player |
| **Goal** | Trigger an equipped ability (Garbage Surge, Shield, Freeze) during a multiplayer match. |
| **Preconditions** | Player owns and has equipped the ability before the game started; Player has enough points if the ability is consumable. |
| **Postconditions (success)** | The ability's server-enforced effect is applied; chatd narrates the event. |
| **Trigger** | Player presses the ability's key during an eligible match. |
| **DB Mapping** | Read-only checks: `db_player_owns_character(id, cid)` (does the player own the character granting this ability) + `db_get_character(cid)` to read the `abilities` bitfield. The **effect itself is runtime** (room state), not persisted. |

**Main Success Scenario**
1. Player triggers the equipped ability.
2. Client sends the ability request (HTTTP `ABILITY`) to the Game Server.
3. Server validates ownership via `db_player_owns_character` and checks the `abilities` bitfield from `db_get_character` (read lock).
4. Server enforces the effect in room state:
   - **Garbage Surge** — +3 garbage lines to the target.
   - **Shield** — ignore incoming garbage for N ticks.
   - **Freeze** — target's moves dropped for ~2 seconds.
5. Server emits an `ABILITY_USED` event; chatd narrates it to the room.

**Extensions / Alternate Flows**
- **3a. Character/ability not owned (`db_player_owns_character` false / `DB_NOT_OWNED` → 403):** Request rejected; no effect.
- **3b. Insufficient points (if consumable, `DB_INSUFFICIENT` → 403):** Rejected with a notice.

**Exceptions**
- **E1. chatd down:** Effect still applies; narration is silently dropped.

**Related Use Cases** — `«extend»` UC-11, UC-12.

---

## Marketplace

### UC-14 — Buy Character

| Field | Content |
|---|---|
| **ID** | UC-14 |
| **Primary Actor** | Player |
| **Goal** | Purchase a character (and its abilities) using wallet points. |
| **Preconditions** | Player is in the Marketplace (Characters tab). (Affordability and ownership are **not** preconditions — `db_buy_character` enforces them atomically and reports the outcome.) |
| **Postconditions (success)** | On `DB_OK`: character added to `owned_characters`, wallet debited by `cost_points`, change persisted (LWW log append). On `DB_EXISTS`: no change (already owned). |
| **Trigger** | Player presses **BUY** on a selected character. |
| **DB Mapping** | `db_buy_character(id, cid)` → `DB_OK`=`200 OK` (bought), `DB_EXISTS`=`200 OK` (already owned, no-op), `DB_INSUFFICIENT`=`403 Forbidden`, `DB_NOT_FOUND`=`404`, `DB_FULL`=`409 Conflict` (inventory at `DB_MAX_OWNED`). All checks + debit + grant run **inside the DB write lock** — check-and-act is one atom. |

**Main Success Scenario**
1. Player selects **Characters** and highlights a character (e.g. Princess, Halloween, Wolf-man, Mirurun).
2. System shows the preview, abilities (Ability 1, Ability 2), and — driven by `db_player_owns_character(id, cid)` read on selection — enables **BUY** / disables **Set as Default** (or the reverse if already owned). *(This read drives button state only; it is not the correctness guard.)*
3. Player presses **BUY**.
4. Server calls `db_buy_character(id, cid)`. Under the write lock it checks existence, ownership, balance, and the inventory cap, then debits `cost_points` and adds the character in one atomic write (`«include»` **UC-16 Deduct Wallet Points**).
5. On `DB_OK`, System confirms the purchase, flips the cached ownership state to owned, disables **BUY**, enables **Set as Default**; the character becomes available to equip (UC-17).

**Extensions / Alternate Flows**
- **4a. Already owned (`DB_EXISTS` → 200, no-op):** No debit, no change; the desired end state (owned) already holds. Client leaves **BUY** disabled and **Set as Default** enabled. This is the server-side backstop for a stray BUY that slipped past the disabled button.
- **4b. Insufficient points (`DB_INSUFFICIENT` → 403):** Refused, wallet unchanged; System shows the shortfall. Response body reason `insufficient_points` distinguishes this from other 403s.
- **4c. Inventory full (`DB_FULL` → 409):** `owned_characters_count` is at `DB_MAX_OWNED`; purchase refused, wallet unchanged.
- **4d. Unknown character (`DB_NOT_FOUND` → 404):** Refused.

**Exceptions**
- **E1. DB I/O error (`DB_IO_ERROR` → 500):** Purchase fails; wallet unchanged.

**Related Use Cases** — `«include»` UC-16 Deduct Wallet Points; enables UC-17 Set Default Character.

---

### UC-15 — Buy Theme

| Field | Content |
|---|---|
| **ID** | UC-15 |
| **Primary Actor** | Player |
| **Goal** | Purchase a theme (color scheme + character nickname/profile picture set) using wallet points. |
| **Preconditions** | Player is in the Marketplace (Themes tab). (Affordability and ownership are **not** preconditions — `db_buy_theme` enforces them atomically and reports the outcome.) |
| **Postconditions (success)** | On `DB_OK`: theme added to `owned_themes`, wallet debited by `cost_points`, change persisted (LWW log append). On `DB_EXISTS`: no change (already owned). |
| **Trigger** | Player presses **BUY** on a selected theme. |
| **DB Mapping** | `db_buy_theme(id, tid)` → `DB_OK`=`200 OK` (bought), `DB_EXISTS`=`200 OK` (already owned, no-op), `DB_INSUFFICIENT`=`403 Forbidden`, `DB_NOT_FOUND`=`404`, `DB_FULL`=`409 Conflict` (inventory at `DB_MAX_OWNED`). All checks + debit + grant run **inside the DB write lock** — check-and-act is one atom. |

**Main Success Scenario**
1. Player selects **Themes** and highlights a theme (e.g. Default, Design and AI, Do u wanna build a snowman, Haaland, John Cena, Claude-ing).
2. System shows the theme's color scheme and character nickname/profile-picture details, and — driven by `db_player_owns_theme(id, tid)` read on selection — enables **BUY** / disables **Set as Default** (or the reverse if owned). *(Read drives button state only; not the correctness guard.)*
3. Player presses **BUY**.
4. Server calls `db_buy_theme(id, tid)`. Under the write lock it checks existence, ownership, balance, and the inventory cap, then debits and grants atomically (`«include»` **UC-16**).
5. On `DB_OK`, System confirms; flips the cached ownership state, disables **BUY**, enables **Set as Default**; theme becomes available to equip (UC-18).

**Extensions / Alternate Flows**
- **4a. Already owned (`DB_EXISTS` → 200, no-op):** No debit, no change; client leaves **BUY** disabled and **Set as Default** enabled. Server-side backstop for a stray BUY.
- **4b. Insufficient points (`DB_INSUFFICIENT` → 403):** Refused, wallet unchanged; shortfall shown. Body reason `insufficient_points`.
- **4c. Inventory full (`DB_FULL` → 409):** `owned_themes_count` at `DB_MAX_OWNED`; refused, wallet unchanged.
- **4d. Unknown theme (`DB_NOT_FOUND` → 404):** Refused.

**Exceptions**
- **E1. DB I/O error (`DB_IO_ERROR` → 500):** Purchase fails; wallet unchanged.

**Related Use Cases** — `«include»` UC-16; enables UC-18 Set Default Theme.

---

### UC-16 — Deduct Wallet Points *(included by UC-14/UC-15)*

| Field | Content |
|---|---|
| **ID** | UC-16 |
| **Primary Actor** | Player (initiator); `libmacminidb` (executor) |
| **Goal** | Atomically debit the points for a purchase and add the item, in a single durable write. |
| **Preconditions** | A purchase is in progress; balance ≥ `cost_points`. |
| **Postconditions (success)** | Wallet is reduced by `cost_points` and the item added to the owned set — as one whole-record write, appended to the append-only log (LWW) and flushed within ≤ 1 s. |
| **Trigger** | `db_buy_character` / `db_buy_theme` reaches its payment step. |
| **DB Mapping** | Not a standalone endpoint — it is the internal effect of `db_buy_*`. Executes under the DB **write lock**; the log append is a page-cache write only (no `fdatasync` under the lock). |

**Main Success Scenario**
1. `db_buy_*` takes the DB write lock.
2. It checks, in order: player & item exist (`DB_NOT_FOUND`), not already owned (`DB_EXISTS`), `wallet_points ≥ cost_points` (`DB_INSUFFICIENT`), inventory below cap (`DB_FULL`).
3. All checks pass → it debits the wallet **and** adds the item to `owned_characters` / `owned_themes` on the same player record.
4. It appends the whole updated record to the log and releases the lock (`DB_OK`).

**Extensions / Alternate Flows**
- **2a. A precheck fails:** Returns the corresponding result (`DB_NOT_FOUND` / `DB_EXISTS` / `DB_INSUFFICIENT` / `DB_FULL`) with **no** change to wallet or inventory.

**Related Use Cases** — included by UC-14, UC-15.

---

### UC-17 — Set Default Character

| Field | Content |
|---|---|
| **ID** | UC-17 |
| **Primary Actor** | Player |
| **Goal** | Choose which owned character is used by default in games and as profile picture. |
| **Preconditions** | Player owns the character. |
| **Postconditions (success)** | `current_equipped_character` is updated and persisted; it appears in Settings and single-player HUD. |
| **Trigger** | Player presses **Set as Default Character** (Marketplace) or **Change Default Character** (Settings). |
| **DB Mapping** | `db_equip_character(id, cid)` → `DB_OK`=`200 OK`, `DB_NOT_OWNED`=`403 Forbidden`. |

**Main Success Scenario**
1. Player selects an owned character.
2. Player presses **Set as Default Character** / **Change Default Character**.
3. Server calls `db_equip_character(id, cid)`; on `DB_OK` the default is updated.
4. System reflects the change in Settings (Default Character, profile picture) and in-game.

**Extensions / Alternate Flows**
- **3a. Character not owned (`DB_NOT_OWNED` → 403):** Rejected; Player must Buy (UC-14) first.

**Related Use Cases** — reachable from Marketplace and Settings (same use case, two entry points).

---

### UC-18 — Set Default Theme

| Field | Content |
|---|---|
| **ID** | UC-18 |
| **Primary Actor** | Player |
| **Goal** | Choose which owned theme is applied by default. |
| **Preconditions** | Player owns the theme. |
| **Postconditions (success)** | `current_equipped_theme` is updated and persisted; the board and UI adopt its color scheme at game start. |
| **Trigger** | Player presses **Set as Default Theme** (Marketplace) or **Change Default Theme** (Settings). |
| **DB Mapping** | `db_equip_theme(id, tid)` → `DB_OK`=`200 OK`, `DB_NOT_OWNED`=`403 Forbidden`. |

**Main Success Scenario**
1. Player selects an owned theme.
2. Player presses **Set as Default Theme** / **Change Default Theme**.
3. Server calls `db_equip_theme(id, tid)`; on `DB_OK` the current theme is updated.
4. System reflects the change in Settings (Current Theme) and loads it on the next game start.

**Extensions / Alternate Flows**
- **3a. Theme not owned (`DB_NOT_OWNED` → 403):** Rejected; Player must Buy (UC-15) first.

**Related Use Cases** — reachable from Marketplace and Settings.

---

## Profile, Settings & Leaderboard

### UC-19 — View Settings / Profile

| Field | Content |
|---|---|
| **ID** | UC-19 |
| **Primary Actor** | Player |
| **Goal** | Review account and profile information in one place. |
| **Preconditions** | Player is authenticated. |
| **Postconditions (success)** | System displays username, profile picture, default character, owned character list, current theme, owned theme list, wallet points, leaderboard score, and leaderboard ranking. |
| **Trigger** | Player selects **Setting** on the Home page. |
| **DB Mapping** | `db_get_player(id, &player)` (profile, wallet, equipped, owned lists) + `db_rank(id, &rank)` (1-based rank). Catalogue names resolved via `db_get_character` / `db_get_theme`. All read-lock reads. |

**Main Success Scenario**
1. Player opens Settings.
2. Server reads the profile with `db_get_player(id, ...)` and the rank with `db_rank(id, ...)` (single read lock each); catalogue lookups resolve owned/equipped ids to names.
3. System displays: Username, profile picture of current default character, Default Character (+ Change), Character List (with current default marked), Current Theme (+ Change), Theme List (with current marked), Wallet Points, Leaderboard Scores, Leaderboard Ranking.

**Extensions / Alternate Flows**
- **3a. Player presses Change Default Character:** → UC-17.
- **3b. Player presses Change Default Theme:** → UC-18.

**Related Use Cases** — leads to UC-17, UC-18.

---

### UC-21 — View Leaderboard

| Field | Content |
|---|---|
| **ID** | UC-21 |
| **Primary Actor** | Player |
| **Goal** | See the global ranking of players by points. |
| **Preconditions** | Player is authenticated. |
| **Postconditions (success)** | The leaderboard is displayed (podium for top 3, list for ranks 4–10, each with points). |
| **Trigger** | Player selects **Leaderboard** on the Home page. |
| **DB Mapping** | `db_leaderboard(out, cap=10, &count)` — top-N by `(score, id)` straight off the skip list; per-row rank is positional. Read-lock read. |

**Main Success Scenario**
1. Player opens the Leaderboard.
2. Server calls `db_leaderboard(out, 10, &count)`, reading the top entries from the skip list under a read lock.
3. System renders the top-3 podium (1st, 2nd, 3rd with names and points) and the ranked list for positions 4–10 with points.

**Extensions / Alternate Flows**
- **2a. Ranking unavailable:** System shows a "temporarily unavailable" message.

**Related Use Cases** — none.

---

## Summary of Relationships

| Base Use Case | Relationship | Target |
|---|---|---|
| UC-02 Log In | `«include»` | UC-02a Connect to Server |
| UC-03 Browse Open Rooms | `«include»` | UC-03a Refresh Room List |
| UC-04 Create Room | `«include»` | UC-04a Select Game Mode |
| UC-08 Start Game | alternate actor path | UC-08b Attempt to Start as Non-Owner |
| UC-10/11/12 Play Game | `«include»` | UC-13 Control Falling Piece |
| UC-11/12 Play Multiplayer | `«extend»` | UC-20 Activate Gaiden Ability |
| UC-14 Buy Character | `«include»` | UC-16 Deduct Wallet Points |
| UC-15 Buy Theme | `«include»` | UC-16 Deduct Wallet Points |
| UC-19 View Settings | navigates to | UC-17 / UC-18 |

## DB Mapping Summary (`libmacminidb`)

Every persisted use case, its `libmacminidb` call, and the `t_db_result → HTTTP` translation. Use cases not listed here are **runtime only** (tetrisd / chatd) and never touch the DB.

| Use case | DB call(s) | Result → HTTTP |
|---|---|---|
| UC-01 Register | `db_signup` | `DB_OK`→201 · `DB_EXISTS`→409 |
| UC-02 Log In | `db_login` | `DB_OK`→200 · `DB_BAD_CREDS`/`DB_NOT_FOUND`→401 |
| UC-14 Buy Character | `db_buy_character` (all checks in-lock) | • `DB_OK`→200<br>• `DB_EXISTS`→200 (no-op)<br>• `DB_INSUFFICIENT`→403<br>• `DB_FULL`→409<br>• `DB_NOT_FOUND`→404 |
| UC-15 Buy Theme | `db_buy_theme` (all checks in-lock) | • `DB_OK`→200<br>• `DB_EXISTS`→200 (no-op)<br>• `DB_INSUFFICIENT`→403<br>• `DB_FULL`→409<br>• `DB_NOT_FOUND`→404 |
| UC-16 Deduct Points | *internal to* `db_buy_*` (atomic, write lock) | — |
| UC-17 Set Default Character | `db_equip_character` | `DB_OK`→200 · `DB_NOT_OWNED`→403 |
| UC-18 Set Default Theme | `db_equip_theme` | `DB_OK`→200 · `DB_NOT_OWNED`→403 |
| UC-10/11/12 Play (post-game) | `db_record_game` once per participant **at game-over** (all three modes; **not** called on mid-game quit) | credits points, updates score, increments `games_played` (and `games_won` on a win) |
| UC-19 View Settings | `db_get_player` + `db_rank` (+ catalogue lookups) | 200 |
| UC-20 Activate Ability | `db_player_owns_character` + `db_get_character` (reads) | valid→effect · `DB_NOT_OWNED`→403 |
| UC-21 View Leaderboard | `db_leaderboard` | 200 |

**Status-code conventions used above**

| Status / result | Used for | Convention |
|---|---|---|
| `403 Forbidden` | Both authorization denials and insufficient funds: equip an unowned item (`DB_NOT_OWNED`), non-owner start (UC-08b), can't-afford a purchase (`DB_INSUFFICIENT`). | Return a distinct error **body/reason** (e.g. `"insufficient_points"` vs `"not_owned"`) so the client can tell "broke" from "not allowed" even though the status code is the same. |
| `401 Unauthorized` | Login failure. | Return the **same** response for bad password and unknown user so the endpoint doesn't leak which usernames exist. |
| `409 Conflict` | **Taken username** (`DB_EXISTS` from `db_signup`), **inventory full** (`DB_FULL` from `db_buy_*`), and runtime room-state conflicts (room full / IN-GAME). | — |
| `DB_EXISTS` (maps two ways by context) | `409` from `db_signup` vs `200` from `db_buy_*`. | Username collision is a real conflict the caller must fix (`409`); already-owned is a harmless no-op — the desired end state already holds (`200`). Same result code, different HTTP status depending on the endpoint. |
