#include "tetrisu.h"

static void	test_initial_focus_and_navigation(void);
static void	test_shortcuts_and_confirm(void);
static void	test_pointer_focus_validation(void);
static void	test_input_batch_boundaries(void);

int	main(void)
{
	test_initial_focus_and_navigation();
	test_shortcuts_and_confirm();
	test_pointer_focus_validation();
	test_input_batch_boundaries();
	return (0);
}

/**
 * @brief Only identical movement keys may be folded into one repaint.
 *
 * Coalescing a command key would act on it before its target state had been
 * drawn; coalescing opposite arrows would swallow a real focus change.
 */
static void	test_input_batch_boundaries(void)
{
	assert(leaderboard_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_LEFT));
	assert(leaderboard_navigation_keys_coalesce(NCKEY_RIGHT, NCKEY_RIGHT));
	assert(leaderboard_navigation_keys_coalesce(NCKEY_TAB, NCKEY_TAB));
	assert(leaderboard_navigation_keys_coalesce('\t', '\t'));
	assert(!leaderboard_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_RIGHT));
	assert(!leaderboard_navigation_keys_coalesce(NCKEY_RIGHT, NCKEY_LEFT));
	assert(!leaderboard_navigation_keys_coalesce(NCKEY_ENTER, NCKEY_ENTER));
	assert(!leaderboard_navigation_keys_coalesce(NCKEY_ESC, NCKEY_ESC));
	assert(!leaderboard_navigation_keys_coalesce('r', 'r'));
	assert(!leaderboard_navigation_keys_coalesce('q', 'q'));
	assert(!leaderboard_navigation_keys_coalesce(NCKEY_UP, NCKEY_UP));
	printf("PASS test_input_batch_boundaries\n");
}

static void	test_initial_focus_and_navigation(void)
{
	leaderboard_state_t	state;

	leaderboard_state_init(&state);
	assert(state.focus == LEADERBOARD_FOCUS_BACK);
	assert(leaderboard_handle_key(&state, NCKEY_RIGHT)
		== LEADERBOARD_ACTION_NONE);
	assert(state.focus == LEADERBOARD_FOCUS_REFRESH);
	assert(leaderboard_handle_key(&state, NCKEY_LEFT)
		== LEADERBOARD_ACTION_NONE);
	assert(state.focus == LEADERBOARD_FOCUS_BACK);
	assert(leaderboard_handle_key(&state, '\t')
		== LEADERBOARD_ACTION_NONE);
	assert(state.focus == LEADERBOARD_FOCUS_REFRESH);
	printf("PASS test_initial_focus_and_navigation\n");
}

static void	test_shortcuts_and_confirm(void)
{
	leaderboard_state_t	state;

	leaderboard_state_init(&state);
	assert(leaderboard_handle_key(&state, NCKEY_ENTER)
		== LEADERBOARD_ACTION_BACK);
	leaderboard_set_focus(&state, LEADERBOARD_FOCUS_REFRESH);
	assert(leaderboard_handle_key(&state, '\n')
		== LEADERBOARD_ACTION_REFRESH);
	assert(leaderboard_handle_key(&state, 'r')
		== LEADERBOARD_ACTION_REFRESH);
	assert(leaderboard_handle_key(&state, NCKEY_ESC)
		== LEADERBOARD_ACTION_BACK);
	assert(leaderboard_handle_key(&state, 'Q')
		== LEADERBOARD_ACTION_QUIT);
	assert(leaderboard_handle_key(&state, 'x')
		== LEADERBOARD_ACTION_NONE);
	assert(leaderboard_action_leaves_screen(LEADERBOARD_ACTION_BACK));
	assert(leaderboard_action_leaves_screen(LEADERBOARD_ACTION_QUIT));
	assert(!leaderboard_action_leaves_screen(LEADERBOARD_ACTION_REFRESH));
	assert(!leaderboard_action_leaves_screen(LEADERBOARD_ACTION_NONE));
	printf("PASS test_shortcuts_and_confirm\n");
}

static void	test_pointer_focus_validation(void)
{
	leaderboard_state_t	state;

	leaderboard_state_init(&state);
	leaderboard_set_focus(&state, LEADERBOARD_FOCUS_REFRESH);
	assert(state.focus == LEADERBOARD_FOCUS_REFRESH);
	leaderboard_set_focus(&state, (leaderboard_focus_t)42);
	assert(state.focus == LEADERBOARD_FOCUS_REFRESH);
	leaderboard_set_focus(NULL, LEADERBOARD_FOCUS_BACK);
	printf("PASS test_pointer_focus_validation\n");
}
