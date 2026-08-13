#include "tetrisroom.h"

/**
 * @brief Decides whether a start request may proceed (UC-08, UC-08a).
 *
 * A SELECTING room is startable, because starting is exactly what closes the
 * window: room_start is the transition the room's own clock takes when both
 * players have chosen. Refusing a second *request* to start one is a
 * different question and belongs to the caller that received it.
 *
 * @param r The room to query.
 * @param requester The player asking to start.
 * @return START_ACCEPTED, START_NOT_OWNER, START_TOO_FEW_PLAYERS, or
 *         START_ALREADY_STARTED.
 */
t_start_verdict	room_can_start(const t_room *r, t_player_id requester)
{
	t_membership	*m;

	if (!r)
		return (START_NOT_OWNER);
	if (r->status == ROOM_IN_GAME || r->status == ROOM_FINISHED)
		return (START_ALREADY_STARTED);
	m = room_find_member((t_room *)r, requester);
	if (!m || !membership_is_owner(m))
		return (START_NOT_OWNER);
	if (r->number_of_players < r->min_to_start)
		return (START_TOO_FEW_PLAYERS);
	return (START_ACCEPTED);
}

/**
 * @brief Starts the game when room_can_start accepts.
 *
 * @param r The room to start.
 * @param requester The player asking to start.
 * @return The room_can_start verdict; the room is IN_GAME only on
 *         START_ACCEPTED, unchanged on every rejection.
 */
t_start_verdict	room_start(t_room *r, t_player_id requester)
{
	t_start_verdict	verdict;

	verdict = room_can_start(r, requester);
	if (verdict == START_ACCEPTED)
		r->status = ROOM_IN_GAME;
	return (verdict);
}

/**
 * @brief Undoes a start the caller could not carry out.
 *
 * A start the caller cannot honour is not a start: the players keep their
 * slots and the room goes back to being startable, rather than sitting
 * IN_GAME with no game behind it. This is not room_finish - nobody played,
 * so nobody is cleared out and no result exists to record.
 *
 * Only IN_GAME is undone. A room that already finished stays finished, so a
 * late abort can never resurrect a room that has moved on.
 *
 * @param r The room whose start is being taken back.
 */
void	room_abort_start(t_room *r)
{
	if (!r || r->status != ROOM_IN_GAME)
		return ;
	r->status = ROOM_WAITING;
	room_recompute_status(r);
}

/**
 * @brief Ends the game: FINISHED status, every slot cleared (UC-11/12).
 *
 * @param r The room to finish.
 */
void	room_finish(t_room *r)
{
	int	i;

	if (!r)
		return ;
	r->status = ROOM_FINISHED;
	i = 0;
	while (i < r->slot_count)
	{
		slot_clear(&r->slots[i]);
		i++;
	}
	r->number_of_players = 0;
}

/**
 * @brief Opens the character-select window on a room everybody has readied.
 *
 * Readiness used to start the match on the spot, which left nowhere to choose
 * a fighter: the last player to press R decided, for both of them, that the
 * match began now. SELECTING is that missing moment - the room is committed,
 * nobody is playing, and both players are looking at the same roster.
 *
 * Only a READY room enters it, so the window cannot be opened on a room that
 * could not have started anyway.
 *
 * @param r The room to hold open.
 * @return 0 when the window opened, -1 when the room was not ready for one.
 */
int	room_begin_selection(t_room *r)
{
	if (!r || r->status != ROOM_READY)
		return (-1);
	r->status = ROOM_SELECTING;
	return (0);
}

/**
 * @brief Closes a select window the room can no longer see through.
 *
 * A player leaving during the window can take the room below its minimum, and
 * a room that stayed SELECTING would hold the remaining player in a roster
 * screen waiting for a match that can never be dealt. Recomputing is what
 * decides whether it lands on READY or WAITING.
 *
 * @param r The room whose window is closing.
 */
void	room_abort_selection(t_room *r)
{
	if (!r || r->status != ROOM_SELECTING)
		return ;
	r->status = ROOM_WAITING;
	room_recompute_status(r);
}

/**
 * @brief Ends the game and keeps everyone in their seats for another one.
 *
 * The other way a match can end. room_finish empties the room, which is right
 * when the room is being handed back - but it made a rematch impossible: the
 * seats went, the room went with them, and the two players who had just
 * finished a game together came back to a room that no longer existed and were
 * put in the lobby to find each other again.
 *
 * So the game ends and the room does not. Memberships and the owner are
 * untouched; only readiness is withdrawn, because a room that came back with
 * everybody still ready would deal the next match before anyone had looked at
 * the result of the last one.
 *
 * Never FINISHED in between - the room goes straight back to WAITING or READY,
 * so nothing that reads the status can catch it in a state it will never leave.
 *
 * @param r The room whose match has ended.
 */
void	room_rematch(t_room *r)
{
	int	i;

	if (!r || (r->status != ROOM_IN_GAME && r->status != ROOM_FINISHED))
		return ;
	r->status = ROOM_WAITING;
	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied)
			r->slots[i].status = SLOT_WAITING;
		i++;
	}
	room_recompute_status(r);
}

/**
 * @brief Maps the room status to its fixed UI STATE string.
 *
 * @param r The room to describe.
 * @return One of the four fixed strings from use_cases §Room STATE message.
 */
const char	*room_state_message(const t_room *r)
{
	if (!r)
		return ("");
	if (r->status == ROOM_READY)
		return ("READY TO START, OWNER CAN START ANYTIME");
	if (r->status == ROOM_SELECTING)
		return ("CHOOSING FIGHTERS");
	if (r->status == ROOM_IN_GAME)
		return ("GAME IN PROGRESS");
	if (r->status == ROOM_FINISHED)
		return ("GAME OVER, RECORDING THE RESULTS");
	return ("WAITING FOR OPPONENT");
}
