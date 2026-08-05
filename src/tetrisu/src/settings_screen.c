#include "tetrisu.h"

static bool	has_inventory(const settings_state_t *state);
static int	section_slots(const settings_state_t *state,
				settings_section_t section);
static void	enter_inventory(settings_state_t *state,
				settings_section_t section, int row, bool rightmost);
static void	move_horizontal(settings_state_t *state, int step);
static void	move_vertical(settings_state_t *state, int step);
static void	move_control(settings_state_t *state, int step);
static settings_action_t	activate_focus(const settings_state_t *state);
static int	clamp_int(int value, int low, int high);
static int	min_int(int left, int right);

/**
 * @brief Starts Settings on the safe Back action.
 *
 * The owned slot counts are captured once here because every later movement is
 * clamped against them: the grids are navigated by drawn slot, not by
 * catalogue index, so focus can never land on an item the panels do not show.
 */
void	settings_state_init(settings_state_t *state, bool signed_in,
	int character_slots, int theme_slots)
{
	if (state == NULL)
		return ;
	state->section = SETTINGS_SECTION_CONTROLS;
	state->focus = SETTINGS_FOCUS_BACK;
	state->character_slot = 0;
	state->theme_slot = 0;
	state->character_slots = clamp_int(character_slots, 0,
			SETTINGS_CHARACTER_SLOTS);
	state->theme_slots = clamp_int(theme_slots, 0, SETTINGS_THEME_SLOTS);
	state->signed_in = signed_in;
	state->ability_info_visible = false;
	if (!signed_in)
	{
		state->character_slots = 0;
		state->theme_slots = 0;
	}
}

/**
 * @brief Reports whether two states would draw differently.
 *
 * The screen repaints on this rather than on "any key arrived", so a burst of
 * repeats that lands back on the same slot costs no bitmap work at all.
 */
bool	settings_state_view_changed(const settings_state_t *before,
	const settings_state_t *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->section != after->section
		|| before->focus != after->focus
		|| before->character_slot != after->character_slot
		|| before->theme_slot != after->theme_slot
		|| before->ability_info_visible != after->ability_info_visible);
}

/**
 * @brief Reports how many grid rows a slot count occupies.
 */
int	settings_slot_rows(int slots)
{
	if (slots <= 0)
		return (0);
	return ((slots + SETTINGS_INVENTORY_COLUMNS - 1)
		/ SETTINGS_INVENTORY_COLUMNS);
}

/**
 * @brief Counts the owned entries a panel will actually draw.
 */
int	settings_owned_count(const app_catalogue_view_model_t *catalogue, int limit)
{
	int	owned;
	int	index;

	if (catalogue == NULL || limit <= 0)
		return (0);
	owned = 0;
	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS
		&& owned < limit)
	{
		if (catalogue->items[index].owned)
			owned++;
		index++;
	}
	return (owned);
}

/**
 * @brief Advances one step through sections, then through the focused row.
 *
 * Tab remains a single linear escape hatch so the whole screen is reachable
 * without knowing the grid shape.
 */
void	settings_state_focus_next(settings_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->section == SETTINGS_SECTION_CONTROLS)
	{
		if (state->focus != SETTINGS_FOCUS_VOLUME_UP)
			move_control(state, 1);
		else if (has_inventory(state))
			enter_inventory(state, SETTINGS_SECTION_CHARACTERS, 0, false);
		else
			move_control(state, 1);
		return ;
	}
	if (state->character_slot + 1 < state->character_slots
		&& state->section == SETTINGS_SECTION_CHARACTERS)
		state->character_slot++;
	else if (state->section == SETTINGS_SECTION_CHARACTERS
		&& state->theme_slots > 0)
		enter_inventory(state, SETTINGS_SECTION_THEMES, 0, false);
	else if (state->section == SETTINGS_SECTION_THEMES
		&& state->theme_slot + 1 < state->theme_slots)
		state->theme_slot++;
	else
	{
		state->section = SETTINGS_SECTION_CONTROLS;
		state->focus = SETTINGS_FOCUS_BACK;
	}
}

/**
 * @brief Steps backwards through the same order Tab advances through.
 */
void	settings_state_focus_previous(settings_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->section == SETTINGS_SECTION_CONTROLS)
	{
		if (state->focus != SETTINGS_FOCUS_BACK)
			move_control(state, -1);
		else if (state->theme_slots > 0)
			enter_inventory(state, SETTINGS_SECTION_THEMES, 0, true);
		else if (state->character_slots > 0)
			enter_inventory(state, SETTINGS_SECTION_CHARACTERS, 0, true);
		else
			move_control(state, -1);
		return ;
	}
	if (state->section == SETTINGS_SECTION_THEMES && state->theme_slot > 0)
		state->theme_slot--;
	else if (state->section == SETTINGS_SECTION_THEMES
		&& state->character_slots > 0)
		enter_inventory(state, SETTINGS_SECTION_CHARACTERS, 0, true);
	else if (state->section == SETTINGS_SECTION_CHARACTERS
		&& state->character_slot > 0)
		state->character_slot--;
	else
	{
		state->section = SETTINGS_SECTION_CONTROLS;
		state->focus = SETTINGS_FOCUS_VOLUME_UP;
	}
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
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT)
	{
		move_horizontal(state, key == NCKEY_RIGHT ? 1 : -1);
		return (SETTINGS_ACTION_NONE);
	}
	if (key == NCKEY_UP || key == NCKEY_DOWN)
	{
		move_vertical(state, key == NCKEY_DOWN ? 1 : -1);
		return (SETTINGS_ACTION_NONE);
	}
	/* Tab advances, matching auth_form_handle_key() and the on-screen hint. */
	if (key == NCKEY_TAB)
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
	return (activate_focus(state));
}

static settings_action_t	activate_focus(const settings_state_t *state)
{
	if (state->section == SETTINGS_SECTION_CHARACTERS)
		return (SETTINGS_ACTION_EQUIP_CHARACTER);
	if (state->section == SETTINGS_SECTION_THEMES)
		return (SETTINGS_ACTION_EQUIP_THEME);
	if (state->focus == SETTINGS_FOCUS_BACK)
		return (SETTINGS_ACTION_BACK);
	if (state->focus == SETTINGS_FOCUS_MARKETPLACE && state->signed_in)
		return (SETTINGS_ACTION_MARKETPLACE);
	if (state->focus == SETTINGS_FOCUS_VOLUME_DOWN)
		return (SETTINGS_ACTION_VOLUME_DOWN);
	if (state->focus == SETTINGS_FOCUS_VOLUME_UP)
		return (SETTINGS_ACTION_VOLUME_UP);
	return (SETTINGS_ACTION_NONE);
}

/**
 * @brief Moves within a row, crossing between the two panels at their edges.
 *
 * The control row wraps because it is a closed strip of four buttons; the
 * inventory grids do not, because their outer edges are where the crossing to
 * the neighbouring panel has to happen.
 */
static void	move_horizontal(settings_state_t *state, int step)
{
	int	slots;
	int	*slot;
	int	column;
	int	row;

	if (state->section == SETTINGS_SECTION_CONTROLS)
	{
		move_control(state, step);
		return ;
	}
	slot = state->section == SETTINGS_SECTION_CHARACTERS
		? &state->character_slot : &state->theme_slot;
	slots = section_slots(state, state->section);
	column = *slot % SETTINGS_INVENTORY_COLUMNS;
	row = *slot / SETTINGS_INVENTORY_COLUMNS;
	if (step > 0 && column + 1 < SETTINGS_INVENTORY_COLUMNS
		&& *slot + 1 < slots)
		*slot += 1;
	else if (step > 0 && state->section == SETTINGS_SECTION_CHARACTERS
		&& state->theme_slots > 0)
		enter_inventory(state, SETTINGS_SECTION_THEMES, row, false);
	else if (step < 0 && column > 0)
		*slot -= 1;
	else if (step < 0 && state->section == SETTINGS_SECTION_THEMES
		&& state->character_slots > 0)
		enter_inventory(state, SETTINGS_SECTION_CHARACTERS, row, true);
}

/**
 * @brief Steps between grid rows, falling through to the control row.
 */
static void	move_vertical(settings_state_t *state, int step)
{
	int	slots;
	int	*slot;
	int	row;
	int	column;
	int	next_row;
	int	next_first;
	int	next_last;

	if (state->section == SETTINGS_SECTION_CONTROLS)
	{
		if (step < 0 && has_inventory(state))
			enter_inventory(state, state->character_slots > 0
				? SETTINGS_SECTION_CHARACTERS : SETTINGS_SECTION_THEMES,
				INT_MAX, false);
		return ;
	}
	slot = state->section == SETTINGS_SECTION_CHARACTERS
		? &state->character_slot : &state->theme_slot;
	slots = section_slots(state, state->section);
	row = *slot / SETTINGS_INVENTORY_COLUMNS;
	column = *slot % SETTINGS_INVENTORY_COLUMNS;
	if (step > 0)
	{
		next_row = row + 1;
		next_first = next_row * SETTINGS_INVENTORY_COLUMNS;
		if (next_first >= slots)
		{
			state->section = SETTINGS_SECTION_CONTROLS;
			return ;
		}
		next_last = min_int(slots - 1, next_first
			+ SETTINGS_INVENTORY_COLUMNS - 1);
		*slot = min_int(next_first + column, next_last);
	}
	else if (row > 0)
	{
		*slot -= SETTINGS_INVENTORY_COLUMNS;
	}
}

/**
 * @brief Cycles the four control buttons, skipping Marketplace when offline.
 */
static void	move_control(settings_state_t *state, int step)
{
	int	focus;

	focus = (int)state->focus;
	focus = (focus + (step > 0 ? 1 : SETTINGS_BUTTON_COUNT - 1))
		% SETTINGS_BUTTON_COUNT;
	if (focus == (int)SETTINGS_FOCUS_MARKETPLACE && !state->signed_in)
		focus = (focus + (step > 0 ? 1 : SETTINGS_BUTTON_COUNT - 1))
			% SETTINGS_BUTTON_COUNT;
	state->focus = (settings_focus_t)focus;
}

/**
 * @brief Focuses a panel at the requested row, clamped to what it draws.
 */
static void	enter_inventory(settings_state_t *state,
	settings_section_t section, int row, bool rightmost)
{
	int	slots;
	int	rows;
	int	slot;

	slots = section_slots(state, section);
	if (slots <= 0)
		return ;
	rows = settings_slot_rows(slots);
	row = clamp_int(row, 0, rows - 1);
	slot = row * SETTINGS_INVENTORY_COLUMNS;
	if (rightmost)
		slot += SETTINGS_INVENTORY_COLUMNS - 1;
	slot = clamp_int(slot, 0, slots - 1);
	state->section = section;
	if (section == SETTINGS_SECTION_CHARACTERS)
		state->character_slot = slot;
	else
		state->theme_slot = slot;
}

static int	section_slots(const settings_state_t *state,
	settings_section_t section)
{
	if (section == SETTINGS_SECTION_CHARACTERS)
		return (state->character_slots);
	if (section == SETTINGS_SECTION_THEMES)
		return (state->theme_slots);
	return (SETTINGS_BUTTON_COUNT);
}

static bool	has_inventory(const settings_state_t *state)
{
	return (state->character_slots > 0 || state->theme_slots > 0);
}

static int	clamp_int(int value, int low, int high)
{
	if (high < low)
		return (low);
	if (value < low)
		return (low);
	if (value > high)
		return (high);
	return (value);
}

static int	min_int(int left, int right)
{
	return (left < right ? left : right);
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

/**
 * @brief Resolves a drawn panel slot back to its catalogue entry.
 *
 * The panels only draw owned items, so the nth focused slot is the nth owned
 * entry rather than catalogue index n.
 */
static int	slot_to_index(const app_catalogue_view_model_t *catalogue, int slot)
{
	int	index;
	int	owned;

	if (catalogue == NULL || slot < 0)
		return (-1);
	index = 0;
	owned = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (catalogue->items[index].owned)
		{
			if (owned == slot)
				return (index);
			owned++;
		}
		index++;
	}
	return (-1);
}

/**
 * @brief Resolves which character the powers card should describe.
 *
 * While the cursor is inside the characters panel the card follows it, so
 * moving across the grid reads as inspecting each character in turn. Anywhere
 * else the card falls back to whatever is equipped, which is what the I key
 * has always shown.
 */
const app_catalogue_item_view_model_t	*settings_card_character(
	const app_settings_view_model_t *settings, const settings_state_t *state)
{
	int	index;

	if (settings == NULL || state == NULL)
		return (NULL);
	if (state->section == SETTINGS_SECTION_CHARACTERS)
	{
		index = slot_to_index(&settings->characters, state->character_slot);
		if (index >= 0)
			return (&settings->characters.items[index]);
	}
	index = 0;
	while (index < settings->characters.count
		&& index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (settings->characters.items[index].equipped)
			return (&settings->characters.items[index]);
		index++;
	}
	return (NULL);
}

/**
 * @brief Reports whether the powers card should be on screen at all.
 */
bool	settings_card_visible(const settings_state_t *state)
{
	if (state == NULL)
		return (false);
	return (state->ability_info_visible
		|| state->section == SETTINGS_SECTION_CHARACTERS);
}

/**
 * @brief Equips the character occupying the focused panel slot.
 *
 * @return true when the equipped entry changed and the screen must repaint.
 */
bool	settings_equip_character_slot(app_settings_view_model_t *settings,
	int slot)
{
	app_catalogue_view_model_t	*catalogue;
	int					target;
	int					index;

	if (settings == NULL || !settings->signed_in)
		return (false);
	catalogue = &settings->characters;
	target = slot_to_index(catalogue, slot);
	if (target < 0 || catalogue->items[target].equipped)
		return (false);
	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		catalogue->items[index].equipped = false;
		index++;
	}
	catalogue->items[target].equipped = true;
	snprintf(settings->profile.character, sizeof(settings->profile.character),
		"%s", catalogue->items[target].name);
	snprintf(settings->profile.portrait_asset,
		sizeof(settings->profile.portrait_asset), "%s",
		catalogue->items[target].portrait_asset);
	return (true);
}

/**
 * @brief Equips the theme occupying the focused panel slot.
 *
 * @return true when the equipped entry changed and the screen must repaint.
 */
bool	settings_equip_theme_slot(app_settings_view_model_t *settings, int slot)
{
	app_catalogue_view_model_t	*catalogue;
	int					target;
	int					index;

	if (settings == NULL || !settings->signed_in)
		return (false);
	catalogue = &settings->themes;
	target = slot_to_index(catalogue, slot);
	if (target < 0 || catalogue->items[target].equipped)
		return (false);
	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		catalogue->items[index].equipped = false;
		index++;
	}
	catalogue->items[target].equipped = true;
	snprintf(settings->profile.theme, sizeof(settings->profile.theme), "%s",
		catalogue->items[target].name);
	return (true);
}
