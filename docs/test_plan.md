# tetriSH — Test Strategy, Decomposition & Unit Test Specification

---

## Table of Contents

- [1. Test Strategy and Decomposition](#1-test-strategy-and-decomposition)
- [2. Tools and Test Environment](#2-tools-and-test-environment)
- [3. Test Environments](#3-test-environments)
- [4. Entry and Exit Criteria](#4-entry-and-exit-criteria)
- [5. Test ID Scheme](#5-test-id-scheme)
- [6. Traceability and Priorities](#6-traceability-and-priorities)
- [7. Unit Test Cases](#7-unit-test-cases)
  - [UC-01 — Register Account](#uc-01--register-account)
  - [UC-02 — Log In](#uc-02--log-in)
  - [UC-03 — Browse Open Rooms](#uc-03--browse-open-rooms)
  - [UC-04 — Create Room](#uc-04--create-room)
  - [UC-05 — Join Room from List](#uc-05--join-room-from-list)
  - [UC-06 — Join Room by Room ID](#uc-06--join-room-by-room-id)
  - [UC-07 — Leave Room (includes UC-07a)](#uc-07--leave-room-includes-uc-07a)
- [8. Implementation and Execution Timeline](#8-implementation-and-execution-timeline)

---

## 1. Test Strategy and Decomposition

The project uses a hybrid **bottom-up and sandwich** integration strategy. Pure C
libraries are tested first because they give deterministic failures. Adapter pairs
are then integrated over controlled OS resources. Controllers are tested with real
domain objects and only the nearest external dependency mocked. Finally, a headless
client drives the complete encrypted request path. This gives quick fault
localisation while still proving complete use cases.

| Level | Strategy | Scope | Dependency treatment | Purpose |
|---|---|---|---|---|
| **L1** — Pure domain and data modules in isolation | Bottom-up | `libtetrisbrain` modules, `libmacminidb` internal structures, room predicates / state transitions | Real objects; no sockets. Temporary directories for DB tests. | Proves deterministic rules and invariant preservation before controllers are added. |
| **L2** — Adapter pairs | Bottom-up | `libtetrissh` client/server over `socketpair`; `libhtttp` serialiser/parser round trip; DB write/reopen recovery | Real peer adapters with controlled OS resources. | Proves each boundary maps bytes/files to domain results correctly. |
| **L3** — Controller + domain + mocked dependency | Sandwich | `AuthController` + fake DB; `LobbyController` + real `Lobby` + fake DB; `RoomController` + real `Room`/`Lobby` | Mocks only the next external boundary. | Follows one or more activation bars from the sequence diagrams while isolating failure causes. |
| **L4** — Server request pipeline | Incremental | Headless test client → `libtetrissh` → `libhtttp` → `tetrisd` dispatcher/controller → `Room`/DB → encrypted response | Real socket or loopback TCP; logger can be a capture stub. | Exercises request parsing, authentication, dispatch, status mapping and state mutation. |
| **L5** — Full system / end-to-end | Top-level confirmation | `tetrish`-launched daemons + `tetrisd` + `tetrislogd` + DB + one or more clients | Headless client for automation; manual notcurses rendering checklist for visual output. | Confirms complete use-case paths and operational failure handling. |

---

## 2. Tools and Test Environment

| Area | Tool / approach |
|---|---|
| C unit tests | Existing assert-based runners and Unity where already used; one test function per case. |
| Mocks / stubs | Hand-written fakes, function-pointer seams or link-time wrappers (`--wrap`); avoid introducing a large mocking dependency. |
| Adapter integration | `socketpair()` for secure-session tests; temporary directories/files for DB and recovery. |
| Component / system orchestration | Shell scripts or Python `subprocess` harness plus a small headless HTTTP test client. |
| Memory / UB | ASan/UBSan during development; Valgrind on Linux/WSL before merge and release. |
| Concurrency | Helgrind or ThreadSanitizer, deterministic stress loops, timeout-based deadlock detection. |
| Coverage | `gcov`/`lcov`; branch coverage reviewed especially for `alt`/error paths. |
| Fuzzing | AFL++ or LLVM libFuzzer for parser, frame receiver, shell parser and DB recovery. |
| TUI | Pure menu/selection state tests plus manual notcurses visual checklist; visual rendering is never the only evidence. |

---

## 3. Test Environments

| Environment | Platform | Purpose |
|---|---|---|
| Developer fast loop | Linux/WSL or macOS | Unit tests, socketpair integration, ASan/UBSan where supported |
| Required memory-safety run | Linux/WSL | Valgrind with non-zero error exit; leak and invalid-access checks |
| Concurrency run | Linux | Helgrind or TSan; repeated concurrent clients with timeouts |
| Release / system run | Fresh checkout on supported machine | Clean build, daemon startup, full use-case regression, logger/control fault injection |
| TUI visual verification | At least one 80+ column real terminal | Layout, key handling, resize/quit behaviour; screenshots support but do not replace tests |

---

## 4. Entry and Exit Criteria

### 4.1 Entry criteria

- The related use case, sequence path or requirement has been identified.
- The target API compiles and its input/output contract is understood.
- Fixtures are isolated and resettable; DB tests use temporary directories.
- Mocks replace only the next external boundary and have explicit call-count expectations.
- The expected state after both success and failure has been written before implementation.

### 4.2 Exit criteria

- Clean compilation under `-Wall -Wextra -Werror`.
- Normal, boundary, negative and state-preservation cases pass.
- No ASan/UBSan or Valgrind issue; no leaked file descriptor or temporary file.
- Concurrent tests show no race/deadlock and pass at least 20 repeated runs.
- A failed assertion prints the test id, input class and expected/actual state.
- Full-system exit requires mandatory use-case paths, logger/control-plane fault
  behaviour and known-defect review.

---

## 5. Test ID Scheme

One scheme only. Earlier drafts mixed `UT-AUTH-01`-style and `UT-01`-style ids;
this document uses the following and nothing else.

| Prefix | Meaning | Range |
|---|---|---|
| `UT-nn` | Unit test, one target unit, all non-adjacent collaborators mocked | UT-01 … UT-52 |
| `IT-nn` | Integration test, two or more real components across a boundary | IT-01 … IT-14 |
| `ST-UCnn-x` | System test, one full use-case path end to end | per use case |

---

## 6. Traceability and Priorities

| Use case / requirement | Behaviour | Test IDs | Priority |
|---|---|---|---|
| UC-01 Register | Password match/mismatch; DB result mapping; duplicate username; starter defaults | UT-01 … UT-07, IT-01, IT-02, ST-UC01-* | PM3 core |
| UC-02 Log In | Handshake gate; credentials; salt lookup; session binding; no username enumeration | UT-08 … UT-13, IT-03, IT-04, ST-UC02-* | PM3 core |
| UC-03 Browse Rooms | Room summary; empty state; header/rank; refresh | UT-14 … UT-20, IT-05, IT-06, ST-UC03-* | Planned as lobby lands |
| UC-04 Create Room | Mode-derived slots; owner membership; WAITING; cancel/error | UT-21 … UT-29, IT-07, ST-UC04-* | Planned as room domain lands |
| UC-05 / UC-06 Join | Found/free/full/in-game; seating; READY transition | UT-30 … UT-43, IT-08, IT-09, ST-UC05/06-* | Planned as room domain lands |
| UC-07 Leave | Release; ownership transfer; room destruction; invalid member | UT-44 … UT-52, IT-10, IT-11, ST-UC07-* | Planned as room domain lands |
| Gameplay | Movement, rotation, gravity, line clear, score | existing `libtetrisbrain` suites | PM3 demo + later full stack |
| Secure transport | Handshake, max frame, tamper/replay, EOF | UT-10, existing `libtetrissh` suites, IT-03, IT-04 | PM3 demo |
| Persistence | Signup, duplicate, recovery, leaderboard invariants | UT-05, UT-06, UT-13, UT-20, existing `libmacminidb` suites, IT-12 | PM3 demo |
| Operations | Logger restart/drop and isolated control plane | IT-13, IT-14 | Later integration |

---

## 7. Unit Test Cases

Convention for every case below: **exactly one target unit**; every collaborator
named in the "Mocked" row is replaced; every collaborator not named is a real
in-memory fixture. Call counts are asserted, not just return values.

---

### UC-01 — Register Account

#### UT-01 — `test_signup_ui_password_mismatch_shows_error`

| Field | Content |
|---|---|
| **Target unit** | `SignUpUI.pressSignUp(username, password, confirmPassword)` |
| **Inputs** | • `username = "alice"`<br>• `password = "ValidPass1!"`<br>• `confirmPassword = "DifferentPass1!"` |
| **Expected outputs** | • `validateMatch(...)` returns `false`.<br>• `show("passwords do not match")` invoked exactly once.<br>• No route to the Login screen occurs.<br>• `HtttpClient.signup(...)` invoked **0** times. |
| **Mocked** | `HtttpClient.signup(username, password)` — asserted to receive 0 calls. |
| **Traces to** | UC-01 ext 5a |

#### UT-02 — `test_signup_client_created_response_returns_player_id`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.signup(username, password)` |
| **Inputs** | • `username = "alice"`<br>• `password = "ValidPass1!"` |
| **Expected outputs** | • Sends `SIGNUP /account` with body `{username, password}`.<br>• Returns `registered(playerId = 17)` to `SignUpUI`.<br>• No error result is returned. |
| **Mocked** | `AuthController.signup("alice", "ValidPass1!")` → `201 Created`, `Player-Id: 17`. |
| **Traces to** | UC-01.6 |

#### UT-03 — `test_auth_signup_db_ok_returns_201_created`

| Field | Content |
|---|---|
| **Target unit** | `AuthController.signup(username, password)` |
| **Inputs** | • `username = "alice"`<br>• `password = "ValidPass1!"` |
| **Expected outputs** | • `Credential.create(password)` invoked exactly once.<br>• `db_signup(...)` invoked exactly once with the generated hash **and** salt.<br>• Plaintext password is never passed to `Db`.<br>• Returns `201 Created` with `Player-Id: 17`. |
| **Mocked** | • `Credential.create(password)` → `credential(hash = H1, salt = S1)`<br>• `Db.db_signup("alice", H1, S1, &id)` → `DB_OK`, `id = 17` |
| **Traces to** | UC-01.5, UC-01.6 |

#### UT-04 — `test_credential_create_generates_salt_and_hash`

| Field | Content |
|---|---|
| **Target unit** | `Credential.create(password)` |
| **Inputs** | `password = "ValidPass1!"` |
| **Expected outputs** | • Returns a `Credential` containing the generated salt and password hash.<br>• Stored hash `!=` plaintext password.<br>• Plaintext password is not retained by the `Credential`.<br>• Two calls with the same password and different salts yield different hashes. |
| **Mocked** | • `SaltGenerator.generateSalt()` → `S1`<br>• `PasswordHasher.hash("ValidPass1!", S1)` → `H1` |
| **Traces to** | UC-01.5 |

#### UT-05 — `test_db_signup_unique_username_persists_starter_player`

| Field | Content |
|---|---|
| **Target unit** | `db_signup(db, username, password_hashed, salt, out_id)` |
| **Inputs** | • `username = "alice"`, `password_hashed = H1`, `salt = S1`<br>• No existing player has username `alice`. |
| **Expected outputs** | • Returns `DB_OK` and a newly assigned player id.<br>• Persists one player with `wallet_points = 0`.<br>• Starter character `1` and starter theme `1` are **owned and equipped**.<br>• No other item is owned. |
| **Mocked** | None — real `libmacminidb` over an isolated temporary directory fixture. |
| **Traces to** | UC-01.7 |
| **Status** | **Implementable today** against the shipped API. |

#### UT-06 — `test_db_signup_duplicate_username_returns_db_exists` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `db_signup(db, username, password_hashed, salt, out_id)` |
| **Inputs** | • Player `alice` already exists with hash `H1`.<br>• Second call with `username = "alice"`, `password_hashed = H2`, `salt = S2`. |
| **Expected outputs** | • Returns `DB_EXISTS`.<br>• `out_id` is not written.<br>• The existing player's hash remains `H1` and salt remains `S1` — **no overwrite**.<br>• Player count is unchanged. |
| **Mocked** | None — real DB over a temporary directory fixture. |
| **Traces to** | UC-01 ext 6a |
| **Status** | **Implementable today.** |

#### UT-07 — `test_auth_signup_db_exists_returns_409_conflict` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `AuthController.signup(username, password)` |
| **Inputs** | • `username = "alice"` (already taken)<br>• `password = "ValidPass1!"` |
| **Expected outputs** | • Returns `409 Conflict`.<br>• Response carries no `Player-Id` header.<br>• `Credential.create` is still invoked once (hashing precedes the DB call).<br>• No retry of `db_signup` occurs. |
| **Mocked** | • `Credential.create(password)` → `credential(H1, S1)`<br>• `Db.db_signup(...)` → `DB_EXISTS` |
| **Traces to** | UC-01 ext 6a, DB mapping `DB_EXISTS = 409` |

---

### UC-02 — Log In

#### UT-08 — `test_login_ui_unauthorized_shows_generic_error`

| Field | Content |
|---|---|
| **Target unit** | `LoginUI.pressLogin(username, password, serverId)` |
| **Inputs** | • `username = "alice"`, `password = "wrong-password"`, `serverId = "local"` |
| **Expected outputs** | • `show("invalid username or password")` invoked exactly once.<br>• The Home page is not shown.<br>• No authenticated UI session is stored.<br>• The message is byte-identical to the unknown-username case (no user enumeration). |
| **Mocked** | `HtttpClient.login(...)` → `error(401 Unauthorized)` |
| **Traces to** | UC-02 ext 6a |

#### UT-09 — `test_login_client_success_returns_authenticated_session`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.login(username, password, serverId)` |
| **Inputs** | • `username = "alice"`, `password = "ValidPass1!"`, `serverId = "local"` |
| **Expected outputs** | • `Session.connect(...)` is invoked **before** any `LOGIN` payload is sent (ordering asserted).<br>• Returns `session(playerId = 17, player)` to `LoginUI`.<br>• No unreachable or unauthorized error is returned. |
| **Mocked** | • `ServerEndpoint.resolve("local")` → `endpoint`<br>• `Session.connect(endpoint)` → session up<br>• `AuthController.login("alice", "ValidPass1!")` → `200 OK` + `Player-Id: 17` |
| **Traces to** | UC-02.5, UC-02.7 |

#### UT-10 — `test_session_connect_valid_handshake_establishes_session`

| Field | Content |
|---|---|
| **Target unit** | `Session.connect(endpoint)` |
| **Inputs** | • Endpoint resolves to the test server.<br>• Server certificate signed by the trusted CA.<br>• Nonce signature and wrapped AES key are valid. |
| **Expected outputs** | • Returns session up / success.<br>• Session is marked established.<br>• **No HTTTP payload is sent before the handshake completes.** |
| **Mocked** | Socket/crypto boundary over `socketpair()`, with valid certificate, signature and RSA-OAEP results. |
| **Traces to** | UC-02a |
| **Status** | **Implementable today** against `libtetrissh`. |

#### UT-11 — `test_auth_login_valid_credentials_binds_session` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `AuthController.login(username, password)` |
| **Inputs** | • `username = "alice"`, `password = "ValidPass1!"`<br>• A `Session` object is available for binding. |
| **Expected outputs** | • Fetches the stored salt for `alice`, then hashes the supplied password with it.<br>• `Session.bind(17)` invoked exactly once.<br>• Returns `200 OK` with `Player-Id: 17`.<br>• Plaintext password never reaches `Db`. |
| **Mocked** | • `Db.db_get_salt(db, "alice", out, cap)` → `DB_OK`, `S1`<br>• `PasswordHasher.hash("ValidPass1!", S1)` → `H1`<br>• `Db.db_login(db, "alice", H1, &player)` → `DB_OK`, `player(id = 17)`<br>• `Session.bind(17)` → bound |
| **Traces to** | UC-02.6, UC-02.7 |
| **Status** | **Unblocked** — `db_get_salt` landed. Awaits `AuthController` (Phase 2). |

#### UT-12 — `test_auth_login_unknown_user_hashes_dummy_salt_and_returns_401` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `AuthController.login(username, password)` |
| **Inputs** | • `username = "ghost"` (no such account), `password = "ValidPass1!"` |
| **Expected outputs** | • Returns `401 Unauthorized`.<br>• A hash **is still computed** against a dummy salt — the hasher is invoked exactly once, as in the known-user path.<br>• `Session.bind` invoked **0** times.<br>• The response body/message is byte-identical to the wrong-password case (compare against UT-08's fixture).<br>• `db_login` is either not called or called and ignored — but the number of hashing operations must not differ between the two paths. |
| **Mocked** | • `Db.db_get_salt(db, "ghost", out, cap)` → `DB_NOT_FOUND`<br>• `PasswordHasher.hash(...)` → recorded, asserted 1 call |
| **Traces to** | UC-02 ext 6a; enumeration contract |
| **Rationale** | `db_get_salt` reports `DB_NOT_FOUND` distinguishably by design. This test is the only thing preventing that from becoming a username-enumeration oracle. |

#### UT-13 — `test_db_login_matching_hash_returns_player`

| Field | Content |
|---|---|
| **Target unit** | `db_login(db, username, password_hashed, out)` |
| **Inputs** | • `username = "alice"`, `password_hashed = H1`<br>• Stored player `alice` has hash `H1`. |
| **Expected outputs** | • Returns `DB_OK`.<br>• Copies the matching player into `out`.<br>• The stored player record is **not** mutated.<br>• Companion case: wrong hash → `DB_BAD_CREDS`; unknown username → `DB_NOT_FOUND`; both map to `401` upstream. |
| **Mocked** | None — real DB over a temporary directory fixture. |
| **Traces to** | UC-02.6, DB mapping |
| **Status** | **Implementable today.** |

---

### UC-03 — Browse Open Rooms

#### UT-14 — `test_lobby_ui_empty_room_list_shows_empty_state`

| Field | Content |
|---|---|
| **Target unit** | `LobbyUI.selectMultiplayer()` |
| **Inputs** | • Authenticated `playerId = 17`.<br>• The server returns no open rooms. |
| **Expected outputs** | • Shows an empty-room message with "Create Room" and "Join by ID".<br>• Does not render any stale room row.<br>• Still displays the returned username, score and rank header. |
| **Mocked** | `HtttpClient.browseRooms(17)` → `roomList = []`, `header(alice, 0, rank 1)` |
| **Traces to** | UC-03 ext 2a |

#### UT-15 — `test_browse_rooms_client_maps_success_response`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.browseRooms(playerId)` |
| **Inputs** | `playerId = 17` |
| **Expected outputs** | • Sends `LIST /rooms` with `Player-Id: 17`.<br>• Returns the room list and header to `LobbyUI`.<br>• Preserves room id, mode, occupancy, status and owner fields without reordering. |
| **Mocked** | `LobbyController.listRooms(17)` → `200 OK` with two `RoomSummary` rows and a `HeaderDto`. |
| **Traces to** | UC-03.2 |

#### UT-16 — `test_lobby_controller_list_rooms_combines_rooms_and_header` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `LobbyController.listRooms(playerId)` |
| **Inputs** | `playerId = 17` |
| **Expected outputs** | • `Lobby.listOpenRooms()` invoked exactly once.<br>• `LobbyController.header(17)` invoked exactly once.<br>• Returns `200 OK` containing `rooms[]` and `header{username, score, rank}`.<br>• **Does not** call `Db` directly — the DB calls belong to `header()`. |
| **Mocked** | • `Lobby.listOpenRooms()` → `[roomA, roomB]`<br>• `LobbyController.header(17)` → `HeaderDto(alice, 1200, 3)` (self-collaborator seam) |
| **Traces to** | UC-03.2, UC-03.4 |

#### UT-17 — `test_lobby_controller_header_reads_player_and_rank` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `LobbyController.header(playerId)` |
| **Inputs** | `playerId = 17` |
| **Expected outputs** | • `db_get_player(17)` invoked exactly once.<br>• `db_rank(17)` invoked exactly once.<br>• Returns `HeaderDto(username = alice, score = 1200, rank = 3)`.<br>• No room state is read or mutated. |
| **Mocked** | • `Db.db_get_player(db, 17, &out)` → `DB_OK`, `player(alice, score 1200)`<br>• `Db.db_rank(db, 17, &rank)` → `DB_OK`, `rank = 3` |
| **Traces to** | UC-03.4 |

#### UT-18 — `test_lobby_list_open_rooms_excludes_in_game_rooms`

| Field | Content |
|---|---|
| **Target unit** | `Lobby.listOpenRooms()` |
| **Inputs** | Lobby contains one `WAITING` room, one `READY` room and one `IN_GAME` room. |
| **Expected outputs** | • Returns summaries for the `WAITING` and `READY` rooms only.<br>• `Room.toSummary()` invoked exactly once per included room, **0** times for the `IN_GAME` room.<br>• The Lobby's room collection is not mutated. |
| **Mocked** | Each included `Room.toSummary()` → a fixed `RoomSummary`. |
| **Traces to** | UC-03.2, UC-03.3 |

#### UT-19 — `test_room_to_summary_projects_current_room_state`

| Field | Content |
|---|---|
| **Target unit** | `Room.toSummary()` |
| **Inputs** | • `roomId = "R-01"`, `mode = DOUBLE`<br>• `numberOfPlayers = 1`, `slotCount = 2`<br>• `status = WAITING`, `ownerName = alice` |
| **Expected outputs** | • Returns `RoomSummary(R-01, DOUBLE, 1/2, WAITING, alice)`.<br>• Room state remains unchanged after projection. |
| **Mocked** | None — real in-memory `Room` fixture. |
| **Traces to** | UC-03.3 |

#### UT-20 — `test_db_rank_existing_player_returns_one_based_rank`

| Field | Content |
|---|---|
| **Target unit** | `db_rank(db, id, out_rank)` |
| **Inputs** | • Scores: `bob 1500`, `alice 1200`, `cara 900`.<br>• `playerId 17` belongs to `alice`. |
| **Expected outputs** | • Returns `DB_OK` with `rank = 2` (one-based).<br>• Does not change leaderboard scores or player records.<br>• Boundary: highest scorer ranks `1`. |
| **Mocked** | None — real DB fixture with deterministic scores. |
| **Traces to** | UC-03.4 |
| **Status** | **Implementable today.** |

---

### UC-04 — Create Room

#### UT-21 — `test_create_room_ui_success_shows_waiting_room`

| Field | Content |
|---|---|
| **Target unit** | `LobbyUI.confirmCreateRoom(mode)` |
| **Inputs** | • Authenticated `playerId = 17`, selected `mode = DOUBLE`. |
| **Expected outputs** | • Shows the Waiting Room with `"WAITING FOR OPPONENT"`.<br>• Stores the returned room id, slot `1` and `OWNER` role.<br>• Does not return to the Lobby error state. |
| **Mocked** | `HtttpClient.createRoom(17, DOUBLE)` → `created(roomId = R-01, slot = 1, role = OWNER)` |
| **Traces to** | UC-04.6 |

#### UT-22 — `test_create_room_client_maps_created_response`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.createRoom(playerId, mode)` |
| **Inputs** | `playerId = 17`, `mode = DOUBLE` |
| **Expected outputs** | • Sends `JOIN /room/<newId>` with header `Mode: DOUBLE` and `Player-Id: 17`.<br>• Returns `created(R-01, slot 1, OWNER)` to `LobbyUI`. |
| **Mocked** | `RoomController.create(17, DOUBLE)` → `201 Created` with room `R-01` and owner seating. |
| **Traces to** | UC-04.4 |

#### UT-23 — `test_room_controller_create_success_returns_201`

| Field | Content |
|---|---|
| **Target unit** | `RoomController.create(playerId, mode)` |
| **Inputs** | `playerId = 17`, `mode = DOUBLE` |
| **Expected outputs** | • `Lobby.createRoom` then `Room.seat` invoked exactly once each, **in that order**.<br>• Returns `201 Created`.<br>• The two narration messages ("joined the room", "set as owner") are requested **after** the `201` is produced. |
| **Mocked** | • `Lobby.createRoom(17, DOUBLE)` → `room R-01`<br>• `Room.seat(17)` → `seated(slot 1, OWNER)`<br>• `Room.narrate(text)` → broadcast ok (asserted 2 calls) |
| **Traces to** | UC-04.5, UC-04.7 |

#### UT-24 — `test_lobby_create_double_room_registers_two_slot_room` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `Lobby.createRoom(ownerId, mode)` |
| **Inputs** | • `ownerId = 17`, `mode = DOUBLE`<br>• No room with the generated id exists. |
| **Expected outputs** | • Returns and registers exactly one new `Room`.<br>• `slotCount == 2` and `minToStart == 2`, derived from `GameMode`.<br>• All slots initialise to `SlotStatus.WAITING`.<br>• Initial room status is `GameRoomStatus.WAITING`.<br>• `findRoom(newId)` subsequently returns the same instance. |
| **Mocked** | None — real `Room`, `Slot` and `GameMode` fixtures (no factory seam). |
| **Traces to** | UC-04.5 |

#### UT-25 — `test_room_seat_first_player_creates_owner_membership` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `Room.seat(playerId)` |
| **Inputs** | • Empty `DOUBLE` room with two `WAITING` slots.<br>• `playerId = 17`, creator seating path. |
| **Expected outputs** | • Slot 1 occupant is a `Membership` with `role == OWNER`, `playerId == 17`, `roomId == "R-01"`.<br>• Slot 1 status transitions `WAITING → JOINING → READY`.<br>• `numberOfPlayers == 1`.<br>• `recomputeStatus()` leaves the room `WAITING` (1 < `minToStart`).<br>• Slot 2 is untouched and still `WAITING`. |
| **Mocked** | None — real `Slot` and `Membership` fixtures; assert observable state. |
| **Traces to** | UC-04.5 |

#### UT-26 — `test_slot_occupy_assigns_membership_and_sets_ready`

| Field | Content |
|---|---|
| **Target unit** | `Slot.occupy(membership)` |
| **Inputs** | • Slot status `= JOINING`, `occupant = null`.<br>• A valid `Membership` is supplied. |
| **Expected outputs** | • Stores the `Membership` as occupant.<br>• Sets status to `READY`.<br>• Slot index is preserved.<br>• No other slot is modified. |
| **Mocked** | None — real `Slot` and `Membership` fixtures. |
| **Traces to** | UC-04.5 |

#### UT-27 — `test_membership_create_owner_preserves_identity_and_role`

| Field | Content |
|---|---|
| **Target unit** | `Membership(playerId, roomId, role)` construction |
| **Inputs** | `playerId = 17`, `roomId = "R-01"`, `role = OWNER` |
| **Expected outputs** | • `playerId` and `roomId` match the inputs.<br>• `isOwner()` returns `true`.<br>• `isMuted()` defaults to `false`.<br>• No unrelated room/player state is mutated. |
| **Mocked** | None — value/domain object test. |
| **Traces to** | UC-04.5 |

#### UT-28 — `test_room_seat_disconnect_resets_slot_to_waiting` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `Room.seat(playerId)` under a disconnect during join |
| **Inputs** | • Empty `DOUBLE` room; `playerId = 17`.<br>• The session drops after `setStatus(JOINING)` and before `occupy(...)`. |
| **Expected outputs** | • Slot 1 status returns to `WAITING`.<br>• Slot 1 occupant remains `null`.<br>• `numberOfPlayers` remains `0`.<br>• Seating reports failure rather than a partial seat. |
| **Mocked** | Session/connection probe → disconnected at the injected point. |
| **Traces to** | UC-04 ext 5a |

#### UT-29 — `test_room_controller_create_disconnect_destroys_room_and_returns_500` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `RoomController.create(playerId, mode)` |
| **Inputs** | `playerId = 17`, `mode = DOUBLE`; seating fails per UT-28. |
| **Expected outputs** | • `Lobby.destroyRoom(roomId)` invoked exactly once.<br>• Returns `500`.<br>• **No** narration is broadcast.<br>• A later `Lobby.findRoom(roomId)` returns `null` — no orphan room leaks. |
| **Mocked** | • `Lobby.createRoom(17, DOUBLE)` → `room R-01`<br>• `Room.seat(17)` → seating failed<br>• `Lobby.destroyRoom("R-01")` → destroyed<br>• `Room.narrate` → asserted 0 calls |
| **Traces to** | UC-04 ext 5a |

---

### UC-05 — Join Room from List

#### UT-30 — `test_join_from_list_room_full_shows_error`

| Field | Content |
|---|---|
| **Target unit** | `LobbyUI.joinHighlightedRoom(roomId)` |
| **Inputs** | Authenticated `playerId = 22`, `highlightedRoomId = "R-01"`. |
| **Expected outputs** | • Shows `"room full"` exactly once.<br>• Does not navigate to the Waiting Room.<br>• Keeps the Lobby usable and the highlight intact. |
| **Mocked** | `HtttpClient.joinRoom(22, R-01)` → `error(409, FULL)` |
| **Traces to** | UC-05 ext 4a |

#### UT-31 — `test_join_room_client_success_returns_room_view`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.joinRoom(playerId, roomId)` |
| **Inputs** | `playerId = 22`, `roomId = "R-01"` |
| **Expected outputs** | • Sends `JOIN /room/R-01` with `Player-Id: 22`.<br>• Returns `joined(room)` to `LobbyUI`.<br>• Does not map the success response to an error. |
| **Mocked** | `RoomController.join(22, R-01)` → `200 OK` with room, slots and `stateMessage`. |
| **Traces to** | UC-05.3 |

#### UT-32 — `test_room_controller_join_accepted_room_seats_player` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `RoomController.join(playerId, roomId)` |
| **Inputs** | `playerId = 22`, `roomId = "R-01"` |
| **Expected outputs** | • `Lobby.findRoom` → `Room.canAccept` → `Room.seat` invoked once each, in that order.<br>• Returns `200 OK` with the updated room view.<br>• Joined-room narration requested exactly once, after the `200`. |
| **Mocked** | • `Lobby.findRoom(R-01)` → `room`<br>• `Room.canAccept()` → `JoinVerdict.ACCEPTED` *(enum, not bool)*<br>• `Room.seat(22)` → `seated(slot 2, PLAYER, READY)`<br>• `Room.narrate(text)` → broadcast ok |
| **Traces to** | UC-05.4 |

#### UT-33 — `test_lobby_find_room_existing_id_returns_same_room`

| Field | Content |
|---|---|
| **Target unit** | `Lobby.findRoom(roomId)` |
| **Inputs** | `roomId = "R-01"`; Lobby contains room `R-01`. |
| **Expected outputs** | • Returns the **same** stored `Room` instance (identity, not a copy).<br>• Does not create, copy or remove a room.<br>• Room count is unchanged. |
| **Mocked** | None — real `Lobby` fixture. |
| **Traces to** | UC-05.4 |

#### UT-34 — `test_room_can_accept_full_room_returns_full_verdict` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `Room.canAccept()` |
| **Inputs** | • `numberOfPlayers == slotCount == 2`.<br>• `status != IN_GAME`. |
| **Expected outputs** | • Returns `JoinVerdict.FULL` *(enum, never a bool)*.<br>• No slot, membership, count or status is changed. |
| **Mocked** | None — real `Room` fixture. |
| **Traces to** | UC-05 ext 4a |

#### UT-35 — `test_room_can_accept_in_game_room_returns_in_game_verdict` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `Room.canAccept()` |
| **Inputs** | • `status == IN_GAME`.<br>• A free slot exists (`numberOfPlayers < slotCount`) — proving status is checked **before** occupancy. |
| **Expected outputs** | • Returns `JoinVerdict.IN_GAME`, not `ACCEPTED`.<br>• No slot or count is changed. |
| **Mocked** | None — real `Room` fixture. |
| **Traces to** | UC-05 ext 4b |

#### UT-36 — `test_room_controller_join_in_game_returns_409` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `RoomController.join(playerId, roomId)` |
| **Inputs** | `playerId = 22`, `roomId = "R-01"`, room is `IN_GAME`. |
| **Expected outputs** | • Returns `409 Conflict`.<br>• `Room.seat()` invoked **0** times.<br>• No narration is broadcast.<br>• Room state is unchanged. |
| **Mocked** | • `Lobby.findRoom(R-01)` → `room`<br>• `Room.canAccept()` → `JoinVerdict.IN_GAME`<br>• `Room.seat` / `Room.narrate` → asserted 0 calls |
| **Traces to** | UC-05 ext 4b |

#### UT-37 — `test_join_slot_occupy_player_membership_sets_ready`

| Field | Content |
|---|---|
| **Target unit** | `Slot.occupy(membership)` |
| **Inputs** | • A free slot in `JOINING` state.<br>• `Membership` with `role = PLAYER`. |
| **Expected outputs** | • Stores the `PLAYER` membership.<br>• Slot status becomes `READY`.<br>• The owner slot is unchanged. |
| **Mocked** | None — real `Slot` and `Membership` fixtures. |
| **Traces to** | UC-05.4 |

#### UT-38 — `test_join_membership_created_with_player_role`

| Field | Content |
|---|---|
| **Target unit** | `Membership(playerId, roomId, role)` construction |
| **Inputs** | `playerId = 22`, `roomId = "R-01"`, `role = PLAYER` |
| **Expected outputs** | • `role == PLAYER`; `isOwner()` returns `false`.<br>• Player and room identifiers match the inputs. |
| **Mocked** | None — value/domain object test. |
| **Traces to** | UC-05.4 |

---

### UC-06 — Join Room by Room ID

#### UT-39 — `test_join_by_id_unknown_room_shows_error_and_refocuses`

| Field | Content |
|---|---|
| **Target unit** | `LobbyUI.joinTypedRoomId(text)` |
| **Inputs** | Authenticated `playerId = 22`, typed room id `= "UNKNOWN"`. |
| **Expected outputs** | • Shows `"no such room"`.<br>• Focus returns to the room-id entry field.<br>• Does not navigate to the Waiting Room. |
| **Mocked** | `HtttpClient.joinRoom(22, "UNKNOWN")` → `error(404 Not Found)` |
| **Traces to** | UC-06 ext 4a |

#### UT-40 — `test_join_by_id_client_maps_not_found_response`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.joinRoom(playerId, typedRoomId)` |
| **Inputs** | `playerId = 22`, `typedRoomId = "UNKNOWN"` |
| **Expected outputs** | • Returns `error(404)` to `LobbyUI`.<br>• Does not fabricate a room view. |
| **Mocked** | `RoomController.join(22, "UNKNOWN")` → `404 Not Found` |
| **Traces to** | UC-06 ext 4a |

#### UT-41 — `test_join_by_id_controller_null_room_returns_404`

| Field | Content |
|---|---|
| **Target unit** | `RoomController.join(playerId, roomId)` |
| **Inputs** | `playerId = 22`, `roomId = "UNKNOWN"` |
| **Expected outputs** | • Returns `404 Not Found`.<br>• `Room.canAccept()` and `Room.seat()` invoked **0** times.<br>• No lobby or room state is mutated. |
| **Mocked** | `Lobby.findRoom("UNKNOWN")` → `null` |
| **Traces to** | UC-06 ext 4a |

#### UT-42 — `test_lobby_find_room_unknown_id_returns_null`

| Field | Content |
|---|---|
| **Target unit** | `Lobby.findRoom(roomId)` |
| **Inputs** | `roomId = "UNKNOWN"`; Lobby does not contain that id. |
| **Expected outputs** | • Returns `null`.<br>• The room map remains unchanged (no negative-cache insertion). |
| **Mocked** | None — real `Lobby` fixture. |
| **Traces to** | UC-06 ext 4a |

#### UT-43 — `test_join_by_id_room_seat_updates_count_and_status` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `Room.seat(playerId)` |
| **Inputs** | • Acceptable `DOUBLE` room with one free slot, one seated owner.<br>• `playerId = 22`; adding this player reaches `minToStart`. |
| **Expected outputs** | • Seats the player in the **first free slot** as `PLAYER`.<br>• `numberOfPlayers` increments exactly once (1 → 2).<br>• `recomputeStatus()` moves room status `WAITING → READY`.<br>• The owner's slot and membership are untouched. |
| **Mocked** | None — real `Slot` and `Membership` fixtures (no factory seam). |
| **Traces to** | UC-06.4, UC-05.4 |

---

### UC-07 — Leave Room (includes UC-07a)

#### UT-44 — `test_leave_ui_success_returns_to_lobby`

| Field | Content |
|---|---|
| **Target unit** | `WaitingRoomUI.pressLeave()` |
| **Inputs** | `playerId = 22`, `roomId = "R-01"` |
| **Expected outputs** | • Shows the Lobby after the leave succeeds.<br>• Clears the current waiting-room view state.<br>• Does not show a leave error. |
| **Mocked** | `HtttpClient.leaveRoom(22, R-01)` → `left()` / `200 OK` |
| **Traces to** | UC-07.5 |

#### UT-45 — `test_leave_client_not_found_returns_error`

| Field | Content |
|---|---|
| **Target unit** | `HtttpClient.leaveRoom(playerId, roomId)` |
| **Inputs** | `playerId = 22`, `roomId = "R-01"` |
| **Expected outputs** | • Returns `error(404)` to `WaitingRoomUI`.<br>• Does not return `left()`. |
| **Mocked** | `RoomController.leave(22, R-01)` → `404 Not Found` |
| **Traces to** | UC-07 ext |

#### UT-46 — `test_room_controller_leave_member_releases_and_returns_200`

| Field | Content |
|---|---|
| **Target unit** | `RoomController.leave(playerId, roomId)` |
| **Inputs** | `playerId = 22` is a member of `R-01`. |
| **Expected outputs** | • `Room.release(22)` invoked exactly once.<br>• Returns `200 OK`.<br>• Left-room narration invoked exactly once, **after** the `200` (ordering asserted per `SD UC-07`). |
| **Mocked** | • `Lobby.findRoom(R-01)` → `room`<br>• `Room.release(22)` → released<br>• `Room.narrate(text)` → broadcast ok |
| **Traces to** | UC-07.3, UC-07.4 |

#### UT-47 — `test_lobby_destroy_last_player_room_removes_room`

| Field | Content |
|---|---|
| **Target unit** | `Lobby.destroyRoom(roomId)` |
| **Inputs** | `roomId = "R-01"`; `R-01` exists and has no players after release. |
| **Expected outputs** | • Removes `R-01` from the room map.<br>• Clears slots and tears down chat exactly once.<br>• A later `findRoom("R-01")` returns `null`.<br>• Other rooms in the lobby are unaffected. |
| **Mocked** | Room teardown/chat cleanup boundary → `ROOM_DESTROYED`. |
| **Traces to** | UC-07 ext 3b |

#### UT-48 — `test_room_release_owner_promotes_successor_and_clears_slot` *(revised)*

| Field | Content |
|---|---|
| **Target unit** | `Room.release(playerId)` |
| **Inputs** | • The leaving player owns slot 1.<br>• Another `PLAYER` remains in the next occupied slot. |
| **Expected outputs** | • `selectSuccessor()` returns the next occupied membership in slot order.<br>• Successor role becomes `OWNER`.<br>• `moveToVacatedSlot(successor)` places the successor in the vacated owner slot.<br>• Owner slot transitions `LEAVING → WAITING` with `occupant == null`.<br>• `numberOfPlayers` decrements exactly once; `recomputeStatus()` runs after. |
| **Mocked** | None — real `Slot` and `Membership` fixtures; assert observable state. |
| **Traces to** | UC-07 ext 3a, UC-07a.1 |

#### UT-49 — `test_room_release_owner_broadcasts_update_and_narrates` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `Room.release(playerId)` — broadcast side of UC-07a |
| **Inputs** | Owner leaves a `DOUBLE` room with one remaining `PLAYER`. |
| **Expected outputs** | • `broadcastRoomUpdate(newOwner)` invoked exactly once with the successor's id.<br>• `narrate("PLAYER <name> set as the owner")` invoked exactly once.<br>• Broadcast occurs **after** the role change, so viewers never see a room with no owner. |
| **Mocked** | Broadcast/narration boundary — call order and arguments recorded. |
| **Traces to** | UC-07a, `SD UC-07` lines 1567–1568 |

#### UT-50 — `test_room_release_successor_disconnected_picks_next_in_slot_order` *(new)*

| Field | Content |
|---|---|
| **Target unit** | `Room.selectSuccessor()` |
| **Inputs** | • Owner leaving a Battle Royale room with three remaining memberships.<br>• The first candidate in slot order is disconnected. |
| **Expected outputs** | • Skips the disconnected candidate and returns the next connected membership in slot order.<br>• Returns `null` only when no connected candidate remains.<br>• No membership role is changed by the selection itself. |
| **Mocked** | Connection-state probe per membership. |
| **Traces to** | UC-07a.1b |

#### UT-51 — `test_slot_clear_data_removes_occupant_and_sets_waiting`

| Field | Content |
|---|---|
| **Target unit** | `Slot.clearData()` |
| **Inputs** | • Slot status `= LEAVING`, containing the leaving player's `Membership`. |
| **Expected outputs** | • Sets `occupant` to `null`.<br>• Sets status to `WAITING`.<br>• Preserves the slot index. |
| **Mocked** | None — real `Slot` fixture. |
| **Traces to** | UC-07.3 |

#### UT-52 — `test_successor_membership_set_role_promotes_to_owner`

| Field | Content |
|---|---|
| **Target unit** | `Membership.setRole(role)` |
| **Inputs** | Membership `role = PLAYER`; requested `role = OWNER`. |
| **Expected outputs** | • Role becomes `OWNER`; `isOwner()` returns `true`.<br>• `playerId` and `roomId` remain unchanged. |
| **Mocked** | None — real `Membership` fixture. |
| **Traces to** | UC-07a.1 |

---

## 8. Implementation and Execution Timeline

| Phase | Duration | Build | Test work | Gate |
|---|---|---|---|---|
| **0 — Test harness** | 3 days | Shared fake-DB, fake-session and call-count assertion helpers; extend the Unity setup in `src/tetrish/tests/unity/` | UT-04–06, UT-10, UT-13, UT-20 | 6 tests green under Valgrind |
| **1 — Room domain** | 1 week | `Room`, `Slot`, `Membership`, `Lobby`, `GameMode` as a **pure library** — no sockets, mirroring the `libtetrisbrain` no-I/O rule | UT-18, 19, 24–28, 33–35, 37, 38, 42, 43, 47–52 (**20 tests**, all real objects) | Domain green; highest-value block, zero network dependencies |
| **2 — Controllers** | 1 week | `AuthController`, `LobbyController`, `RoomController` against the fake DB | UT-03, 07, 11, 12, 16, 17, 23, 29, 32, 36, 41, 46 (**12 tests**) | Controllers green; UT-12 must pass before login ships |
| **3 — Client boundary** | 4 days | `HtttpClient` + UI state objects in `tetrisu` | Client: UT-02, 09, 15, 22, 31, 40, 45 (7)<br>UI: UT-01, 08, 14, 21, 30, 39, 44 (7) | **All 52 unit tests green** |
| **4 — Integration L2/L3** | 1 week | Wire `libtetrissh` + `libhtttp` into the dispatcher | IT-01 … IT-12; DB reopen/recovery; frame round-trip | Full request path exercised |
| **5 — System L4/L5** | 1 week | Headless HTTTP test client; `tetrislogd` capture stub | ST-UC01 … ST-UC07; logger restart/drop (IT-13, IT-14) | Mandatory use-case paths pass on a fresh checkout |
| **6 — Hardening** | ongoing from Phase 1 | — | AFL++ on parser/frame/DB recovery; Helgrind/TSan; `gcov` branch review of `alt` paths | §4.2 exit criteria met |

