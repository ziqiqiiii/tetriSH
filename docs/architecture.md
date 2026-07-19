# docs/architecture.md — Full System Architecture

> **Usage:** Read this when you need deep detail on a subsystem. Not loaded on every agent request — reference it with `cat docs/architecture.md` or search it when needed.

---

## Architecture layers

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

TCP comes from the kernel. We build the upper two layers. No TLS, no SSL_* API, no reverse proxy.

---

## libtetrissh — secure session handshake (7 steps)

1. Client connects, sends fresh 32-byte nonce
2. Server sends its X.509 certificate (DER-encoded, length-prefixed)
3. Server signs the client nonce with its private key using **RSA-PSS (SHA-256)**
4. Server sends the signature
5. Client verifies cert against bundled CA (`cacsertificate.crt`)
6. Client verifies the signature using the public key from the cert
7. Client generates a 32-byte AES-256 session key, **RSA-OAEP** encrypts it with the server public key, sends it
8. All subsequent traffic: `[4-byte big-endian length][AES-256 ciphertext]` per HTTTP message

Frame size limit: **64 KiB**. Larger messages rejected with `413 Payload Too Large`.

Crypto primitives come from `common.c` (course-provided, read-only). No other crypto allowed.

### libtetrissh public API

```c
int     session_handshake_server(int fd, session_t *sess, const char *cert, const char *key);
int     session_handshake_client(int fd, session_t *sess, const char *ca);
ssize_t session_send(session_t *sess, const void *buf, size_t len);
ssize_t session_recv(session_t *sess, void *buf, size_t max);
void    session_close(session_t *sess);
```

Returns 0 on success, -1 on failure. On failure, all partial state is cleaned up — no leaks.

---

## HTTTP protocol

HTTP-like, custom. Every message travels inside a libtetrissh-encrypted frame.

### Required methods

| Method | Path | Notes |
|--------|------|-------|
| `JOIN` | `/arena/<name>` | Client joins a room |
| `START` | `/room/<id>` | Start a game |
| `MOVE` | `/room/<id>/player/<pid>` | Body: `LEFT` or `RIGHT` |
| `ROTATE` | `/room/<id>/player/<pid>` | Body: `CW` or `CCW` |
| `DROP` | `/room/<id>/player/<pid>` | Body: `SOFT` or `HARD` |
| `STATE` | `/room/<id>` | Server-originated — pushed broadcast, never a client request |
| `CHAT` | `/room/<id>` | TetriSocial extension |
| `ABILITY` | `/room/<id>/player/<pid>` | TetriSocial — triggers ability validation via marketd |
| `SPECTATE` | `/room/<id>` | TetriSocial extension |

### Required status codes: `200 201 400 401 403 404 409 429 500`

### Required headers

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server STATE broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

### Sample exchange

```
JOIN /arena/main HTTTP/1.0
Host: tetrish.local
User: alice
Mode: battle-royale
Content-Length: 35

{"skin":"cyan","start_level":0}

---

HTTTP/1.0 200 OK
Session-Id: s-8f31a2
Player-Id: p17
Tick-Rate: 20
Content-Type: application/json
Content-Length: 94

{"arena":"main","player_id":"p17","board_width":10,"board_height":20,"next_tick":48122}
```

---

## tetrisd internal thread model

```
listener_thread        accept() loop; spawns one client_thread per connection
client_thread[N]       owns a player: handshake → HTTTP loop → room_t mutations
ticker_thread[M]       one per active room: gravity, line clear, garbage, STATE broadcast
logshipper_thread      drains in-memory ring_buffer → SOCK_DGRAM to tetrislogd
ctl_listener_thread    separate Unix SOCK_STREAM for tetrisctl (isolated from TCP)
signal_thread          sigwaitinfo(): SIGTERM (shutdown), SIGHUP (reload), SIGUSR1 (dump)
```

**Signal masking:** At daemon start, all threads block every signal via `sigfillset` + `pthread_sigmask(SIG_BLOCK)` before spawning. `signal_thread` then calls `sigwaitinfo()` in a loop — all signals delivered synchronously, no async-signal-safety concerns anywhere else.

**Stack size:** Use `pthread_attr_setstacksize()` to reduce stack on simple threads (logshipper, signal_thread) to 64 KB. Default 8 MB × many threads = large virtual memory footprint.

### room_t locking rules

```c
/* ALWAYS:
 *   lock → do work → copy to local buffer → unlock → send()
 *
 * NEVER:
 *   lock → send()           (TCP backpressure stalls room)
 *   lock_room_A → lock_room_B  without acquiring in pointer-address order
 */
```

`room_t` is not a thread — it is a mutex-protected struct. A single `room_t` serves both players (Battle Royale). The `ticker_thread` injects garbage directly into the opponent's board under the same room mutex.

---

## Battle Royale garbage flow

```
ticker_thread (source room)
  → brain_tick() returns lines_cleared >= 2
  → build game_event_t { GE_GARBAGE_SENT, lines_cleared - 1, target_room_id }
  → mq_send(br_mq, O_NONBLOCK)   ← never blocks; drop on full
  → room->stats.garbage_drops++ on EWOULDBLOCK

ticker_thread (target room, next tick)
  → mq_receive(br_mq, O_NONBLOCK)  ← drain events addressed to this room
  → brain_inject_garbage(room->board, line_count)  under room->mutex
```

Queue name: `/tetris-garbage` (configurable in `.tetrishrc`). Bounded buffer — explicit drop semantics are the correct behaviour. Never retry.

---

## Logging pipeline

```
[any thread in tetrisd / chatd / marketd]
  log_push(level, fmt, ...)
    → ring_buffer_push()     ← O(1), non-blocking, drops on full, increments drop counter

[logshipper_thread]
  → drain ring_buffer
  → sendto(logd_sock, record, MSG_DONTWAIT)
    → if ENOENT / ECONNREFUSED: increment logd_drops, continue

[tetrislogd]
  → recv on Unix SOCK_DGRAM
  → write to log file
  → on SIGHUP: reopen log file (log rotation)
  → on SIGTERM: flush, close, exit
  → on tetrisd restart: the socket just reconnects automatically (DGRAM is connectionless)
```

tetrislogd must survive a tetrisd restart without dying — SOCK_DGRAM is connectionless, so when tetrisd comes back and starts sending again, tetrislogd simply receives the new datagrams.

---

## game_event.h schema

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
    /* append-only after Week 4 freeze — never renumber */
} game_event_type_t;

typedef struct {
    uint8_t  version;
    uint8_t  event_type;       /* game_event_type_t */
    uint32_t seq;              /* monotonic per-room sequence */
    uint32_t room_id;
    uint32_t player_id;
    uint16_t dest_mask;        /* GE_DEST_CHATD | GE_DEST_MARKETD bitmask */
    union {
        struct { uint8_t lines; uint32_t target_room_id; } garbage;
        struct { uint8_t ability_id; uint32_t target_player_id; } ability;
        struct { uint32_t points; } points_earned;
        struct { uint32_t item_id; } item;
    } payload;
} game_event_t;

#define GE_DEST_CHATD   (1 << 0)
#define GE_DEST_MARKETD (1 << 1)
```

`tetrisd/event_pub.c` publishes; `chatd` and `marketd` consume via their own `event_consumer_thread`.

---

## TetriSocial IPC detail

### tetrisu ↔ chatd
`tetrisu` maintains **two concurrent TCP connections**: one to `tetrisd` (game) and one to `chatd` (chat). Independent HTTTP sessions. A two-producer / one-consumer POSIX message queue mediates delivery from the `chatd` network thread to the `tetrisu` render thread.

### tetrisd ↔ marketd (loadout query at JOIN time)
At JOIN, `tetrisd/loadout.c` makes a **synchronous** request over Unix `SOCK_STREAM` to `marketd/loadout_server.c`. Wire format defined in `include/loadout_ipc.h`. This is the only blocking IPC call from tetrisd — it happens once per player join, not in the game loop.

### marketd ↔ tetrisd (async equip update)
When a player equips a new character or ability via the marketplace TUI, `marketd` publishes a `GE_CHARACTER_EQUIPPED` event back to `tetrisd` via POSIX mq (separate queue from Battle Royale). `tetrisd` updates the player's loadout in `room_t` on the next client_thread iteration.

---

## Tetris Battle Gaiden — ability mechanics

All ability flags live in `room_t` inside `tetrisd`. `libtetrisbrain` never sees them.

| Ability | Flag set by | Where enforced |
|---------|-------------|----------------|
| Garbage Surge (+3 lines to target) | `tetrisd/ability.c` | Writes to Battle Royale mq directly |
| Shield (block incoming garbage) | Sets `player.shield_ticks = N` | `ticker_thread` skips `brain_inject_garbage()` while `shield_ticks > 0` |
| Freeze (target can't move for 2s) | Sets `player.frozen = true` | `client_thread` drops MOVE requests while `frozen == true`; timer clears it |

Ability activation flow:
```
tetrisu → HTTTP ABILITY frame
  → tetrisd/client_thread receives it
  → tetrisd/ability.c validates: query marketd loadout (owns + equipped?)
  → if valid: apply flags in room_t under room->mutex
  → publish GE_ABILITY_USED to game_event pipeline (chatd narrates, marketd charges)
  → broadcast STATE
```

---

## .tetrishrc required directives

| Directive | Purpose |
|-----------|---------|
| `listen_port` | TCP port for client connections |
| `cert_path` | Server X.509 certificate |
| `key_path` | Server private key |
| `ca_path` | CA certificate bundle |
| `log_path` | Where tetrislogd writes log records |
| `log_ipc` | IPC address between tetrisd and tetrislogd (Unix socket path or mq name) |

Optional: `max_rooms`, `max_players_per_room`, `tick_hz`, `log_level`, `ctl_socket_path`, `chat_socket_path`, `market_socket_path`

---

## Key design decisions and rationale

| Decision | Rationale |
|----------|-----------|
| Thread-per-client over epoll | Epoll synchronisation complexity too high for 2-person team under time pressure. Linear thread growth is fine for 10–20 concurrent users. |
| Not master+worker-process | Extremely difficult to debug across process boundaries, especially with valgrind/helgrind. |
| SOCK_DGRAM for log shipping | Datagram preserves record boundaries; MSG_DONTWAIT in one syscall; connectionless = survives tetrislogd restarts automatically. |
| POSIX mq for Battle Royale | Built-in bounded buffer with explicit drop semantics; O_NONBLOCK never blocks the game loop. |
| SOCK_STREAM for control plane | Needs request/response semantics; completely separate from public TCP so it works under TCP flood. |
| libtetrisbrain is pure | Exhaustively unit-testable; compiles without linking OpenSSL or pthreads; cannot cause race conditions. |
| Second TCP socket for chat | Chat failures can't interfere with game frame delivery; chatd can be deployed independently. |
