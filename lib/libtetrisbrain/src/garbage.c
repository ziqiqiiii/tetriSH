#include "tetrisbrain.h"

// Garbage sent per line-clear count, indexed by lines_cleared (0-4).
// Rule: max(0, N - 1) -- spec says "those rows minus 1 are converted to
// garbage". Tetris 99 sends 4 on a Tetris; this follows the spec's N-1.
//
//   1 line  -> 0 garbage  (single clears don't trigger Battle Royale)
//   2 lines -> 1 garbage
//   3 lines -> 2 garbage
//   4 lines -> 3 garbage  (Tetris)
static const int GARBAGE_TABLE[] = {0, 0, 1, 2, 3};

/* AI-assisted: pure lookup into GARBAGE_TABLE after clamping the input.
 * Back-to-back bonus and t-spin multipliers require per-player persistent
 * state, so they are handled by tetrisd, not here. */
int garbage_lines_from_clear(int lines_cleared)
{
	if (lines_cleared <= 0)
		return (0);
	if (lines_cleared > 4)
		lines_cleared = 4;
	return (GARBAGE_TABLE[lines_cleared]);
}
