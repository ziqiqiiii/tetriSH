#include "tetrisroom.h"

/**
 * @brief Initialises a slot as WAITING and unoccupied.
 *
 * @param s The slot to initialise.
 * @param index The slot's 1-based display index, preserved for life.
 */
void	slot_init(t_slot *s, int index)
{
	/* TODO: zero s; s->index = index; SLOT_WAITING; occupied false. */
	(void)s;
	(void)index;
}

/**
 * @brief Stores a membership in a JOINING slot and marks it READY.
 *
 * @param s The slot to occupy.
 * @param m The membership to store.
 * @return 0 on success, -1 when the slot is not in JOINING (unchanged).
 */
int	slot_occupy(t_slot *s, t_membership m)
{
	/* TODO: reject status != SLOT_JOINING; store m; occupied true;
	   status SLOT_READY; index preserved. */
	(void)s;
	(void)m;
	return (-1);
}

/**
 * @brief Clears a slot back to WAITING with no occupant.
 *
 * @param s The slot to clear.
 */
void	slot_clear(t_slot *s)
{
	/* TODO: occupied false; zero membership; SLOT_WAITING; keep index. */
	(void)s;
}
