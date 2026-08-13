# Architecture

How tetriSH is put together: the layers, the binaries, the libraries, the
protocol, and where the source lives. Setup and play instructions are in
[`README.md`](../README.md); each component's own `README.md` is the authority
on its scope and API.

---

## Table of Contents

- [Status](#status)
- [Layers](#layers)
- [Binaries](#binaries)
- [Libraries](#libraries)
- [Protocol: HTTTP](#protocol-htttp)
- [Configuration: .tetrishrc](#configuration-tetrishrc)
- [Project Structure](#project-structure)

---

## Status

| Component | Status |
|---|---|
| `lib/*` | Implemented — all eight libraries, unit tested, valgrind-clean |
| `src/tetrish` | Implemented — REPL, builtins, `.tetrishrc`, system programs under `bin/` |
| `src/tetrisd` | Implemented — Single and Double end to end; Battle Royale designed and unbuilt (Linux only) |
| `src/tetrislogd` | Implemented — receives, validates, writes, rotates on `SIGHUP` |
| `src/tetrisctl` | Partial — `start`/`status`/`stop`/`restart` by pidfile and signal; control socket is a later step |
| `src/tetrisu` | Playable — Solo, Double, Battle Royale; Battle Royale's rivals still modelled in-process |

---

## Layers

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

TCP reliability, ordering, and congestion control come from the kernel; tetriSH
implements the two layers above it.

- **Secure session** — `libtetrissh` runs on both ends, so handshake behaviour
  cannot drift: client nonce, server certificate plus RSA-PSS/SHA-256 signature
  over it, then an AES-256 key wrapped with RSA-OAEP/SHA-256. Every frame after
  it is `frame_len[4] || nonce[12] || tag[16] || ciphertext` under AES-256-GCM,
  with per-direction sequence counters as AAD so replays fail tag verification.
- **One owner of game state** — `tetrisd` is event-driven: one reactor thread in
  `epoll_wait` owns the listener, every connection, the lobby, the rooms, the
  games and every outbox, and gravity is one `timerfd` rather than a thread per
  room. Beside it run only a handshake worker pool and a log shipper, which are
  what the two surviving locks guard.
- **Processes and IPC** — `tetrisd` ships log records to `tetrislogd` through
  `libcoreipc`'s non-blocking ring buffer, dropped rather than blocked when
  full. The three counters are not interchangeable: **Dropped** is `tetrisd`'s
  ring, **Rejected** is malformed on arrival, **Degraded** is valid with the
  sink unavailable.
- **Battle Royale** — one room of 4–99 slots, where clearing N ≥ 2 lines queues
  N − 1 garbage rows against another player in that same room, landing at that
  player's next piece lock. Designed and unbuilt on the server; `tetrisu` models
  the rivals in-process meanwhile.

Design constraints binding every component are the Invariants section of
[`CLAUDE.md`](../CLAUDE.md); rationale lives in the rest of [`docs/`](.).

---

## Binaries

| Binary | Role | Detail |
|---|---|---|
| `tetrish` | Interactive shell — REPL, `.tetrishrc`, system programs into `bin/`; its `dspawn`/`dcheck`/`dkill` daemonise arbitrary programs and are not the game daemons' manager | [README](../src/tetrish/README.md) |
| `tetrisd` | Server-authoritative game server — secure session before any HTTTP byte, rooms, game logic, chat, marketplace, `STATE` broadcast | [README](../src/tetrisd/README.md) |
| `tetrislogd` | Dedicated logger — a separate process, not a thread, so it survives game-server restarts; holds an exclusive `flock` on the log file | [README](../src/tetrislogd/README.md) |
| `tetrisctl` | Admin CLI — owns both daemons' lifecycle by pidfile and signal | [README](../src/tetrisctl/README.md) |
| `tetrisu` | Terminal client — connects, handshakes, renders the board, reads input; everything with a number in it comes from the server | [README](../src/tetrisu/README.md) |

---

## Libraries

Every library is a self-contained directory with its own `Makefile`,
`include/`, `src/`, and `tests/`, building `libXXX.a` in place for static
linking.

| Library | Role | Linked into |
|---|---|---|
| `libtetrisbrain` | Pure game logic: board, pieces, gravity, line clear, scoring, abilities, bag, charge, effects | `tetrisd`, optionally `tetrisu` |
| `libtetrisroom` | Pure lobby domain: seating, ownership succession, start verdicts, room listing | `tetrisd` |
| `libmacminidb` | In-memory player/character/theme store with an append-only log and crash recovery | `tetrisd` |
| `libtetrissh` | Secure session: cert auth, RSA-wrapped AES key exchange, encrypted framing | `tetrisd`, `tetrisu` |
| `libcoreipc` | IPC primitives: log records, ring buffer, `AF_UNIX` helpers, self-pipe, POSIX message queues | `tetrisd`, `tetrislogd` |
| `libcoredaemon` | Detach, readiness pipe and pidfile claim for the daemons; pidfile read and probe for the CLI | `tetrisd`, `tetrislogd`, `tetrisctl` |
| `libhtttp` | HTTTP parser, serialiser, validation, and method dispatch | `tetrisd`, `tetrisu` |
| `libstatusbody` | HTTTP message-body codec — `tetrisd` encodes, `tetrisu` decodes | `tetrisd`, `tetrisu` |

Each library builds, tests, and links on its own:

```bash
make -C lib/libtetrisbrain test
gcc my_program.c lib/libtetrisbrain/libtetrisbrain.a -I lib/libtetrisbrain/include -o my_program
```

Networked binaries additionally link OpenSSL (`-lssl -lcrypto`);
`libmacminidb` requires `-lpthread`.

---

## Protocol: HTTTP

HTTTP (HyperText Tetris Transfer Protocol) is the application-layer protocol.
Its wire format is fixed.

```
REQUEST       ::= REQUEST-LINE *(HEADER CRLF) CRLF [BODY]
REQUEST-LINE  ::= METHOD SP PATH SP "HTTTP/1.0" CRLF
RESPONSE      ::= STATUS-LINE *(HEADER CRLF) CRLF [BODY]
STATUS-LINE   ::= "HTTTP/1.0" SP STATUS-CODE SP REASON-PHRASE CRLF
```

Methods: `SIGNUP`, `LOGIN`, `LIST`, `JOIN`, `LEAVE`, `START`, `READY`, `CHAT`,
`MOVE`, `ROTATE`, `DROP`, `ABILITY`, `STATE`, `BUY`, `EQUIP`, `PROFILE`,
`LEADERBOARD`, plus the `tetrisctl` admin set.

Status codes: `200`, `201`, `400`, `401`, `403`, `404`, `409`, `413`, `429`,
`500`.

Required headers:

- `Content-Length` on every message with a body
- `Content-Type: application/tetris-command` on client requests with a body
- `Content-Type: application/tetris-state` on server `STATE` broadcasts
- `Player-Id` on every authenticated request
- `Date` on every response (RFC 1123 format)

`STATE` is always server-originated and `CHAT` travels both ways — up as a
command, down as a room's feed — so clients must read pushed frames unprompted
while interleaving their own request-response cycles. The full grammar and
method table live in [`lib/libhtttp/README.md`](../lib/libhtttp/README.md);
body formats in [`lib/libstatusbody/README.md`](../lib/libstatusbody/README.md).

---

## Configuration: .tetrishrc

`.tetrishrc` is the shell start-up file, executed one command per line; blank
lines and `#` lines are ignored. The shell reads the project-local file first,
then `$HOME/.tetrishrc`, and creates an empty project file if neither exists.
Set `$TETRISHRC` to override the path.

Its role at startup is to declare the daemons and launch them in dependency
order:

```
export TETRISCTL_DAEMONS="tetrislogd tetrisd"   # logger first, so it captures everything
tetrisctl start                                 # blocks until both are actually up
```

`TETRISCTL_DAEMONS` is the only place launch order is written down;
`tetrisctl stop` reverses it.

Daemon settings are `export` lines in the same file, so each is both an ordinary
shell command and a line the daemon parses at boot; `tetrisd` re-reads them on
`SIGHUP`:

```
export TETRISD_PORT=4242                             # TCP port
export TETRISD_DATA_DIR=tmp/tetrisd                  # player store
export TETRISD_CONFIG_DIR=lib/libmacminidb/config    # item catalogues
export TETRISD_CERT_PATH=certs/server.crt            # server certificate
export TETRISD_LOG_IPC=tmp/tetrisd/tetrislogd.sock   # tetrisd -> tetrislogd
export TETRISLOGD_LOG_PATH=tmp/tetrislogd/tetrislogd.log  # where records are written
export TETRISD_MAX_CLIENTS=64                        # connection limit
```

Every key is documented inline in [`.tetrishrc`](../.tetrishrc) itself — that
file, not this one, is the list. All paths are relative to the project root, and
`tetrisd` refuses to boot without the certificates `make certs` writes into the
git-ignored `certs/`.

### Certificates and offline play

**Only one CA is trusted per run**, paired with the host launched against —
`certs/demo-ca.crt` for a named server, the scratch `certs/ca.crt` for a local
one. Typing a *different* address into **SERVER ID** on the client's sign-in
screen fails with `certificate signature failure`; concatenating both CAs is not
a workaround, since `load_cert_file` in the frozen
[`common.c`](../lib/libtetrissh/src/common.c) reads one certificate and ignores
the rest.

**No server answering is not a failed run.** Nothing answering the named server
launches the client offline rather than refusing: **PLAY OFFLINE** (or `O`)
reaches a Solo game on this machine's own rules. **LOGIN**, **SIGN UP**, the
lobby, Double, Battle Royale, the store and the leaderboard all need a server,
and `--offline` asks for that outcome without probing first — the one route that
needs no certificates at all.

---

## Project Structure

```
MacMini_tetriSH/
├── bin/                Symlinks to every built binary (make bin-link)
├── lib/                Self-contained libraries → lib/libXXX/libXXX.a
│   └── libXXX/         Makefile, include/XXX.h, src/, tests/, scripts/run_tests.sh
├── src/
│   ├── tetrish/        Shell → macmini_shell, system programs → bin/
│   ├── tetrisu/        notcurses client → src/tetrisu/bin/tetrisu
│   ├── tetrisd/        Game server → tetrisd
│   ├── tetrislogd/     Logger daemon → tetrislogd
│   └── tetrisctl/      Admin CLI → tetrisctl
├── docs/               Glossary, specs, diagrams, and bug post-mortems
├── scripts/            Dependency, certificate, and launch helpers
│   ├── play.sh         One command from a fresh clone to a client
│   ├── container.sh    Engine, client image, and how the client runs
│   └── terminal.sh     Which terminal draws, and whether one can open
├── .claude/skills/     Code, Makefile, and README style guides
├── .tetrishrc          Shell start-up file — launches the daemons
├── Dockerfile          Client image — builds tetrisu, never draws
└── Makefile            Umbrella; recurses into every component
```
