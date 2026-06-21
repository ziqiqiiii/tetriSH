#include "tetrisbrain.h"

/**
 * @brief Clears full rows and compacts the surviving stack downward.
 *
 * Scans from the bottom up: full rows are dropped, every surviving row is
 * copied down into the next free slot, and whatever remains above the write
 * cursor after the scan is zeroed to empty.
 *
 * @param b Pointer to the board to clear lines from.
 * @return The number of full rows cleared (0-4).
 */
int	board_clear_lines(board_t *b)
{
	int		cleared;
	int		write;
	int		read;
	int		col;
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
				sizeof(cell_t) * BOARD_WIDTH);
		write--;
	}
	for (read = write; read >= 0; read--)
		memset(&b->cells[read][0], 0, sizeof(cell_t) * BOARD_WIDTH);
	return (cleared);
}

