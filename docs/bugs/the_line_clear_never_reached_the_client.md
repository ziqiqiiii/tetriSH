# Bug Note — The line clear never reached the client

## What the bug was

Online, rows vanished. No flash, no `TETRIS`, nothing — the stack simply had
one fewer row on the next frame. Offline the same board animated fine.

`lock_piece` in `src/tetrisd/src/game.c` did the whole of a clear in one call:

```c
piece_stamp(&g->board, &g->piece);
cleared = board_clear_lines(&g->board);
...
score_apply_clear(&g->score, cleared, g->level, T_SPIN_NONE, perfect);
...
spawn_next(g);
```

Stamp, clear, score, spawn — between two ticks. No snapshot was ever taken
with the completed rows still on the board, so there was no frame in which a
clear was happening. `BODY_PHASE_CLEARING` was never set and
`clearing_count` / `clearing_rows` / `clearing_ms` were never written, even
though `t_body_state` has carried all four since the body codec was written.

The client half was missing too, and independently. `net_solo_apply` copied
the board, the piece, the queue, the hold and the counters, and skipped
`clearing_*` and `last_clear` entirely. `apply_phase` set `SOLO_CLEARING` if
the server ever said so, but `render_solo` draws the animation from
`clear_rows`, `clear_count` and `clear_elapsed_ms` — none of which arrived. So
even a server that had reported a clear would have animated nothing.

Two missing halves of one feature, on opposite sides of the wire, in a field
the protocol already specified.

## Why it was not caught

The offline path is fully covered — `test_solo_game.c` has clear duration,
frame boundaries, and the level split. Every one of those tests drives
`solo_game_update`, which is the client's own rules.

The server side had no test for a clear at all, because making a real one over
the wire is hard: filling ten columns needs dozens of pieces, depends on what
the bag deals, and runs into per-connection input rate limiting. The suite
tested what was easy to reach — a lock, a top-out, gravity — and a clear was
neither.

Same root cause as [the countdown](the_countdown_only_ticked_offline.md): both
sides of a seam were tested, and the seam was not.

## The fix

The clearing phase moved to the server, because that is where the board is.

- `t_game` gained `clearing_rows` / `clearing_count` / `clearing_ms`.
  `clearing_count > 0` is the whole of "am I clearing?".
- `lock_piece` split. `begin_clear` finds the completed rows and holds them —
  nothing cleared, nothing scored, no piece dealt. `finish_clear` is
  everything the lock used to do inline, and is also what a lock that
  completed nothing calls straight away.
- `game_gravity` is the clear's timer while one is running, and returns `true`
  every tick so the client gets frames to animate with rather than one flash
  at each end. `game_move`, `game_rotate`, `game_drop` and `game_hold` refuse
  while rows are held: the piece locked and the next one has not been dealt,
  so there is nothing to drive.
- `clear_duration_ms` moved into `libtetrisbrain`'s `scoring.c`, next to
  `gravity_interval_ms`. Both ends need the same answer and both link the
  library; two copies of the table would drift into a client animating a clear
  the server had already finished. `solo_clear_duration_ms` is now a wrapper.
- On the wire, `clearing <count> <elapsed_ms> [rows...]` is how far *through*
  the clear the server is, not how long is left, and the rows named are still
  filled in the board of the same snapshot.
- `net_solo_apply` copies all of it, and reads `last_clear` for the score
  banner. The banner fires on the score moving rather than on the label being
  set, because the label stays set until the next lock — triggering on it
  would restart the banner on every snapshot of one clear and it would never
  fade.

Tests: `src/tetrisd/tests/test_clearing.c` (eight cases, driving `t_game`
in-process — hold, no early score, input refused, completion, no-clear
unchanged, pause, snapshot, and an encode/decode round trip) and
`src/tetrisu/tests/test_net_state_apply.c` (six cases, feeding `net_solo_apply`
snapshots by hand).

There is deliberately no end-to-end "play until a row clears" check. Filling a
row over a real socket takes ~80 paced pieces to stay inside the input rate
limit, and top-out makes it non-deterministic — it would be a flaky test, which
is worse than the two deterministic ones plus the round trip that join them.

## What to take from this

1. **A protocol field nobody writes is not a feature.** `clearing_count` was
   in `statusbody.h`, documented, encoded, decoded, and unit-tested by the
   codec's own suite. It had never been non-zero.

2. **"Presentation stays on the client" needs a line drawn under it.** The
   countdown is presentation. The clearing phase is not — it decides when a
   piece spawns and when input is accepted, so it is a rule, and rules live
   with the board. The same wrong instinct produced both bugs.

3. **Hard to test is where the bugs are.** A clear was the one gameplay event
   the server suite could not easily produce, and it was the one that had
   never worked. When a case is skipped because it is awkward to reach, reach
   it a different way — here, in-process rather than over the wire.
