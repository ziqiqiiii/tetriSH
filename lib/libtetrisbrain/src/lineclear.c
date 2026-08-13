#include "tetrisbrain.h"

/**
 * @brief Finds every full row on the board, from the bottom up.
 *
 * @param b Board to scan (not modified).
 * @param rows Caller-owned output array of length BRAIN_MAX_CLEAR_LINES,
 * filled with the row indices of the full rows found.
 * @return The number of full rows found (0 to BRAIN_MAX_CLEAR_LINES).
 */
int	board_find_full_lines(const t_board *b, int rows[BRAIN_MAX_CLEAR_LINES])
{
	int		count;
	int		row;
	int		col;
	bool	full;

	count = 0;
	for (row = BOARD_HEIGHT - 1; row >= 0; row--)
	{
		full = true;
		for (col = 0; col < BOARD_WIDTH; col++)
			if (b->cells[row][col].type == CELL_EMPTY)
			{
				full = false;
				break;
			}
		if (full && count < BRAIN_MAX_CLEAR_LINES)
			rows[count++] = row;
	}
	return (count);
}

/**
 * @brief Checks whether every cell on the board is empty.
 *
 * @param b Board to check (not modified).
 * @return true if every cell is CELL_EMPTY, false if any cell is occupied.
 */
bool	board_is_empty(const t_board *b)
{
	int	row;
	int	col;

	for (row = 0; row < BOARD_HEIGHT; row++)
		for (col = 0; col < BOARD_WIDTH; col++)
			if (b->cells[row][col].type != CELL_EMPTY)
				return (false);
	return (true);
}

/**
 * @brief Clears every full row and compacts the rows above down into the gap.
 *
 * @param b Board to clear and compact in place.
 * @return The number of lines cleared (0 to BRAIN_MAX_CLEAR_LINES).
 */
int	board_clear_lines(t_board *b)
{
	int		cleared;
	int		write;
	int		read;
	int		col;
	int		row;
	bool	full;

	cleared = 0;
	write = BOARD_HEIGHT - 1;
	for (read = BOARD_HEIGHT - 1; read >= 0; read--)
	{
		full = true;
		for (col = 0; col < BOARD_WIDTH; col++)
			if (b->cells[read][col].type == CELL_EMPTY)
			{
				full = false;
				break;
			}
		if (full)
		{
			cleared++;
			continue;
		}
		if (write != read)
			memcpy(&b->cells[write][0], &b->cells[read][0],
				sizeof(t_cell) * BOARD_WIDTH);
		write--;
	}
	for (row = write; row >= 0; row--)
		memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);
	return (cleared);
}
