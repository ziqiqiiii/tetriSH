# docs/requirements.md — Full Requirements and Sprint Plan

> **Usage:** Reference when implementing a specific feature or reviewing scope. Not loaded on every agent request.

---

## tetriSH baseline MVP requirements

Must be demonstrable to pass (zero risk otherwise):

| # | Requirement |
|---|-------------|
| 1 | Player can connect using `tetrisu` |
| 2 | Client completes the secure session handshake before any HTTTP traffic |
| 3 | Player can JOIN a room |
| 4 | Player can START a game |
| 5 | Server creates and maintains game state |
| 6 | A Tetris piece falls over time |
| 7 | Player can move the piece left and right |
| 8 | Player can rotate the piece |
| 9 | Player can soft drop and hard drop |
| 10 | Pieces lock when they reach the bottom or collide with existing blocks |
| 11 | Completed lines are cleared |
| 12 | Score or line count updates after line clears |
| 13 | Game detects game over |
| 14 | Server sends STATE messages to the client |
| 15 | `tetrisu` renders the board in the terminal |
| 16 | Client can quit cleanly |
| 17 | Server logs game events through `tetrislogd` |

---

## TetriSocial functional requirements

### chatd

| ID | Requirement |
|----|-------------|
| FR-C1 | Accept multiple concurrent TCP connections using `libtetrissh` RSA-PSS + AES-256 |
| FR-C2 | Parse and dispatch: `CHAT`, `JOIN`, `LEAVE`, `ABILITY` |
| FR-C3 | Per-game-room broadcast: message from one client forwarded to all others in the room |
| FR-C4 | Consume `game_event_t` from tetrisd via `SOCK_DGRAM`; format into human-readable system messages and broadcast |
| FR-C5 | Create/destroy rooms on `GE_GAME_STARTED` / `GE_GAME_ENDED` from tetrisd |
| FR-C6 | Per-session rate limiting via token-bucket in `libchatcore`; excess messages silently dropped |
| FR-C7 | Three roles: `player`, `spectator`, `admin`; assigned at join |
| FR-C8 | Log to `tetrislogd` via `SOCK_DGRAM` fire-and-forget |
| FR-C9 | Unix `SOCK_STREAM` control plane for `chatctl`: `mute`, `kick`, `broadcast`, `room-list` |

### marketd

| ID | Requirement |
|----|-------------|
| FR-M1 | Consume `POINTS_EARNED` and `ABILITY_USED` events from tetrisd via `SOCK_DGRAM`; credit player balances atomically |
| FR-M2 | Points ledger in `libmarketcore`, protected by mutex for concurrent R/W |
| FR-M3 | Serve tetrisu marketplace TUI via Unix `SOCK_STREAM`: `browse`, `purchase`, `equip` |
| FR-M4 | Player inventory as a bitfield per player; equipped loadout (`theme`, `character`, `ability`) as a separate selection record |
| FR-M5 | Validate ownership + equipped state before tetrisd honours an `ABILITY` activation |
| FR-M6 | Persist ledger + inventory to append-only binary ledger file; replay full ledger on startup |
| FR-M7 | Unix `SOCK_STREAM` control plane for `marketctl`: `award`, `deduct`, `inventory`, `reset` |

### Gaiden abilities

| ID | Ability | Cost | Mechanic |
|----|---------|------|----------|
| FR-A1 | Garbage Surge | 600 pts | `tetrisd` sends +3 garbage lines to target room via Battle Royale mq |
| FR-A2 | Shield | 800 pts | `tetrisd` sets `player.shield_ticks = N`; ticker skips garbage injection while `shield_ticks > 0` |
| FR-A3 | Freeze | 1000 pts | `tetrisd` sets `player.frozen = true`; client_thread drops MOVE requests for 2s then clears flag |

### tetrisu extensions

| ID | Requirement |
|----|-------------|
| FR-U1 | Full-screen split-panel notcurses marketplace from lobby; three TAB-navigable panes: Balance/Stats, Store, Loadout |
| FR-U2 | Balance pane: current points, rank, games played, wins |
| FR-U3 | Store pane: purchasable items with costs; `[B] Buy` |
| FR-U4 | Loadout pane: equipped theme/character/ability; `[E] Equip`, `[U] Unequip`, `[P] Preview` |
| FR-U5 | Split chat panel alongside game board during active sessions |

---

## Non-functional requirements

### Concurrency

| ID | Requirement |
|----|-------------|
| NFR-CO1 | No data races. All shared structures must be protected by a documented mutex. Locking strategy written in README. |
| NFR-CO2 | No mutex held across a blocking syscall. Lock → copy to local buffer → unlock → syscall. |
| NFR-CO3 | Deadlock prevention by lock ordering. Multiple locks always acquired in globally consistent order (ascending pointer address). Documented and enforced. |
| NFR-CO4 | Non-blocking IPC from tetrisd. All `SOCK_DGRAM` sends to `chatd.sock` and `marketd.sock` use `MSG_DONTWAIT`. Game loop never stalled by social layer. |
| NFR-CO5 | Lock-free log shipping. `libcoreipc` ring buffer. Non-blocking push; drop counter observable. |
| NFR-CO6 | Atomic ledger credit. `player_ledger_t` mutex held for full read-modify-write. |

### Reliability

| ID | Requirement |
|----|-------------|
| NFR-R1 | Game never blocked by social layer. If `chatd`/`marketd` not running, `tetrisd` continues normally. |
| NFR-R2 | `chatd` survives slow/dead client. Broadcast is write-nonblocking; slow client is kicked, not waited on. |
| NFR-R3 | Ledger durability. Append-only binary file is authoritative. Replay on startup. No points lost across restarts. |
| NFR-R4 | No memory leaks or undefined behaviour. All code paths clean under `valgrind --leak-check=full --error-exitcode=1`. |
| NFR-R5 | Graceful shutdown. `chatd` and `marketd` handle `SIGTERM`: stop listener, drain ring buffer to tetrislogd, flush ledger file, exit. |
| NFR-R6 | `tetrislogd` reconnect. If tetrislogd unreachable, daemons continue. Ring buffer drops incremented. Resumes automatically when tetrislogd comes back. |

### Performance

| ID | Requirement |
|----|-------------|
| NFR-P1 | Chat latency ≤ 50 ms end-to-end on localhost, up to 16 concurrent clients per room |
| NFR-P2 | Marketplace response ≤ 20 ms (browse/purchase via Unix SOCK_STREAM) |
| NFR-P3 | Points credit reflected in ledger within one event_consumer_thread tick |
| NFR-P4 | Rate limiter overhead < 1 µs per check; no per-bucket mutex on hot path |
| NFR-P5 | `marketd` startup: complete ledger replay within 2 seconds for ≤ 10,000 records |

---

## Sprint plan

| Sprint | Week | Goal | Sanjan | Zi Qi | Both |
|--------|------|------|--------|-------|------|
| S0 | 3 | Setup | `game_event.h` draft; `Makefile` skeleton | `certs/gen_test_certs.sh`; CA + cert generation | `common_types.h`; `.tetrishrc.example` |
| S1 | 4 | Corestack foundations | `libcoreipc`: ring_buffer, mq_helpers, unix_socket | `libtetrissh`: handshake, session (both ends) | `libhtttp`: parser, serialiser |
| S2 | 5 | Shell + chatd skeleton | `tetrish`: REPL, builtins, rc_parser, process (dspawn/dcheck) | `libhtttp`: dispatch; `chatd/client.c` handshake | `game_event.h` schema frozen; `chatd/main.c` + listener |
| S3 | 6 | tetrisd scaffolding | `tetrisd`: main, listener, client, logshipper, signal_handler | `tetrisd`: ctl_listener; `tetrislogd`; `tetrisctl` | `tetrisd`: room.c skeleton; JOIN/LEAVE/START dispatch |
| S4 | 7 | Game loop end-to-end | `tetrisd`: ticker, room_t mutex, STATE broadcast | `libtetrisbrain`: board, pieces, gravity, lineclear, scoring | First playable session: two tetrisu clients, pieces falling, lines clearing |
| S5 | 8 | Chat full + market scaffold | `chatd`: room_registry, event_consumer; `libchatcore` full; `chatctl` | `marketd`: main, client; `libmarketcore`: points, inventory | `event_pub.c` wired into tetrisd; chatd narration broadcasting |
| S6 | 9 | Market full + ability logic | `tetrisd`: ability, loadout, event_pub; ability flags in room_t | `marketd`: ledger, catalogue, loadout_server; `marketctl` | Ability full-stack: Freeze, Shield, Garbage Surge activate end-to-end |
| S7 | 10 | Battle Royale + tetrisu TUI | `tetrisd/garbage.c` POSIX MQ integration | `libmarketcore` ledger persistence; `marketd/persist_thread` | `tetrisu`: chat_net, render_chat, render_market |
| S8 | 11 | Integration hardening | IPC pipeline integration tests; signal shutdown paths; valgrind baseline | HTTTP parser edge-case tests; loadout sync; helgrind on marketd | `test_game_event.c`; full-stack integration test |
| S9 | 12 | System tests + concurrency | helgrind on chatd; broadcast race tests; chatroom lifecycle stress | Ledger atomicity under concurrent threads; test_market_points.c | `tests/` complete; README; `docs/architecture.md` |
| S10 | 13 | Checkoff | Demo dry-run; live extension practice | `docs/threat_model.md` | Tagged `v1.0-br`; clean build from fresh checkout verified |

### Release tags

| Tag | When | What |
|-----|------|------|
| `v0.1-foundation` | End S2 | Corestack libs compiling and tested |
| `v0.5-mvp` | End S5 | tetriSH baseline demo-able |
| `v1.0-br` | End S8 | Battle Royale + full TetriSocial stack functional |

---

## Test pyramid

```
make test-unit          # libtetrisbrain, libhtttp, libchatcore, libmarketcore
make test-integration   # game_event.h pipeline across real SOCK_DGRAM sockets
make test-system        # full clean build + scripted tetrisu sessions
make test               # all three in order
```

Each library is self-contained and also runs its own unit tests standalone
(`make -C lib/libtetrisbrain test`, etc.); the umbrella `make test-unit` simply
recurses into each library's test target. All tiers must pass under
`valgrind --leak-check=full --error-exitcode=1`.

| File | Tier | Covers |
|------|------|--------|
| `test_tetrisbrain.c` | Unit | board ops, SRS rotation, line clear, scoring, ability transforms |
| `test_htttp_parser.c` | Unit + Whitebox | all parser paths, all error branches, edge-case frames |
| `test_game_event.c` | Integration | publish → consume pipeline across real SOCK_DGRAM sockets |
| `test_market_points.c` | Unit + Concurrency | ledger correctness, inventory, concurrent credit/debit |
| `test_chatcore.c` | Unit | token-bucket, room lifecycle, role management |

---

## Binary linkage reference

Each corestack library below is a self-contained directory producing
`lib/libXXX/libXXX.a`. A binary links the archives it needs and adds each library's
header path (`-I lib/libXXX/include`); the `-l*` entries are external system libs.

| Binary | Libraries |
|--------|-----------|
| `tetrish` | *(none from corestack)* |
| `tetrisd` | `libtetrissh libhtttp libtetrisbrain libcoreipc -lssl -lcrypto -lm -lpthread` |
| `tetrislogd` | `-lpthread` |
| `tetrisctl` | *(none from corestack)* |
| `tetrisu` | `libtetrissh libhtttp libcoreipc -lssl -lcrypto $(pkg-config --libs notcurses) -lpthread` |
| `chatd` | `libtetrissh libhtttp libcoreipc libchatcore -lssl -lcrypto -lpthread` |
| `chatctl` | *(none from corestack)* |
| `marketd` | `libtetrissh libhtttp libcoreipc libmarketcore -lssl -lcrypto -lsqlite3 -lpthread` |
| `marketctl` | *(none from corestack)* |
