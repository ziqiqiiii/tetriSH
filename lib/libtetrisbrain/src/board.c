#include "tetrisbrain.h"

/**
 * @brief Initialises a board to all-empty, unfilled cells.
 *
 * @param b Board to initialise.
 */
void	board_init(t_board *b)
{
	memset(b, 0, sizeof(*b));
}

/**
 * @brief Checks whether a (col, row) coordinate lies within the board.
 *
 * @param col Column index to check.
 * @param row Row index to check.
 * @return true if the coordinate is within the board, false otherwise.
 */
bool	board_in_bounds(int col, int row)
{
	return (col >= 0 && col < BOARD_WIDTH && row >= 0 && row < BOARD_HEIGHT);
}

/**
 * @brief Reads the cell at (col, row); out of bounds reads as a solid wall.
 *
 * @param b Board to read from.
 * @param col Column index to read.
 * @param row Row index to read.
 * @return The cell at (col, row), or CELL_FILLED if out of bounds.
 */
t_cell	board_get(const t_board *b, int col, int row)
{
	if (!board_in_bounds(col, row))
		return ((t_cell){CELL_FILLED, 0});
	return (b->cells[row][col]);
}

/**
 * @brief Shifts the board up and fills the vacated bottom rows with garbage,
 * leaving hole_col open in each one to drop into.
 *
 * @param b Board to modify.
 * @param lines Garbage lines to inject; clamped to [0, BOARD_HEIGHT].
 * @param hole_col Column left empty in each injected garbage row.
 */
void	board_inject_garbage(t_board *b, int lines, int hole_col)
{
	int	row;
	int	col;

	if (lines <= 0)
		return ;
	if (lines > BOARD_HEIGHT)
		lines = BOARD_HEIGHT;
	for (row = 0; row < BOARD_HEIGHT - lines; row++)
		memcpy(&b->cells[row][0], &b->cells[row + lines][0],
			sizeof(t_cell) * BOARD_WIDTH);
	for (row = BOARD_HEIGHT - lines; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
			if (col == hole_col)
				b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
			else
				b->cells[row][col] = (t_cell){CELL_GARBAGE, 0};
}

/**
 * @brief Writes a cell at (col, row) if the coordinate is in bounds.
 *
 * @param b Board to modify.
 * @param col Column index to write.
 * @param row Row index to write.
 * @param cell Cell value to store.
 */
void	board_set(t_board *b, int col, int row, t_cell cell)
{
	if (board_in_bounds(col, row))
		b->cells[row][col] = cell;
}

/**
 * @brief Copies the full contents of one board into another.
 *
 * @param dst Destination board.
 * @param src Source board to copy from.
 */
void	board_copy(t_board *dst, const t_board *src)
{
	memcpy(dst, src, sizeof(t_board));
}
