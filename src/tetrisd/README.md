# tetrisd

`tetrisd` is the concurrent, server-authoritative game server for tetriSH: it owns accounts, the lobby, and every live game, and it is the only process that decides what happened.

---

## Table of Contents

- [Build And Test](#build-and-test)
- [Run](#run)
- [Configuration](#configuration)
- [Routes](#routes)
- [Bodies](#bodies)
- [Threading Model](#threading-model)
- [Lock Order](#lock-order)
- [Client Lifetime](#client-lifetime)
- [Logging](#logging)
- [Project Structure](#project-structure)

---

## Build And Test

```bash
make -C src/tetrisd
make -C src/tetrisd test
make -C src/tetrisd test FILTER=game
make -C src/tetrisd clean|fclean|re
```

Build output is `src/tetrisd/tetrisd`. It links all seven CoreStack archives plus OpenSSL, pthreads, and librt; the archives are built in place first.

The suite is expected to pass under valgrind, which is where thread teardown is actually proven:

```bash
valgrind --leak-check=full --error-exitcode=1 src/tetrisd/tests/bin/test_shutdown
```

Valgrind proves teardown; it says nothing about data races. Those need the
suite rebuilt with ThreadSanitizer, which is a separate and non-negotiable
check for a server with this many threads:

```bash
make -C lib/<each> re FLAGS="<its flags minus -Werror> -fsanitize=thread -g -O1"
make -C src/tetrisd test FLAGS="-Wall -Wextra -fsanitize=thread -g -O1" \
     LDLIBS="-lssl -lcrypto -lpthread -lrt -fsanitize=thread"
```

Both are expected to be clean. Two real races were found this way that
reading the code with the lock order in hand did not catch — see
[Lock Order](#lock-order).

## Run

```bash
make certs                 # once per clone: dev CA + server certificate
make -C src/tetrisd run    # or: dspawn tetrisd, from inside the shell
```

`tetrisd` does not daemonise — `dspawn` already did that before exec'ing it. It refuses to boot when `cert_path` or `key_path` cannot be read, so it can never silently serve players unauthenticated.

| Signal | Effect |
|---|---|
| `SIGTERM`, `SIGINT` | Stop accepting, shut every client down, join every thread, close the store |
| `SIGHUP` | Re-read `.tetrishrc`; applies the log level and tick period |
| `SIGUSR1` | Dump the whole server state to the log: settings, every connection, every room, every live game, and the log drop count |
| `SIGPIPE` | Ignored — a vanished client kills its own connection, not the server |

## Configuration

Every path and setting comes from `.tetrishrc`, resolved in the order `argv[1]` → `$TETRISHRC` → `./.tetrishrc`, then overlaid with any matching environment variables. Each is an ordinary `export` line, so the shell exports it for `dspawn`'d daemons and `tetrisd` parses the same file itself.

| Key | Default | Meaning |
|---|---|---|
| `TETRISD_PORT` | `4242` | TCP port; `0` lets the kernel choose (tests) |
| `TETRISD_DATA_DIR` | `tmp/tetrisd` | Where the player store keeps its append-only log |
| `TETRISD_CONFIG_DIR` | `lib/libmacminidb/config` | Read-only character and theme catalogues |
| `TETRISD_CERT_PATH` | `certs/server.crt` | Server certificate (required) |
| `TETRISD_KEY_PATH` | `certs/server.key` | Server private key (required) |
| `TETRISD_CA_PATH` | `certs/ca.crt` | CA clients verify the server against |
| `TETRISD_LOG_IPC` | `tmp/tetrisd/tetrislogd.sock` | Datagram socket `tetrislogd` binds |
| `TETRISD_LOG_LEVEL` | `info` | `debug`, `info`, `warning`, `error` |
| `TETRISD_MAX_CLIENTS` | `64` | Simultaneous connections; beyond it, accepts are refused |
| `TETRISD_TICK_MS` | `12` | Room ticker period |
| `TETRISD_BR_SLOTS` | `4` | Slots in a Battle Royale room (2–16) |
| `TETRISD_INPUT_BURST` | `120` | Per-connection input tokens held at once |
| `TETRISD_INPUT_RATE` | `60` | Input tokens refilled per second |

## Routes

Milestone 1 serves Single mode end to end. Every route below `LOGIN` requires a `Player-Id` header matching the player bound to the connection (ADR-0001); a mismatch is `401`, never a guess.

| Method | Path | Success | Refusals |
|---|---|---|---|
| `SIGNUP` | `/account` | `201` | `409` name taken, `400` bad body |
| `LOGIN` | `/session` | `200` + `Player-Id` | `401` bad credentials, `409` this connection is already bound, `503` a previous connection would not release the player |
| `LIST` | `/rooms` | `200` room rows | `401` |
| `JOIN` | `/rooms` | `201` room created, caller is owner | `400` unknown mode, `409` `lobby-full` / `already-in-room` |
| `JOIN` | `/room/<name>` | `200` seated | `404` no such room, `409` `full` / `in-game` |
| `LEAVE` | `/room/<name>` | `200` | `404` not in that room |
| `START` | `/room/<name>` | `200` | `403` `not-owner`, `409` `too-few-players` / `already-started` |
| `MOVE` | `/room/<name>/player/<pid>` | `200` | `409` `input-blocked`, `403` not your board, `400` bad body, `429` too fast |
| `ROTATE` | `/room/<name>/player/<pid>` | `200` | as `MOVE` |
| `DROP` | `/room/<name>/player/<pid>` | `200` | as `MOVE` |
| `STATE` | `/room/<name>/player/<pid>` | **server-pushed** | — |

Rooms are never named by clients: the lobby assigns `S-01`, `D-02`, `BR-03`, so creation addresses the collection (`JOIN /rooms`) and everything after it addresses the room by the name the server assigned.

Inputs are rate limited per connection by a token bucket sized from
`.tetrishrc`; exceeding it is `429` with a `Retry-After` header rather than
work handed to the room's ticker. A player has at most one connection, so a
`LOGIN` for a player who is already connected closes the older connection and
waits for it to finish leaving before binding the new one (ADR-0004).

A refusal carries its verdict as a `reason <verdict>` body line — the room domain knows *why* it said no, and a player who cannot tell a full room from one already in game cannot act on the answer. A malformed message is `400` and an unknown method `501`. `413` answers a message that parses as oversized; a *frame* beyond the 64 KiB session cap cannot be decrypted at all, so `libtetrissh` drops that connection before HTTTP ever sees it.

Finishing a game clears every slot (the room domain's rule), so the room empties and is handed back to the lobby — a player joins a fresh room for their next game rather than staying seated.

## Bodies

Requests and responses use the same plaintext `key value` lines the status bodies use; input commands are a single upper-case word.

Every request carrying a body must declare `Content-Type:
application/tetris-command`; one that does not is `400`. Response bodies are
`application/tetris-status`, and `STATE` pushes are `application/tetris-state`
— the three are kept distinct so a message's direction is legible from its
type alone.

```text
SIGNUP /account          username amber\npassword hunter2\n
LOGIN /session           username amber\npassword hunter2\n
JOIN /rooms              mode single|double|br\n
MOVE .../player/<pid>    LEFT | RIGHT
ROTATE .../player/<pid>  CW | CCW
DROP .../player/<pid>    SOFT | HARD
```

`LIST` answers with a `libstatusbody` room-row body; `STATE` carries a `libstatusbody` snapshot with `Content-Type: application/tetris-state`. Passwords are salted and SHA-256 hashed here — the store never sees one.

## Threading Model

```text
main loop      poll(listener, self-pipe) -> accept -> hand off, never blocks
per client     reader thread   blocks in session_recv, parses, dispatches
               writer thread   the only session_send caller for that connection
per room       ticker thread   gravity on a fixed tick, then STATE snapshots
logging        shipper thread  drains the ring, datagrams records to tetrislogd
```

Each client owns a bounded response FIFO plus a one-slot STATE mailbox. Responses that overflow the FIFO close the connection — a client that cannot keep up must not be able to grow the server's memory — while snapshots overwrite, so a stalled peer loses intermediate frames and never delays a room's ticker.

Gravity accumulates real elapsed time against each player's own `gravity_interval_ms(level)`, so a descheduled ticker catches up rather than slowing the game down. Inputs mark the game dirty under the room's mutex; the ticker is the only thing that encodes and pushes, so inputs and gravity produce one STATE stream instead of two racing ones.

## Lock Order

```text
lobby_mutex  >  room->mutex  >  registry rwlock  >  outbox mutex
```

Strictly descending, never re-entered upward. No `db_*`, `session_*`, or IPC send happens under any lock — the outbox push is the sole exception, and it cannot block. Database writes (recording a finished game) happen after the room mutex is released.

Two rules sit underneath the order and are easy to break without breaking it.
A room is guarded by *its own* mutex everywhere, including while the lobby
destroys it — holding only `lobby_mutex` there would leave one room protected
by two different locks depending on the caller, which is a data race rather
than an ordering bug. And the registry's client count and its `empty_cond` are
published under one hold of `empty_mutex`, so a waiter cannot observe an empty
registry and destroy the condition variable while a departing thread is still
about to signal it. Both were found by building the suite with
`-fsanitize=thread`, which is the check a lock-order review does not perform.

## Client Lifetime

The registry's rwlock is the lifetime guard. An enqueuer holds the read lock across its outbox push, so the client cannot be freed underneath it. Teardown, on the client's own reader thread, runs in one order:

1. forfeit — leaving, topping out, and a dropped connection are the same event (ADR-0002)

2. take the write lock, unlink from the registry, release it
3. `shutdown(fd)`, close the outbox, join the writer thread
4. close the session and socket, free the client

A room that nobody is sitting in is returned to the lobby, so a long-running server does not fill up with the ghosts of finished games.

## Logging

Every significant event becomes a `libcoreipc` log record pushed into a ring buffer. The shipper thread drains it and datagrams each record to `tetrislogd`, falling back to stderr while the logger is unreachable and retrying the connection. Under pressure records are **dropped and counted**, never waited on — `rb_drops()` is the number `tetrisctl dropped-logs` will report.

## Project Structure

```text
src/tetrisd/
├── Makefile
├── include/tetrisd.h     # every type and prototype; src/*.c include only this
├── src/
│   ├── main.c            # thin shim over server_start / server_stop
│   ├── cfg.c             # .tetrishrc parsing and validation
│   ├── log.c             # ring buffer, shipper thread, stderr fallback
│   ├── net.c             # listener, accept, paths, monotonic time
│   ├── server.c          # bring-up, main loop, shutdown, SIGHUP reload
│   ├── signals.c         # handlers: a flag and one byte down the self-pipe
│   ├── registry.c        # client registry, the lifetime guard
│   ├── client.c          # reader and writer threads, teardown
│   ├── outbox.c          # bounded FIFO + latest-STATE mailbox
│   ├── dispatch.c        # frame -> route -> response, status mapping
│   ├── h_account.c       # SIGNUP, LOGIN, password hashing
│   ├── h_lobby.c         # LIST, JOIN, LEAVE, START
│   ├── h_input.c         # MOVE, ROTATE, DROP
│   ├── game.c            # the per-player game aggregate over libtetrisbrain
│   └── room.c            # room runtime, ticker, STATE push, forfeit
├── tests/                # harness.c is the headless client; test_*.c the suites
└── scripts/run_tests.sh
```

`t_game` is the aggregate `libtetrisbrain` deliberately does not own — board, piece, bag, score, charge, and effects, with the ordering rules between them enforced in one place. `tetrisu` will consume the same shape through STATE rather than reinventing it.
