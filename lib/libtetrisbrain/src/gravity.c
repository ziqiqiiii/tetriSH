#include "tetrisbrain.h"

/**
 * @brief Advances a piece one row under gravity.
 *
 * Attempts to move the piece down by one row; if it cannot fall, the piece
 * has landed and should be locked by the caller.
 *
 * @param b Pointer to the board to test against.
 * @param p Pointer to the piece to drop (modified on success).
 * @return BRAIN_OK if the piece fell one row, BRAIN_LOCKED if it has landed.
 */
brain_result_t	gravity_tick(const board_t *b, piece_t *p)
{
	if (piece_move(b, p, 0, 1) == BRAIN_OK)
		return (BRAIN_OK);
	return (BRAIN_LOCKED);
}

/**
 * @brief Drops a piece one row in response to a player soft drop.
 *
 * Same fall-by-one / lock-on-landing rule as gravity_tick, but triggered by
 * the player instead of the ticker. The caller awards the soft-drop score
 * bonus on BRAIN_OK, separately from this result.
 *
 * @param b Pointer to the board to test against.
 * @param p Pointer to the piece to drop (modified on success).
 * @return BRAIN_OK if the piece fell one row, BRAIN_LOCKED if it has landed.
 */
brain_result_t	piece_soft_drop(const board_t *b, piece_t *p)
{
	return (gravity_tick(b, p));
}

/**
 * @brief Drops a piece straight down until it lands.
 *
 * Repeatedly moves the piece down by one row until it can fall no further,
 * leaving it resting on the stack or floor.
 *
 * @param b Pointer to the board to test against.
 * @param p Pointer to the piece to drop (modified in place).
 */
void	piece_hard_drop(const board_t *b, piece_t *p)
{
	while (piece_move(b, p, 0, 1) == BRAIN_OK)
		;
}

