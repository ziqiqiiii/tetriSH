#include "tetrisu.h"

/**
 * @brief Returns the terminal rows occupied by the authored Solo artwork.
 *
 * The source artwork ends at y=368, exactly 23 authored 16-pixel rows.
 *
 * @param tile_rows Terminal rows used for one authored 16-pixel row.
 * @return Content height in terminal rows, or 0 for invalid input.
 */
int	solo_layout_content_rows(int tile_rows)
{
	if (tile_rows <= 0)
		return (0);
	return (tile_rows * SOLO_CONTENT_TILE_ROWS);
}

/**
 * @brief Returns the complete Solo height including the crisp control legend.
 *
 * @param tile_rows Terminal rows used for one authored 16-pixel row.
 * @return Total canvas height in terminal rows, or 0 for invalid input.
 */
int	solo_layout_canvas_rows(int tile_rows)
{
	int	content_rows;

	content_rows = solo_layout_content_rows(tile_rows);
	if (content_rows == 0)
		return (0);
	return (content_rows + SOLO_TERMINAL_CONTROLS_ROWS);
}

/**
 * @brief Tests whether one integer Solo scale fits a terminal.
 *
 * @param terminal_rows Available terminal rows.
 * @param terminal_cols Available terminal columns.
 * @param tile_rows Candidate terminal rows per authored tile.
 * @param tile_cols Candidate terminal columns per authored tile.
 * @return true when both dimensions fit.
 */
bool	solo_layout_terminal_fits(int terminal_rows, int terminal_cols,
	int tile_rows, int tile_cols)
{
	if (terminal_rows <= 0 || terminal_cols <= 0
		|| tile_rows <= 0 || tile_cols <= 0)
		return (false);
	return (solo_layout_canvas_rows(tile_rows) <= terminal_rows
		&& tile_cols * 32 <= terminal_cols);
}
