# MacMini_tetriSH

> A terminal-based Battle Royale Tetris system written in C — combining a custom Unix shell, concurrent daemon processes, authenticated encrypted networking, and a bespoke application-layer protocol (HTTTP).

Part of the **CoreStack Challenge** (50.003 × 50.005), Singapore University of Technology and Design.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Components](#components)
- [Libraries](#libraries)
- [Protocol: HTTTP](#protocol-htttp)
- [Secure Session: libtetrissh](#secure-session-libtetrissh)
- [IPC Design](#ipc-design)
- [Concurrency Model](#concurrency-model)
- [Battle Royale Mode](#battle-royale-mode)
- [Configuration: .tetrishrc](#configuration-tetrishrc)
- [File System Layout](#file-system-layout)
- [Build Instructions](#build-instructions)
- [Running the System](#running-the-system)
- [Security Assumptions](#security-assumptions)
- [Known Limitations](#known-limitations)

---

## Overview

tetriSH is a single integrated project that merges:

- **PA1** — shell, daemon lifecycle, process management, signals, and IPC
- **PA2** — authenticated and confidential client-server communication

The result is a fully terminal-based, server-authoritative Battle Royale Tetris game. All game state lives in `tetrisd`. Clients (`tetrisu`) connect over TCP, complete a secure session handshake, and communicate exclusively via HTTTP — a custom HTTP-like application protocol.

---

## Architecture

The system has exactly three layers above the kernel:

```
+---------------------------------------------+
|  Application: HTTTP messages                |
|  (HyperText Tetris Transfer Protocol)       |
+---------------------------------------------+
|  Secure session                             |
|  (cert auth, RSA-wrapped AES, framed)       |
+---------------------------------------------+
|  Transport: TCP via POSIX sockets           |
+---------------------------------------------+
```

TCP reliability, ordering, and congestion control are provided by the kernel. tetriSH implements the two layers above it.

---

## Components

| Binary | Role |
|---|---|
| `tetrish` | Interactive shell — reads `.tetrishrc`, launches daemons, entry point for the user |
| `tetrisd` | Concurrent game server — accepts clients, manages rooms, runs game logic |
| `tetrislogd` | Dedicated logger daemon — receives log records over IPC and writes to disk |
| `tetrisctl` | Admin CLI — issues control commands to a running `tetrisd` over a local IPC channel |
| `tetrisu` | Terminal game client — connects, completes secure handshake, renders board, reads input |

### tetrish

`tetrish` is the entry shell. It implements the full PA1 REPL: `fork()` + `execvp()`, builtins (`cd`, `help`, `exit`, `usage`, `env`, `setenv`, `unsetenv`), `.tetrishrc` execution on startup, and background process tracking via `sys`, `dspawn`, and `dcheck`. From inside `tetrish`, users can launch `tetrisd` and `tetrislogd` in the background, run `tetrisctl` queries, or start `tetrisu` to play.

### tetrisd

`tetrisd` is the game daemon. When launched in background from `tetrish`, it:
- Detaches from the controlling terminal
- Binds to the TCP port from `.tetrishrc`
- Accepts multiple concurrent clients
- Establishes a secure session (via `libtetrissh`) before any HTTTP traffic
- Parses and serialises HTTTP messages (via `libhtttp`)
- Maintains rooms with multiple players, runs game logic (via `libtetrisbrain`), and broadcasts STATE
- Handles `SIGTERM` (graceful shutdown), `SIGHUP` (reload config), `SIGUSR1` (dump state to log)
- Ignores `SIGPIPE`; detects broken connections via `EPIPE` on `write()`
- Forwards all log records to `tetrislogd` over IPC using a non-blocking ring buffer
- Exposes a control plane to `tetrisctl` via a separate local-only IPC channel

### tetrislogd

`tetrislogd` is a separate process (not a thread inside `tetrisd`). It:
- Accepts log records from `tetrisd` over IPC
- Writes records to the log file specified in `.tetrishrc`
- Maintains a dropped-records counter and emits a summary periodically (e.g. `dropped 47 records in last 30s`)
- Handles `SIGTERM` (flush, close, exit) and `SIGHUP` (reopen log file for rotation)
- Survives `tetrisd` restarts without exiting — it accepts reconnections

### tetrisctl

`tetrisctl` is an admin CLI binary that issues commands to a running `tetrisd` over a local-only IPC channel (not the public TCP port). This ensures the control plane remains available even when the public listener is saturated. At minimum it supports `status` and `shutdown`.

### tetrisu

`tetrisu` is the terminal game client. It:
- Connects over TCP and completes the secure session handshake (via `libtetrissh`)
- Sends HTTTP requests for game actions (via `libhtttp`)
- Receives and renders server-pushed STATE frames
- Handles keyboard input non-blocking (input + network simultaneously)
- Exits cleanly on `q` or `SIGINT`

---

## Libraries

| Library | Role |
|---|---|
| `libtetrissh` | Secure session: cert auth, RSA-wrapped AES key exchange, encrypted framing |
| `libhtttp` | HTTTP protocol parser and serialiser |
| `libtetrisbrain` | Tetris game logic: board, pieces, gravity, rotation, line clear, scoring |

All libraries are statically linked into the binaries that use them.

### libtetrissh

`libtetrissh` implements the secure session handshake. It is linked into both `tetrisd` (server-side) and `tetrisu` (client-side) to prevent protocol drift. `libtetrissh` is also part of the shared CoreStack library for use by the 50.003 application.

Key API surface (exact signatures defined in `include/tetrissh.h`):
- `session_handshake_server()` — server-side handshake
- `session_handshake_client()` — client-side handshake
- `session_send()` / `session_recv()` — encrypted framed I/O
- `session_close()` — teardown and resource cleanup

### libhtttp

`libhtttp` parses incoming HTTTP messages and serialises outgoing ones. Both `tetrisd` and `tetrisu` link against it. See the [Protocol](#protocol-htttp) section for the fixed wire format.

### libtetrisbrain

`libtetrisbrain` implements Tetris game rules with no I/O, no networking, and no side-effects — pure logic. `tetrisd` links against it for server-authoritative game state. `tetrisu` may optionally link against it for client-side prediction.

---

## Protocol: HTTTP

HTTTP (HyperText Tetris Transfer Protocol) is the application-layer protocol. Its wire format is fixed.

### Message Grammar

```
REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
RESPONSE      ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE   ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF
```

### Required Methods

| Method | Path | Purpose |
|---|---|---|
| `JOIN` | `/room/<id>` | Join or create a room |
| `LEAVE` | `/room/<id>` | Leave a room |
| `START` | `/room/<id>` | Begin the game (room owner only) |
| `MOVE` | `/room/<id>/player/<pid>` | Body: `LEFT` or `RIGHT` |
| `ROTATE` | `/room/<id>/player/<pid>` | Body: `CW` or `CCW` |
| `DROP` | `/room/<id>/player/<pid>` | Body: `SOFT` or `HARD` |
| `STATE` | `/room/<id>` | Server-originated — pushed broadcast of board state |

`STATE` is the only server-originated message. Clients must read these unprompted while interleaving with their own request-response cycles.

### Required Status Codes

`200`, `201`, `400`, `401`, `403`, `404`, `409`, `429`, `500`

### Required Headers

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server STATE broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

---

## Secure Session: libtetrissh

The handshake sequence before any HTTTP traffic:

1. Client connects, sends a fresh nonce
2. Server sends its X.509 certificate
3. Client verifies the certificate against the bundled CA (`cacsertificate.crt`)
4. Server signs the client nonce with its private key (RSA-PSS)
5. Client verifies the signature using the public key from the certificate
6. Client generates a 32-byte AES-256 session key, RSA-OAEP encrypts it with the server's public key, sends it
7. From this point on, every frame is `[4-byte big-endian length][AES ciphertext]` carrying one HTTTP message

Frame size limit: 64 KiB. Larger HTTTP messages must be split by the application or rejected with `413 Payload Too Large`.

Cryptographic primitives come exclusively from `common.c` (PA2). `common.c` and `common.h` are not modified. No TLS, no `SSL_*` API, no reverse proxy.

---

## IPC Design

### tetrisd ↔ tetrislogd (Log Forwarding)

**Mechanism:** [document your chosen mechanism: Unix domain socket / POSIX message queue / named pipe / shared memory ring buffer]

**Wire format:** [document: line-based / length-prefixed binary / JSON / custom]

**Non-blocking guarantee:** Game-critical threads enqueue log records into an in-process ring buffer. A dedicated log-shipper thread drains the buffer and forwards to `tetrislogd` over IPC. If the buffer is full, records are dropped and the drop counter is incremented. The drop counter is exposed via `tetrisctl dropped-logs`.

**Lifecycle:** [document startup order: independent launch vs. tetrisd spawning tetrislogd, reconnection logic if tetrislogd restarts]

### tetrisd ↔ tetrisctl (Control Plane)

**Mechanism:** [document: Unix domain socket path from `.tetrishrc` / named pipe / POSIX message queue / signals + state file]

**Wire format:** [document the control plane framing]

The control plane listens on a separate socket from the public TCP port. `tetrisctl shutdown` works even when the public listener is overwhelmed.

### tetrisd ↔ tetrisd rooms (Battle Royale Garbage)

**Mechanism:** [document: POSIX shared memory + semaphore / POSIX message queue / Unix domain socket]

Garbage transfer between rooms is server-side managed. Direct function calls across room boundaries while holding another room's mutex are not used.

---

## Concurrency Model

**Thread model:** [document your chosen model — thread-per-client / event loop with epoll / master + per-room workers]

### Lock Discipline

| Mutex | Protects |
|---|---|
| `room_t.mutex` | Room state: board, player list, current tick, garbage queue |
| `log_buffer.mutex` | In-process log ring buffer |
| [add your mutexes] | [what they protect] |

**Global lock acquisition order:** [document the strict ordering — e.g. `room_mutex` must always be acquired before `player_mutex`]

No mutex is held across a blocking syscall. If `fork()` is called from a multi-threaded process, the child calls `execve()` immediately.

---

## Battle Royale Mode

When a player clears N ≥ 2 lines in a single move, N − 1 garbage rows are inserted at the bottom of a randomly selected other player's board in a different room.

- Garbage transfer is managed server-side via IPC (not direct function calls across room boundaries)
- The targeting room is selected at the time of line clear
- Garbage injection in the receiving room is synchronised under that room's mutex

---

## Configuration: .tetrishrc

Required directives:

```
listen_port  <port>           # TCP port for tetrisd
cert_path    <path>           # Server certificate
key_path     <path>           # Server private key
ca_path      <path>           # CA certificate for client verification
log_path     <path>           # Path where tetrislogd writes log records
log_ipc      <address>        # IPC address between tetrisd and tetrislogd
```

Optional directives (with sensible defaults if absent):

```
max_rooms              <n>
max_players_per_room   <n>
tick_hz                <n>
log_level              <debug|info|warning|error>
ctl_socket             <path>   # Control plane socket path
prompt                 <string>
```

All paths are relative to the project root. No hard-coded paths exist in the source.

---

## File System Layout

Each library is a **self-contained directory** — it owns its `Makefile`,
`src/`, `include/`, and `tests/`, and builds its archive (`libXXX.a`) in place.
A top-level umbrella `Makefile` will be added to recurse into the libraries and
link the archives into the binaries as those land; for now each library builds
and is tested on its own.

```
project/
    bin/
        tetrish
        tetrisd
        tetrislogd
        tetrisctl
        tetrisu
    lib/                           ← all self-contained libraries live here
        libtetrisbrain/            ← self-contained library (pattern for all libs)
            Makefile               ← make -C lib/libtetrisbrain [test|clean|fclean|re]
            include/tetrisbrain.h  ← public header (-I lib/libtetrisbrain/include)
            src/*.c                ← board, pieces, gravity, lineclear, scoring, abilities
            tests/test_*.c         ← unit tests (each with its own main)
            scripts/run_tests.sh   ← formatted test runner
            obj/                   ← generated objects
            libtetrisbrain.a       ← generated archive
        libtetrissh/               ← same self-contained layout
        libhtttp/                  ← same self-contained layout
    auth/
        server.crt
        server.key
        cacsertificate.crt
        generate_keys.sh
    sample.tetrishrc
    var/
        log/        (created at runtime)
        run/        (sock, pid files)
    src/
        tetrish/
        tetrisd/
        tetrislogd/
        tetrisctl/
        tetrisu/
    include/                   ← cross-component shared headers only
    Makefile                   ← umbrella, recurses into each library (planned)
    README.md
```

---

## Build Instructions

```bash
# Clone the repository
git clone <repo-url>
cd tetriSH

# Generate certificates (first time only)
bash auth/generate_keys.sh
```

Requirements: `gcc`, `make`, `OpenSSL` (libssl + libcrypto), `notcurses` (+ `pkg-config`, pulls in `ffmpeg`) for `tetrisu`, POSIX-compliant OS (Linux recommended).

Each library is self-contained and builds and tests on its own — this is the current build path:

```bash
make -C lib/libtetrisbrain          # build lib/libtetrisbrain/libtetrisbrain.a
make -C lib/libtetrisbrain test     # run its unit tests (formatted output)
make -C lib/libtetrisbrain clean    # remove its objects + test binaries
```

Once the binaries are added, a top-level umbrella `Makefile` will build everything from the repo root:

```bash
make             # build all libraries + binaries
make clean && make   # clean build
```

To compile your own code against a library, link the archive and add its include path:

```bash
gcc my_program.c lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include -o my_program
```

---

## Running the System

**1. Copy and edit the config:**
```bash
cp sample.tetrishrc .tetrishrc
# Edit listen_port, cert_path, key_path, ca_path, log_path, log_ipc
```

**2. Start the shell:**
```bash
./bin/tetrish
```

**3. From inside tetrish, launch the daemons:**
```
tetrish$ dspawn tetrislogd &
tetrish$ dspawn tetrisd &
```

**4. Connect a client (in a separate terminal):**
```bash
./bin/tetrisu
```

**5. Check server status:**
```bash
./bin/tetrisctl status
```

**6. Graceful shutdown:**
```bash
./bin/tetrisctl shutdown
```

---

## Security Assumptions

- The CA certificate (`cacsertificate.crt`) is trusted and bundled at compile time; no dynamic CA update path exists
- The server certificate and private key are stored on the file system and paths are configured in `.tetrishrc`; access control is the operator's responsibility
- RSA-PSS is used for the server signature over the client nonce; RSA-OAEP is used for AES key wrapping
- Every post-handshake frame is AES-256 encrypted (CBC or GCM — [document your choice])
- No replay protection beyond the session nonce is implemented in the baseline; per-session monotonic counters are an optional extension
- The control plane (`tetrisctl`) is local-only (Unix domain socket / named pipe); it is not exposed over the network

---

## Known Limitations

- [Document any known bugs or incomplete features here before submission]
- Battle Royale targeting is uniformly random; no score-based targeting in the baseline
- No client-side reconnection after a dropped session
- Log records may be dropped under sustained high load; the drop counter is observable via `tetrisctl dropped-logs`
- Frame size is capped at 64 KiB; HTTTP messages exceeding this are rejected with `413`
- `tetrisu` rendering assumes a terminal width of at least 80 columns

---

## Authors

| Role | Member | Owns |
|---|---|---|
| Systems | [Name] | tetrish, tetrisd process model, concurrency, tetrislogd, tetrisctl, signal handling, IPC channels |
| Networking & Security | [Name] | libtetrissh, libhtttp, secure session correctness, HTTTP parser/serialiser, threat model, request dispatch |
| Application & Integration | [Name] | tetrisu, libtetrisbrain, room lifecycle, game loop, build system, integration tests |

---

*50.003 × 50.005 CoreStack Challenge — SUTD, Summer 2026*
