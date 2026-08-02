#include "tetrisroom.h"

/**
 * @brief Decides whether a start request may proceed (UC-08, UC-08a).
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
	if (r->status == ROOM_IN_GAME)
		return ("GAME IN PROGRESS");
	if (r->status == ROOM_FINISHED)
		return ("GAME OVER, RECORDING THE RESULTS");
	return ("WAITING FOR OPPONENT");
}
