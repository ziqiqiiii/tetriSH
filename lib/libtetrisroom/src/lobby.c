#include "tetrisroom.h"

// Static Functions
static t_room	*entry_by_name(const t_lobby *l, const char *name);

/**
 * @brief Initialises an empty lobby with clamped capacity settings.
 *
 * @param l The lobby to initialise.
 * @param max_rooms Room cap, clamped to 1..LOBBY_MAX_ROOMS.
 * @param br_slot_count Battle Royale slot count passed to room_init.
 * @return 0 on success, -1 on NULL lobby.
 */
int	lobby_init(t_lobby *l, size_t max_rooms, int br_slot_count)
{
	if (!l)
		return (-1);
	memset(l, 0, sizeof(*l));
	if (max_rooms < 1)
		max_rooms = 1;
	if (max_rooms > LOBBY_MAX_ROOMS)
		max_rooms = LOBBY_MAX_ROOMS;
	l->max_rooms = max_rooms;
	l->br_slot_count = br_slot_count;
	l->next_id[MODE_SINGLE] = 0;
	l->next_id[MODE_DOUBLE] = 0;
	l->next_id[MODE_BATTLE_ROYALE] = 0;
	return (0);
}

/**
 * @brief Registers a new empty room, taking its id from that mode's
 * counter (S-01, D-02, BR-10).
 *
 * Each counter is monotonic: destroying a room never recycles its id, so
 * a stale rejoin can't land on a different room. Does not seat anyone -
 * the controller calls room_seat afterwards (create -> seat, UT-23).
 *
 * @param l The lobby to register into.
 * @param mode The game mode for the new room.
 * @param out Receives the registered room (identity, caller never frees).
 * @return 0 on success, -1 when the lobby is at max_rooms or mode is bad.
 */
int	lobby_create_room(t_lobby *l, t_game_mode mode, t_room **out)
{
	size_t	i;

	if (!l || !out || mode > MODE_BATTLE_ROYALE)
		return (-1);
	i = 0;
	while (i < l->max_rooms && l->in_use[i])
		i++;
	if (i == l->max_rooms)
		return (-1);
	if (room_init(&l->rooms[i], mode, l->next_id[mode] + 1,
			l->br_slot_count) != 0)
		return (-1);
	l->next_id[mode]++;
	l->in_use[i] = true;
	*out = &l->rooms[i];
	return (0);
}

/**
 * @brief Looks a room up by display name.
 *
 * Name, not id: ids run per mode, so only the name (S-01, D-02, BR-10) is
 * unique lobby-wide, and it is what clients send back.
 *
 * @param l The lobby to search.
 * @param name The room name to find.
 * @return The stored room (identity, not a copy), or NULL when unknown -
 *         the map is never mutated by a miss.
 */
t_room	*lobby_find_room(t_lobby *l, const char *name)
{
	return (entry_by_name(l, name));
}

/**
 * @brief Removes a room from the lobby and clears its slots (UC-07 ext 3b).
 *
 * Chat teardown is the caller's job - this is pure registry state.
 *
 * @param l The lobby to remove from.
 * @param name The name of the room to destroy.
 * @return 0 on success, -1 when the name is unknown.
 */
int	lobby_destroy_room(t_lobby *l, const char *name)
{
	t_room	*r;
	size_t	index;
	int		i;

	r = entry_by_name(l, name);
	if (!r)
		return (-1);
	i = 0;
	while (i < r->slot_count)
	{
		slot_clear(&r->slots[i]);
		i++;
	}
	r->number_of_players = 0;
	index = (size_t)(r - l->rooms);
	l->in_use[index] = false;
	return (0);
}

/**
 * @brief Lists WAITING and READY rooms as lobby rows (UC-03).
 *
 * IN_GAME and FINISHED rooms are excluded. Never mutates the lobby.
 *
 * @param l The lobby to list.
 * @param out Caller array receiving the summaries.
 * @param cap Capacity of out; extra rooms are silently dropped.
 * @return Number of summaries written.
 */
size_t	lobby_list_open(const t_lobby *l, t_room_summary *out, size_t cap)
{
	size_t	n;
	size_t	i;

	if (!l || !out)
		return (0);
	n = 0;
	i = 0;
	while (i < l->max_rooms && n < cap)
	{
		if (l->in_use[i] && (l->rooms[i].status == ROOM_WAITING
				|| l->rooms[i].status == ROOM_READY))
		{
			room_to_summary(&l->rooms[i], &out[n]);
			n++;
		}
		i++;
	}
	return (n);
}

/**
 * @brief Lists every room as an admin snapshot, all statuses (UC-25).
 *
 * @param l The lobby to list.
 * @param out Caller array receiving the snapshots.
 * @param cap Capacity of out; extra rooms are silently dropped.
 * @return Number of snapshots written.
 */
size_t	lobby_list_all(const t_lobby *l, t_room_snapshot *out, size_t cap)
{
	size_t	n;
	size_t	i;

	if (!l || !out)
		return (0);
	n = 0;
	i = 0;
	while (i < l->max_rooms && n < cap)
	{
		if (l->in_use[i])
		{
			room_to_snapshot(&l->rooms[i], &out[n]);
			n++;
		}
		i++;
	}
	return (n);
}

/**
 * @brief Counts the rooms currently registered.
 *
 * @param l The lobby to count.
 * @return Number of live rooms.
 */
size_t	lobby_room_count(const t_lobby *l)
{
	size_t	n;
	size_t	i;

	if (!l)
		return (0);
	n = 0;
	i = 0;
	while (i < l->max_rooms)
	{
		if (l->in_use[i])
			n++;
		i++;
	}
	return (n);
}

/**
 * @brief Finds a live room entry by its display name.
 *
 * @param l The lobby to search.
 * @param name The room name to match.
 * @return The stored room, or NULL when no live entry matches.
 */
static t_room	*entry_by_name(const t_lobby *l, const char *name)
{
	t_room	*rooms;
	size_t	i;

	if (!l || !name)
		return (NULL);
	rooms = (t_room *)l->rooms;
	i = 0;
	while (i < l->max_rooms)
	{
		if (l->in_use[i] && strcmp(rooms[i].name, name) == 0)
			return (&rooms[i]);
		i++;
	}
	return (NULL);
}
