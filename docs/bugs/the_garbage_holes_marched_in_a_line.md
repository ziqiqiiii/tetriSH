# Bug Note — The garbage holes marched in a line

**Status: fixed.** Covered by `test_the_holes_are_not_a_diagonal` in
`src/tetrisd/tests/test_garbage.c`, which fails against the old arithmetic.

## What the bug was

Playing a Battle Royale, the gaps in received garbage formed a clean diagonal
across the board — every row's hole one column right of the row below it. The
player reported it as a broken random number generator, and as a platform
difference: it showed on their Linux machine and not on their Mac.

Neither half was true, and both were reasonable readings.

There was no random number generator to break. `src/tetrisd/src/game.c`:

```c
board_inject_garbage(&g->board, 1, (int)(g->garbage_seq % (uint32_t)BOARD_WIDTH));
g->garbage_seq++;
```

`garbage_seq` is a counter incremented once per row. Taken modulo
`BOARD_WIDTH`, it puts the holes on columns 0, 1, 2, 3 in order and starts over
at 9. That is a staircase, deterministically, on every platform.

The Mac played the same code. `tetrisu` never injects garbage — the client only
draws what `STATE` sends it — and macOS cannot run `tetrisd` at all, so both
machines were attacking the same server. What differed was how much garbage
arrived: one row here and there is a hole in a column, and it takes a Battle
Royale's worth of rows before the shape is a shape.

## Why it got written that way

The comment above it says what it was defending against, and it was a real
thing to defend against:

> A run of rows sharing one hole would be a wall rather than a handicap.

True. Ten rows with the hole in one column can only be answered by an I piece
on end, and the receiver is dead on arrival. So the hole had to move.

The defect is that *moving* was answered with *marching*. A counter modulo the
width does move the hole every row, and it never stacks — it satisfies the
stated requirement completely. It is also the single most legible pattern
available, which the requirement never said anything about.

## Why the test did not catch it

There was a test, and it asserted the requirement as written:

```c
assert(row_hole(&g, BOARD_HEIGHT - 1) != row_hole(&g, BOARD_HEIGHT - 2));
```

Adjacent rows differ. They did — by exactly one, every time, which is the
defect. The test encoded the sentence in the comment rather than the property a
player would look at, and a diagonal passes it at every neighbouring pair.

## The fix

The column is drawn from the game's own seeded LCG — the same one `apply_bomb`
already used — and only the *previous* column is excluded, which is all the
original worry was ever about:

```c
static int	next_garbage_hole(t_game *g)
{
	int	column;

	g->garbage_seq = g->garbage_seq * 1664525u + 1013904223u;
	if (g->garbage_hole < 0 || BOARD_WIDTH < 2)
		column = (int)((g->garbage_seq >> 16) % (uint32_t)BOARD_WIDTH);
	else
	{
		column = (int)((g->garbage_seq >> 16) % (uint32_t)(BOARD_WIDTH - 1));
		if (column >= g->garbage_hole)
			column++;
	}
	g->garbage_hole = column;
	return (column);
}
```

Drawing over `BOARD_WIDTH - 1` and stepping past the previous hole leaves every
remaining column equally likely, with no rejection loop that can spin.

`garbage_seq` becomes a state rather than a count, seeded in `game_start` from
the game's own seed, so a match replayed from one seed still lands the same
rows in the same places. No randomness enters `libtetrisbrain`: the column
crosses as an argument, as it always did.

`garbage_hole` starts at `-1` rather than at a column, so the first draw is
uniform over all ten rather than over nine of them.

## The second mistake, which is the more useful one

The first version of the regression test asserted that the step from one hole
to the next is not the same number every time — and it **passed against the
broken code**.

A march wraps. Columns 8, 9, 0 reads as steps of `+1` then `-9`, and a plain
subtraction calls that variety. The test was measuring on a line; the columns
live on a cylinder. Taking the step modulo `BOARD_WIDTH` makes a march exactly
one step repeated fourteen times, and the test fails as it should:

```
test_garbage.c:368: test_the_holes_are_not_a_diagonal: Assertion `varies' failed.
```

Both mistakes are the same mistake at different scales: asserting the words the
implementation was described with instead of the property somebody would
actually see. A regression test that has not been run against the defect is a
test of the fix, not of the bug.

## The lesson

*"It moves"* is not *"it is not a pattern."* When the requirement is that
something look unstructured, the test has to ask about structure — over a run,
in the space the values actually live in — because every mechanical rule
satisfies "it changes every time" and most of them are visible on sight.

And when a bug is reported as a platform difference, check whether the code even
differs per platform before believing it. Here it could not: one machine cannot
run the server, the client never generated the value, and the real variable was
how much of the output the player had seen.
