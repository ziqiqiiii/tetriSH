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
 * @brief Stores a membership in a JOINING slot, seated and ready.
 *
 * Ready is the state a seat starts in, because arriving in a room is already
 * the answer to the question readiness asks. Only room_seat reaches here, and
 * only once its probe has said the player is still connected, so a seat is
 * ready exactly when somebody has joined and the connection held.
 *
 * That is not the same thing as readiness meaning "somebody is sitting here",
 * which is what this used to do and what broke: readiness was derived from
 * occupancy, so a player who pressed the ready key to withdraw it saw their
 * own client agree for half a second and then be overruled by the next
 * refresh, the server having never had an opinion to change. Readiness is its
 * own stored fact now and room_set_ready still moves it either way - this
 * only decides which way it starts.
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
