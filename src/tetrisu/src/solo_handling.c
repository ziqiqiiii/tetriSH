#include "tetrisu.h"

// Static Functions
static bool	key_action(uint32_t key, t_solo_action *action);
static void	press_horizontal(t_solo_handling_state *state,
				const t_solo_handling_config *config, int direction);
static bool	release_horizontal(t_solo_handling_state *state,
				const t_solo_handling_config *config, int direction,
				t_solo_action *action);
static int	soft_drop_interval(const t_solo_handling_config *config,
				int gravity_ms);
static void	emit_repeats(int *wait_ms, int interval_ms, int elapsed_ms,
				t_solo_action action, t_solo_action *actions, int capacity,
				int *count);

/**
 * @brief Returns roadmap defaults for responsive Solo handling.
 *
 * @return Default DAS, ARR, and soft-drop factor.
 */
t_solo_handling_config	solo_handling_default_config(void)
{
	t_solo_handling_config	config;

	config.das_ms = SOLO_DEFAULT_DAS_MS;
	config.arr_ms = SOLO_DEFAULT_ARR_MS;
	config.soft_drop_factor = SOLO_DEFAULT_SOFT_DROP_FACTOR;
	return (config);
}

/**
 * @brief Clears every held-key and repeat timer.
 *
 * @param state Handling state to reset.
 */
void	solo_handling_reset(t_solo_handling_state *state)
{
	memset(state, 0, sizeof(*state));
}

/**
 * @brief Converts one terminal event into held state and an immediate action.
 *
 * Unknown event types are legacy terminal taps: they act once and never become
 * held. Press/repeat/release events use application-owned timing, avoiding
 * dependence on host keyboard-repeat settings.
 *
 * @param state Mutable handling state.
 * @param config Timing configuration.
 * @param key Notcurses key identifier.
 * @param event_type Notcurses press, repeat, release, or unknown event type.
 * @param action Output immediate action when the return value is true.
 * @return true when an immediate gameplay action should be applied.
 */
bool	solo_handling_event(t_solo_handling_state *state,
	const t_solo_handling_config *config, uint32_t key,
	ncintype_e event_type, t_solo_action *action)
{
	int	direction;

	if (!key_action(key, action))
		return (false);
	if (event_type == NCTYPE_UNKNOWN)
		return (true);
	direction = 0;
	if (key == NCKEY_LEFT)
		direction = -1;
	else if (key == NCKEY_RIGHT)
		direction = 1;
	if (event_type == NCTYPE_RELEASE)
	{
		if (direction != 0)
			return (release_horizontal(state, config, direction, action));
		state->down_held = false;
		state->soft_drop_wait_ms = 0;
		return (false);
	}
	if (event_type == NCTYPE_REPEAT)
	{
		if ((direction < 0 && state->left_held)
			|| (direction > 0 && state->right_held)
			|| (direction == 0 && state->down_held))
			return (false);
	}
	if (direction != 0)
	{
		press_horizontal(state, config, direction);
		return (true);
	}
	if (state->down_held)
		return (false);
	state->down_held = true;
	state->soft_drop_wait_ms = 0;
	return (true);
}

/**
 * @brief Emits application-timed horizontal and soft-drop repeats.
 *
 * @param state Mutable handling state.
 * @param config Timing configuration.
 * @param gravity_ms Current level gravity interval.
 * @param elapsed_ms Monotonic elapsed time since the previous update.
 * @param actions Output action buffer.
 * @param capacity Number of output entries available.
 * @return Number of actions written.
 */
int	solo_handling_update(t_solo_handling_state *state,
	const t_solo_handling_config *config, int gravity_ms, int elapsed_ms,
	t_solo_action *actions, int capacity)
{
	t_solo_action	action;
	int				count;
	int				interval_ms;

	if (elapsed_ms <= 0 || capacity <= 0)
		return (0);
	count = 0;
	if (state->horizontal_direction != 0)
	{
		action = SOLO_MOVE_LEFT;
		if (state->horizontal_direction > 0)
			action = SOLO_MOVE_RIGHT;
		emit_repeats(&state->horizontal_wait_ms, config->arr_ms,
			elapsed_ms, action, actions, capacity, &count);
	}
	if (state->down_held && count < capacity)
	{
		interval_ms = soft_drop_interval(config, gravity_ms);
		if (state->soft_drop_wait_ms <= 0)
			state->soft_drop_wait_ms = interval_ms;
		emit_repeats(&state->soft_drop_wait_ms, interval_ms, elapsed_ms,
			SOLO_SOFT_DROP, actions, capacity, &count);
	}
	return (count);
}

/**
 * @brief Returns time until next application-owned repeat.
 *
 * @param state Current handling state.
 * @param config Timing configuration.
 * @param gravity_ms Current level gravity interval.
 * @return Milliseconds until next repeat, or -1 when no key is held.
 */
int	solo_handling_next_wake_ms(const t_solo_handling_state *state,
	const t_solo_handling_config *config, int gravity_ms)
{
	int	wake_ms;
	int	soft_ms;

	wake_ms = -1;
	if (state->horizontal_direction != 0)
		wake_ms = state->horizontal_wait_ms;
	if (state->down_held)
	{
		soft_ms = state->soft_drop_wait_ms;
		if (soft_ms <= 0)
			soft_ms = soft_drop_interval(config, gravity_ms);
		if (wake_ms < 0 || soft_ms < wake_ms)
			wake_ms = soft_ms;
	}
	if (wake_ms < 0)
		return (-1);
	return (wake_ms);
}

/**
 * @brief Maps handling-owned keys to Solo actions.
 */
static bool	key_action(uint32_t key, t_solo_action *action)
{
	if (key == NCKEY_LEFT)
		*action = SOLO_MOVE_LEFT;
	else if (key == NCKEY_RIGHT)
		*action = SOLO_MOVE_RIGHT;
	else if (key == NCKEY_DOWN)
		*action = SOLO_SOFT_DROP;
	else
		return (false);
	return (true);
}

/**
 * @brief Makes the latest horizontal press authoritative.
 */
static void	press_horizontal(t_solo_handling_state *state,
	const t_solo_handling_config *config, int direction)
{
	state->sequence++;
	if (direction < 0)
	{
		state->left_held = true;
		state->left_order = state->sequence;
	}
	else
	{
		state->right_held = true;
		state->right_order = state->sequence;
	}
	state->horizontal_direction = direction;
	state->horizontal_wait_ms = config->das_ms;
}

/**
 * @brief Releases one direction and restores the still-held opposing key.
 */
static bool	release_horizontal(t_solo_handling_state *state,
	const t_solo_handling_config *config, int direction,
	t_solo_action *action)
{
	bool	was_active;

	was_active = state->horizontal_direction == direction;
	if (direction < 0)
		state->left_held = false;
	else
		state->right_held = false;
	if (!state->left_held && !state->right_held)
	{
		state->horizontal_direction = 0;
		state->horizontal_wait_ms = 0;
		return (false);
	}
	if (!was_active)
		return (false);
	if (state->left_held && (!state->right_held
			|| state->left_order > state->right_order))
	{
		state->horizontal_direction = -1;
		*action = SOLO_MOVE_LEFT;
	}
	else
	{
		state->horizontal_direction = 1;
		*action = SOLO_MOVE_RIGHT;
	}
	state->horizontal_wait_ms = config->das_ms;
	return (true);
}

/**
 * @brief Converts current gravity into a 20x soft-drop repeat interval.
 */
static int	soft_drop_interval(const t_solo_handling_config *config,
	int gravity_ms)
{
	int	factor;
	int	interval_ms;

	if (gravity_ms <= 0)
		return (1);
	factor = config->soft_drop_factor;
	if (factor < 1)
		factor = 1;
	interval_ms = gravity_ms / factor;
	if (interval_ms < SOLO_MIN_REPEAT_MS)
		interval_ms = SOLO_MIN_REPEAT_MS;
	return (interval_ms);
}

/**
 * @brief Advances one repeat clock and appends every due action.
 */
static void	emit_repeats(int *wait_ms, int interval_ms, int elapsed_ms,
	t_solo_action action, t_solo_action *actions, int capacity, int *count)
{
	if (interval_ms < 1)
		interval_ms = 1;
	if (*wait_ms <= 0)
		*wait_ms = interval_ms;
	*wait_ms -= elapsed_ms;
	while (*wait_ms <= 0 && *count < capacity)
	{
		actions[*count] = action;
		(*count)++;
		*wait_ms += interval_ms;
	}
	if (*wait_ms <= 0)
		*wait_ms = interval_ms;
}
