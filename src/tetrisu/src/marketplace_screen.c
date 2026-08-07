#include "tetrisu.h"

static bool	has_inventory(const marketplace_state_t *state);
static int	section_slots(const marketplace_state_t *state,
				marketplace_section_t section);
static void	enter_inventory(marketplace_state_t *state,
				marketplace_section_t section, int row, bool rightmost);
static void	move_horizontal(marketplace_state_t *state, int step);
static void	move_vertical(marketplace_state_t *state, int step);
static void	move_control(marketplace_state_t *state, int step);
static void	focus_control_column(marketplace_state_t *state, int column);
static marketplace_action_t	activate_focus(const marketplace_state_t *state);
static int	slot_rows(int slots);
static int	clamp_int(int value, int low, int high);
static int	min_int(int left, int right);
static app_catalogue_view_model_t	*mutable_catalogue(
				app_marketplace_view_model_t *market,
				const marketplace_state_t *state);
static void	clear_equipped(app_catalogue_view_model_t *catalogue);

/**
 * @brief Starts the Marketplace on the safe Back action.
 *
 * The drawn slot counts are captured once because every later movement is
 * clamped against them: the grids are navigated by drawn slot rather than by
 * catalogue index, so focus can never land on a slot the panels do not show.
 * preview starts on the characters panel so the detail card describes
 * something from the first frame instead of opening blank.
 */
void	marketplace_state_init(marketplace_state_t *state, bool signed_in,
	int character_slots, int theme_slots)
{
	if (state == NULL)
		return ;
	state->section = MARKETPLACE_SECTION_CONTROLS;
	state->preview = MARKETPLACE_SECTION_CHARACTERS;
	state->focus = MARKETPLACE_FOCUS_BACK;
	state->feedback = MARKETPLACE_FEEDBACK_NONE;
	state->feedback_value = 0;
	state->character_slot = 0;
	state->theme_slot = 0;
	state->character_slots = clamp_int(character_slots, 0,
			MARKETPLACE_CHARACTER_SLOTS);
	state->theme_slots = clamp_int(theme_slots, 0, MARKETPLACE_THEME_SLOTS);
	state->signed_in = signed_in;
	if (!signed_in)
	{
		state->character_slots = 0;
		state->theme_slots = 0;
	}
	if (state->character_slots == 0 && state->theme_slots > 0)
		state->preview = MARKETPLACE_SECTION_THEMES;
}

/**
 * @brief Reports whether two states would draw differently.
 *
 * The screen repaints on this rather than on "any key arrived", so a burst of
 * repeats that lands back on the same slot costs no bitmap work at all.
 */
bool	marketplace_state_view_changed(const marketplace_state_t *before,
	const marketplace_state_t *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->section != after->section
		|| before->preview != after->preview
		|| before->focus != after->focus
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value
		|| before->character_slot != after->character_slot
		|| before->theme_slot != after->theme_slot);
}

/**
 * @brief Reports whether a queued key belongs to one navigation repeat batch.
 *
 * Only identical arrows or Tab repeats may be collapsed before repainting. A
 * different direction or any action key stays queued until the focus produced
 * by the current batch is on screen, so nothing is bought blind.
 */
bool	marketplace_navigation_keys_coalesce(uint32_t active_key,
	uint32_t queued_key)
{
	if (active_key != queued_key)
		return (false);
	return (active_key == NCKEY_LEFT || active_key == NCKEY_RIGHT
		|| active_key == NCKEY_UP || active_key == NCKEY_DOWN
		|| active_key == NCKEY_TAB);
}

/**
 * @brief Reports whether an action crosses the Marketplace screen boundary.
 *
 * Queued terminal repeats are discarded at these boundaries so input produced
 * for the Marketplace cannot leak into Home or shutdown handling.
 */
bool	marketplace_action_leaves_screen(marketplace_action_t action)
{
	return (action == MARKETPLACE_ACTION_BACK
		|| action == MARKETPLACE_ACTION_QUIT);
}

/**
 * @brief Returns the panel the detail card and Buy button act on.
 *
 * While the cursor is inside a grid that is the grid it is in; from the
 * control row it is the panel the cursor came from, so stepping down to Buy
 * keeps pointing at the item that was being inspected.
 */
marketplace_section_t	marketplace_focused_section(
	const marketplace_state_t *state)
{
	if (state == NULL)
		return (MARKETPLACE_SECTION_CHARACTERS);
	if (state->section == MARKETPLACE_SECTION_CONTROLS)
		return (state->preview);
	return (state->section);
}

/**
 * @brief Returns the drawn slot the detail card and Buy button act on.
 */
int	marketplace_focused_slot(const marketplace_state_t *state)
{
	if (state == NULL)
		return (-1);
	if (marketplace_focused_section(state) == MARKETPLACE_SECTION_CHARACTERS)
		return (state->character_slot);
	return (state->theme_slot);
}

/**
 * @brief Reports whether the acted-on panel is the characters grid.
 */
bool	marketplace_focused_is_character(const marketplace_state_t *state)
{
	return (marketplace_focused_section(state)
		== MARKETPLACE_SECTION_CHARACTERS);
}

/**
 * @brief Returns the catalogue the acted-on panel draws from.
 */
const app_catalogue_view_model_t	*marketplace_focused_catalogue(
	const app_marketplace_view_model_t *market,
	const marketplace_state_t *state)
{
	if (market == NULL || state == NULL)
		return (NULL);
	if (marketplace_focused_is_character(state))
		return (&market->characters);
	return (&market->themes);
}

/**
 * @brief Resolves the catalogue entry the detail card describes.
 *
 * Returns NULL rather than a clamped neighbour when the slot is past what the
 * panel draws, so callers cannot buy or equip something the user never saw.
 */
const app_catalogue_item_view_model_t	*marketplace_focused_item(
	const app_marketplace_view_model_t *market,
	const marketplace_state_t *state)
{
	const app_catalogue_view_model_t	*catalogue;
	int									slot;
	int									limit;

	catalogue = marketplace_focused_catalogue(market, state);
	if (catalogue == NULL)
		return (NULL);
	limit = marketplace_focused_is_character(state)
		? MARKETPLACE_CHARACTER_SLOTS : MARKETPLACE_THEME_SLOTS;
	slot = marketplace_focused_slot(state);
	if (slot < 0 || slot >= settings_catalogue_count(catalogue, limit))
		return (NULL);
	return (&catalogue->items[slot]);
}

/**
 * @brief Reports whether the wallet covers one catalogue entry.
 */
bool	marketplace_can_afford(const app_marketplace_view_model_t *market,
	const app_catalogue_item_view_model_t *item)
{
	if (market == NULL || item == NULL)
		return (false);
	return (market->profile.wallet_points >= item->price);
}

/**
 * @brief Buys the focused entry, debiting the wallet exactly once.
 *
 * The wallet is the authority the whole screen reads from, so the debit and
 * the ownership flip happen together here rather than in the renderer or the
 * input loop.
 *
 * @return Bought, already owned, insufficient funds, or invalid selection.
 */
marketplace_purchase_result_t	marketplace_buy_focused(
	app_marketplace_view_model_t *market, const marketplace_state_t *state)
{
	app_catalogue_view_model_t				*catalogue;
	const app_catalogue_item_view_model_t	*item;
	int										slot;

	item = marketplace_focused_item(market, state);
	catalogue = mutable_catalogue(market, state);
	if (item == NULL || catalogue == NULL || !market->signed_in
		|| market->offline)
		return (MARKETPLACE_PURCHASE_INVALID);
	if (item->owned)
		return (MARKETPLACE_PURCHASE_OWNED);
	if (!marketplace_can_afford(market, item))
		return (MARKETPLACE_PURCHASE_INSUFFICIENT);
	slot = marketplace_focused_slot(state);
	market->profile.wallet_points -= catalogue->items[slot].price;
	catalogue->items[slot].owned = true;
	return (MARKETPLACE_PURCHASE_BOUGHT);
}

/**
 * @brief Equips the focused entry when it is already owned.
 *
 * Buying does not equip: a purchase and a loadout change are separate
 * decisions, and equipping silently would overwrite a chosen character.
 *
 * @return Changed, unchanged, locked, or invalid selection result.
 */
settings_equip_result_t	marketplace_equip_focused(
	app_marketplace_view_model_t *market, const marketplace_state_t *state)
{
	app_catalogue_view_model_t				*catalogue;
	const app_catalogue_item_view_model_t	*item;
	int										slot;

	item = marketplace_focused_item(market, state);
	catalogue = mutable_catalogue(market, state);
	if (item == NULL || catalogue == NULL || !market->signed_in
		|| market->offline)
		return (SETTINGS_EQUIP_INVALID);
	if (!item->owned)
		return (SETTINGS_EQUIP_LOCKED);
	if (item->equipped)
		return (SETTINGS_EQUIP_UNCHANGED);
	slot = marketplace_focused_slot(state);
	clear_equipped(catalogue);
	catalogue->items[slot].equipped = true;
	if (marketplace_focused_is_character(state))
	{
		snprintf(market->profile.character, sizeof(market->profile.character),
			"%s", catalogue->items[slot].name);
		snprintf(market->profile.portrait_asset,
			sizeof(market->profile.portrait_asset), "%s",
			catalogue->items[slot].portrait_asset);
	}
	else
		snprintf(market->profile.theme, sizeof(market->profile.theme), "%s",
			catalogue->items[slot].name);
	return (SETTINGS_EQUIP_CHANGED);
}

/**
 * @brief Advances one step through sections, then through the focused row.
 *
 * Tab remains a single linear escape hatch so the whole screen is reachable
 * without knowing the grid shape.
 */
void	marketplace_state_focus_next(marketplace_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->section == MARKETPLACE_SECTION_CONTROLS)
	{
		if (state->focus != MARKETPLACE_FOCUS_VOLUME_UP)
			move_control(state, 1);
		else if (has_inventory(state))
			enter_inventory(state, state->character_slots > 0
				? MARKETPLACE_SECTION_CHARACTERS
				: MARKETPLACE_SECTION_THEMES, 0, false);
		else
			move_control(state, 1);
		return ;
	}
	if (state->section == MARKETPLACE_SECTION_CHARACTERS
		&& state->character_slot + 1 < state->character_slots)
		state->character_slot++;
	else if (state->section == MARKETPLACE_SECTION_CHARACTERS
		&& state->theme_slots > 0)
		enter_inventory(state, MARKETPLACE_SECTION_THEMES, 0, false);
	else if (state->section == MARKETPLACE_SECTION_THEMES
		&& state->theme_slot + 1 < state->theme_slots)
		state->theme_slot++;
	else
	{
		state->section = MARKETPLACE_SECTION_CONTROLS;
		state->focus = MARKETPLACE_FOCUS_BACK;
	}
}

/**
 * @brief Steps backwards through the same order Tab advances through.
 *
 * Stepping backwards out of the controls lands on the very last slot the
 * panels draw, which is the rightmost slot of the last row rather than the
 * first row: asking for row zero would leave later rows unreachable.
 */
void	marketplace_state_focus_previous(marketplace_state_t *state)
{
	if (state == NULL)
		return ;
	if (state->section == MARKETPLACE_SECTION_CONTROLS)
	{
		if (state->focus != MARKETPLACE_FOCUS_BACK)
			move_control(state, -1);
		else if (state->theme_slots > 0)
			enter_inventory(state, MARKETPLACE_SECTION_THEMES, INT_MAX, true);
		else if (state->character_slots > 0)
			enter_inventory(state, MARKETPLACE_SECTION_CHARACTERS, INT_MAX,
				true);
		else
			move_control(state, -1);
		return ;
	}
	if (state->section == MARKETPLACE_SECTION_THEMES && state->theme_slot > 0)
		state->theme_slot--;
	else if (state->section == MARKETPLACE_SECTION_THEMES
		&& state->character_slots > 0)
		enter_inventory(state, MARKETPLACE_SECTION_CHARACTERS, INT_MAX, true);
	else if (state->section == MARKETPLACE_SECTION_CHARACTERS
		&& state->character_slot > 0)
		state->character_slot--;
	else
	{
		state->section = MARKETPLACE_SECTION_CONTROLS;
		state->focus = MARKETPLACE_FOCUS_VOLUME_UP;
	}
}

/**
 * @brief Converts Marketplace keyboard input into semantic actions.
 */
marketplace_action_t	marketplace_handle_key(marketplace_state_t *state,
	uint32_t key)
{
	if (state == NULL)
		return (MARKETPLACE_ACTION_NONE);
	if (key == 'q' || key == 'Q')
		return (MARKETPLACE_ACTION_QUIT);
	if (key == NCKEY_ESC)
		return (MARKETPLACE_ACTION_BACK);
	if (key == '+' || key == '=')
		return (MARKETPLACE_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (MARKETPLACE_ACTION_VOLUME_DOWN);
	if (state->signed_in && (key == 'b' || key == 'B'))
		return (MARKETPLACE_ACTION_BUY);
	if (state->signed_in && (key == 'e' || key == 'E'))
		return (MARKETPLACE_ACTION_EQUIP);
	/* Any movement retires the last result: it described the old cursor. */
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT)
	{
		state->feedback = MARKETPLACE_FEEDBACK_NONE;
		move_horizontal(state, key == NCKEY_RIGHT ? 1 : -1);
		return (MARKETPLACE_ACTION_NONE);
	}
	if (key == NCKEY_UP || key == NCKEY_DOWN)
	{
		state->feedback = MARKETPLACE_FEEDBACK_NONE;
		move_vertical(state, key == NCKEY_DOWN ? 1 : -1);
		return (MARKETPLACE_ACTION_NONE);
	}
	/* Tab advances, matching Settings and the on-screen hint. */
	if (key == NCKEY_TAB)
	{
		state->feedback = MARKETPLACE_FEEDBACK_NONE;
		marketplace_state_focus_next(state);
		return (MARKETPLACE_ACTION_NONE);
	}
	if (key != NCKEY_ENTER && key != '\n' && key != '\r')
		return (MARKETPLACE_ACTION_NONE);
	return (activate_focus(state));
}

/**
 * @brief Resolves Enter against whatever currently holds focus.
 *
 * Inside a grid Enter reads as "act on this item": buy it when it is locked,
 * equip it when it is already owned. The renderer labels the slot accordingly,
 * so the same key never means two things at once from the user's side.
 */
static marketplace_action_t	activate_focus(const marketplace_state_t *state)
{
	if (state->section != MARKETPLACE_SECTION_CONTROLS)
		return (MARKETPLACE_ACTION_BUY);
	if (state->focus == MARKETPLACE_FOCUS_BACK)
		return (MARKETPLACE_ACTION_BACK);
	if (state->focus == MARKETPLACE_FOCUS_BUY && state->signed_in)
		return (MARKETPLACE_ACTION_BUY);
	if (state->focus == MARKETPLACE_FOCUS_VOLUME_DOWN)
		return (MARKETPLACE_ACTION_VOLUME_DOWN);
	if (state->focus == MARKETPLACE_FOCUS_VOLUME_UP)
		return (MARKETPLACE_ACTION_VOLUME_UP);
	return (MARKETPLACE_ACTION_NONE);
}

/**
 * @brief Moves within a row, crossing between the two panels at their edges.
 *
 * The control row wraps because it is a closed strip of four buttons; the
 * inventory grids do not, because their outer edges are where the crossing to
 * the neighbouring panel has to happen.
 */
static void	move_horizontal(marketplace_state_t *state, int step)
{
	int	slots;
	int	*slot;
	int	column;
	int	row;

	if (state->section == MARKETPLACE_SECTION_CONTROLS)
	{
		move_control(state, step);
		return ;
	}
	slot = state->section == MARKETPLACE_SECTION_CHARACTERS
		? &state->character_slot : &state->theme_slot;
	slots = section_slots(state, state->section);
	column = *slot % MARKETPLACE_INVENTORY_COLUMNS;
	row = *slot / MARKETPLACE_INVENTORY_COLUMNS;
	if (step > 0 && column + 1 < MARKETPLACE_INVENTORY_COLUMNS
		&& *slot + 1 < slots)
		*slot += 1;
	else if (step > 0 && state->section == MARKETPLACE_SECTION_CHARACTERS
		&& state->theme_slots > 0)
		enter_inventory(state, MARKETPLACE_SECTION_THEMES, row, false);
	else if (step < 0 && column > 0)
		*slot -= 1;
	else if (step < 0 && state->section == MARKETPLACE_SECTION_THEMES
		&& state->character_slots > 0)
		enter_inventory(state, MARKETPLACE_SECTION_CHARACTERS, row, true);
}

/**
 * @brief Steps between grid rows, falling through to the control row.
 */
static void	move_vertical(marketplace_state_t *state, int step)
{
	int	slots;
	int	*slot;
	int	row;
	int	column;
	int	next_first;
	int	next_last;

	if (state->section == MARKETPLACE_SECTION_CONTROLS)
	{
		if (step < 0 && has_inventory(state))
			enter_inventory(state, state->character_slots > 0
				? MARKETPLACE_SECTION_CHARACTERS : MARKETPLACE_SECTION_THEMES,
				INT_MAX, false);
		return ;
	}
	slot = state->section == MARKETPLACE_SECTION_CHARACTERS
		? &state->character_slot : &state->theme_slot;
	slots = section_slots(state, state->section);
	row = *slot / MARKETPLACE_INVENTORY_COLUMNS;
	column = *slot % MARKETPLACE_INVENTORY_COLUMNS;
	if (step > 0)
	{
		next_first = (row + 1) * MARKETPLACE_INVENTORY_COLUMNS;
		if (next_first >= slots)
		{
			state->section = MARKETPLACE_SECTION_CONTROLS;
			focus_control_column(state, column);
			return ;
		}
		next_last = min_int(slots - 1, next_first
				+ MARKETPLACE_INVENTORY_COLUMNS - 1);
		*slot = min_int(next_first + column, next_last);
	}
	else if (row > 0)
		*slot -= MARKETPLACE_INVENTORY_COLUMNS;
}

/**
 * @brief Focuses the control button sitting under the grid column just left.
 *
 * The grid and the control strip are both four wide, so dropping out of the
 * grid lands on the button directly below rather than on whichever button
 * happened to be focused before the panels were entered.
 */
static void	focus_control_column(marketplace_state_t *state, int column)
{
	int	focus;

	focus = clamp_int(column, 0, MARKETPLACE_BUTTON_COUNT - 1);
	if (focus == (int)MARKETPLACE_FOCUS_BUY && !state->signed_in)
		focus = (int)MARKETPLACE_FOCUS_BACK;
	state->focus = (marketplace_focus_t)focus;
}

/**
 * @brief Cycles the four control buttons, skipping Buy when signed out.
 */
static void	move_control(marketplace_state_t *state, int step)
{
	int	focus;

	focus = (int)state->focus;
	focus = (focus + (step > 0 ? 1 : MARKETPLACE_BUTTON_COUNT - 1))
		% MARKETPLACE_BUTTON_COUNT;
	if (focus == (int)MARKETPLACE_FOCUS_BUY && !state->signed_in)
		focus = (focus + (step > 0 ? 1 : MARKETPLACE_BUTTON_COUNT - 1))
			% MARKETPLACE_BUTTON_COUNT;
	state->focus = (marketplace_focus_t)focus;
}

/**
 * @brief Focuses a panel at the requested row, clamped to what it draws.
 */
static void	enter_inventory(marketplace_state_t *state,
	marketplace_section_t section, int row, bool rightmost)
{
	int	slots;
	int	rows;
	int	slot;

	slots = section_slots(state, section);
	if (slots <= 0)
		return ;
	rows = slot_rows(slots);
	row = clamp_int(row, 0, rows - 1);
	slot = row * MARKETPLACE_INVENTORY_COLUMNS;
	if (rightmost)
		slot += MARKETPLACE_INVENTORY_COLUMNS - 1;
	slot = clamp_int(slot, 0, slots - 1);
	state->section = section;
	state->preview = section;
	if (section == MARKETPLACE_SECTION_CHARACTERS)
		state->character_slot = slot;
	else
		state->theme_slot = slot;
}

static int	section_slots(const marketplace_state_t *state,
	marketplace_section_t section)
{
	if (section == MARKETPLACE_SECTION_CHARACTERS)
		return (state->character_slots);
	if (section == MARKETPLACE_SECTION_THEMES)
		return (state->theme_slots);
	return (MARKETPLACE_BUTTON_COUNT);
}

static bool	has_inventory(const marketplace_state_t *state)
{
	return (state->character_slots > 0 || state->theme_slots > 0);
}

static int	slot_rows(int slots)
{
	if (slots <= 0)
		return (0);
	return ((slots + MARKETPLACE_INVENTORY_COLUMNS - 1)
		/ MARKETPLACE_INVENTORY_COLUMNS);
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
 * @brief Returns the writable catalogue behind the focused panel.
 */
static app_catalogue_view_model_t	*mutable_catalogue(
	app_marketplace_view_model_t *market, const marketplace_state_t *state)
{
	if (market == NULL || state == NULL)
		return (NULL);
	if (marketplace_focused_is_character(state))
		return (&market->characters);
	return (&market->themes);
}

static void	clear_equipped(app_catalogue_view_model_t *catalogue)
{
	int	index;

	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		catalogue->items[index].equipped = false;
		index++;
	}
}

/**
 * @brief Records the result of the last action for the control-row readout.
 */
void	marketplace_set_feedback(marketplace_state_t *state,
	marketplace_feedback_t feedback, int value)
{
	if (state == NULL)
		return ;
	state->feedback = feedback;
	state->feedback_value = value;
}

/**
 * @brief Renders the last result as one line, or NULL when there is nothing.
 *
 * Both renderers read this so the bitmap and cell paths always report the same
 * outcome in the same words.
 */
const char	*marketplace_feedback_text(const marketplace_state_t *state,
	char *out, size_t size)
{
	if (state == NULL || out == NULL || size == 0)
		return (NULL);
	if (state->feedback == MARKETPLACE_FEEDBACK_BOUGHT)
		snprintf(out, size, "PURCHASED - PRESS E TO EQUIP IT");
	else if (state->feedback == MARKETPLACE_FEEDBACK_EQUIPPED)
		snprintf(out, size, "EQUIPPED");
	else if (state->feedback == MARKETPLACE_FEEDBACK_OWNED)
		snprintf(out, size, "ALREADY OWNED");
	else if (state->feedback == MARKETPLACE_FEEDBACK_INSUFFICIENT)
		snprintf(out, size, "NOT ENOUGH WALLET POINTS");
	else if (state->feedback == MARKETPLACE_FEEDBACK_LOCKED)
		snprintf(out, size, "LOCKED - BUY IT FIRST");
	else if (state->feedback == MARKETPLACE_FEEDBACK_VOLUME)
		snprintf(out, size, "MUSIC VOLUME %d%%", state->feedback_value);
	else
		return (NULL);
	return (out);
}
