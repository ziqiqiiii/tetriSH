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
	/* TODO: IN_GAME/FINISHED -> ALREADY_STARTED; requester not the owner
	   -> NOT_OWNER; count < min_to_start -> TOO_FEW_PLAYERS. */
	(void)r;
	(void)requester;
	return (START_NOT_OWNER);
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
	/* TODO: verdict = room_can_start; on ACCEPTED status = ROOM_IN_GAME. */
	(void)r;
	(void)requester;
	return (START_NOT_OWNER);
}

/**
 * @brief Ends the game: FINISHED status, every slot cleared (UC-11/12).
 *
 * @param r The room to finish.
 */
void	room_finish(t_room *r)
{
	/* TODO: status = ROOM_FINISHED; slot_clear each slot; count = 0. */
	(void)r;
}

/**
 * @brief Maps the room status to its fixed UI STATE string.
 *
 * @param r The room to describe.
 * @return One of the four fixed strings from use_cases §Room STATE message.
 */
const char	*room_state_message(const t_room *r)
{
	/* TODO: WAITING/READY/IN_GAME/FINISHED -> the four fixed strings. */
	(void)r;
	return ("");
}
