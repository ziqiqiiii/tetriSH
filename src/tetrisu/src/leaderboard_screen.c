#include "tetrisu.h"

/**
 * @brief Starts the leaderboard with the safe Back action focused.
 */
void	leaderboard_state_init(t_leaderboard_state *state)
{
	if (state != NULL)
		state->focus = LEADERBOARD_FOCUS_BACK;
}

/**
 * @brief Reports whether two queued keys are the same focus-only movement.
 *
 * A terminal repeat rate outruns a bitmap repaint, so held navigation keys
 * pile up. Only identical movement keys collapse: a different key may carry a
 * different target state and has to be painted before it is acted on.
 *
 * @param active_key Key currently being handled.
 * @param queued_key Key taken from the input queue.
 * @return true when @p queued_key may be folded into @p active_key.
 */
bool	leaderboard_navigation_keys_coalesce(uint32_t active_key,
	uint32_t queued_key)
{
	if (active_key != queued_key)
		return (false);
	return (active_key == NCKEY_LEFT || active_key == NCKEY_RIGHT
		|| active_key == NCKEY_TAB || active_key == '\t');
}

/**
 * @brief Reports whether an action ends the leaderboard input loop.
 *
 * @param action Action returned by leaderboard_handle_key().
 * @return true when queued input must be discarded before navigation.
 */
bool	leaderboard_action_leaves_screen(t_leaderboard_action action)
{
	return (action == LEADERBOARD_ACTION_BACK
		|| action == LEADERBOARD_ACTION_QUIT);
}

/**
 * @brief Maps keyboard input to semantic leaderboard actions.
 */
t_leaderboard_action	leaderboard_handle_key(t_leaderboard_state *state,
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
void	leaderboard_set_focus(t_leaderboard_state *state,
	t_leaderboard_focus focus)
{
	if (state != NULL && (focus == LEADERBOARD_FOCUS_BACK
			|| focus == LEADERBOARD_FOCUS_REFRESH))
		state->focus = focus;
}
