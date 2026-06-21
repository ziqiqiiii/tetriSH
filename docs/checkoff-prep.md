# docs/checkoff-prep.md — Live Checkoff Preparation

> **Usage:** Read this before the checkoff. Use it to prepare answers. Not loaded on every agent request.

---

## Checkoff structure

| Phase | Duration | What happens |
|-------|----------|-------------|
| Baseline demo | ~20 min | Run from clean checkout — demonstrate full system |
| Systems inspection | ~20 min | Instructor inspects IPC, logs, failure handling, plays the game |
| Live extension | 20 min build + 5 min walkthrough | Implement one task from the pool below |
| Live Q&A | 30 min | 6 questions, each member on their declared role |

### Q&A multiplier scheme

| Performance | Multiplier |
|-------------|-----------|
| Strong answer (specific, code-grounded) | ×1.00 |
| Weak/vague answer | ×0.85 |
| Cannot answer | ×0.70 |

Multipliers compound. Three "cannot answer" results → ~×0.34 of Q&A mark. Bullshitting that gets caught on follow-up is treated as "cannot answer".

**Honesty note:** "I don't know, but here's how I'd find out in my code" is worth more than a confident wrong answer.

---

## Baseline MVP requirements (must pass to avoid zero)

- Player can connect using `tetrisu`
- Client completes the secure session handshake **before** any HTTTP traffic
- Player can JOIN a room and START a game
- A Tetris piece falls over time (server is ticking)
- Player can move left/right, rotate, soft drop, hard drop
- Pieces lock on bottom collision or piece collision
- Completed lines are cleared; score/line count updates
- Game detects game over
- Server sends STATE messages; `tetrisu` renders the board in the terminal
- Client quits cleanly
- Server logs game events through `tetrislogd`

**The server is authoritative.** No local-only board updates.

---

## Live extension task pool

Prepare for any of these. The architecture should make all of them a small diff.

### Networking and Security (Zi Qi)
- Add a new HTTTP method (e.g. `PAUSE` on `/room/<id>`) — new parser case, dispatch entry, reachable `409`, log entry
- Add a new required header and reject requests missing it with the correct status code
- Add `Retry-After` on every `429` response and have the client honour it
- Swap RSA-OAEP for RSA-PKCS1v15 (or vice versa) — explain the security difference
- Add HMAC over the ciphertext frame and verify before decrypting
- Add per-session replay protection: monotonic counter header, server rejects out-of-order/duplicate with `400`

### Systems (Sanjan)
- Add a new `tetrisctl` subcommand returning a structured snapshot of one room
- Add `SIGUSR2` to `tetrisd` for a one-shot full state dump to log without restart
- Implement size-based log rotation in `tetrislogd` without dropping records during rotation
- Add a per-IP connection limit in `.tetrishrc`, enforced at `accept()`
- Implement `tetrisctl drain`: stop accepting new connections, let in-flight games finish, then shutdown cleanly
- Add a `kick <player>` admin command that closes the session and broadcasts the room update

### Application and Integration (Both)
- Add a hold-piece feature (one swap per piece, preview rendered in `tetrisu`)
- Add a ghost piece projection at the landing position (decide: does the server need to send extra state?)
- Switch rotation system (SRS ↔ classic) — demonstrate a kick that worked before but doesn't after
- Change Battle Royale targeting from "random other room" to "room with highest current score"
- Add `spectate` mode in `tetrisu` — subscribe to a room without joining as a player

---

## Q&A question pool with preparation notes

### Concurrency and lifecycle

**Q: Walk me through what happens, line by line, when a client sends MOVE LEFT while the room ticker is mid-gravity. Which lock do you hold? In what order?**

> Preparation: Be able to trace from `client_thread` receiving the MOVE HTTTP frame → calling `room_apply_move()` → acquiring `room->mutex` → calling `brain_apply_move()` → releasing mutex. If `ticker_thread` holds the mutex at that moment, `client_thread` blocks in `pthread_mutex_lock`. No deadlock because both only hold `room->mutex` (one lock, no ordering issue). Ticker releases after its tick, client gets the lock. Mention that ticker NEVER calls `send()` while holding the mutex.

**Q: I kill -SIGTERM your tetrisd while a game is in progress. Walk me through the shutdown path.**

> Preparation: `signal_thread` receives SIGTERM via `sigwaitinfo()`. Sets a global `shutdown_flag`. `listener_thread` checks the flag on its next `accept()` wakeup and exits. Each `client_thread` checks the flag on its next HTTTP recv loop iteration — sends a `CONNECTION_CLOSING` STATE frame, then exits. `ticker_thread` checks the flag at the top of its tick loop and exits. `logshipper_thread` drains the ring buffer one last time, sends remaining records. Main thread joins all threads. Daemon exits. Show the shutdown_flag variable and where each thread reads it.

**Q: Show me where you handle a slow client. What happens to the room if one client stops reading?**

> Preparation: `client_thread` sets `SO_SNDTIMEO` on the socket. If `send()` times out (EAGAIN after timeout), the client is kicked — `client_thread` closes the fd, removes the player from `room_t` under `room->mutex`, and exits. The `ticker_thread` continues normally — it skips sending STATE to any player whose fd is -1 (sentinel for disconnected). The room keeps running.

**Q: What is your global lock acquisition order? Show me one site that respects it and one that could deadlock if you got the order wrong.**

> Preparation: Our order is by ascending pointer address. Show `tetrisd/room.c` where two rooms are locked for Battle Royale garbage. Show the comment that documents the order. Then explain: if Thread A holds `room_A->mutex` and tries to acquire `room_B->mutex`, while Thread B holds `room_B->mutex` and tries `room_A->mutex` — that's a deadlock. The pointer-address rule prevents it because both threads always acquire the lower-address lock first.

**Q: Your tetrislogd IPC channel fills up. Show me the line that decides to drop.**

> Preparation: Point to `ring_buffer_push()` in `libcoreipc/ring_buffer.c`. Show the atomic check: if `(tail + 1) % capacity == head`, the buffer is full, the function returns -1 (drop), and `atomic_fetch_add(&logd_drops, 1)` increments the counter. The counter is exposed via `tetrisctl dropped-logs`.

---

### Networking and security

**Q: Walk me through the handshake from accept() to the first encrypted HTTTP frame.**

> Preparation: `accept()` returns fd → `client_thread` calls `session_handshake_server(fd, &sess, cert_path, key_path)` → inside: recv 32-byte nonce, send DER cert, sign nonce with `EVP_DigestSign` (RSA-PSS SHA-256), send signature, recv `RSA_OAEP_decrypt()`'d AES-256 key, store in `sess->aes_key`. From this point, every HTTTP message goes through `session_send/recv` which AES-encrypts with `sess->aes_key`. Show the `session_t` struct and the key zeroing in `session_close()`.

**Q: A man-in-the-middle replays a MOVE frame from earlier in the session. What stops it?**

> Preparation: The AES session key is per-connection and generated fresh by the client each time. A replayed frame from a previous session uses a different key — it will fail to decrypt (EVP_DecryptFinal returns error). Within a session, AES-CBC/GCM with a per-frame IV (or counter mode) means replaying the same ciphertext in a different position is detectable. Point to the IV handling in `session_send`. If you use GCM, the auth tag catches any replay directly.

**Q: What attack does RSA-PSS prevent that RSA-PKCS1v15 signing does not?**

> Preparation: RSA-PKCS1v15 is deterministic — the same message always produces the same signature, which enables existential forgery attacks via chosen-message attacks (Bleichenbacher-style). RSA-PSS uses a random salt, making signatures probabilistic. RSA-PSS also has a tight security proof reducing to the hardness of RSA. PKCS1v15 has no such proof. In practice for this use case (signing a nonce), the salt makes RSA-PSS strictly more secure.

**Q: Show me where you free the X509* and EVP_PKEY* from the handshake. What happens if the handshake fails halfway?**

> Preparation: Show `session_handshake_server` in `libtetrissh/handshake.c`. The function uses `goto cleanup` on every error path. The `cleanup:` label calls `X509_free(cert)`, `EVP_PKEY_free(key)`, `EVP_MD_CTX_free(ctx)` — only for the ones that were allocated (each pointer is initialised to NULL, `free(NULL)` is a no-op). No resource leaks on partial handshakes.

---

### Protocol and parsing

**Q: A client sends a request with `\n` line endings instead of `\r\n`. What happens?**

> Preparation: HTTTP requires `\r\n`. Show the parser in `libhtttp/parser.c`. If it sees `\n` without `\r`, it returns `HTTTP_ERR_MALFORMED_HEADER`. `client_thread` sends a `400 Bad Request` response and continues reading (does not close the connection). Show the error response path.

**Q: A header has a value with a colon in it. How does your parser handle it?**

> Preparation: Show `htttp_parse_header_line()`. It splits on the **first** colon only: `char *colon = strchr(line, ':')`. Everything after the first colon is the value — including any additional colons. This correctly handles `Date: Mon, 10 Jun 2026 12:00:00 GMT` (the colons in the time are part of the value).

**Q: Two clients hit the same room with START simultaneously. What happens?**

> Preparation: Both client_threads try to acquire `room->mutex`. One gets it first, checks `room->state == ROOM_WAITING`, sets `room->state = ROOM_STARTING`, releases mutex. Second thread acquires mutex, checks state — it's now `ROOM_STARTING`, not `ROOM_WAITING` — returns `409 Conflict`. Show the `room->state` enum and the check inside the START handler.

---

### IPC and control plane

**Q: I run tetrisctl shutdown while the public TCP listener is being flooded. Why does the control command still work?**

> Preparation: `ctl_listener_thread` listens on a **completely separate** Unix `SOCK_STREAM` socket (path from `.tetrishrc`, under `var/run/`). This socket is independent of the public TCP port. TCP flood on port 8888 does not affect a Unix socket. `tetrisctl` connects to the Unix socket, sends the `shutdown` command, `ctl_listener_thread` receives it, sets `shutdown_flag`. The TCP listener is irrelevant.

**Q: Your tetrislogd dies. What does tetrisd do?**

> Preparation: `logshipper_thread` calls `sendto(logd_sock, ...)`. If tetrislogd dies, the socket path disappears — `sendto` returns -1 with `errno == ENOENT`. `logshipper_thread` increments `logd_drops`, does not retry, does not block. The ring buffer continues filling; records are dropped when it's full. When tetrislogd comes back and recreates the socket, `sendto` starts succeeding again automatically — no reconnect logic needed because SOCK_DGRAM is connectionless.

---

### Application and game logic

**Q: Show me the line clear function. Walk through it on a board where rows 18 and 20 are full.**

> Preparation: Show `lib/libtetrisbrain/src/lineclear.c`. The function scans from the top row down. Rows 18 and 20 are full (`all CELL_FILLED`). The function removes them by shifting all rows above each cleared row down by one. Order matters: row 20 is removed first (bottom up), shifting rows 0–19 down by 1. Then row 18 (now effectively row 18 again) is removed, shifting rows 0–17 down. Two blank rows are inserted at the top. Returns `lines_cleared = 2`.

**Q: A garbage row arrives from another room while the local ticker is mid-tick. What synchronises this?**

> Preparation: The local `ticker_thread` owns `room->mutex` for the full tick. It drains the POSIX mq and calls `brain_inject_garbage()` while holding the lock. The garbage events are pulled from the mq **by the same thread** that runs the tick — there's no separate thread reading the mq. So there's no race: the ticker reads the mq at the start of each tick cycle, injects any pending garbage, then runs gravity. The mq itself is a bounded buffer — concurrent `mq_send` from other rooms' tickers is safe (mq is kernel-managed).

---

## Prize eligibility requirements (full list)

- Complete baseline MVP
- Complete Battle Royale mode
- Pass live demo and live Q&A
- All members demonstrate ownership of declared roles
- Builds from clean checkout
- No academic integrity issues
- Tagged release bundle (`v1.0-br`) — build instructions, sample `.tetrishrc`, certs, install steps — that any cohort group can clone and run without assistance
- Survive cohort-scale playtest (~10–20+ concurrent connections, ill-behaved clients, malformed HTTTP, slow readers, half-open handshakes)
- Project server CLI in front of cohort, live-log sessions, peek into rooms, manage live users

### Prize consideration criteria

| Criterion | What the instructor looks for |
|-----------|------------------------------|
| Correctness | System works reliably during the demo |
| Architecture | Process model, library boundaries, IPC choices, control plane are clean and defensible |
| Code quality | Readable, modular, maintainable, proper error handling |
| Systems depth | Processes, signals, IPC, concurrency, shutdown behaviour |
| Security depth | Session implementation correct, threat model understood |
| Protocol robustness | HTTTP parsing, status handling, framing, malformed input |
| Application quality | Terminal client is usable, game logic is stable |
| Testing + debugging | Evidence of testing, logging, debugging discipline |
| Q&A performance | All members demonstrate ownership |
| Extensions | Battle Royale, admin controls, observability, tests — well-integrated |

A smaller but cleaner and better-understood system may beat a larger but fragile one.
