#include "tetrisbrain.h"

/* Tetris Battle Gaiden character specials. Cooldowns, charges, and
 * targeting live in tetrisd/ability.c (server-side, stateful) - these are
 * just the pure t_board transforms each special applies. */

/**
 * @brief Cuts the top N rows off the board, shifting the rest up (Wolfman).
 *
 * @param b Board to modify.
 * @param n Rows to cut from the top; clamped to [0, BOARD_HEIGHT].
 */
void	board_cut_top(t_board *b, int n)
{
	int	row;

	if (n <= 0)
		return ;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = 0; row < BOARD_HEIGHT - n; row++)
		memcpy(&b->cells[row][0], &b->cells[row + n][0],
			sizeof(t_cell) * BOARD_WIDTH);
	for (row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++)
		memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);
}

/**
 * @brief Cuts the bottom N rows off the board, shifting the rest down
 * (Mirurun).
 *
 * @param b Board to modify.
 * @param n Rows to cut from the bottom; clamped to [0, BOARD_HEIGHT].
 */
void	board_cut_bottom(t_board *b, int n)
{
	int	row;

	if (n <= 0)
		return ;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = BOARD_HEIGHT - 1; row >= n; row--)
		memcpy(&b->cells[row][0], &b->cells[row - n][0],
			sizeof(t_cell) * BOARD_WIDTH);
	for (row = 0; row < n; row++)
		memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);
}

/**
 * @brief Drops every suspended cell straight down per column, preserving
 * relative order (Wolfman level-4).
 *
 * @param b Board to modify.
 */
void	board_apply_gravity(t_board *b)
{
	int	col;
	int	row;
	int	write;

	for (col = 0; col < BOARD_WIDTH; col++)
	{
		write = BOARD_HEIGHT - 1;
		for (row = BOARD_HEIGHT - 1; row >= 0; row--)
		{
			if (b->cells[row][col].type != CELL_EMPTY)
			{
				b->cells[write][col] = b->cells[row][col];
				write--;
			}
		}
		for (row = write; row >= 0; row--)
			b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
	}
}

/**
 * @brief Inverts the board: empty cells become garbage, occupied cells
 * become empty (Halloween "Dark").
 *
 * @param b Board to modify.
 */
void	board_invert(t_board *b)
{
	int		row;
	int		col;
	t_cell	*c;

	for (row = 0; row < BOARD_HEIGHT; row++)
	{
		for (col = 0; col < BOARD_WIDTH; col++)
		{
			c = &b->cells[row][col];
			if (c->type == CELL_EMPTY)
				*c = (t_cell){CELL_GARBAGE, 0};
			else
				*c = (t_cell){CELL_EMPTY, 0};
		}
	}
}

/**
 * @brief Overwrites the bottom N rows in place with garbage, leaving
 * hole_col empty in each (Halloween "Burn"). An out-of-range hole_col
 * leaves the rows fully solid, so they clear for points on the next tick.
 *
 * @param b Board to modify.
 * @param n Rows to fill from the bottom; clamped to [0, BOARD_HEIGHT].
 * @param hole_col Column left empty in each filled row.
 */
void	board_fill_rows(t_board *b, int n, int hole_col)
{
	int	row;
	int	col;

	if (n <= 0)
		return ;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
			if (col == hole_col)
				b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
			else
				b->cells[row][col] = (t_cell){CELL_GARBAGE, 0};
}

/**
 * @brief Clears an arbitrary scattered set of cells (Halloween "Bomb").
 * Out-of-range targets are silently ignored (board_set bounds-checks).
 *
 * @param b Board to modify.
 * @param cols Column indices of the cells to clear, parallel to rows.
 * @param rows Row indices of the cells to clear, parallel to cols.
 * @param count Number of (col, row) pairs to clear.
 */
void	board_clear_cells(t_board *b, int cols[], int rows[], int count)
{
	int	i;

	for (i = 0; i < count; i++)
		board_set(b, cols[i], rows[i], (t_cell){CELL_EMPTY, 0});
}

/**
 * @brief Clears columns [start_col, end_col] across every row, no
 * compaction (Princess laser). Bounds are clamped to the board. Uses
 * memset so cleared cells are byte-identical to a board_init'd board.
 *
 * @param b Board to modify.
 * @param start_col First column to clear; clamped up to 0 if negative.
 * @param end_col Last column to clear; clamped to BOARD_WIDTH - 1.
 */
void	board_delete_columns(t_board *b, int start_col, int end_col)
{
	int	row;
	int	col;

	if (start_col < 0)
		start_col = 0;
	if (end_col >= BOARD_WIDTH)
		end_col = BOARD_WIDTH - 1;
	for (row = 0; row < BOARD_HEIGHT; row++)
		for (col = start_col; col <= end_col; col++)
			memset(&b->cells[row][col], 0, sizeof(t_cell));
}

/**
 * @brief Clears full lines and lets survivors fall, repeating while the
 * fall completes new lines (Wolfman "Thwack"). A no-op pass leaves the
 * board untouched - no stray gravity.
 *
 * @param b Board to modify.
 * @return Total lines cleared across the cascade; can exceed
 * BRAIN_MAX_CLEAR_LINES since it's a running sum, not one pass.
 */
int	board_cascade_clear(t_board *b)
{
	int	total;
	int	cleared;

	total = 0;
	cleared = board_clear_lines(b);
	while (cleared > 0)
	{
		total += cleared;
		board_apply_gravity(b);
		cleared = board_clear_lines(b);
	}
	return (total);
}
