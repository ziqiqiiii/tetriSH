# The piece locked the moment it landed

## What broke

Playing Solo against `tetrisd`, a piece could only ever be placed in the
column it happened to be in when it reached the stack. Touching down and
locking were the same event, so there was no moment in which a landed piece
was still the player's: no sliding a piece sideways under an overhang, no
last-instant rotation into a well, no correction of a piece dropped one column
off.

Offline it worked. `tetrisu`'s local rules already implemented the Guideline's
lock delay — `SOLO_LOCK_DELAY_MS`, `SOLO_LOCK_RESET_LIMIT`, and the
`lock_resets` bookkeeping in `solo_game.c`. So the same build played two
different games depending on whether a server was listening, and the one
people actually played was the worse of the two.

## Why

`game_gravity` had one clock:

```c
while (g->active && g->accum_ms >= step_interval(g))
{
	g->accum_ms -= step_interval(g);
	if (gravity_tick(&g->board, &g->piece) == BRAIN_LOCKED)
		lock_piece(g);
	changed = true;
}
```

`gravity_tick` reports `BRAIN_LOCKED` for "this piece cannot descend", which
is a fact about the board. The server read it as a decision about the game and
stamped the piece. `game_drop(g, false)` did the same thing on a soft drop
into the floor, which took the delay away from precisely the players who reach
the floor fastest.

The rule was never written down anywhere both binaries could see it. `tetrisu`
had it; `tetrisd` did not know it existed.

## The fix

Falling and locking are two clocks. `libtetrisbrain` gained `lockdown.c` and
`t_lockdown`, which owns the Guideline's Extended Placement lock down and
nothing else: half a second, fifteen resets, the budget refilled whenever the
piece falls past its lowest row so far. It keeps no time of its own — the
caller hands it the elapsed milliseconds and it answers whether the piece is
out of time.

`game_gravity` now charges the same tick against both:

```c
changed = apply_fall(g, elapsed_ms);
return (apply_lockdown(g, elapsed_ms) || changed);
```

`game_move` and `game_rotate` spend a reset, reading grounded-ness *before*
the move so that sliding a piece off a ledge is the move that pays. `game_hold`
and `spawn_next` arm a fresh one. `game_drop(g, true)` still locks
immediately, because a hard drop is the input that means "and I am done with
it"; `game_drop(g, false)` no longer locks at all and now answers `409
input-blocked` on the floor, the same as any other refused input.

`tetrisu`'s two constants are `#define`d to the brain's, so the offline rules
and the server cannot disagree about the numbers.

## The lesson

`BRAIN_LOCKED` was a fact ("nothing below this piece") that read like a verdict
("so it is finished"). A caller that treats a predicate as a decision has made
a rule up, and the rule it made up here was one the other half of the same
project had already written correctly.

The tell was available before the player reported it: the same game played
differently online and offline. Two implementations of one rule is a bug that
has not been noticed yet — which is why the rule now lives in the library both
binaries link, next to `gravity_interval_ms` and `clear_duration_ms`, the two
other numbers that had to mean the same thing at both ends.
