#include "tetrisbrain.h"

/**
 * @brief Attempts to move a piece down by one row (gravity's basic step).
 *
 * @param b Board to test the move against.
 * @param p Piece to move; updated in place on success.
 * @return BRAIN_OK if the piece moved down one row, BRAIN_LOCKED if it has
 * landed.
 */
t_brain_result	gravity_tick(const t_board *b, t_piece *p)
{
	if (piece_move(b, p, 0, 1) == BRAIN_OK)
		return (BRAIN_OK);
	return (BRAIN_LOCKED);
}

/**
 * @brief Moves a piece down by one row in response to a player input; the
 * same rule as gravity_tick, just player-triggered.
 *
 * @param b Board to test the move against.
 * @param p Piece to move; updated in place on success.
 * @return BRAIN_OK if the piece moved down one row, BRAIN_LOCKED if it has
 * landed.
 */
t_brain_result	piece_soft_drop(const t_board *b, t_piece *p)
{
	return (gravity_tick(b, p));
}

/**
 * @brief Drops a piece straight down until it lands.
 *
 * @param b Board to test the move against.
 * @param p Piece to move; updated in place.
 */
void	piece_hard_drop(const t_board *b, t_piece *p)
{
	while (piece_move(b, p, 0, 1) == BRAIN_OK)
		;
}

/**
 * @brief Computes how far a piece would fall if hard-dropped, without
 * modifying it (e.g. for a drop preview).
 *
 * @param b Board to test the move against.
 * @param p Piece whose projected fall distance is computed.
 * @return Number of rows the piece would fall before landing.
 */
int	piece_drop_distance(const t_board *b, const t_piece *p)
{
	t_piece	projected;
	int		distance;

	projected = *p;
	distance = 0;
	while (piece_move(b, &projected, 0, 1) == BRAIN_OK)
		distance++;
	return (distance);
}
