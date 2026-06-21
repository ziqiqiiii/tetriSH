#include "tetrisbrain.h"

/**
 * @brief Resets every cell of a board to the empty state.
 *
 * Zeroes the whole board structure so all cells read as CELL_EMPTY with no
 * colour, leaving the board ready for a new game.
 *
 * @param b Pointer to the board to initialise.
 */
void	board_init(board_t *b)
{
	memset(b, 0, sizeof(*b));
}

/**
 * @brief Reports whether a column/row pair lies inside the board.
 *
 * @param col Column index to test.
 * @param row Row index to test.
 * @return true if the coordinates fall within the board, false otherwise.
 */
bool	board_in_bounds(int col, int row)
{
	return (col >= 0 && col < BOARD_WIDTH && row >= 0 && row < BOARD_HEIGHT);
}

/**
 * @brief Reads the cell at the given coordinates.
 *
 * Out-of-bounds reads return a solid CELL_FILLED wall so collision checks
 * work uniformly without range guards in every caller.
 *
 * @param b Pointer to the board to read from.
 * @param col Column index of the cell.
 * @param row Row index of the cell.
 * @return The cell at (col, row), or a CELL_FILLED wall when out of bounds.
 */
cell_t	board_get(const board_t *b, int col, int row)
{
	if (!board_in_bounds(col, row))
		return ((cell_t){CELL_FILLED, 0});
	return (b->cells[row][col]);
}

/**
 * @brief Pushes garbage rows onto the bottom of the board.
 *
 * Shifts the existing stack up by `lines` rows (the topmost rows fall off the
 * top) and fills the freed bottom rows with garbage, leaving a single empty
 * hole column in each.
 *
 * @param b Pointer to the board to modify.
 * @param lines Number of garbage rows to inject; values <= 0 are ignored.
 * @param hole_col Column left empty in each injected garbage row.
 */
void	board_inject_garbage(board_t *b, int lines, int hole_col)
{
	int	row;
	int	col;

	if (lines <= 0)
		return;
	if (lines > BOARD_HEIGHT)
		lines = BOARD_HEIGHT;
	for (row = 0; row < BOARD_HEIGHT - lines; row++)
		memcpy(&b->cells[row][0], &b->cells[row + lines][0],
			sizeof(cell_t) * BOARD_WIDTH);
	for (row = BOARD_HEIGHT - lines; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
			if (col == hole_col)
				b->cells[row][col] = (cell_t){CELL_EMPTY, 0};
			else
				b->cells[row][col] = (cell_t){CELL_GARBAGE, 0};
}

/**
 * @brief Writes a cell at the given coordinates if they are in bounds.
 *
 * Out-of-bounds writes are silently ignored, so callers need no range guard.
 *
 * @param b Pointer to the board to modify.
 * @param col Column index of the target cell.
 * @param row Row index of the target cell.
 * @param cell The cell value to store.
 */
void	board_set(board_t *b, int col, int row, cell_t cell)
{
	if (board_in_bounds(col, row))
		b->cells[row][col] = cell;
}

/**
 * @brief Copies the entire contents of one board into another.
 *
 * @param dst Pointer to the destination board.
 * @param src Pointer to the source board.
 */
void	board_copy(board_t *dst, const board_t *src)
{
	memcpy(dst, src, sizeof(board_t));
}

