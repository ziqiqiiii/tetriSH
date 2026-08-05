#include "tetrisu.h"

/**
 * @brief Starts Settings on the safe Back action.
 */
void	settings_state_init(settings_state_t *state, bool signed_in)
{
	if (state == NULL)
		return ;
	state->focus = SETTINGS_FOCUS_BACK;
	state->signed_in = signed_in;
	state->ability_info_visible = false;
}

/**
 * @brief Advances through only the actions available in this session.
 */
void	settings_state_focus_next(settings_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->focus == SETTINGS_FOCUS_BACK)
		state->focus = state->signed_in ? SETTINGS_FOCUS_MARKETPLACE
			: SETTINGS_FOCUS_VOLUME_DOWN;
	else if (state->focus == SETTINGS_FOCUS_MARKETPLACE)
		state->focus = SETTINGS_FOCUS_VOLUME_DOWN;
	else if (state->focus == SETTINGS_FOCUS_VOLUME_DOWN)
		state->focus = SETTINGS_FOCUS_VOLUME_UP;
	else if (state->focus == SETTINGS_FOCUS_VOLUME_UP && state->signed_in)
		state->focus = SETTINGS_FOCUS_CHARACTER_PREVIOUS;
	else if (state->focus == SETTINGS_FOCUS_CHARACTER_PREVIOUS)
		state->focus = SETTINGS_FOCUS_CHARACTER_NEXT;
	else
		state->focus = SETTINGS_FOCUS_BACK;
}

/**
 * @brief Moves backwards through the same accessible action order.
 */
void	settings_state_focus_previous(settings_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->focus == SETTINGS_FOCUS_BACK)
		state->focus = state->signed_in ? SETTINGS_FOCUS_CHARACTER_NEXT
			: SETTINGS_FOCUS_VOLUME_UP;
	else if (state->focus == SETTINGS_FOCUS_MARKETPLACE)
		state->focus = SETTINGS_FOCUS_BACK;
	else if (state->focus == SETTINGS_FOCUS_VOLUME_DOWN)
		state->focus = state->signed_in ? SETTINGS_FOCUS_MARKETPLACE
			: SETTINGS_FOCUS_BACK;
	else if (state->focus == SETTINGS_FOCUS_VOLUME_UP)
		state->focus = SETTINGS_FOCUS_VOLUME_DOWN;
	else if (state->focus == SETTINGS_FOCUS_CHARACTER_PREVIOUS)
		state->focus = SETTINGS_FOCUS_VOLUME_UP;
	else
		state->focus = SETTINGS_FOCUS_CHARACTER_PREVIOUS;
}

/**
 * @brief Converts Settings keyboard input into semantic actions.
 */
settings_action_t	settings_handle_key(settings_state_t *state, uint32_t key)
{
	if (state == NULL)
		return (SETTINGS_ACTION_NONE);
	if (key == 'q' || key == 'Q')
		return (SETTINGS_ACTION_QUIT);
	if (key == NCKEY_ESC)
		return (SETTINGS_ACTION_BACK);
	if (key == '+' || key == '=')
		return (SETTINGS_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (SETTINGS_ACTION_VOLUME_DOWN);
	if (state->signed_in && (key == '[' || key == ','))
		return (SETTINGS_ACTION_CHARACTER_PREVIOUS);
	if (state->signed_in && (key == ']' || key == '.'))
		return (SETTINGS_ACTION_CHARACTER_NEXT);
	if (state->signed_in && (key == 'i' || key == 'I'))
	{
		state->ability_info_visible = !state->ability_info_visible;
		return (SETTINGS_ACTION_NONE);
	}
	if (key == NCKEY_UP || key == NCKEY_LEFT)
	{
		settings_state_focus_previous(state);
		return (SETTINGS_ACTION_NONE);
	}
	/* Tab advances, matching auth_form_handle_key() and the on-screen hint. */
	if (key == NCKEY_DOWN || key == NCKEY_RIGHT || key == NCKEY_TAB)
	{
		settings_state_focus_next(state);
		return (SETTINGS_ACTION_NONE);
	}
	if (key == 'm' || key == 'M')
	{
		if (state->signed_in)
			return (SETTINGS_ACTION_MARKETPLACE);
		return (SETTINGS_ACTION_NONE);
	}
	if (key != NCKEY_ENTER && key != '\n' && key != '\r')
		return (SETTINGS_ACTION_NONE);
	if (state->focus == SETTINGS_FOCUS_BACK)
		return (SETTINGS_ACTION_BACK);
	if (state->focus == SETTINGS_FOCUS_MARKETPLACE && state->signed_in)
		return (SETTINGS_ACTION_MARKETPLACE);
	if (state->focus == SETTINGS_FOCUS_VOLUME_DOWN)
		return (SETTINGS_ACTION_VOLUME_DOWN);
	if (state->focus == SETTINGS_FOCUS_VOLUME_UP)
		return (SETTINGS_ACTION_VOLUME_UP);
	if (state->focus == SETTINGS_FOCUS_CHARACTER_PREVIOUS)
		return (SETTINGS_ACTION_CHARACTER_PREVIOUS);
	if (state->focus == SETTINGS_FOCUS_CHARACTER_NEXT)
		return (SETTINGS_ACTION_CHARACTER_NEXT);
	return (SETTINGS_ACTION_NONE);
}

/**
 * @brief Cycles through owned character fixtures and equips the selection.
 */
bool	settings_select_character(app_settings_view_model_t *settings,
	int direction)
{
	app_catalogue_view_model_t	*catalogue;
	int					current;
	int					candidate;
	int					visited;

	if (settings == NULL || direction == 0 || !settings->signed_in)
		return (false);
	catalogue = &settings->characters;
	if (catalogue->count <= 0 || catalogue->count > APP_CATALOGUE_MAX_ITEMS)
		return (false);
	current = 0;
	while (current < catalogue->count
		&& !catalogue->items[current].equipped)
		current++;
	if (current == catalogue->count)
		current = 0;
	candidate = current;
	visited = 0;
	while (visited < catalogue->count)
	{
		candidate += direction > 0 ? 1 : -1;
		if (candidate >= catalogue->count)
			candidate = 0;
		if (candidate < 0)
			candidate = catalogue->count - 1;
		if (catalogue->items[candidate].owned)
			break ;
		visited++;
	}
	if (candidate == current || !catalogue->items[candidate].owned)
		return (false);
	catalogue->items[current].equipped = false;
	catalogue->items[candidate].equipped = true;
	snprintf(settings->profile.character,
		sizeof(settings->profile.character), "%s",
		catalogue->items[candidate].name);
	snprintf(settings->profile.portrait_asset,
		sizeof(settings->profile.portrait_asset), "%s",
		catalogue->items[candidate].portrait_asset);
	return (true);
}
