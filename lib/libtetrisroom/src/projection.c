#include "tetrisroom.h"

// Static Functions
static const char	*owner_name_of(const t_room *r);

/**
 * @brief Projects a room into a lobby row (UC-03.3).
 *
 * Pure read: the room is never mutated by projection.
 *
 * @param r The room to project.
 * @param out Receives id, name, mode, players/slots, status, and owner
 *            name.
 */
void	room_to_summary(const t_room *r, t_room_summary *out)
{
	if (!r || !out)
		return ;
	memset(out, 0, sizeof(*out));
	out->id = r->id;
	memcpy(out->name, r->name, ROOM_NAME_MAX);
	out->mode = r->mode;
	out->players = r->number_of_players;
	out->slot_count = r->slot_count;
	out->status = r->status;
	strncpy(out->owner_name, owner_name_of(r), ROOM_USER_MAX - 1);
}

/**
 * @brief Projects a room into an admin row with member ids (UC-25/26).
 *
 * @param r The room to project.
 * @param out Receives the summary plus every seated player id, in slot
 *            order.
 */
void	room_to_snapshot(const t_room *r, t_room_snapshot *out)
{
	int	i;

	if (!r || !out)
		return ;
	memset(out, 0, sizeof(*out));
	room_to_summary(r, &out->summary);
	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied)
		{
			out->member_pids[out->member_count] = r->slots[i].membership.player_id;
			out->member_count++;
		}
		i++;
	}
}

/**
 * @brief Finds the display name of a room's owner.
 *
 * @param r The room to inspect.
 * @return The owner's username, or "" while the room has no owner yet.
 */
static const char	*owner_name_of(const t_room *r)
{
	int	i;

	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied && membership_is_owner(&r->slots[i].membership))
			return (r->slots[i].membership.username);
		i++;
	}
	return ("");
}
