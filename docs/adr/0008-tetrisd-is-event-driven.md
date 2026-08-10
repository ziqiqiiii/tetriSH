# tetrisd is event-driven; one thread owns all game state

_Status: accepted and implemented. Steps 1 to 5 are done, which is the whole of
the decision this record describes: `tetrisd` is a reactor with a handshake
pool and one gravity timer, and there are no locks over game state. Steps 6 and
7 at the end are Double mode and Battle Royale, which this ADR only sequences -
they belong to [ADR-0009](./0009-cross-player-effects-resolve-at-piece-lock.md)._

`tetrisd` becomes a reactor. One thread waits in `epoll_wait` and owns the
lobby, every room, every game, the client registry and every outbox. A small
bounded pool of handshake workers exists beside it, and each worker owns only
its own descriptor until it hands a completed `t_session` back. Nothing else
touches game state, so nothing guards it.

The sentence that replaces the lock order in `tetrisd.h` is stronger than the
lock order ever was:

> `tetrisd` has exactly one owner of all mutable game state.

This was not chosen for throughput. At `TETRISD_MAX_CLIENTS=64` — even at the
`TETRISD_MAX_CLIENTS_LIMIT` ceiling of 4096 — the threaded server is nowhere
near a scaling limit, and the arithmetic on the tick budget says so: 16 Battle
Royale
rooms of 4 players, every game dirty every tick, is roughly 0.64 ms of work
inside a 12 ms tick. Anyone reading this file for a performance justification
will not find one. It was chosen because the invariants had become things a
person had to hold in their head — a four-level lock order, a displaced-login
handshake between two client threads, a ticker racing a teardown — and the
cheapest way to check an invariant is to make it structural.

`login_handler` is the case that makes the argument concrete. Under threads,
`displace_previous` called `registry_displace` and then `registry_wait_absent`
with a three-second timeout, because
[ADR-0004](./0004-one-connection-per-player.md)
requires that the displaced connection be fully gone before the new one acts
as that player, and only the displaced connection's own reader thread could
finish it. Under the reactor that code was not ported, it was deleted: the loop
owns both connections, so displacement is a synchronous unlink and a mark for
reaping. `registry_wait_absent`, `TD_DISPLACE_WAIT_MS`, `empty_mutex` and
`empty_cond` went with it. The invariant stopped being enforced and started
being true by construction.

This is also the one place the staging below was wrong, and it is worth saying
why rather than quietly renumbering. The wait was listed under step 5 with the
locks. It could not wait that long: `login_handler` runs *on* the loop, and the
displaced connection can only be reaped by the loop, so a wait for it is a
thread waiting on itself. Step 3 does not merely make the wait unnecessary — it
makes it a deadlock, so the deletion is part of step 3 and not a lock removal
brought forward.

## The handshake stays on threads, and that is the whole design

The only genuinely blocking thing in `tetrisd` is the secure handshake.
`session_recv` and `session_send` are blocking because
`lib/libtetrissh/README.md` says they are — the library has no timeout,
cancellation, or non-blocking state machine — but their blocking is *framing*,
and framing can be lifted out. `session_handshake_server` is different: it is
six ordered I/O steps around two RSA private-key operations, and it waits on a
32-byte client nonce it has no reason to trust will arrive.

So the seam goes there. Established connections move into the reactor;
handshakes stay on threads that are allowed to block.

A pure single-threaded server was considered and rejected. It would require
rewriting `handshake.c` as a resumable state machine — the hardest file in
`libtetrissh` — and would put an RSA sign and an RSA-OAEP decrypt directly on
the path that drives gravity, so a burst of connections would stutter every
room in the process. Sharded reactors, each owning a disjoint set of rooms,
were rejected for the opposite reason: a client's connection and the room it
joins would land on different shards, reintroducing exactly the cross-thread
reasoning this decision exists to delete.

## libtetrissh's data path becomes pure

The reactor owns the socket and the per-client read buffer, extracts a
complete length-prefixed frame itself, and calls `session_frame_open` /
`session_frame_seal` — which perform no I/O. Keys and the per-direction
sequence counters stay inside `t_session`, and a counter still advances only
on a fully accepted frame.

This obeys the rule the project already imposes on `libtetrisbrain` and
`libtetrisroom`, and it turns a torn-frame bug into a unit test over a byte
array instead of a socket race. `session_send` and `session_recv` survive as
thin blocking wrappers over the same core, so `tetrisu`, the handshake pool
and all seven existing suites compile unchanged.

Two alternatives were rejected: a non-blocking `session_recv_nb` returning
`TSH_AGAIN`, and a `session_feed`/`session_next` buffering pair. Both leave
`libtetrissh` owning hidden buffer state, and the first leaves it doing
syscalls.

## epoll, for the pointer and not for the speed

`epoll` is chosen over `poll` because `epoll_event.data.ptr` carries the
`t_client *` back from the kernel, so no fd-to-client mapping exists to be
kept in sync. Docker-only deployment removes the portability objection, but
that is permission, not a reason. At this scale `poll` would be fast enough
and the performance argument for `epoll` would be dishonest.

The cost is real and must be treated as a rule rather than a habit. A stale
`data.ptr` is a use-after-free: one `epoll_wait` returns a batch, processing an
early event tears a client down, and a later event in the same batch still
points at it. `poll` cannot do this, because it re-derives everything from the
fd array each iteration. So:

> **No client is freed inside the event loop.** `cli_kill` marks it dead,
> issues `EPOLL_CTL_DEL`, unlinks it from the registry and pushes it onto a
> zombie list. The zombie list is drained once, after every event in the batch
> has been processed. That drain is the only `free()` site for a client.

This rule replaces the registry rwlock, and it is the one thing in this ADR
that a future reader can quietly break without any test failing.

## Gravity is one timer

A single `timerfd` armed at `tick_ms` sits in the epoll set like any other
descriptor. On expiry the loop computes elapsed time once from the monotonic
clock, walks the in-game rooms, advances every game by that same elapsed, and
pushes `STATE` for whatever came back dirty.

`t_game` was already built for this: `accum_ms` accumulates against each
player's own `gravity_interval_ms(level)`, so a uniform coarse tick still
produces per-player speeds correctly, and `tick_once` already derives elapsed
from a real clock rather than assuming the tick fired on time — so a late or
coalesced tick stays correct with no extra work.

A per-game deadline heap was rejected: it buys millisecond precision nobody
has asked for, and adds a structure that six different events (piece lock,
level up, forfeit, room start, room end, `SIGHUP`) can leave stale, which is a
silent wrong-speed bug rather than a loud one.

## The outbox keeps its shape and loses its threading

The bounded response FIFO and the one-slot latest-`STATE` mailbox are kept
exactly as they are. That split is the load-bearing idea — it is what stops a
slow client from making the server buffer an unbounded backlog of stale boards
— and it has nothing to do with threading. What goes is the mutex, the
condition variable, the writer thread and `atomic_bool overflowed`.

What arrives is a partial-write cursor. `write(2)` on a non-blocking socket
may return short, and `tsh_write_exact`'s retry loop is no longer available, so
the outbox tracks how far into the head frame it has got. `reply` seals the
frame, appends it, and attempts the write immediately; only a short write or
`EAGAIN` arms `EPOLLOUT`.

## Consequences

- **The handshake pool needs a deadline, or it is a denial of service.** Under
  threads, a client that connects and never sends its nonce stalls one thread
  out of many. With a bounded pool it stalls a shared resource: N such
  connections block every subsequent login server-wide. The pool holds **four**
  workers, configurable as `TETRISD_HANDSHAKE_WORKERS` in `.tetrishrc`. Four is
  chosen against the shape of the load rather than the client count: a
  handshake is brief and one-off, so the pool sizes the *arrival* rate, not the
  number of connections being served. The key exists so that the deadline and
  the pool size can be tuned together, since they are the two halves of one
  guarantee — so the deadline is a key too, `TETRISD_HANDSHAKE_TIMEOUT_MS`,
  defaulting to five seconds.

  This paragraph originally said the guarantee was `SO_RCVTIMEO` and
  `SO_SNDTIMEO`, "as `lib/libtetrissh/README.md` already recommends for exactly
  this case". **That was wrong, and implementing step 3 is what showed it.**
  Those options bound a single `recv`, and `sessionio_read_exact` loops until
  it has the byte count it asked for, so a peer that sends one byte just before
  every timeout never trips one and holds a worker for as long as it likes.
  What the pool actually carries is a budget for the whole handshake, checked
  by the reactor — which shuts the socket down, because the worker is blocked
  inside the handshake and cannot check a clock itself. The socket options are
  still set: they retire the common case, a peer that says nothing at all,
  without the reactor having to intervene. The regression test is
  `test_a_dribbling_peer_does_not_hold_the_pool_forever`.

  The budget runs from when a worker *picks the connection up*, not from when
  it was accepted. Counting queue time was tried first and is wrong: an honest
  client that queues behind a full-budget attacker inherits only what the
  attacker left, so under attack the server starts cutting off exactly the
  logins the deadline exists to protect. A queued client holds no worker, and
  the bounded handshakes ahead of it are what bound its wait.
- The four-level lock order — `lobby_mutex > room->mutex > registry rwlock >
  outbox mutex` — is deleted from `tetrisd.h`, along with every lock in it and
  the rule that no `db_*`, `session_*` or IPC send may happen under one. With a
  single owner there is nothing to order.
- `srv->tick_ms` stops needing to be atomic. `SIGHUP` retiming becomes a
  `timerfd_settime` call on the loop that owns the timer, rather than a store
  read by ninety-nine ticker threads.
- The cert bytes and the parsed `EVP_PKEY` are loaded once at boot and held in
  `t_server`. `session_handshake_server` used to open and parse both files on
  **every** accepted connection; that was a defect independent of this decision
  and was fixed early (step 2), where it also removes disk I/O from the pool.
  The loaded `t_tetrissh_credentials` is immutable, so the pool shares one
  copy.
- `server_start` and `server_stop` keep their present shape. `server_start`
  still creates the reactor thread and returns, because all seven suites drive
  a real server in-process through that seam, and `harness.c` speaks to it with
  the blocking wrappers. `tetrisd` is therefore not literally single-threaded —
  it is single-*owner*, which is the property that matters and the one worth
  stating precisely.
- `db_*` calls move onto the loop. This is safe: reads are pure memory, and
  writes append with one `write(2)` and deliberately do not `fsync` —
  durability is the flusher thread's one-second job. The database is not a
  reason to keep threads.
- `SIGUSR1`'s state dump becomes consistent for free. It runs on the thread
  that owns everything it prints, so there is no snapshot to coordinate.
- The `tetrisctl` control socket ([ADR-0007](./0007-daemons-detach-themselves.md))
  becomes markedly easier rather than harder. It is one more listening
  descriptor in the epoll set; its connections carry no handshake, and
  `STATUS`, `ROOMS`, `PLAYERS` and `DROPPED-LOGS` read state the loop already
  owns exclusively.
- `TD_MAX_GAMES` is removed in step 7. It caps games per room at 16 while
  `libtetrisroom` allows 99 slots and UC-12 names "Player (4–99)", so a
  99-player Battle Royale is impossible today. **Battle Royale is one Room of
  4–99 slots**, so the cap is what gives way, not the mode: `t_game` is
  allocated per seated slot instead of as a fixed array in `t_room_rt`. The
  comment that justifies the cap — "sizing every room for the maximum would
  cost tens of megabytes" — was correct about a fixed array in all
  `LOBBY_MAX_ROOMS` rooms and stops applying once games are allocated on
  seating. With one owning thread the allocation carries no lifetime hazard.

## Migration

Seven steps, each ending with `make test` green and valgrind clean, so any
regression bisects to one step. Steps 1 and 2 hold their value even if the
rest is abandoned.

1. **Done.** `libtetrissh` gains `session_frame_open` / `session_frame_seal` with
   unit tests over byte arrays; `session_send` / `session_recv` become wrappers.
   Nothing else changes.
2. **Done.** Certificate bytes and private key are hoisted to boot into
   `t_server`, behind an opaque `t_tetrissh_credentials`.
   `session_handshake_server` takes that instead of two paths, so it performs no
   disk I/O at all.
3. **Done.** The reactor replaces the per-client reader and writer threads,
   with the handshake pool and its handoff queue. Room tickers remain, so
   `room->mutex`, `lobby_mutex`, the registry rwlock and the outbox mutex all
   remain load-bearing; a ticker enqueues and pokes the wake pipe, and the
   reactor is what writes. `registry_wait_absent`, `TD_DISPLACE_WAIT_MS`,
   `empty_mutex`, `empty_cond` and `registry_wait_empty` are deleted here
   rather than in step 5, for the reason given above. `TETRISD_HANDSHAKE_WORKERS`
   arrives with a five-second handshake deadline beside it.
4. **Done.** One `timerfd` replaces the ninety-nine ticker threads. `t_game`
   needed no change, as predicted. `server_room_begin` stops being able to
   fail, so `rollback_start` - which existed only to undo a start whose ticker
   thread could not be created - is deleted with them, and `srv->tick_ms` stops
   being atomic. (`server_room_begin` has since been folded into
   `server_room_start`, which is what deals the boards and sets the flag
   together; there was no longer anything to separate them.)
5. **Done.** `room->mutex`, `lobby_mutex`, the registry rwlock and the outbox
   mutex are deleted together, and the concurrency note in `tetrisd.h` is
   rewritten around the one-owner sentence. (`registry_wait_absent` and
   `TD_DISPLACE_WAIT_MS` went at step 3, for the reason given above.)
   `t_outbox.overflowed` stops being atomic with them. **This is the commit
   this ADR describes.**
6. Double mode: the pending-effect queue, applied at piece lock
   ([ADR-0009](./0009-cross-player-effects-resolve-at-piece-lock.md)).
7. Battle Royale: Target selection, elimination and ranking.
