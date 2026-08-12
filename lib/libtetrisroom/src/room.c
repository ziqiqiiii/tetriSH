#include "tetrisroom.h"

// Static Variables
// id prefix per t_game_mode; const table, not mutable module state
static const char	*g_mode_prefix[] = {"S", "D", "BR"};

// Static Functions
static void		apply_mode_shape(t_room *r, t_game_mode mode, int br_slots);
static t_slot	*first_free_slot(t_room *r);

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
	int	i;

	if (!r || mode > MODE_BATTLE_ROYALE || id < 1)
		return (-1);
	memset(r, 0, sizeof(*r));
	r->id = id;
	snprintf(r->name, ROOM_NAME_MAX, "%s-%02d", g_mode_prefix[mode], id);
	r->mode = mode;
	apply_mode_shape(r, mode, br_slots);
	i = 0;
	while (i < r->slot_count)
	{
		slot_init(&r->slots[i], i + 1);
		i++;
	}
	r->number_of_players = 0;
	r->status = ROOM_WAITING;
	return (0);
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
	if (!r)
		return (JOIN_FULL);
	/*
	 * SELECTING refuses a newcomer for the same reason IN_GAME does: the
	 * match is already being set up for the players in it, and a seat taken
	 * halfway through the window would be one nobody had chosen a fighter for.
	 */
	if (r->status == ROOM_SELECTING || r->status == ROOM_IN_GAME
		|| r->status == ROOM_FINISHED)
		return (JOIN_IN_GAME);
	if (r->number_of_players >= r->slot_count)
		return (JOIN_FULL);
	return (JOIN_ACCEPTED);
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
	t_slot		*s;
	t_room_role	role;

	if (room_can_accept(r) != JOIN_ACCEPTED || room_find_member(r, pid))
		return (-1);
	s = first_free_slot(r);
	if (!s)
		return (-1);
	s->status = SLOT_JOINING;
	if (probe && !probe(probe_ctx, pid))
	{
		s->status = SLOT_WAITING;
		return (-1);
	}
	role = ROLE_PLAYER;
	if (r->number_of_players == 0)
		role = ROLE_OWNER;
	if (slot_occupy(s, membership_make(pid, username, role)) != 0)
	{
		s->status = SLOT_WAITING;
		return (-1);
	}
	r->number_of_players++;
	room_recompute_status(r);
	return (s->index);
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
	if (!r || (r->status != ROOM_WAITING && r->status != ROOM_READY))
		return ;
	if (r->number_of_players >= r->min_to_start)
		r->status = ROOM_READY;
	else
		r->status = ROOM_WAITING;
}

/**
 * @brief Sets whether one seated player has declared themselves ready.
 *
 * Readiness is a seat's own fact, not a consequence of the seat being taken:
 * a room whose every occupant was ready by definition could never be told
 * that one of them was not.
 *
 * Never touched once a room is in game or finished - a match does not care,
 * and letting it change there would leave a finished room looking startable.
 *
 * @param r The room holding the seat.
 * @param pid The player declaring.
 * @param ready true to declare ready, false to withdraw it.
 * @return 0 when the seat was found and set, -1 otherwise.
 */
int	room_set_ready(t_room *r, t_player_id pid, bool ready)
{
	int	i;

	if (!r || r->status == ROOM_IN_GAME || r->status == ROOM_FINISHED)
		return (-1);
	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied
			&& r->slots[i].membership.player_id == pid)
		{
			if (ready)
				r->slots[i].status = SLOT_READY;
			else
				r->slots[i].status = SLOT_WAITING;
			return (0);
		}
		i++;
	}
	return (-1);
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
	int	i;

	if (!r)
		return (NULL);
	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied && r->slots[i].membership.player_id == pid)
			return (&r->slots[i].membership);
		i++;
	}
	return (NULL);
}

/**
 * @brief Sets a room's slot count and start threshold from its mode.
 *
 * @param r The room being initialised.
 * @param mode The game mode.
 * @param br_slots Requested slot count, honoured only for BATTLE_ROYALE.
 */
static void	apply_mode_shape(t_room *r, t_game_mode mode, int br_slots)
{
	if (mode == MODE_SINGLE)
	{
		r->slot_count = 1;
		r->min_to_start = 1;
	}
	else if (mode == MODE_DOUBLE)
	{
		r->slot_count = 2;
		r->min_to_start = 2;
	}
	else
	{
		if (br_slots < 4)
			br_slots = 4;
		if (br_slots > ROOM_MAX_SLOTS)
			br_slots = ROOM_MAX_SLOTS;
		r->slot_count = br_slots;
		r->min_to_start = 4;
	}
}

/**
 * @brief Finds the first free slot a joining player may take.
 *
 * @param r The room to scan.
 * @return The first unoccupied WAITING slot, or NULL when none is free.
 */
static t_slot	*first_free_slot(t_room *r)
{
	int	i;

	i = 0;
	while (i < r->slot_count)
	{
		if (!r->slots[i].occupied && r->slots[i].status == SLOT_WAITING)
			return (&r->slots[i]);
		i++;
	}
	return (NULL);
}
