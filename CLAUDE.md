# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the CoreStack Challenge (50.003 × 50.005) at SUTD. The project is in early development — currently only `libtetrisbrain` is partially implemented.

## Build & Test

Each library is **self-contained**: it owns a Makefile that builds its archive
(`libXXX.a`) in place and runs its own tests. There is no root Makefile yet —
build and test each library directly. A top-level umbrella `Makefile` will be
added to recurse into the libraries and link their `.a` files into the binaries
once those exist.

```bash
# Build a library standalone (produces lib/libtetrisbrain/libtetrisbrain.a):
make -C lib/libtetrisbrain
make -C lib/libtetrisbrain clean   # remove objects + test binaries
make -C lib/libtetrisbrain fclean  # also remove the .a archive
make -C lib/libtetrisbrain re      # fclean + rebuild

# Run that library's own unit tests (formatted output):
make -C lib/libtetrisbrain test
make -C lib/libtetrisbrain test FILTER=abilities   # run a single suite
```

**Linking a library into other code** — add the archive and its header path:

```bash
gcc ... lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include ...
```

The networked binaries (`tetrish`, `tetrisd`, `tetrisu`, ...) will each link the
library archives they need plus OpenSSL (`-lssl -lcrypto`).

## Architecture

Three layers above the kernel:

```
HTTTP (application protocol)
Secure session (cert auth, RSA-OAEP key exchange, AES-256 frames)
TCP (POSIX sockets)
```

**Binaries:**

- `tetrish` — interactive shell (REPL, builtins, `.tetrishrc`, daemon lifecycle via `dspawn`/`dcheck`)
- `tetrisd` — concurrent game server; server-authoritative; manages rooms, game logic, clients
- `tetrislogd` — separate logger process; receives log records over IPC; survives `tetrisd` restarts
- `tetrisctl` — admin CLI; talks to `tetrisd` over a local-only control-plane IPC channel (not the public TCP port)
- `tetrisu` — terminal client; renders board, handles input + network simultaneously

**Libraries (statically linked):**

Each library is a self-contained directory with its own `Makefile`, `src/`,
`include/`, and `tests/`, and builds into `lib/libXXX/libXXX.a` (see layout below).

- `libtetrisbrain/` — pure game logic (no I/O, no networking); linked into `tetrisd` and optionally `tetrisu` for client-side prediction
- `libtetrissh/` — secure session handshake and encrypted framing; linked into both `tetrisd` and `tetrisu`
- `libhtttp/` — HTTTP parser and serialiser; linked into both `tetrisd` and `tetrisu`
- `libcoreipc/` — Unix socket helpers, POSIX mq helpers, ring buffer, non-blocking IPC utilities
- `libchatcore/` — chat room registry, token-bucket rate limiter, roles, game-event formatter
- `libcoredb/` — append-only account DB for auth, points, inventory, equipped theme, and high score; `marketd` owns durable account/market state by calling it

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

The header lives at `lib/libtetrisbrain/include/tetrisbrain.h` and declares all modules. Implement each in its own `.c` file under `lib/libtetrisbrain/src/`:

| File | Responsibility |
|---|---|
| `board.c` | `board_init`, `board_get/set`, `board_in_bounds`, `board_inject_garbage`, `board_copy` — **done** |
| `pieces.c` | `piece_spawn`, `piece_is_valid`, `piece_move`, `piece_rotate`, `piece_stamp` |
| `gravity.c` | `gravity_tick`, `piece_soft_drop`, `piece_hard_drop` |
| `lineclear.c` | `board_clear_lines` (returns lines cleared 0–4) |
| `scoring.c` | `score_on_clear`, `level_from_lines`, `gravity_interval_ms` |
| `abilities.c` | Board manipulation for Battle Royale abilities |

`brain_result_t` return codes: `BRAIN_OK`, `BRAIN_BLOCKED`, `BRAIN_LOCKED`, `BRAIN_GAME_OVER`, `BRAIN_CLEARED`.

Out-of-bounds reads via `board_get` return `CELL_FILLED` (solid wall), so collision checks work uniformly without range guards in every caller.

## Key Design Constraints

- `common.c`/`common.h` (PA2 crypto primitives) are **not modified** — all crypto goes through them.
- No TLS, no `SSL_*` API — handshake is implemented manually in `libtetrissh`.
- `libtetrisbrain` has **no I/O, no side effects** — pure logic only.
- No hard-coded paths anywhere; all paths come from `.tetrishrc`.
- `tetrislogd` and `tetrisd` communicate over IPC with a non-blocking ring buffer — log records are dropped (not blocked) under pressure.
- No mutex held across a blocking syscall. Lock acquisition order must be documented and strictly followed to prevent deadlocks.
- Never call `libcoredb` while holding `room->mutex`; copy request data, unlock the room, then call `marketd`/`coredb` so disk I/O cannot stall game state.
- `libcoredb` writes append-only records first, flushes, then updates its in-memory hash table under its DB mutex.
- `libcoredb` stores salted password hashes only; use OpenSSL PBKDF2/RAND primitives, never raw passwords or custom crypto.
- Frame size cap: 64 KiB. HTTTP messages exceeding this → `413 Payload Too Large`.

## HTTTP Protocol

Custom HTTP-like protocol. Only `STATE` is server-originated (pushed); all other methods are client-initiated request/response. The `Player-Id` header is required on every authenticated request. See README.md for the full grammar and method table.
