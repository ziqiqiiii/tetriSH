#include "tetrisu.h"

// Static Variables
static char	g_stub_buf[64];

/**
 * @brief Computes the next app_state_t given the current one and a keycode.
 *
 * The only two state-level transitions are: APP_SPLASH advances to
 * APP_MAIN_MENU on any key, and APP_MAIN_MENU advances to APP_QUIT on 'q'.
 * Menu selection movement (arrow keys) is handled separately by
 * menu_move_selection() and never changes app_state_t.
 *
 * @param current The current application state.
 * @param key The key code just read from render_wait_key() (a Unicode
 * codepoint, an NCKEY_* constant, or (uint32_t)-1 on input error/EOF).
 * @return The next application state.
 */
app_state_t	app_handle_key(app_state_t current, uint32_t key)
{
	if (current == APP_SPLASH)
		return (APP_MAIN_MENU);
	if (current == APP_MAIN_MENU && key == 'q')
		return (APP_QUIT);
	return (current);
}

/**
 * @brief Moves the menu selection up or down, wrapping at the ends.
 *
 * @param m Pointer to the selection state to update.
 * @param key The key just read; only NCKEY_UP and NCKEY_DOWN have any effect.
 */
void	menu_move_selection(menu_selection_t *m, uint32_t key)
{
	if (key == NCKEY_UP)
		m->selected = (m->selected + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
	else if (key == NCKEY_DOWN)
		m->selected = (m->selected + 1) % MENU_ITEM_COUNT;
}

/**
 * @brief Returns the fixed-order label for a menu item.
 *
 * @param index Menu item index, 0 to MENU_ITEM_COUNT - 1.
 * @return The item's label, or an empty string if index is out of range.
 */
const char	*menu_item_label(int index)
{
	static const char	*labels[MENU_ITEM_COUNT] =
	{
		"Solo Battle",
		"Multiplayer Battle",
		"Marketplace",
		"Options",
	};

	if (index < 0 || index >= MENU_ITEM_COUNT)
		return ("");
	return (labels[index]);
}

/**
 * @brief Builds the "not wired up yet" stub message for a menu item.
 *
 * @param selected_index Index of the selected menu item.
 * @return Pointer to an internal static buffer holding the message; valid
 * until the next call to this function.
 */
const char	*menu_stub_text(int selected_index)
{
	snprintf(g_stub_buf, sizeof(g_stub_buf), "[%s] not wired up yet",
		menu_item_label(selected_index));
	return (g_stub_buf);
}
