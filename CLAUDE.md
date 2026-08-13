# CLAUDE.md

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the
CoreStack Challenge (50.003 × 50.005) at SUTD: a shell, three daemons, a
notcurses client, and eight self-contained static libraries.

Every component owns a `README.md` that is the authority on its scope, API, and
build — read that component's README before changing it. `src/tetrish/` owns a
second `CLAUDE.md` for the shell's pipeline, test conventions, and 42-school
code style.

## Status

| Component | Status |
|---|---|
| `lib/*` — all eight libraries | implemented, suites pass, valgrind-clean |
| `src/tetrish` | implemented — REPL, builtins, `.tetrishrc`, `bin/` system programs |
| `src/tetrisd` | Single and Double served end to end; the event-driven migration is complete (steps 1–5) and Double is step 6. **Battle Royale (step 7) is designed and unbuilt** |
| `src/tetrislogd` | implemented — sink + reclaim, counters, self-detach, pidfile |
| `src/tetrisctl` | `start`/`status`/`stop`/`restart` by pidfile and signal; the control socket is a later step |
| `src/tetrisu` | playable — Solo, Double, Battle Royale, and a server-authoritative store, leaderboard and room chat. Battle Royale's rivals are still modelled in-process, awaiting step 7 |

## Build & Test

The root `Makefile` is an umbrella: deps, then every `lib/lib*/` with a
Makefile, then the shell, then whichever daemon directories exist (matched via
`wildcard`, so unbuilt components are skipped rather than erroring). `README.md`
and the root `Makefile`'s `.PHONY` line carry the target list; each library
exposes the same five targets (`all`, `test`, `clean`, `fclean`, `re`) and the
same `FILTER=` convention:

```bash
make -C lib/libtetrisbrain test FILTER=abilities   # one suite
gcc ... lib/libX/libX.a -I lib/libX/include        # to link one
```

What the targets do not say out loud:

- `make reset` stops the daemons, then `fclean`s **and wipes their runtime
  state** — `tmp/`, `archive/`, `bin/` — but stashes the player store
  (`TETRISD_DATA_DIR/players.log`) across the wipe and puts it back, so a
  rebuild never costs the accounts people signed up with. `make del-db` is the
  only target that deletes it.
- `AUTO_INSTALL_DEPS=0` makes the dependency step check-only (CI).
- **macOS runs the client only.** `tetrisd` is built on `epoll_create1`/
  `timerfd` and `libcoreipc` on POSIX `mq_open`; Darwin ships none of them, so
  neither compiles there and `make play` drops the server steps.
- The daemons are launched by `tetrisctl start` from inside the shell (see
  `.tetrishrc`), never from the root Makefile. `TETRISCTL_DAEMONS` is the one
  place launch order is written down — logger → game server, teardown reversed,
  so `tetrisd`'s shutdown still reaches the log.

`make play` (host build) and `make play-image` (container build) both end in a
kitty window on the shared server; `README.md` owns the terminal matrix, the
one-CA-per-run rule, and the flags. Three scripts own one concern each —
`scripts/container.sh` the engine, image and run; `scripts/terminal.sh` which
terminal draws; `scripts/play.sh` the order.

## Architecture

```
HTTTP (application protocol)
Secure session (cert auth, RSA-OAEP key exchange, AES-256 frames)
TCP (POSIX sockets)
```

**Binaries** — `tetrish` (interactive shell; its `dspawn`/`dcheck`/`dkill`
daemonise arbitrary programs and are *not* the game daemons' lifecycle
manager), `tetrisd` (server-authoritative game server — rooms, game logic,
chat, narration, and the marketplace over the same authenticated session),
`tetrislogd` (separate logger, survives `tetrisd` restarts), `tetrisctl`
(admin CLI owning both daemons' lifecycle), `tetrisu` (terminal client).

**Libraries** — `libtetrisbrain` (pure game logic), `libtetrisroom` (pure
lobby/room/slot domain), `libmacminidb` (in-memory store with WAL and crash
recovery), `libtetrissh` (handshake + encrypted framing), `libcoreipc` (IPC
primitives, built first, no internal deps), `libcoredaemon` (both sides of
daemonising), `libhtttp` (parser/serialiser + the protocol grammar and method
table), `libstatusbody` (message-body codec — `tetrisd` encodes, `tetrisu`
decodes).

**HTTTP** is HTTP-like. `STATE` is server-originated; `CHAT` is the only method
travelling both ways — up as a typed command (`application/tetris-command`),
down as a feed line (`application/tetris-chat`) — so `htttp_validate` accepts
either type for it and one type for everything else. `Player-Id` is required on
every authenticated request.

## Invariants

These bind every change. Breaking one is a design regression, not a bug.

- `common.c` / `common.h` are **frozen** (`lib/libtetrissh/src/common.c`,
  `include/libs/common.h`) — all crypto goes through them as they stand, and the
  handshake is manual: no TLS, no `SSL_*`.
- `libtetrisbrain` and `libtetrisroom` are **pure** — logic only, no I/O, no
  side effects. External facts arrive as a caller-supplied `probe` callback.
- `libcoreipc` reports through errno-style returns alone, because it *is* the
  log path and logging from it recurses into itself.
- Paths come from `.tetrishrc` or from the caller, at every site.
- `tetrisd` has **one owner** of all mutable game state: the reactor thread. The
  two surviving locks (the handshake pool's, `libmacminidb`'s internal) guard
  none of it, and wanting a third means the work is on the wrong thread.
  Everywhere else in the project, lock order is documented and no mutex is held
  across a blocking syscall.
- No client is freed inside the event loop: `client_kill` parks it on the zombie
  list and `client_reap` is the only `free()` site, which is what keeps
  `epoll_event.data.ptr` valid.
- The **single-instance guard** is the `flock` on each daemon's pidfile and
  nothing else — claimed after the double-fork (the pid written must be the
  detached process's) and before anything a second instance could damage, which
  for `tetrislogd` means before `unixsock_dgram_bind` unlinks its socket path.
  The fork lives in each `main.c` only, so in-process suites never fork.
- A daemon keeps `stderr` on the terminal until boot succeeds, then moves it to
  its configured error file: boot failures have to reach the person who typed
  the command.
- Log-record vocabulary is three different counts — **Dropped** (producer-side
  ring full, `tetrisd`), **Rejected** (malformed on arrival, `tetrislogd`),
  **Degraded** (valid, sink unavailable, written to stderr). See
  `docs/CONTEXT.md`.
- Cross-player effects (garbage, offensive abilities) are queued against a
  **Target** and applied at that player's next piece lock; a player's own inputs
  apply immediately. Injecting garbage under an active piece can produce a board
  `piece_is_valid` rejects, so this is a game rule rather than an optimisation.
  Garbage never crosses rooms.
- Frame cap is 64 KiB; a larger HTTTP message answers `413 Payload Too Large`.
- Everything compiles clean under `-Wall -Wextra -Werror`, and test binaries
  pass `valgrind --leak-check=full --error-exitcode=1`.

## Traps

Non-obvious rules that have each already cost a bug.

- `board_get` returns `CELL_FILLED` out of bounds (a solid wall), so collision
  checks need no range guard in every caller.
- `db_player_owns_character` / `db_player_owns_theme` answer a predicate and
  return `t_db_bool` — `DB_TRUE`, `DB_FALSE`, `DB_UNKNOWN`. `DB_FALSE` is a
  successful read meaning "does not own it", so comparing these against `DB_OK`
  turns every non-owner into an error.
- A Player carries three running numbers answering three questions:
  `leaderboard_score` is the **best single game** (what the board ranks on),
  `lifetime_points` is every point ever scored (what the wallet rate is charged
  against), `wallet_points` is what is left to spend. `award_game` in
  `src/tetrisd/src/room.c` credits and ranks in one call — never rank on a
  total.
- Catalogue ids live in players' owned lists, so they are never renumbered: they
  carry gaps and are never a position. Enumerate a catalogue with
  `db_characters` / `db_themes`; probing ids stops at the first gap.
- A username is printable ASCII with no space (`db_username_valid`) because
  every body naming a player is whitespace-delimited — one player called
  `amber lee` had the whole leaderboard rejected as malformed. `tetrisu` keeps
  its own copy of the rule in `auth_form.c`, since it cannot link the archive.
- A Room is two objects sharing a lobby index — the domain `t_room` and the
  runtime beside it — and `src/tetrisd/src/room.c` is the only module holding
  either. Handlers ask the Room (`server_room_seat`, `server_room_start`,
  `server_room_input`, `server_room_describe`, `server_room_resolve`) rather
  than reaching through `->room`.
- Audio is optional, and a warning-level dependency never reaches the install
  path: `check_deps.sh` warns about missing SDL2 and exits `0`, so audio gets
  its own step in `src/tetrisu/scripts/deps.sh` through `install_deps.sh audio`.
  `AUDIO_STAMP` (`obj/.audio-config`) puts `-DTETRISU_ENABLE_AUDIO` in the
  dependency graph, because make compares timestamps rather than flags and SDL2
  arriving after a silent build would otherwise stay silent until an `fclean`.
  The container engine gets its own step in `scripts/deps.sh` for the same
  reason; `WANT_ENGINE=0` turns it off and `make play` sets it, since a native
  build should not install Docker as a side effect.
- **`--network host` is decided by the daemon, not by `uname`.** On WSL the CLI
  is in the distro and Docker Desktop's daemon is in another VM, so the "host"
  namespace is not this distro's and a containerised client dialling `127.0.0.1`
  never reaches a local `tetrisd`. `engine_is_desktop` in
  `scripts/container.sh` asks `docker info`, and that case drops host networking
  and rewrites loopback to the distro's own address (`wsl_distro_address`) —
  never `host.docker.internal`, which is the *Windows* host and reaches this
  distro only if WSL's localhost forwarding relays it. A distro-local
  `docker.io` really does share the namespace and keeps the Linux answer.
- **Sound does not ride the pty the way the board does.** The board is escape
  sequences the host terminal renders, so the container needs no display; audio
  is SDL2 opening a device, and a container has none. `audio_args` in
  `scripts/container.sh` bind-mounts the host's PulseAudio socket
  (`/mnt/wslg/PulseServer` under WSLg, else `$XDG_RUNTIME_DIR/pulse/native`)
  plus the cookie when one exists. `--device /dev/snd` is deliberately not a
  fallback — it takes exclusive access to the card. macOS finds no socket and
  stays silent.

## Docs

- **A domain term you are about to invent** → `docs/CONTEXT.md` first, the
  shared glossary
- **Writing C, a Makefile, or a README** → the auto-invoked skills in
  `.claude/skills/`; `c-style` discloses layout and test conventions,
  `makefile-style` discloses dependency-script rules
- **A design defect's history** → `docs/bugs/*.md`, one post-mortem each: what
  broke, the fix, the lesson
- **Why a hardening or migration was done the way it was** →
  `docs/superpowers/{specs,plans}/`, completed work kept as a record. Each
  carries a status banner; the component README, not the plan, is current
- **Gameplay, ability text, or the economy** → `docs/use_cases.md`,
  `docs/themes.md` (source of truth for ability text),
  `docs/game-economics.md`
- **Naming a new symbol or prefix** → `docs/naming.md` §2 is the live namespace
- **Diagrams** → `docs/diagrams/`, per use case and per component
- **A `.tetrishrc` key** → `.tetrishrc`, documented inline
