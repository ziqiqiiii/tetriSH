#include "tetrisu.h"

/**
 * @brief Starts the leaderboard with the safe Back action focused.
 */
void	leaderboard_state_init(leaderboard_state_t *state)
{
	if (state != NULL)
		state->focus = LEADERBOARD_FOCUS_BACK;
}

/**
 * @brief Maps keyboard input to semantic leaderboard actions.
 */
leaderboard_action_t	leaderboard_handle_key(leaderboard_state_t *state,
	uint32_t key)
{
	if (state == NULL)
		return (LEADERBOARD_ACTION_NONE);
	if (key == NCKEY_ESC)
		return (LEADERBOARD_ACTION_BACK);
	if (key == 'q' || key == 'Q')
		return (LEADERBOARD_ACTION_QUIT);
	if (key == 'r' || key == 'R')
		return (LEADERBOARD_ACTION_REFRESH);
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT || key == '\t')
	{
		if (state->focus == LEADERBOARD_FOCUS_BACK)
			state->focus = LEADERBOARD_FOCUS_REFRESH;
		else
			state->focus = LEADERBOARD_FOCUS_BACK;
		return (LEADERBOARD_ACTION_NONE);
	}
	if (key == NCKEY_ENTER || key == '\n' || key == '\r')
	{
		if (state->focus == LEADERBOARD_FOCUS_REFRESH)
			return (LEADERBOARD_ACTION_REFRESH);
		return (LEADERBOARD_ACTION_BACK);
	}
	return (LEADERBOARD_ACTION_NONE);
}

/**
 * @brief Applies a validated pointer focus.
 */
void	leaderboard_set_focus(leaderboard_state_t *state,
	leaderboard_focus_t focus)
{
	if (state != NULL && (focus == LEADERBOARD_FOCUS_BACK
			|| focus == LEADERBOARD_FOCUS_REFRESH))
		state->focus = focus;
}
