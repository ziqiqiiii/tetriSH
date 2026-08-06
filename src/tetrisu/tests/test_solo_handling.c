#include "tetrisu.h"

// Static Functions
static void	test_default_config(void);
static void	test_legacy_events_are_safe_taps(void);
static void	test_das_and_arr_timing(void);
static void	test_release_stops_repeat(void);
static void	test_last_pressed_direction_wins(void);
static void	test_releasing_inactive_direction_keeps_timer(void);
static void	test_terminal_repeat_is_ignored(void);
static void	test_soft_drop_uses_gravity_factor(void);

/**
 * @brief Runs terminal-aware Solo handling tests.
 *
 * @return 0 when every assertion passes.
 */
int	main(void)
{
	test_default_config();
	test_legacy_events_are_safe_taps();
	test_das_and_arr_timing();
	test_release_stops_repeat();
	test_last_pressed_direction_wins();
	test_releasing_inactive_direction_keeps_timer();
	test_terminal_repeat_is_ignored();
	test_soft_drop_uses_gravity_factor();
	return (0);
}

/**
 * @brief Checks releasing the older direction leaves latest timing untouched.
 */
static void	test_releasing_inactive_direction_keeps_timer(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[2];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_PRESS, &action));
	assert(solo_handling_event(&state, &config, NCKEY_RIGHT,
			NCTYPE_PRESS, &action));
	assert(solo_handling_update(&state, &config, 800, 100, actions, 2) == 0);
	assert(state.horizontal_wait_ms == 67);
	assert(!solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_RELEASE, &action));
	assert(state.horizontal_direction == 1);
	assert(state.horizontal_wait_ms == 67);
	assert(solo_handling_update(&state, &config, 800, 67, actions, 2) == 1);
	assert(actions[0] == SOLO_MOVE_RIGHT);
	printf("PASS test_releasing_inactive_direction_keeps_timer\n");
}

/**
 * @brief Checks roadmap handling defaults.
 */
static void	test_default_config(void)
{
	t_solo_handling_config	config;

	config = solo_handling_default_config();
	assert(config.das_ms == 167);
	assert(config.arr_ms == 33);
	assert(config.soft_drop_factor == 20);
	printf("PASS test_default_config\n");
}

/**
 * @brief Checks legacy events act once without inventing held state.
 */
static void	test_legacy_events_are_safe_taps(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[4];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_UNKNOWN, &action));
	assert(action == SOLO_MOVE_LEFT);
	assert(state.horizontal_direction == 0);
	assert(solo_handling_next_wake_ms(&state, &config, 800) == -1);
	assert(solo_handling_update(&state, &config, 800, 1000, actions, 4) == 0);
	printf("PASS test_legacy_events_are_safe_taps\n");
}

/**
 * @brief Checks immediate press, 167 ms DAS, and 33 ms ARR.
 */
static void	test_das_and_arr_timing(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[4];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_RIGHT,
			NCTYPE_PRESS, &action));
	assert(action == SOLO_MOVE_RIGHT);
	assert(solo_handling_next_wake_ms(&state, &config, 800) == 167);
	assert(solo_handling_update(&state, &config, 800, 166, actions, 4) == 0);
	assert(solo_handling_next_wake_ms(&state, &config, 800) == 1);
	assert(solo_handling_update(&state, &config, 800, 1, actions, 4) == 1);
	assert(actions[0] == SOLO_MOVE_RIGHT);
	assert(solo_handling_next_wake_ms(&state, &config, 800) == 33);
	assert(solo_handling_update(&state, &config, 800, 32, actions, 4) == 0);
	assert(solo_handling_update(&state, &config, 800, 1, actions, 4) == 1);
	assert(actions[0] == SOLO_MOVE_RIGHT);
	printf("PASS test_das_and_arr_timing\n");
}

/**
 * @brief Checks release clears repeat state.
 */
static void	test_release_stops_repeat(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[4];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_PRESS, &action));
	assert(!solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_RELEASE, &action));
	assert(state.horizontal_direction == 0);
	assert(solo_handling_update(&state, &config, 800, 1000, actions, 4) == 0);
	printf("PASS test_release_stops_repeat\n");
}

/**
 * @brief Checks opposing directions use latest press then restore prior hold.
 */
static void	test_last_pressed_direction_wins(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[4];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_PRESS, &action));
	assert(action == SOLO_MOVE_LEFT);
	assert(solo_handling_event(&state, &config, NCKEY_RIGHT,
			NCTYPE_PRESS, &action));
	assert(action == SOLO_MOVE_RIGHT);
	assert(state.horizontal_direction == 1);
	assert(solo_handling_update(&state, &config, 800, 167, actions, 4) == 1);
	assert(actions[0] == SOLO_MOVE_RIGHT);
	assert(solo_handling_event(&state, &config, NCKEY_RIGHT,
			NCTYPE_RELEASE, &action));
	assert(action == SOLO_MOVE_LEFT);
	assert(state.horizontal_direction == -1);
	assert(solo_handling_update(&state, &config, 800, 167, actions, 4) == 1);
	assert(actions[0] == SOLO_MOVE_LEFT);
	printf("PASS test_last_pressed_direction_wins\n");
}

/**
 * @brief Checks host repeat does not bypass application timing.
 */
static void	test_terminal_repeat_is_ignored(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_PRESS, &action));
	assert(!solo_handling_event(&state, &config, NCKEY_LEFT,
			NCTYPE_REPEAT, &action));
	assert(state.horizontal_wait_ms == 167);
	printf("PASS test_terminal_repeat_is_ignored\n");
}

/**
 * @brief Checks soft drop repeats at gravity divided by twenty.
 */
static void	test_soft_drop_uses_gravity_factor(void)
{
	t_solo_handling_config	config;
	t_solo_handling_state	state;
	t_solo_action			action;
	t_solo_action			actions[4];

	config = solo_handling_default_config();
	solo_handling_reset(&state);
	assert(solo_handling_event(&state, &config, NCKEY_DOWN,
			NCTYPE_PRESS, &action));
	assert(action == SOLO_SOFT_DROP);
	assert(solo_handling_next_wake_ms(&state, &config, 800) == 40);
	assert(solo_handling_update(&state, &config, 800, 39, actions, 4) == 0);
	assert(solo_handling_update(&state, &config, 800, 1, actions, 4) == 1);
	assert(actions[0] == SOLO_SOFT_DROP);
	assert(!solo_handling_event(&state, &config, NCKEY_DOWN,
			NCTYPE_RELEASE, &action));
	assert(solo_handling_next_wake_ms(&state, &config, 800) == -1);
	printf("PASS test_soft_drop_uses_gravity_factor\n");
}
