#include "tetrisroom.h"

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
	/* TODO: zero l; clamp max_rooms; store br_slot_count; every
	   next_id[mode] = 0 (room_init is handed ++id, so ids start at 1). */
	(void)l;
	(void)max_rooms;
	(void)br_slot_count;
	return (-1);
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
	/* TODO: find free entry; room_init(room, mode, ++l->next_id[mode],
	   l->br_slot_count); mark in_use; *out = room. */
	(void)l;
	(void)mode;
	(void)out;
	return (-1);
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
	/* TODO: scan in_use entries for a strcmp match on room->name. */
	(void)l;
	(void)name;
	return (NULL);
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
	/* TODO: find entry; slot_clear each slot; in_use false. */
	(void)l;
	(void)name;
	return (-1);
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
	/* TODO: room_to_summary per WAITING/READY room, up to cap. */
	(void)l;
	(void)out;
	(void)cap;
	return (0);
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
	/* TODO: room_to_snapshot per in_use room, up to cap. */
	(void)l;
	(void)out;
	(void)cap;
	return (0);
}

/**
 * @brief Counts the rooms currently registered.
 *
 * @param l The lobby to count.
 * @return Number of live rooms.
 */
size_t	lobby_room_count(const t_lobby *l)
{
	/* TODO: count in_use entries. */
	(void)l;
	return (0);
}
