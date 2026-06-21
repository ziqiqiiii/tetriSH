#include "tetrisbrain.h"

// Tetris Battle Gaiden character specials. Cooldowns/charges/targeting live
// in tetrisd/ability.c (server-side, stateful) - these are just the pure
// board_t transforms each special applies.

/**
 * @brief Wolfman: cuts off the top N rows of the stack.
 *
 * Rows n..H-1 shift up to 0..H-n-1 and the vacated bottom n rows become
 * empty. The cut is clamped to the board height.
 *
 * @param b Pointer to the board to modify.
 * @param n Number of rows to cut from the top; values <= 0 are ignored.
 */
void	board_cut_top(board_t *b, int n)
{
	int	row;

	if (n <= 0)
		return;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = 0; row < BOARD_HEIGHT - n; row++)
		memcpy(&b->cells[row][0], &b->cells[row + n][0],
			sizeof(cell_t) * BOARD_WIDTH);
	for (row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++)
		memset(&b->cells[row][0], 0, sizeof(cell_t) * BOARD_WIDTH);
}

/**
 * @brief Mirurun: smashes down and removes the bottom N lines.
 *
 * Rows 0..H-n-1 shift down to n..H-1 and the vacated top n rows become
 * empty. The copy walks bottom-up so it never overwrites a row it still
 * needs to read.
 *
 * @param b Pointer to the board to modify.
 * @param n Number of rows to cut from the bottom; values <= 0 are ignored.
 */
void	board_cut_bottom(board_t *b, int n)
{
	int	row;

	if (n <= 0)
		return;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = BOARD_HEIGHT - 1; row >= n; row--)
		memcpy(&b->cells[row][0], &b->cells[row - n][0],
			sizeof(cell_t) * BOARD_WIDTH);
	for (row = 0; row < n; row++)
		memset(&b->cells[row][0], 0, sizeof(cell_t) * BOARD_WIDTH);
}

/**
 * @brief Wolfman lvl4: drops every suspended block straight down.
 *
 * AI-assisted. Column by column, compacts non-empty cells to the bottom of
 * each column preserving their relative order, then zeroes whatever is left
 * above, as if every floating block fell to rest on the floor or stack.
 *
 * @param b Pointer to the board to modify.
 */
void	board_apply_gravity(board_t *b)
{
	int	col;
	int	row;
	int	write;

	for (col = 0; col < BOARD_WIDTH; col++)
	{
		write = BOARD_HEIGHT - 1;
		for (row = BOARD_HEIGHT - 1; row >= 0; row--)
			if (b->cells[row][col].type != CELL_EMPTY)
			{
				b->cells[write][col] = b->cells[row][col];
				write--;
			}
		for (row = write; row >= 0; row--)
			b->cells[row][col] = (cell_t){CELL_EMPTY, 0};
	}
}

/**
 * @brief Halloween "Dark": inverts every cell of the board.
 *
 * Empty space becomes garbage and any occupied cell becomes empty.
 *
 * @param b Pointer to the board to modify.
 */
void	board_invert(board_t *b)
{
	int		row;
	int		col;
	cell_t	*c;

	for (row = 0; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
		{
			c = &b->cells[row][col];
			if (c->type == CELL_EMPTY)
				*c = (cell_t){CELL_GARBAGE, 0};
			else
				*c = (cell_t){CELL_EMPTY, 0};
		}
}

/**
 * @brief Halloween "Burn": overwrites the bottom N rows with garbage in place.
 *
 * AI-assisted. Fills the bottom n rows (no shift) with garbage, leaving
 * hole_col empty in each. If hole_col is outside the board the rows come out
 * fully solid, so board_clear_lines sees them as full on the next tick and
 * "burns" them for points.
 *
 * @param b Pointer to the board to modify.
 * @param n Number of bottom rows to fill; values <= 0 are ignored.
 * @param hole_col Column left empty in each filled row.
 */
void	board_fill_rows(board_t *b, int n, int hole_col)
{
	int	row;
	int	col;

	if (n <= 0)
		return;
	if (n > BOARD_HEIGHT)
		n = BOARD_HEIGHT;
	for (row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
			if (col == hole_col)
				b->cells[row][col] = (cell_t){CELL_EMPTY, 0};
			else
				b->cells[row][col] = (cell_t){CELL_GARBAGE, 0};
}

/**
 * @brief Halloween "Bomb": clears a scattered set of cells.
 *
 * cols and rows are parallel arrays of length count. board_set bounds-checks,
 * so a malformed ABILITY frame with out-of-range targets is silently ignored.
 *
 * @param b Pointer to the board to modify.
 * @param cols Array of column indices to clear.
 * @param rows Array of row indices to clear, parallel to cols.
 * @param count Number of (col, row) pairs to clear.
 */
void	board_clear_cells(board_t *b, int cols[], int rows[], int count)
{
	int	i;

	for (i = 0; i < count; i++)
		board_set(b, cols[i], rows[i], (cell_t){CELL_EMPTY, 0});
}

/**
 * @brief Princess: laser-clears a range of columns across every row.
 *
 * Clears columns [start_col, end_col] inclusive on every row with no
 * compaction. Out-of-range bounds are clamped to the board so a bad target
 * cannot write out of bounds. Cells are zeroed with memset (not a compound
 * literal) so cleared cells - padding included - are byte-identical to a
 * board_init'd board, which callers compare against.
 *
 * @param b Pointer to the board to modify.
 * @param start_col First column to clear; clamped to 0.
 * @param end_col Last column to clear; clamped to BOARD_WIDTH - 1.
 */
void	board_delete_columns(board_t *b, int start_col, int end_col)
{
	int	row;
	int	col;

	if (start_col < 0)
		start_col = 0;
	if (end_col >= BOARD_WIDTH)
		end_col = BOARD_WIDTH - 1;
	for (row = 0; row < BOARD_HEIGHT; row++)
		for (col = start_col; col <= end_col; col++)
			memset(&b->cells[row][col], 0, sizeof(cell_t));
}

