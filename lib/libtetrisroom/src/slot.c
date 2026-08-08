#include "tetrisroom.h"

/**
 * @brief Initialises a slot as WAITING and unoccupied.
 *
 * @param s The slot to initialise.
 * @param index The slot's 1-based display index, preserved for life.
 */
void	slot_init(t_slot *s, int index)
{
	if (!s)
		return ;
	memset(s, 0, sizeof(*s));
	s->index = index;
	s->status = SLOT_WAITING;
	s->occupied = false;
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
	if (!s || s->status != SLOT_JOINING)
		return (-1);
	s->membership = m;
	s->occupied = true;
	s->status = SLOT_READY;
	return (0);
}

/**
 * @brief Clears a slot back to WAITING with no occupant.
 *
 * @param s The slot to clear.
 */
void	slot_clear(t_slot *s)
{
	if (!s)
		return ;
	s->occupied = false;
	memset(&s->membership, 0, sizeof(s->membership));
	s->status = SLOT_WAITING;
}
