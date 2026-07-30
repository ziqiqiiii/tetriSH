#include "tetrisu.h"

static void	test_initial_focus_and_navigation(void);
static void	test_shortcuts_and_confirm(void);
static void	test_pointer_focus_validation(void);

int	main(void)
{
	test_initial_focus_and_navigation();
	test_shortcuts_and_confirm();
	test_pointer_focus_validation();
	return (0);
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
