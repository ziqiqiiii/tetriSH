# docs/corestack.md — Full Project Reference

> **This is the deep reference document for the CoreStack Challenge.**
> It contains the full architecture, all requirements, the complete IPC design,
> the Tetris Battle Gaiden mechanic specs, the live Q&A question pool with
> preparation notes, the sprint plan, and all code documentation standards.
>
> **When to read this:** When the user asks a specific project question,
> when you need to understand a requirement in detail, when you're about to
> implement a new component and want the full spec, or when `AGENTS.md`
> references a concept and you need more depth.
>
> **Do not load this on every request.** Read `AGENTS.md` first.
> Open this file with `cat docs/corestack.md` when you need the detail.

---

## Mac Mini Team · SUTD 50.005 × 50.003

| Name              | Student ID |
|-------------------|------------|
| Sanjan Krishna Sarat | 1009153 |
| Thong Zi Qi       | 1009160  |

**Repository:** https://github.com/ziqiqiiii/tetriSH.git

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [The CoreStack Challenge — What It Is](#2-the-corestack-challenge--what-it-is)
3. [tetriSH — 50.005 Component](#3-tetrish--50005-component)
   - [Architecture Layers](#31-architecture-layers)
   - [Required Binaries](#32-required-binaries)
   - [Required Libraries](#33-required-libraries)
   - [The HTTTP Protocol](#34-the-htttp-protocol)
   - [The Secure Session (libtetrissh)](#35-the-secure-session-libtetrissh)
   - [Concurrency Model](#36-concurrency-model)
   - [Battle Royale Mode](#37-battle-royale-mode)
   - [Logging Architecture](#38-logging-architecture)
   - [The .tetrishrc Config File](#39-the-tetrishrc-config-file)
   - [Grading, Checkoff, and Prize](#310-grading-checkoff-and-prize)
4. [TetriSocial — 50.003 Component](#4-tetrisocial--50003-component)
   - [Overview](#41-overview)
   - [chatd — Chat Daemon](#42-chatd--chat-daemon)
   - [marketd — Marketplace Daemon](#43-marketd--marketplace-daemon)
   - [Domain Libraries](#44-domain-libraries)
   - [Shared Event Contract (game_event.h)](#45-shared-event-contract-game_eventh)
   - [tetrisu Extensions](#46-tetrisu-extensions)
5. [Tetris Battle Gaiden — Character & Ability System](#5-tetris-battle-gaiden--character--ability-system)
6. [IPC Architecture — Full System](#6-ipc-architecture--full-system)
7. [Thread Model](#7-thread-model)
8. [Shared CoreStack Library Boundaries](#8-shared-corestack-library-boundaries)
9. [Role Declarations](#9-role-declarations)
10. [Sprint Plan](#10-sprint-plan)
11. [Testing Strategy](#11-testing-strategy)
12. [Build System](#12-build-system)
13. [Memory Safety](#13-memory-safety)
14. [File Structure](#14-file-structure)
15. [Key Design Decisions & Rationale](#15-key-design-decisions--rationale)
16. [Live Q&A Readiness — Question Pool](#16-live-qa-readiness--question-pool)
17. [Out of Scope](#17-out-of-scope)
18. [Code Documentation Standards](#18-code-documentation-standards)
19. [How to Explain Changes to the Team](#19-how-to-explain-changes-to-the-team)

---

## 1. Project Overview

This repository implements the **CoreStack Challenge** — a competitive alternative track jointly offered by two SUTD courses:

- **50.005 (Computer Systems Engineering)** — the `tetriSH` component: a full terminal-based Battle Royale Tetris system built in C, replacing PA1 (shell + daemon + IPC) and PA2 (authenticated, confidential client-server communication) with one large integrated system.
- **50.003 (Elements of Software Construction)** — the `TetriSocial` component: a live multiplayer chat layer and a points-based in-game marketplace built as independent C daemons that share the same corestack libraries.

Both courses are served by a **single shared repository**. Each library is a self-contained directory that builds its own `libXXX.a` (with its own `Makefile`, `src/`, `include/`, `tests/`); a top-level umbrella `Makefile` recurses into them and links the archives into the daemons. Both components share a common corestack of reusable C libraries (`libtetrissh`, `libhtttp`, `libtetrisbrain`, `libcoreipc`).

This is **not a bonus project**. On the 50.005 side it replaces PA1 and PA2 entirely, with the same mark ceiling. There are no extra marks for attempting it — only a chance at a hardware prize (up to S$3,000 in Apple products for the top groups). On the 50.003 side it runs under the normal project rubric.

---

## 2. The CoreStack Challenge — What It Is

### What "CoreStack" means

A CoreStack group builds one **shared systems core** (the corestack libraries: shell, concurrency utilities, IPC primitives, secure session, logging client) and uses it to deliver **two separate applications**:

- The **50.005 application** is always `tetriSH`.
- The **50.003 application** is the group's choice — ours is `TetriSocial`.

### Eligibility and commitment rules

- Groups of exactly 3 (our team has 2 — approved exception).
- All members must be enrolled in **both** 50.003 and 50.005.
- Intent declared by **Week 3, Monday 6 PM** to both instructors.
- Withdrawal deadline: **Week 3, Friday 6 PM** — after that, the group is fully committed.
- Failing to deliver the MVP at the final checkoff results in **0 marks for both PA1 and PA2** on the 50.005 side.

### Mark tiers (50.005)

| Outcome | Requirement |
|---------|-------------|
| Avoid zero | Baseline MVP must work |
| Complete tetriSH properly | MVP + Battle Royale must work |
| Good marks | Correct, clean, documented, understood; pass live Q&A and live patching |
| Prize eligible | Full required system + strong engineering + flawless Q&A + shippable cohort bundle + credible git history |

### Live checkoff structure

The final checkoff is 60–90 minutes and consists of four phases:

1. **Baseline demo** — run from a clean checkout, demonstrate full system
2. **Systems inspection** — instructor inspects IPC behaviour, failure handling, live logs; plays the game
3. **Live extension** — 20 minutes to implement a small extension from a task pool; 5 minutes to walk the diff
4. **Live Q&A** — each member answers questions about their declared role; compound multiplier applied for weak/missed answers

---

## 3. tetriSH — 50.005 Component

### 3.1 Architecture Layers

The system sits in exactly **three layers above the kernel**:

```
+---------------------------------------------+
|  Application: HTTTP messages                 |
|  (HyperText Tetris Transfer Protocol)        |
+---------------------------------------------+
|  Secure session                              |
|  (cert auth, RSA-wrapped AES, framed)        |
+---------------------------------------------+
|  Transport: TCP via POSIX sockets            |
+---------------------------------------------+
```

TCP is provided by the kernel. We implement the upper two layers ourselves. No TLS, no SSL_* API, no reverse proxy — the secure session is built from scratch in `libtetrissh` using OpenSSL primitives from PA2's `common.c`.

---

### 3.2 Required Binaries

| Binary | Role |
|--------|------|
| `tetrish` | Interactive shell — entry point; reads `.tetrishrc`, launches daemons in background, runs `tetrisctl`, starts `tetrisu` |
| `tetrisd` | Concurrent game server — the core of the system; contains the game loop, room management, player state, signal handling, IPC to logger and control plane |
| `tetrislogd` | Dedicated logger daemon — separate process; receives log records from `tetrisd` over IPC and writes them to disk |
| `tetrisctl` | Admin CLI — communicates with a running `tetrisd` over a local IPC control plane (not the public TCP port) |
| `tetrisu` | Terminal-based game client — connects via TCP, establishes secure session, sends HTTTP game actions, renders server-pushed STATE frames in ncurses |

#### tetrish: the shell

All PA1 shell behaviour is required:
- REPL with `fork()` + `execvp()`
- Builtins: `cd`, `help`, `exit`, `usage`, `env`, `setenv`, `unsetenv`
- `.tetrishrc` execution on startup (runs every line as a shell command)
- Background process tracking: `sys`, `dspawn`, `dcheck`
- No crashes on bad input

Our `.tetrishrc` launches `tetrislogd` and `tetrisd` in the background via `dspawn`, then is queryable via `dcheck`. We also add custom builtins like `tetris-up` (one-shot launch of both daemons) and `tetris-status` (wrapper around `tetrisctl status`).

#### tetrisd: the game daemon

When launched in background from `tetrish`, it must:
- Detach from the controlling terminal (full daemon detach via double-fork)
- Bind to the TCP port configured in `.tetrishrc`
- Accept multiple concurrent clients
- Establish a secure session (via `libtetrissh`) with each client before any HTTTP traffic
- Parse and serialise HTTTP messages (via `libhtttp`)
- Maintain rooms with multiple players, run game logic (via `libtetrisbrain`), broadcast STATE
- Handle `SIGTERM` (graceful shutdown), `SIGHUP` (reload config), `SIGUSR1` (dump state to log)
- Ignore `SIGPIPE`; detect broken connections via `write()` returning `EPIPE`
- Forward all log records to `tetrislogd` over IPC, non-blocking from game-critical threads
- Expose a control plane to `tetrisctl`

#### tetrislogd: the logger daemon

`tetrislogd` is a **separate process**, not a thread. Required behaviour:
- Accept log records from `tetrisd` over IPC
- Write records to the log file from `.tetrishrc`
- Maintain a "dropped records" counter; emit periodic summaries (`dropped 47 records in last 30s`)
- Handle `SIGTERM` (flush, close, exit) and `SIGHUP` (reopen log file for log rotation)
- Survive a `tetrisd` restart without dying (accept reconnections)

Design rationale:
- **Separation of failure domains** — a bug in the logger cannot bring down the game daemon
- **Real IPC requirement** — forces a real producer-consumer IPC channel
- **Production realism** — mirrors how `systemd-journald` and `rsyslogd` work

Our IPC choice: **Unix domain socket (`SOCK_DGRAM`)**. Datagram semantics preserve record boundaries; `MSG_DONTWAIT` gives non-blocking drops; the socket survives `tetrislogd` restarts because `tetrisd` simply retries the send on the next record.

#### tetrisctl: the admin CLI

- A separate binary communicating with `tetrisd` via a real IPC mechanism (not the public TCP socket)
- **Must remain available even when the public TCP listener is saturated** — this is why it uses a separate Unix `SOCK_STREAM` socket (`ctl_listener_thread`)
- Required commands: `status`, `shutdown`
- Our additional commands: `rooms`, `players`, `kick <player>`, `reload` (sends SIGHUP), `log-level <level>`, `dropped-logs`

#### tetrisu: the game client

Required behaviour:
- Connect via TCP, complete the secure session handshake (via `libtetrissh`)
- Send HTTTP requests for game actions (via `libhtttp`)
- Receive and render server-pushed STATE frames
- Handle keyboard input non-blocking (so input and network reading happen simultaneously)
- Exit cleanly on `q` or `SIGINT`

Our rendering: **ncurses** with Unicode half-block characters (`▀`) and 24-bit ANSI true colour. Character/ability animations are rendered as pixel-art-style sprites. For SSH sessions where `COLORTERM` may not propagate, we gracefully degrade to 256-colour mode.

---

### 3.3 Required Libraries

| Library | Role |
|---------|------|
| `libtetrissh` | Secure session: cert auth, RSA-wrapped AES, framing |
| `libhtttp` | HTTTP protocol parser and serialiser |
| `libtetrisbrain` | Tetris game logic: board, pieces, gravity, line clear |

All libraries are **statically linked** into the binaries that need them. Each has one responsibility with clean public headers.

---

### 3.4 The HTTTP Protocol

**HTTTP (HyperText Tetris Transfer Protocol)** is HTTP-like protocol. Every byte flows inside an encrypted session established by `libtetrissh`.

#### Wire format

Every HTTTP message is wrapped in a session frame: `[4-byte big-endian length][AES-256 ciphertext]`. Frame size limit: **64 KiB**. Larger messages must be split or rejected with `413 Payload Too Large`.

#### Required HTTTP methods

| Method | Path | Notes |
|--------|------|-------|
| `JOIN` | `/arena/<name>` | Client joins a room |
| `START` | `/room/<id>` | Start a game |
| `MOVE` | `/room/<id>/player/<pid>` | Body: `LEFT` or `RIGHT` |
| `ROTATE` | `/room/<id>/player/<pid>` | Body: `CW` or `CCW` |
| `DROP` | `/room/<id>/player/<pid>` | Body: `SOFT` or `HARD` |
| `STATE` | `/room/<id>` | Server-originated; pushed broadcast of board state |

`STATE` is the only server-originated message. Clients must read these unprompted while interleaving with their own request-response cycles.

We also add `CHAT`, `ABILITY`, and `SPECTATE` methods for TetriSocial integration.

#### Required status codes

`200, 201, 400, 401, 403, 404, 409, 429, 500`

#### Required headers

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server STATE broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

#### Sample messages

```
JOIN /arena/main HTTTP/1.0
Host: tetrish.local
User: alice
Mode: battle-royale
Client-Version: 1.0
Content-Length: 35

{"skin":"cyan","start_level":0}
```

```
HTTTP/1.0 200 OK
Session-Id: s-8f31a2
Player-Id: p17
Tick-Rate: 20
Content-Type: application/json
Content-Length: 94

{"arena":"main","player_id":"p17","board_width":10,"board_height":20,"next_tick":48122}
```

---

### 3.5 The Secure Session (libtetrissh)

Every byte of HTTTP traffic flows inside an authenticated, confidential session. The session protocol follows the PA2 pattern exactly:

1. Client connects, sends a fresh nonce
2. Server sends its X.509 certificate
3. Client verifies the certificate against the bundled CA (`cacsertificate.crt`)
4. Server signs the client nonce with its private key (**RSA-PSS**)
5. Client verifies the signature using the public key from the certificate
6. Client generates a 32-byte AES-256 session key, **RSA-OAEP** encrypts it with the server's public key, sends it
7. From this point on, every frame is `[4-byte big-endian length][AES ciphertext]` carrying one HTTTP message

All cryptographic primitives come from PA2's `common.c`. We are not allowed to modify `common.c` or use any crypto library other than OpenSSL.

`libtetrissh` is linked into both `tetrisd` (server-side handshake) and `tetrisu` (client-side handshake). Linking the same library into both ends prevents protocol drift.

Public API shape (our design):

```c
int session_handshake_server(int fd, session_t *sess, const char *cert_path, const char *key_path);
int session_handshake_client(int fd, session_t *sess, const char *ca_path);
ssize_t session_send(session_t *sess, const void *buf, size_t len);
ssize_t session_recv(session_t *sess, void *buf, size_t max_len);
void session_close(session_t *sess);
```

---

### 3.6 Concurrency Model

We chose the **thread-per-client + one ticker thread per active room** design. We rejected:
- The epoll event loop — too high synchronisation complexity for a 2-person team under time pressure
- The master+worker-process model — very difficult to debug across process boundaries

#### Threads inside tetrisd

| Thread | Multiplicity | Responsibility |
|--------|--------------|----------------|
| `listener_thread` | 1 | Sits in `accept()`, spawns a fresh `client_thread` per new TCP connection |
| `client_thread` | One per player | Owns that player from handshake to disconnect; does the session handshake, reads HTTTP requests, locks `room_t` to apply moves |
| `ticker_thread` | One per active room | Wakes at fixed Hz, locks `room_t`, calls `brain_tick()` (gravity + line clears), broadcasts STATE to all clients in the room |
| `signal_thread` | 1 | Calls `sigwaitinfo()` in a loop; handles `SIGTERM` (shutdown), `SIGHUP` (reload config), `SIGUSR1` (dump state); all other threads have signals blocked |
| `logshipper_thread` | 1 | Drains the in-memory ring buffer and fires records to `tetrislogd` via Unix socket; non-blocking — increments drop counter on full socket |
| `ctl_listener_thread` | 1 | Listens on a separate Unix socket exclusively for `tetrisctl`; completely isolated from the public TCP port |

#### Key locking rules

- `room_t` is a mutex-protected struct (not a thread). Every board, piece, and score lives here.
- A single `room_t` contains a `players[]` array. The `ticker_thread` handles both boards under the same mutex and injects garbage directly. We rejected separate-room-per-player design — it belongs to the master+worker-process architecture.
- **Never hold a room mutex across `send()`** (TCP backpressure would stall the game loop). Pattern: lock → copy to local buffer → unlock → syscall.
- Lock acquisition order is globally documented and enforced. Where two mutexes must be acquired, always take them in ascending pointer-address order.

---

### 3.7 Battle Royale Mode

Battle Royale is **required** for full tetriSH completion.

#### Rule

When a player clears N ≥ 2 lines in a single move, N − 1 garbage rows are inserted at the bottom of a randomly selected other player's board in a different room.

#### IPC requirement

Garbage transfer between rooms **must be server-side managed** and **must use a real IPC mechanism**. A direct function call into another room's state while holding its mutex from the source room's thread does not count, even if it works.

#### Our design: POSIX message queue

```
mq: /tetris-garbage    ← POSIX message queue; bounded buffer; O_NONBLOCK on send
```

A room that clears lines writes a garbage event to the message queue. Other rooms poll the queue in their `ticker_thread` and inject garbage under their own room mutex. `mq_send()` with `O_NONBLOCK` means the game loop is **never stalled** by a full queue.

Garbage event struct (defined in `game_event.h`):

```c
typedef struct {
    uint8_t  event_type;      /* GE_GARBAGE_SENT */
    uint32_t source_room_id;
    uint32_t target_room_id;
    uint8_t  line_count;
    uint64_t seq;
    /* ... */
} game_event_t;
```

---

### 3.8 Logging Architecture

The full logging pipeline:

```
[client_thread]
   log("connection from %s", ...)
       |
       v
   ring_buffer_push()   ← non-blocking; drops on full; increments counter
       |
       v
   [logshipper_thread]  ← drains ring buffer
       |
       v  Unix SOCK_DGRAM (MSG_DONTWAIT)
       |
       v
   [tetrislogd]
       |
       v  write to disk
```

Requirements:
- Game-critical threads (listener, client handlers, room tickers) must **never block** on the logger
- Every connection event, secure session establishment, HTTTP request/response, room state change, and admin action must be logged with a timestamp
- Log records carry severity levels: `DEBUG`, `INFO`, `WARN`, `ERROR`

---

### 3.9 The .tetrishrc Config File

#### Required directives

| Directive | Purpose |
|-----------|---------|
| `listen_port` | TCP port for client connections |
| `cert_path` | Path to server X.509 certificate |
| `key_path` | Path to server private key |
| `ca_path` | Path to CA certificate bundle |
| `log_path` | Path where `tetrislogd` writes log records |
| `log_ipc` | Address of IPC channel between `tetrisd` and `tetrislogd` (Unix socket path or mq name) |

#### Our optional directives

`max_rooms`, `max_players_per_room`, `tick_hz`, `log_level`, `ctl_socket_path`, `logd_socket_path`, `chat_socket_path`, `market_socket_path`, prompt string, custom aliases.

---

### 3.10 Grading, Checkoff, and Prize

#### Marks breakdown

| Component | Weight |
|-----------|--------|
| Functionality demo | 5% |
| Live extension | 5% |
| Live Q&A | 10% |

#### Baseline MVP requirements (must pass to avoid zero)

The following must be demonstrable:
- Player can connect using `tetrisu`
- Client completes the secure session handshake before any HTTTP traffic
- Player can `JOIN` a room and `START` a game
- Server creates and maintains game state
- A Tetris piece falls over time
- Player can move left/right, rotate, soft drop, hard drop
- Pieces lock on bottom or collision
- Completed lines are cleared; score/line count updates
- Game detects game over
- Server sends STATE messages; `tetrisu` renders the board
- Client quits cleanly; server logs game events through `tetrislogd`

**The server is authoritative.** The full stack is:
```
tetrisu → libhtttp → libtetrissh → TCP → tetrisd → libtetrisbrain → STATE back to tetrisu
```

#### Q&A multiplier scheme

| Performance | Multiplier applied |
|-------------|-------------------|
| Strong answer (specific, code-grounded) | ×1.00 (no change) |
| Weak answer (vague, generic) | ×0.85 |
| Cannot answer | ×0.70 |

Multipliers compound: 3 "cannot answer" results in approximately ×0.34 of the Q&A mark.

#### Prize eligibility requirements

- Complete baseline MVP
- Complete Battle Royale mode
- Pass live demo and live Q&A
- All three members demonstrate ownership of declared roles
- Builds successfully from a clean checkout
- No academic integrity issues
- Ship a clean release bundle from a tagged commit (build instructions, sample `.tetrishrc`, certificates, install steps) that any other cohort group can clone and run without assistance
- Survive a **cohort-scale playtest** (~10–20+ concurrent connections)
- Must project server CLI in front of cohort, live-log sessions, peek into rooms, and manage live users when asked

---

## 4. TetriSocial — 50.003 Component

### 4.1 Overview

TetriSocial extends `tetriSH` with a **live multiplayer chat layer** and a **points-based in-game marketplace**, implemented as independent C daemons sharing the same corestack libraries. It introduces:

- Two new daemons: `chatd` (live chat), `marketd` (points marketplace)
- Two admin CLIs: `chatctl`, `marketctl`
- Two domain libraries: `libchatcore`, `libmarketcore`
- A new shared event contract: `game_event.h` (in the corestack repo)
- A split-screen ncurses marketplace TUI integrated into `tetrisu`
- **Tetris Battle Gaiden characters** purchasable from the marketplace, with server-enforced abilities

The social and economy layers are deliberately **fire-and-forget** with respect to `tetriSH`. Neither `chatd` nor `marketd` can stall the game daemon under any failure condition.

---

### 4.2 chatd — Chat Daemon

| ID | Requirement |
|----|-------------|
| FR-C1 | Accept multiple concurrent TCP client connections using `libtetrissh` RSA-PSS cert auth and AES-256 framed session |
| FR-C2 | Parse and dispatch HTTTP message types: `CHAT`, `JOIN`, `LEAVE`, `ABILITY` |
| FR-C3 | Per-game-room message broadcast: a message from one client in a room is forwarded to all others |
| FR-C4 | Consume `SOCK_DGRAM` game events from `tetrisd` via `game_event.h`; format into human-readable system messages and broadcast (e.g. `"[SYS] Sanjan used Freeze on Ziqi"`, `"[SYS] Ziqi cleared 4 lines"`) |
| FR-C5 | Create and destroy rooms in response to `ROOM_CREATED` / `ROOM_DESTROYED` events from `tetrisd` |
| FR-C6 | Enforce per-session message rate limiting via a token-bucket in `libchatcore`; excess messages are silently dropped |
| FR-C7 | Three client roles: `player`, `spectator`, `admin`; assigned at join time |
| FR-C8 | Ship structured log entries to `tetrislogd` via `SOCK_DGRAM` fire-and-forget |
| FR-C9 | Expose a Unix `SOCK_STREAM` control plane for `chatctl`: `mute <player>`, `kick <player>`, `broadcast <msg>`, `room-list` |

---

### 4.3 marketd — Marketplace Daemon

| ID | Requirement |
|----|-------------|
| FR-M1 | Consume `POINTS_EARNED` and `ABILITY_USED` events from `tetrisd` via `SOCK_DGRAM` to credit player point balances atomically |
| FR-M2 | Maintain a points ledger (player → balance) in `libmarketcore`, protected by a mutex for concurrent R/W |
| FR-M3 | Serve `tetrisu`'s marketplace TUI via Unix `SOCK_STREAM`: handle `browse`, `purchase`, and `equip` request/response flows |
| FR-M4 | Manage player inventory: owned items stored as a bitfield per player; equipped loadout (`theme`, `character`, `ability`) is a separate selection record |
| FR-M5 | Validate that an ability is owned and equipped before `tetrisd` honours an `ABILITY` activation request |
| FR-M6 | Persist the points ledger and inventory to an append-only binary ledger file on disk; replay the full ledger on `marketd` startup to restore state |
| FR-M7 | Expose a Unix `SOCK_STREAM` control plane for `marketctl`: `award <player> <pts>`, `deduct <player> <pts>`, `inventory <player>`, `reset <player>` |

---

### 4.4 Domain Libraries

#### libchatcore

| ID | Requirement |
|----|-------------|
| FR-LC1 | Room lifecycle management: create, look up, and destroy `room_t` structs; maintain list of connected sessions per room |
| FR-LC2 | Token-bucket rate limiter: one bucket per session, configurable capacity and refill rate |
| FR-LC3 | Role management: assign and query `player / spectator / admin` role per session |
| FR-LC4 | Game event formatter: translate a `game_event_t` into a human-readable narration string for broadcast |

#### libmarketcore

| ID | Requirement |
|----|-------------|
| FR-LM1 | Points ledger: thread-safe `credit()`, `debit()`, `balance()` operations over a `player_ledger_t` hash map |
| FR-LM2 | Inventory system: `grant_item()`, `has_item()`, `equip()`, `loadout()` over a per-player owned-items bitfield |
| FR-LM3 | Theme loader: map a `theme_id` to a `theme_t` struct (`bg_color`, `piece_colors[7]`, `border_glyph`, `piece_glyphs[7]`); provide to `tetrisu` at game start |
| FR-LM4 | Ability registry: `validate_equipped_ability(player_id, ability_enum)` — returns true only if the player owns and has equipped the ability |

---

### 4.5 Shared Event Contract (game_event.h)

`game_event.h` is the **single cross-application event type**, shared between `tetrisd` (publisher) and `chatd` / `marketd` (consumers). The schema is frozen by **Week 4 (end of Sprint 2)**.

```c
typedef enum {
    GE_PLAYER_JOINED      = 0,
    GE_PLAYER_LEFT        = 1,
    GE_PLAYER_ELIMINATED  = 2,
    GE_GAME_STARTED       = 3,
    GE_GAME_ENDED         = 4,
    GE_LINE_CLEARED       = 5,
    GE_GARBAGE_SENT       = 6,
    GE_ABILITY_USED       = 7,
    GE_ITEM_PURCHASED     = 8,
    GE_CHARACTER_EQUIPPED = 9,
    /* new types appended ONLY — no renumbering after freeze */
} game_event_type_t;

typedef struct {
    uint8_t  version;          /* envelope versioning */
    uint8_t  event_type;       /* game_event_type_t */
    uint32_t seq;              /* monotonic sequence number */
    uint32_t room_id;
    uint32_t player_id;
    uint16_t dest_mask;        /* GE_DEST_* bitmask for fan-out routing */
    union {
        struct { uint8_t lines; uint32_t target_room_id; } garbage;
        struct { uint8_t ability_id; uint32_t target_player_id; } ability;
        struct { uint32_t points; } points_earned;
        struct { uint32_t item_id; } item;
        /* ... */
    } payload;
} game_event_t;
```

Fan-out destinations via `dest_mask`:

```c
#define GE_DEST_CHATD   (1 << 0)   /* route to chatd.sock */
#define GE_DEST_MARKETD (1 << 1)   /* route to marketd.sock */
```

**Post-freeze rules:** New event types may be appended to the enum. New fields may be added to the payload union. But the offsets of existing fields are immutable. Any change after the freeze requires a joint decision documented in a GitHub issue before the commit is made.

---

### 4.6 tetrisu Extensions

| ID | Requirement |
|----|-------------|
| FR-U1 | Full-screen split-panel ncurses marketplace accessible from the lobby, with three TAB-navigable panes: **Balance/Stats**, **Store**, **Loadout** |
| FR-U2 | Balance pane: current point balance, rank, games played, wins |
| FR-U3 | Store pane: purchasable items (themes, characters, Gaiden abilities) with point costs; `[B] Buy` |
| FR-U4 | Loadout pane: currently equipped theme, character, ability; `[E] Equip`, `[U] Unequip`, `[P] Preview` |
| FR-U5 | Split chat panel alongside the game board during active sessions, showing room messages and system event narration |

`tetrisu` maintains two concurrent TCP connections: one to `tetrisd` (the existing game socket) and one to `chatd` (a second socket for the chat layer). A two-producer / one-consumer POSIX message queue mediates event delivery from `chatd` to the `tetrisu` render thread.

---

## 5. Tetris Battle Gaiden — Character & Ability System

TetriSocial implements four characters inspired by **Tetris Battle Gaiden**, each with a unique combat ability. Character abilities are **server-side enforced inside `tetrisd/ability.c`** — a client cannot forge an ability use via a crafted HTTTP frame.

### Characters

| Character | Cost | Status |
|-----------|------|--------|
| Wolfman   | Free | Default available |
| Mirurun   | Free | Default available |
| Halloween | 1200 pts | Purchasable via `marketd` |
| Princess  | 1500 pts | Purchasable via `marketd` |

### Abilities (implemented for 50.003)

| ID | Ability | Cost | Mechanic |
|----|---------|------|----------|
| FR-A1 | **Garbage Surge** | 600 pts | `tetrisd` sends +3 garbage lines to target room via the Battle Royale garbage queue on activation |
| FR-A2 | **Shield** | 800 pts | `tetrisd` sets `player.shield_ticks = N` in `room_t`; the `ticker_thread` skips garbage injection while `shield_ticks > 0` |
| FR-A3 | **Freeze** | 1000 pts | `tetrisd` sets `player.frozen = true` in `room_t`; the `client_thread` drops `MOVE` requests for 2 seconds, then clears the flag |

### Ability flow

```
tetrisu sends HTTTP ABILITY frame
   → tetrisd/ability.c receives it
   → query marketd via POSIX mq (loadout_ipc.h) to validate ownership
   → if valid: apply ability flags in room_t under room mutex
   → publish GE_ABILITY_USED event via game_event.h to chatd (narration) + marketd (deduct charge)
   → broadcast STATE to all clients
```

`libtetrisbrain` is kept **pure** — no ability flags, no server-side state, no I/O. All ability logic lives in `tetrisd/ability.c`.

---

## 6. IPC Architecture — Full System

### IPC channel table

| Channel | Mechanism | Direction | Reason |
|---------|-----------|-----------|--------|
| `tetrisd` → `tetrislogd` | Unix domain socket (`SOCK_DGRAM`) + `MSG_DONTWAIT` | One-way, fire-and-forget | Datagram preserves record boundaries; non-blocking drops; survives `tetrislogd` restarts |
| `tetrisctl` → `tetrisd` | Unix domain socket (`SOCK_STREAM`) | Request / response | Fully separate from public TCP; always reachable under TCP flood; easy length-prefixed framing |
| Battle Royale garbage between rooms | POSIX message queue (`mq_open`) | room → dispatcher → room | Bounded buffer with built-in drop semantics; `O_NONBLOCK` never blocks game loop |
| `tetrish` → daemon lifecycle | Signals + PID file | shell → daemon | Standard UNIX daemon pattern; `tetrish` writes PIDs from `dspawn`, reads them for `dcheck` |
| `tetrisd` → `chatd` | Unix domain socket (`SOCK_DGRAM`) + `MSG_DONTWAIT` | One-way, fire-and-forget | Same pattern as logshipper; game loop never stalls on social layer failure |
| `tetrisd` → `marketd` | Unix domain socket (`SOCK_DGRAM`) + `MSG_DONTWAIT` | One-way, fire-and-forget | Game loop never stalls on economy layer failure |
| `marketd` → `tetrisd` (loadout query) | Unix domain socket (`SOCK_STREAM`) | Synchronous request/response | JOIN-time loadout query from `tetrisd` to `marketd` before game starts |
| `marketd` → `tetrisd` (async equip) | POSIX message queue | One-way, fire-and-forget | Async `GE_CHARACTER_EQUIPPED` updates back to `tetrisd` after marketplace equip |
| `tetrisu` → `chatd` | TCP + `libtetrissh` + HTTTP | Bidirectional | Second TCP socket on `tetrisu`; same secure session pattern as game socket |
| `chatctl` → `chatd` | Unix domain socket (`SOCK_STREAM`) | Request / response | Admin control plane; isolated from public TCP |
| `marketctl` → `marketd` | Unix domain socket (`SOCK_STREAM`) | Request / response | Admin control plane; isolated from public TCP |

### Critical isolation principle

All daemon-to-daemon IPC from the game layer to the social/economy layer uses **fire-and-forget `SOCK_DGRAM` with `MSG_DONTWAIT`**. If the target socket does not exist (`ENOENT`) or is not reachable (`ECONNREFUSED`), `tetrisd` logs the failure and discards the message. **No retry, no timeout wait.** The game loop is completely isolated from social layer failures.

---

## 7. Thread Model

### tetrisd thread summary

```
listener_thread         accept() loop → spawns client_thread per connection
client_thread[N]        one per player: handshake, HTTTP dispatch, room_t mutations
ticker_thread[M]        one per active room: gravity, line clear, garbage inject, STATE broadcast
logshipper_thread       drains ring_buffer → SOCK_DGRAM to tetrislogd
ctl_listener_thread     separate Unix socket for tetrisctl commands
signal_thread           sigwaitinfo() loop: SIGTERM, SIGHUP, SIGUSR1
```

### chatd thread summary

```
listener_thread         accept() loop → spawns client_thread per connection
client_thread[N]        handshake, HTTTP dispatch, rate limiting, room broadcast
event_consumer_thread   reads game_event_t from tetrisd SOCK_DGRAM → narrates + broadcasts
logshipper_thread       drains ring_buffer → SOCK_DGRAM to tetrislogd
ctl_listener_thread     separate Unix socket for chatctl commands
signal_thread           SIGTERM, SIGHUP
```

### marketd thread summary

```
event_consumer_thread   reads game_event_t from tetrisd SOCK_DGRAM → credits points ledger
store_thread            listens on SOCK_STREAM for tetrisu browse/purchase/equip requests
loadout_server_thread   listens on SOCK_STREAM for tetrisd synchronous loadout queries
persist_thread          drains pending ledger writes to disk (off critical path)
logshipper_thread       drains ring_buffer → SOCK_DGRAM to tetrislogd
ctl_listener_thread     separate Unix socket for marketctl commands
signal_thread           SIGTERM, SIGHUP
```

### Stack size caution

With many threads, default 8 MB stack × N threads = large virtual memory footprint. We use `pthread_attr_setstacksize()` to reduce stack size for simpler threads (logshipper, signal handler) to 64 KB.

---

## 8. Shared CoreStack Library Boundaries

| Library | What it provides | How 50.003 uses it |
|---------|------------------|--------------------|
| `libtetrissh` | Full secure handshake: RSA-PSS cert auth, RSA-OAEP key wrap, AES-256 framing, `session_send/recv/close` | `chatd` uses it for all client sessions; `marketd` uses it for `tetrisu` TCP connections |
| `libhtttp` | HTTTP parser, serialiser, method dispatch table, header map, status codes | `chatd` dispatches `CHAT`, `JOIN`, `LEAVE`, `ABILITY` methods; `marketd` dispatches `browse`, `purchase`, `equip` |
| `libtetrisbrain` | Tetris game logic: board operations, SRS rotation, line clear, scoring, gravity, garbage injection | `lib/libtetrisbrain/src/abilities.c` provides ability-aware board transforms called by `tetrisd/ability.c` |
| `libcoreipc` | Thin wrappers: Unix socket helpers, lock-free ring buffer, atomic drop counter, mq helpers | Both `chatd` and `marketd` use the ring buffer + logshipper pattern; all daemons use the Unix socket helpers |

### Architectural purity rule

**`libtetrisbrain` is strictly pure logic — no I/O, no side effects, no server-side flags.** All server-side flags, IPC, and ability dispatch live in `tetrisd/ability.c`. This boundary must be preserved. A code review that catches `libtetrisbrain` calling any POSIX function other than `stdlib.h`/`string.h`/`math.h` equivalents should be rejected.

---

## 9. Role Declarations

| Role | Owns | Member |
|------|------|--------|
| **Systems** (50.005) | `tetrish`, `tetrisd` process model, concurrency, `tetrislogd`, `tetrisctl`, signal handling, IPC channels | **Sanjan** |
| **Networking and Security** (50.005) | `libtetrissh`, `libhtttp`, secure session correctness, HTTTP parser and serialiser, threat model, request dispatch | **Zi Qi** |
| **Application and Integration** (50.005 + 50.003) | `tetrisu`, `libtetrisbrain`, room lifecycle, game loop, build system, integration tests | **Sanjan & Zi Qi** |
| **Social Systems** (50.003) | `chatd` process model, concurrency, `libchatcore`, `chatctl`, `event_pub.c` in `tetrisd` | **Sanjan** |
| **Economy & Protocol** (50.003) | `marketd` (TCP, libtetrissh integration, BUY/EQUIP dispatch), `libmarketcore`, `loadout_server.c`, loadout IPC wire format (`loadout_ipc.h`), `marketctl` | **Zi Qi** |

---

## 10. Sprint Plan

| Sprint | Week | Primary Goal | Sanjan | Zi Qi | Both |
|--------|------|-------------|--------|-------|------|
| S0 | 3 | Setup | `game_event.h` draft; `Makefile` skeleton; repo structure | `certs/gen_test_certs.sh`; CA + server cert generation | `include/common_types.h`; `.tetrishrc.example` |
| S1 | 4 | Corestack foundations | `libcoreipc`: `ring_buffer.c`, `mq_helpers.c`, `unix_socket.c` | `libtetrissh`: `handshake.c`, `session.c` | `libhtttp`: `parser.c`, `serialiser.c` |
| S2 | 5 | Shell + chatd skeleton | `tetrish`: REPL, builtins, `rc_parser.c`, `process.c` (dspawn/dcheck) | `libhtttp`: `dispatch.c`; `chatd/client.c` handshake path | `game_event.h` schema frozen; `chatd/main.c` + listener |
| S3 | 6 | tetrisd scaffolding | `tetrisd`: `main.c`, `listener.c`, `client.c`, `logshipper.c`, `signal_handler.c` | `tetrisd`: `ctl_listener.c`; `tetrislogd`; `tetrisctl` | `tetrisd`: `room.c` skeleton; basic JOIN/LEAVE/START dispatch |
| S4 | 7 | Game loop end-to-end | `tetrisd`: `ticker.c`; `room_t` mutex discipline; STATE broadcast | `libtetrisbrain`: `board.c`, `pieces.c`, `gravity.c`, `lineclear.c`, `scoring.c` | First playable session: two `tetrisu` clients, pieces falling, lines clearing |
| S5 | 8 | Chat full + market scaffold | `chatd`: `room_registry.c`, `event_consumer.c`; `libchatcore` full; `chatctl` | `marketd`: `main.c`, `client.c`; `libmarketcore`: `points.c`, `inventory.c` | `game_event.h` publisher wired into `tetrisd/event_pub.c`; chatd narration broadcasting |
| S6 | 9 | Market full + ability logic | `tetrisd`: `ability.c`, `loadout.c`, `event_pub.c`; ability flags in `room_t` | `marketd`: `ledger.c`, `catalogue.c`, `loadout_server.c`; `marketctl` | Ability full-stack: Freeze, Shield, Garbage Surge activate end-to-end |
| S7 | 10 | Battle Royale + tetrisu TUI | `tetrisd/garbage.c` Battle Royale POSIX MQ integration | `libmarketcore` ledger persistence; `marketd persist_thread` | `tetrisu`: `chat_net.c`, `render_chat.c`, `render_market.c` (split-screen TUI) |
| S8 | 11 | Integration hardening | IPC pipeline integration tests; signal shutdown paths; `valgrind` baseline | HTTTP parser edge-case tests; loadout sync protocol; `helgrind` on `marketd` | `tests/test_game_event.c`; ability + chat + market full-stack integration test |
| S9 | 12 | System tests + concurrency | `helgrind` on `chatd`; broadcast race tests; chatroom lifecycle stress | Ledger atomicity under concurrent threads; `test_market_points.c` concurrency cases | `tests/` suite complete; README, `docs/architecture.md`, known limitations |
| S10 | 13 | Checkoff | Checkoff demo dry-run; live extension practice | `docs/threat_model.md` | Tagged `v1.0-br` release; clean build from fresh checkout verified |

### Release tags

| Tag | When | State |
|-----|------|-------|
| `v0.1-foundation` | End of Sprint 2 | Corestack libs compiling and tested |
| `v0.5-mvp` | End of Sprint 5 | tetriSH baseline demo-able |
| `v1.0-br` | End of Sprint 8 | Battle Royale + full TetriSocial stack functional |

---

## 11. Testing Strategy

Three-tier test pyramid, all driven by `make test`. Every test at every tier must pass under `valgrind --leak-check=full --error-exitcode=1`.

```
make test-unit          # pure library tests, no OS resources
make test-integration   # daemons started with test config, real IPC
make test-system        # full clean build + scripted tetrisu sessions
make test               # runs all three in order
```

### Test files

| File | Tier | Covers |
|------|------|--------|
| `test_tetrisbrain.c` | Unit | `libtetrisbrain`: board operations, SRS rotation, line clear, scoring, ability transforms |
| `test_htttp_parser.c` | Unit + Whitebox | `libhtttp`: all parser paths, all error branches, edge-case frames |
| `test_game_event.c` | Integration | `game_event.h` publish → consume pipeline across real `SOCK_DGRAM` sockets |
| `test_market_points.c` | Unit + Concurrency | `libmarketcore`: ledger correctness, inventory operations, concurrent credit/debit |
| `test_chatcore.c` | Unit | `libchatcore`: token-bucket, room lifecycle, role management (to be added Sprint 5) |

### Definition of Done

A task is done when: the feature compiles clean under `gcc -Wall -Wextra -Werror`, its associated unit or integration tests pass, the binary is clean under `valgrind --leak-check=full --error-exitcode=1`, and the PR has been approved and merged into `main` by both members.

---

## 12. Build System

Each corestack library is a **self-contained directory** — its own `Makefile`,
`src/`, `include/`, and `tests/` — that builds `lib/libXXX/libXXX.a` in place and runs
its own unit tests (`make -C lib/libXXX [test]`). A top-level umbrella `Makefile`
recurses into each library (in dependency order) and links the `.a` archives into
the binaries, adding each library's header path with `-I lib/libXXX/include`. No
external build tools beyond `gcc`, `ar`, `make`, and `pkg-config` for SQLite.

> Status: `libtetrisbrain` is implemented self-contained today and builds/tests
> via `make -C lib/libtetrisbrain`. The umbrella `Makefile` and remaining libraries
> land as the corestack is built out.

### Compilation flags

```makefile
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -g -D_POSIX_C_SOURCE=200809L
OPTFLAGS ?= -O2
```

`-Werror` is non-negotiable: a warning is a build failure. `-g` is kept even in the default build for valgrind traces.

### Build targets

| Target | Description |
|--------|-------------|
| `make all` | Build all libraries then all binaries |
| `make libs` | Build the six static libraries only |
| `make bins` | Build the ten binaries (requires `libs` first) |
| `make test` | Run all test tiers under valgrind |
| `make test-unit` | Unit tests only |
| `make test-integration` | Integration tests only |
| `make clean` | Remove all `.o`, `.a`, and binary artifacts |
| `make certs` | Run `certs/gen_test_certs.sh` to generate dev CA and server cert |

### Library build order (dependency-ordered)

```
libcoreipc          ← no internal dependencies; built first
libtetrissh         ← uses libcoreipc socket helpers
libhtttp            ← no inter-library dependencies
libtetrisbrain      ← no inter-library dependencies
libchatcore         ← depends on libcoreipc
libmarketcore       ← depends on SQLite (-lsqlite3)
```

### Binary linkage

| Binary | Linked libraries |
|--------|-----------------|
| `tetrish` | *(none from corestack)* |
| `tetrisd` | `libtetrissh libhtttp libtetrisbrain libcoreipc -lssl -lcrypto -lm -lpthread` |
| `tetrislogd` | `-lpthread` |
| `tetrisctl` | *(none from corestack)* |
| `tetrisu` | `libtetrissh libhtttp libcoreipc -lssl -lcrypto -lncurses -lpthread` |
| `chatd` | `libtetrissh libhtttp libcoreipc libchatcore -lssl -lcrypto -lpthread` |
| `chatctl` | *(none from corestack)* |
| `marketd` | `libtetrissh libhtttp libcoreipc libmarketcore -lssl -lcrypto -lsqlite3 -lpthread` |
| `marketctl` | *(none from corestack)* |

### TEST_BUILD flag

Test binaries are compiled with `-DTEST_BUILD`. This flag enables `#ifdef TEST_BUILD` blocks that export internal injection hooks from `event_pub.c` (so integration tests can inject synthetic `game_event_t` events directly) and from `logshipper.c` (so tests can force a ring buffer overflow). These blocks are compiled out in production builds.

---

## 13. Memory Safety

Memory safety is a hard requirement — not best-effort. A demo with visible leaks at checkoff loses marks regardless of feature completeness.

Three valgrind tools are used:

**Memcheck** (run against every test binary and daemon startup/shutdown):
```bash
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --error-exitcode=1 \
         ./bin/tetrisd --test-config
```

**Helgrind** (run against integration test suite and concurrency tests):
```bash
valgrind --tool=helgrind \
         --error-exitcode=1 \
         ./bin/chatd --test-config
```

**DRD** (second thread sanitizer for `marketd` ledger operations):
```bash
valgrind --tool=drd \
         --error-exitcode=1 \
         ./bin/marketd --test-config
```

Critical memory rules enforced by code review:
- Every `malloc`'d buffer, `X509*`, and `EVP_PKEY*` must be freed by the owner
- Session handshake failure must free all partially-allocated state
- No mutex held across blocking syscalls
- `fork()` from a multi-threaded process: child must call `execve()` immediately or use only async-signal-safe functions

---

## 14. File Structure

```
project/
    bin/
        tetrish tetrisd tetrislogd tetrisctl tetrisu
        chatd chatctl marketd marketctl

    # --- self-contained libraries (each owns its Makefile/src/include/tests) ---
    lib/
        libtetrissh/
            Makefile  include/tetrissh.h  src/*.c  tests/test_*.c
            libtetrissh.a        ← built in place by `make -C lib/libtetrissh`
        libhtttp/
            Makefile  include/htttp.h  src/*.c  tests/test_*.c
            libhtttp.a
        libtetrisbrain/
            Makefile  scripts/run_tests.sh
            include/tetrisbrain.h
            src/
                board.c pieces.c gravity.c lineclear.c scoring.c
                abilities.c      ← ability-aware board transforms (pure logic only)
            tests/test_*.c
            libtetrisbrain.a
        libcoreipc/
            Makefile  include/coreipc.h
            src/ring_buffer.c src/mq_helpers.c src/unix_socket.c
            tests/test_*.c  libcoreipc.a
        libchatcore/
            Makefile  include/chatcore.h
            src/room_registry.c src/rate_limiter.c src/role.c src/event_formatter.c
            tests/test_*.c  libchatcore.a
        libmarketcore/
            Makefile  include/marketcore.h
            src/ledger.c src/inventory.c src/catalogue.c src/theme_loader.c
            tests/test_*.c  libmarketcore.a

    include/                     ← cross-component shared headers only
        game_event.h             ← shared event contract (frozen Week 4)
        loadout_ipc.h            ← loadout wire format (frozen when both sides in review)
        common_types.h
    auth/
        server.crt  server.key  cacsertificate.crt  gen_test_certs.sh
    sample.tetrishrc
    var/
        log/                     (created at runtime)
        run/                     (sock files, pid files)
    src/                         ← daemons + clients (link the .a archives above)
        tetrish/
        tetrisd/
            ability.c            ← server-side ability dispatch
            event_pub.c          ← game_event.h publisher
            loadout.c            ← synchronous loadout query to marketd
            garbage.c            ← Battle Royale POSIX MQ integration
        tetrislogd/
        tetrisctl/
        tetrisu/
            chat_net.c  render_chat.c  render_market.c
        chatd/  chatctl/  marketd/  marketctl/
    docs/
        architecture.md
        threat_model.md
    Makefile                     ← umbrella: recurse into libs, link + build bins (planned)
    README.md
    AGENTS.md
```

---

## 15. Key Design Decisions & Rationale

### Why thread-per-client over epoll?

The epoll event loop is more scalable but the synchronisation complexity is very high for a 2-person team under time pressure. Our thread model is easier to reason about and debug. Linear thread count growth is acceptable for a cohort-scale playtest of 10–20 concurrent connections.

### Why not master+worker-process?

Isolation and error handling are better, but cross-process debugging (especially under valgrind/helgrind) is extremely difficult. Ruled out after research.

### Why SOCK_DGRAM for log shipping?

Datagram semantics preserve record boundaries without length-prefix framing overhead. `MSG_DONTWAIT` gives a single syscall that either succeeds or drops — no partial write state to manage. The socket survives `tetrislogd` restarts because UDP sendto simply returns `ENOENT` when the socket path is gone; `tetrisd` logs that failure and continues.

### Why POSIX mq for Battle Royale garbage?

Built-in bounded buffer with explicit drop semantics. `mq_send(O_NONBLOCK)` never blocks the game loop under any condition. The queue also serves as a natural decoupling point — the source room's `ticker_thread` writes and returns; the target room's `ticker_thread` reads on its own next tick.

### Why SOCK_STREAM for the control plane?

The control plane needs request/response semantics — `tetrisctl` needs to receive a response. `SOCK_STREAM` gives a reliable ordered byte stream with simple length-prefixed framing. The socket path lives in `.tetrishrc` under `var/run/`. Completely separate from the public TCP port means it remains reachable even during a TCP flood.

### Why is libtetrisbrain pure?

Testability and boundary enforcement. Pure board logic with no I/O, networking, or side effects can be tested exhaustively in isolation, compiled without linking OpenSSL or pthreads, and reasoned about independently of the network/concurrency layer. This is the most tested and most stable piece of the codebase.

### Why a second TCP socket from tetrisu to chatd?

The game socket and chat socket are independent streams with independent HTTTP sessions. This means:
- Chat failures cannot interfere with game frame delivery
- The chat connection can be dropped and reconnected without disrupting the game session
- `chatd` can be deployed on a different host if needed
- Testing `chatd` in isolation doesn't require a running `tetrisd`

### Why not hold mutex across send()?

TCP backpressure: if the receiving end is slow, `send()` may block. Holding a room mutex while blocked on `send()` would starve the `ticker_thread` — preventing game ticks and causing all other players in the room to freeze. Pattern: lock → copy to send buffer → unlock → send.

---

## 16. Live Q&A Readiness — Question Pool

These are the types of questions that will be asked at the live checkoff. Every code path referenced should be answerable by the owning member with specific line references.

### Concurrency and lifecycle

- Walk through what happens, line by line, when a client sends `MOVE LEFT` while the room ticker is mid-gravity. Which lock do you hold? In what order? Why?
- I kill `SIGTERM` on `tetrisd` while a game is in progress. Walk through the shutdown path. Where does each thread learn it should exit?
- Show where you handle a slow client. What happens to the room if one client stops reading?
- What is your global lock acquisition order? Show one site that respects it and one that could deadlock if you got the order wrong.
- Your `tetrislogd` IPC channel fills up. Show the line that decides to drop. What is the drop counter, and how does it become visible?

### Networking and security

- Walk through the handshake from `accept()` to the first encrypted HTTTP frame.
- A man-in-the-middle replays a `MOVE` frame from earlier in the session. What stops it? Show the code.
- The client sends an HTTTP request with `Content-Length` larger than the actual body. Where do you detect this? What status code do you return?
- What attack does RSA-PSS prevent that RSA-PKCS1v15 signing does not?
- Show where you free the `X509*` and `EVP_PKEY*` from the handshake. What happens if the handshake fails halfway?

### Protocol and parsing

- A client sends a request with `\n` line endings instead of `\r\n`. What happens?
- A header has a value with a colon in it. Show how your parser handles this.
- Walk through the path of a STATE broadcast from the room ticker to the client's terminal.
- Two clients hit the same room with `START` simultaneously. What happens? Show the code path.

### IPC and control plane

- I run `tetrisctl shutdown` while the public TCP listener is being flooded. Why does the control command still work?
- Your `tetrislogd` dies. What does `tetrisd` do? Show the "reconnection" logic.
- Walk through the wire format on your control plane channel. Show the parser.

### Application and game logic

- Show the line clear function. Walk through it on a board where rows 18 and 20 are full.
- Where does lock delay live? Walk through a piece being held in place by repeated rotations.
- A garbage row arrives from another room while the local ticker is mid-tick. What synchronises this?
- Your client is rendering at 30 Hz but the server ticks at 60 Hz. What happens visually? Where in the code?

---

## 17. Out of Scope

The following are explicitly **not implemented** and will not gain special consideration:

- Reimplementing TCP, UDP, or any reliability protocol on top of UDP — use TCP from the kernel
- Implementing custom crypto primitives — use OpenSSL via PA2's `common.c` only
- Web frontend, GUI client, or mobile client — terminal only
- Any use of `SSL_*` API or TLS libraries — the secure session is built manually in `libtetrissh`

---

## Notes for AI Agents

If you are an AI agent working in this codebase:

1. **The server is always authoritative.** Never update client-side state without a round-trip to `tetrisd`. Any move, drop, rotation, or ability activation must flow through the full stack.

2. **libtetrisbrain must stay pure.** No POSIX calls, no I/O, no global mutable state. If you find yourself adding a network call or mutex to `libtetrisbrain`, you are in the wrong file. Move the logic to `tetrisd/ability.c` or `tetrisd/ticker.c`.

3. **Never hold a mutex across a blocking syscall.** The pattern is always: lock → copy to local buffer → unlock → syscall. Violating this causes game freezes under TCP backpressure.

4. **All IPC from tetrisd to chatd/marketd is MSG_DONTWAIT.** If you add a new IPC send from the game loop to a social daemon, it must use `MSG_DONTWAIT`. A blocking send from the game loop is a critical regression.

5. **game_event.h is frozen after Week 4.** Do not change the offsets of existing fields. New types may only be appended to the enum; new payload fields only appended to the union.

6. **The live extension task pool is real.** Architecture should make it easy to add a new HTTTP method (new entry in the dispatch table, handler function, log entry). Resist designs that require touching many files to add a single method.

7. **Valgrind is mandatory.** Every PR must produce a clean `valgrind --leak-check=full --error-exitcode=1` run. A visible leak at checkoff loses marks regardless of feature completeness.

8. **Commit messages matter.** The git history is part of the prize evaluation. Use meaningful commit messages that describe intent. Reference the component (e.g. `[tetrisd] add garbage injection to ticker_thread`).

9. **Always explain every change.** The checkoff Q&A will ask about any line of submitted code. When you write or modify code, follow Section 18 (Code Documentation Standards) and Section 19 (How to Explain Changes). Never write a function without a header comment. Never write a non-obvious expression without an inline comment.

10. **Write code the team member can defend.** Both Sanjan and Zi Qi must be able to explain every line in their declared role's files at the checkoff. If you write code that is not obviously explainable, add a comment that says exactly what it does and why — not just what the language construct means, but the systems reason for it.

---

## 18. Code Documentation Standards

The live checkoff Q&A will ask about **any line of submitted code**. The instructor's exact words: *"Code query — Instructor is free to ask about ANY line of submitted code."* This means every non-trivial line must be explainable on the spot. The comments in the code are the team's preparation material.

### File-level header comment (required on every .c and .h file)

```c
/*
 * tetrisd/ticker.c — room ticker thread
 *
 * One ticker_thread is spawned per active room when START is received.
 * It wakes at TICK_HZ (configurable in .tetrishrc), locks room->mutex,
 * calls brain_tick() to apply gravity and check line clears, injects any
 * pending garbage from the Battle Royale message queue, then broadcasts
 * a STATE frame to every connected client in the room.
 *
 * The ticker exits when room->state == ROOM_OVER (set by the client_thread
 * that receives the last player's game-over condition).
 *
 * Locking: holds room->mutex for the full tick duration except during
 * send() calls. Never holds room->mutex while blocked on any syscall.
 * See docs/architecture.md §Concurrency for the global lock order.
 */
```

### Function header comment (required on every non-trivial function)

Use this format — it directly answers the Q&A question "walk me through function X":

```c
/*
 * session_handshake_server() — server side of the libtetrissh handshake
 *
 * Performs the full 7-step session establishment on an already-accept()ed fd:
 *   1. Recv client nonce (32 bytes)
 *   2. Send server X.509 certificate (DER-encoded, length-prefixed)
 *   3. Sign the nonce with the server private key using RSA-PSS (SHA-256)
 *   4. Send the signature
 *   5. Recv AES-256 session key, RSA-OAEP decrypted with server private key
 *   6. Populate sess->aes_key and sess->aes_iv
 *   7. From this point, all I/O goes through session_send/session_recv
 *
 * Returns 0 on success, -1 on any failure (partial handshake state is
 * cleaned up before returning — no resource leaks on error paths).
 *
 * Thread safety: called once per client_thread before room join; no shared
 * state is accessed. The sess struct is per-connection and not shared.
 */
int session_handshake_server(int fd, session_t *sess,
                             const char *cert_path, const char *key_path);
```

### Inline comments — when to write them

Write an inline comment whenever a reader could ask "why is this here?":

```c
/* --- REQUIRED --- */

/* Locking comments — state which mutex is held and why */
pthread_mutex_lock(&room->mutex);   /* hold room->mutex for the full tick */
brain_tick(room->board, &result);
pthread_mutex_unlock(&room->mutex); /* release before send() — never hold across blocking I/O */

/* Non-obvious constants */
nanosleep(&(struct timespec){0, 1000000000L / TICK_HZ}, NULL);
/*         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * Sleep for exactly 1/TICK_HZ seconds between ticks.
 * TICK_HZ defaults to 60 (configurable in .tetrishrc).
 */

/* Error handling that drops instead of retrying — explain why */
if (mq_send(garbage_mq, (char *)&ev, sizeof(ev), 0) == -1) {
    /* O_NONBLOCK: if the queue is full, drop the event rather than
     * blocking the game loop. The target room will miss one garbage
     * injection — acceptable data loss vs. stalling all clients. */
    room->stats.garbage_drops++;
}

/* Any place where the code looks wrong but is intentional */
write(fd, buf, len);  /* deliberately ignoring return value — SIGPIPE is
                       * blocked; EPIPE surfaces on next write and is
                       * caught there. See client_thread() error path. */

/* All MSG_DONTWAIT sends */
sendto(logd_sock, record, record_len, MSG_DONTWAIT,
       (struct sockaddr *)&logd_addr, logd_addrlen);
/* MSG_DONTWAIT: non-blocking fire-and-forget to tetrislogd.
 * If the socket buffer is full the record is dropped; drop counter
 * is incremented. The game loop must never block on logging. */
```

### What NOT to write (useless comments)

```c
/* BAD — describes syntax, not intent */
i++;              /* increment i */
ret = -1;         /* set ret to -1 */
free(sess);       /* free sess */

/* GOOD — describes the systems reason */
i++;              /* advance past the '\r' before reading '\n' */
ret = -1;         /* signal handshake failure; caller must close fd */
free(sess);       /* session_close() already zeroed the AES key material */
```

### Concurrency comments — mandatory pattern

Every mutex lock/unlock pair must have matching comments that state what is protected:

```c
/* --- acquire room->mutex: protects board[], piece, score, state --- */
pthread_mutex_lock(&room->mutex);

    /* safe to read/write board state here */
    result = brain_tick(room->board, &room->current_piece);
    if (result.lines_cleared > 0) {
        room->score += score_for_lines(result.lines_cleared);
        /* enqueue garbage for other rooms via Battle Royale mq */
        if (result.lines_cleared >= 2)
            enqueue_garbage(room, result.lines_cleared - 1);
    }

/* --- copy STATE snapshot before releasing lock --- */
memcpy(&snap, room, sizeof(room_snapshot_t));

pthread_mutex_unlock(&room->mutex);
/* --- room->mutex released: now safe to call send() --- */

broadcast_state(&snap);   /* send() calls happen outside the lock */
```

### IPC channel comments — mandatory pattern

Every IPC send or receive must name the channel, direction, and failure mode:

```c
/* Send log record to tetrislogd via Unix SOCK_DGRAM (fire-and-forget).
 * Channel: tetrisd → tetrislogd (logd.sock path from .tetrishrc).
 * Failure: ENOENT if tetrislogd not running — increment drop counter,
 *          do not retry, do not block. */
ssize_t n = sendto(logd_fd, rec, rec_len, MSG_DONTWAIT,
                   (struct sockaddr *)&logd_sa, logd_sa_len);
if (n == -1) {
    atomic_fetch_add(&logd_drops, 1);
}
```

### Signal handling comments

```c
/* signal_thread() is the sole handler for all signals in the process.
 * At startup, all other threads block every signal via:
 *   sigfillset(&mask); pthread_sigmask(SIG_BLOCK, &mask, NULL);
 * This thread then calls sigwaitinfo() in a loop so signals are
 * delivered synchronously here — no async-signal-safety concerns. */
```

---

## 19. How to Explain Changes to the Team

**This is critical.** The live Q&A and live extension task both require that Sanjan and Zi Qi can explain every line of every change made to their declared files — including changes made by an AI agent. When you modify the codebase, always follow this protocol.

### Rule 1: Write a full summary of every change

After making any change to the codebase, produce a summary in this format. Do not skip this even for "small" changes — the checkoff treats every line equally.

```
## Change Summary: <short title>

### What changed
- <file>:<function/line> — one-sentence description of what was added/modified/deleted

### Why it was changed
<The systems-level reason. Reference the architecture: which requirement this satisfies,
which IPC contract it touches, which mutex discipline it follows or introduces.>

### What the new code does, line by line
<Walk through the changed lines exactly as a Q&A answer would. Example:>

  pthread_mutex_lock(&room->mutex);
  ^ We acquire room->mutex here because brain_tick() reads and writes
    room->board[] and room->current_piece. Without the lock, the
    client_thread could be processing a MOVE simultaneously, causing
    a torn read of the piece position.

  result = brain_tick(room->board, &room->current_piece);
  ^ brain_tick() is a pure function in libtetrisbrain — no I/O, no
    side effects. It returns a brain_result_t describing what changed
    (lines cleared, new piece needed, game over flag).

  pthread_mutex_unlock(&room->mutex);
  ^ Released before the broadcast send() call. Never hold room->mutex
    across send() — TCP backpressure on a slow client would stall all
    other clients in the room.

### What could go wrong / edge cases covered
<Mention any failure mode the code handles, and how.>

### What is deliberately NOT handled (and why)
<If something is dropped, retried, or silently ignored, explain why.>
```

### Rule 2: Explain every non-trivial addition before writing it

Before writing a new function, state out loud (in your response):
- What file it goes in, and why that file owns this logic
- What arguments it takes and what each means
- What mutex state is assumed on entry
- What it returns on success and on each failure path
- Whether it can block, and under what conditions

This forces the design to be sound before the code exists.

### Rule 3: When modifying shared/boundary code, flag it explicitly

If a change touches any of these files, say so prominently at the top of your response:

| File | Why it needs a flag |
|------|---------------------|
| `game_event.h` | Frozen after Week 4; any field change may break chatd and marketd |
| `include/loadout_ipc.h` | Wire format frozen once both sides are in review |
| `include/htttp.h` | Changing the dispatch table interface breaks both tetrisd and chatd |
| `libtetrisbrain/*.h` | Public API used by tetrisd, chatctl, and tests — changes require updating all callers |
| `common.c` / `common.h` | **Must not be modified** — these are provided by course staff |
| `src/tetrisd/room.c` | Core shared state; changes to `room_t` struct break ticker, client, and ability code simultaneously |

Flag format:
```
⚠️ SHARED BOUNDARY CHANGE: This modifies game_event.h (frozen schema).
   Reason: <why this change is necessary>
   Impact: chatd's event_consumer.c and marketd's event_consumer.c must
           be updated to handle the new field. See game_event.h freeze rules
           in AGENTS.md §4.5.
```

### Rule 4: Explain the threading and IPC implications of every change

For every change, explicitly state which of the following it affects:

- **Does it run inside a mutex-held section?** State which mutex.
- **Does it call send() or recv()?** If so, confirm no mutex is held.
- **Does it send over IPC?** Name the channel and confirm it uses the correct blocking mode (fire-and-forget → `MSG_DONTWAIT`; control plane → blocking `SOCK_STREAM`).
- **Does it touch a signal handler or signal-blocked thread?** Confirm async-signal-safety.
- **Does it allocate memory?** State where and by whom it is freed.

### Rule 5: Comment format for AI-generated code

Any function substantially written by an AI agent must include this comment at the top:

```c
/* NOTE: This function was generated with AI assistance (Claude).
 * Sanjan/Zi Qi: reviewed, tested, and understood before merge.
 * Key things to know for Q&A:
 *   - <bullet: the most important thing to understand about this function>
 *   - <bullet: the concurrency property — which lock, what it protects>
 *   - <bullet: the failure mode — what happens when this goes wrong>
 */
```

This is not a disclaimer — it is preparation material. The checkoff instructor specifically checks whether AI-generated code is understood. A group that submits working AI-generated code but cannot explain it faces an immediate 10% penalty and becomes ineligible for the prize.

### Example: what a good change explanation looks like

> **Task:** Add the garbage event enqueue to the Battle Royale ticker path.
>
> **Change summary:**
>
> `src/tetrisd/ticker.c` → `room_ticker_thread()`: After `brain_tick()` returns, if `result.lines_cleared >= 2`, we now call `br_enqueue_garbage()` with `lines_cleared - 1` as the garbage count.
>
> `src/tetrisd/garbage.c` (new file) → `br_enqueue_garbage()`: Opens the POSIX message queue `/tetris-garbage` (name from `.tetrishrc`) with `O_NONBLOCK`, serialises a `game_event_t` with `event_type = GE_GARBAGE_SENT`, and calls `mq_send()`. If the queue is full, increments `room->stats.garbage_drops` and returns — the game loop is never blocked.
>
> **Line-by-line walk:**
>
> ```c
> if (result.lines_cleared >= 2) {
> ```
> Battle Royale rule: only trigger garbage if 2+ lines cleared in one tick. Single-line clears give no garbage. The "≥ 2" is from the handout spec.
>
> ```c
>     br_enqueue_garbage(room, result.lines_cleared - 1);
> ```
> `lines_cleared - 1` is the garbage count. Clearing 2 lines sends 1 row; clearing 4 lines (a Tetris) sends 3 rows. This is called while `room->mutex` is still held — `br_enqueue_garbage()` only writes to the mq and a stats counter, both of which are safe to do under the lock.
>
> ```c
> game_ev.event_type = GE_GARBAGE_SENT;
> game_ev.payload.garbage.line_count = garbage_count;
> game_ev.payload.garbage.target_room_id = pick_random_other_room(room->id);
> ```
> Populates the `game_event_t` on the stack — no heap allocation, no free needed.
>
> ```c
> if (mq_send(br_mq, (char *)&game_ev, sizeof(game_ev), 0) == -1) {
>     room->stats.garbage_drops++;
> }
> ```
> `br_mq` was opened with `O_NONBLOCK` at daemon start. If the queue is full, `mq_send` returns `EWOULDBLOCK` immediately — never blocks. We increment the drop counter so it's observable via `tetrisctl`. The target room's own `ticker_thread` will drain this queue on its next tick under its own mutex — no cross-room mutex acquisition.

### Example: what a bad change explanation looks like (avoid this)

> "I added a garbage function that sends garbage to other rooms using a message queue."

This is useless for Q&A prep. It describes what was done, not how or why. The checkoff will ask "show me the line that decides to drop" and "what synchronises the garbage injection in the target room?" — and those questions cannot be answered from the description above.
