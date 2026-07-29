#include "tetrisroom.h"

// Static Variables
// id prefix per t_game_mode; const table, not mutable module state
static const char	*g_mode_prefix[] = {"S", "D", "BR"};

/**
 * @brief Initialises a room and derives its display name from mode and id.
 *
 * Slots/threshold come from the mode: SINGLE 1/1, DOUBLE 2/2,
 * BATTLE_ROYALE br_slots (clamped 4-99)/4. The name is "<prefix>-<id>"
 * zero-padded to two digits (S-01, D-02, BR-10); callers never supply
 * either field.
 *
 * @param r The room to initialise.
 * @param mode The game mode.
 * @param id Per-mode room id, 1-based; the lobby owns the counter.
 * @param br_slots Requested slot count, used only for BATTLE_ROYALE.
 * @return 0 on success, -1 on NULL room, unknown mode, or id < 1.
 */
int	room_init(t_room *r, t_game_mode mode, int id, int br_slots)
{
	/* TODO: validate r/mode/id; zero r; store id; snprintf name as
	   "<g_mode_prefix[mode]>-%02d"; slot_count/min_to_start from mode;
	   slot_init each slot with 1-based index; ROOM_WAITING. */
	(void)r;
	(void)mode;
	(void)id;
	(void)br_slots;
	(void)g_mode_prefix;
	return (-1);
}

/**
 * @brief Decides whether a join attempt may proceed.
 *
 * Status is checked before occupancy, so an IN_GAME room with a free slot
 * still answers JOIN_IN_GAME (UT-35). Never mutates the room.
 *
 * @param r The room to query.
 * @return JOIN_ACCEPTED, JOIN_FULL, or JOIN_IN_GAME.
 */
t_join_verdict	room_can_accept(const t_room *r)
{
	/* TODO: IN_GAME/FINISHED -> JOIN_IN_GAME; players == slot_count ->
	   JOIN_FULL; else JOIN_ACCEPTED. */
	(void)r;
	return (JOIN_FULL);
}

/**
 * @brief Seats a player in the first free slot.
 *
 * Walks WAITING -> JOINING -> (probe) -> READY; a failed probe aborts back
 * to WAITING with no partial seat (UC-04 ext 5a). The first seated player
 * becomes ROLE_OWNER. Increments the count and recomputes status.
 *
 * @param r The room to seat into.
 * @param pid The joining player's id.
 * @param username The joining player's display name.
 * @param probe Connection probe; NULL means always connected.
 * @param probe_ctx Opaque context passed to probe.
 * @return The 1-based slot index, or -1 (full, in-game, duplicate pid,
 *         or disconnected during join).
 */
int	room_seat(t_room *r, t_player_id pid, const char *username,
		bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx)
{
	/* TODO: verdict check; duplicate check; first free slot -> JOINING;
	   probe fail -> slot back to WAITING, -1; membership_make (owner if
	   count == 0); slot_occupy; count++; room_recompute_status. */
	(void)r;
	(void)pid;
	(void)username;
	(void)probe;
	(void)probe_ctx;
	return (-1);
}

/**
 * @brief Recomputes WAITING/READY from the player count.
 *
 * Only crossing min_to_start flips the status; IN_GAME and FINISHED are
 * never touched (those transitions belong to room_start/room_finish).
 *
 * @param r The room to recompute.
 */
void	room_recompute_status(t_room *r)
{
	/* TODO: skip unless WAITING/READY; count >= min -> READY else
	   WAITING. */
	(void)r;
}

/**
 * @brief Finds the seated membership for a player.
 *
 * @param r The room to search.
 * @param pid The player to find.
 * @return Pointer to the stored membership (identity, not a copy), or
 *         NULL when the player is not seated.
 */
t_membership	*room_find_member(t_room *r, t_player_id pid)
{
	/* TODO: scan occupied slots for pid. */
	(void)r;
	(void)pid;
	return (NULL);
}
