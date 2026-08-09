#include "tetrisbrain.h"

/*
** Extended Placement Lock Down, from the Tetris Design Guideline.
**
** A piece that touches down is not finished: it belongs to the player for
** another half-second, which is what lets a piece be slid into an overhang
** instead of merely dropped on top of one. Each move or rotation buys that
** half-second back, so the delay is a budget rather than a deadline.
**
** The budget has to be finite or a player could hover a piece indefinitely,
** so it is fifteen resets per piece - refilled every time the piece falls
** past the lowest row it has occupied, which is the Guideline's way of
** saying that progress downward earns more time and shuffling sideways does
** not.
**
** No time is kept here beyond what the caller hands over, and the state is
** the caller's: this module decides when a piece is out of time, not what
** happens to it then.
*/

/**
 * @brief Arms a fresh lock down for a newly spawned or newly held piece.
 *
 * The spawn row is recorded as the lowest row reached so far, so the very
 * first row the piece falls to already refills its reset budget.
 *
 * @param lock Lock-down state to arm.
 * @param p Piece the state belongs to.
 */
void	lockdown_init(t_lockdown *lock, const t_piece *p)
{
	if (lock == NULL)
		return ;
	lock->elapsed_ms = 0;
	lock->resets = 0;
	lock->lowest_row = 0;
	if (p != NULL)
		lock->lowest_row = p->row;
}

/**
 * @brief Reports whether a piece is resting on the stack or the floor.
 *
 * The piece is probed on a copy, so asking the question never moves it.
 *
 * @param b Board the piece is falling through.
 * @param p Piece to test.
 * @return true when the piece cannot descend another row.
 */
bool	lockdown_grounded(const t_board *b, const t_piece *p)
{
	t_piece	probe;

	if (b == NULL || p == NULL)
		return (false);
	probe = *p;
	return (piece_move(b, &probe, 0, 1) != BRAIN_OK);
}

/**
 * @brief Refills the reset budget when the piece reaches a new lowest row.
 *
 * Called after a descent, whether gravity or the player caused it. A piece
 * that is only rising - a rotation kicked upward, say - leaves the budget
 * alone, because the row it already reached is the one it has to beat.
 *
 * @param lock Lock-down state to refill.
 * @param p Piece in its position after the descent.
 */
void	lockdown_on_fall(t_lockdown *lock, const t_piece *p)
{
	if (lock == NULL || p == NULL || p->row <= lock->lowest_row)
		return ;
	lock->lowest_row = p->row;
	lock->resets = 0;
	lock->elapsed_ms = 0;
}

/**
 * @brief Spends one reset on a move or rotation the player just made.
 *
 * Only a piece that was already resting spends anything: a move made in the
 * air costs nothing, because the timer it would refresh is not running.
 * Grounded-ness is read before the move rather than after, so sliding a piece
 * off the edge of the stack is the move that spends the reset, not the one
 * that comes after it lands again.
 *
 * @param lock Lock-down state to charge.
 * @param was_grounded Whether the piece was resting before the move.
 */
void	lockdown_on_shift(t_lockdown *lock, bool was_grounded)
{
	if (lock == NULL || !was_grounded || lock->resets >= LOCKDOWN_MAX_RESETS)
		return ;
	lock->elapsed_ms = 0;
	lock->resets++;
}

/**
 * @brief Charges elapsed time against a resting piece.
 *
 * A piece in the air is not on the clock at all, and its timer is cleared
 * rather than paused - a piece pushed off a ledge starts its half-second
 * over when it lands again.
 *
 * @param lock Lock-down state to advance.
 * @param grounded Whether the piece is resting right now.
 * @param elapsed_ms Milliseconds since the last tick.
 * @return true when the piece is out of time and must lock.
 */
bool	lockdown_tick(t_lockdown *lock, bool grounded, int elapsed_ms)
{
	if (lock == NULL)
		return (false);
	if (!grounded)
	{
		lock->elapsed_ms = 0;
		return (false);
	}
	if (elapsed_ms > 0)
		lock->elapsed_ms += elapsed_ms;
	return (lock->elapsed_ms >= LOCKDOWN_DELAY_MS);
}
