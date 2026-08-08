#include "tetrisu.h"

// Static Variables
static char	g_stub_buf[64];

// Static Functions
static bool	navigation_target(const t_app_navigation *navigation,
				t_app_nav_action action, t_app_screen *target);

/**
 * @brief Initializes the application navigation state.
 */
void	app_navigation_init(t_app_navigation *navigation,
	t_app_screen initial)
{
	if (navigation == NULL)
		return ;
	if (initial < APP_SCREEN_ENTRY || initial >= APP_SCREEN_COUNT)
		initial = APP_SCREEN_ENTRY;
	navigation->current = initial;
	navigation->previous = initial;
	navigation->offline = false;
}

/**
 * @brief Applies one validated screen transition.
 *
 * Invalid routes are rejected rather than silently jumping between unrelated
 * screens. This keeps future renderers and network adapters on the same state
 * graph.
 */
bool	app_navigation_dispatch(t_app_navigation *navigation,
	t_app_nav_action action)
{
	t_app_screen	target;

	if (navigation == NULL
		|| !navigation_target(navigation, action, &target))
		return (false);
	if (action == APP_NAV_PLAY_OFFLINE)
		navigation->offline = true;
	else if (action == APP_NAV_AUTHENTICATED)
		navigation->offline = false;
	else if (target == APP_SCREEN_ENTRY || target == APP_SCREEN_LOGIN)
		navigation->offline = false;
	navigation->previous = navigation->current;
	navigation->current = target;
	return (true);
}

/**
 * @brief Returns the deterministic Back destination for one screen.
 */
t_app_screen	app_screen_parent(t_app_screen screen)
{
	if (screen == APP_SCREEN_SIGN_UP || screen == APP_SCREEN_HOME)
		return (APP_SCREEN_LOGIN);
	if (screen == APP_SCREEN_SOLO || screen == APP_SCREEN_MARKETPLACE
		|| screen == APP_SCREEN_SETTINGS || screen == APP_SCREEN_LEADERBOARD
		|| screen == APP_SCREEN_MULTIPLAYER_MODE)
		return (APP_SCREEN_HOME);
	/*
	 * Back out of the lobby returns to the mode picker rather than the home
	 * menu, so changing your mind about Double versus Battle Royale costs one
	 * key instead of a round trip through Home.
	 */
	if (screen == APP_SCREEN_LOBBY)
		return (APP_SCREEN_MULTIPLAYER_MODE);
	if (screen == APP_SCREEN_CREATE_ROOM_MODAL
		|| screen == APP_SCREEN_WAITING_ROOM)
		return (APP_SCREEN_LOBBY);
	if (screen == APP_SCREEN_DOUBLE
		|| screen == APP_SCREEN_BATTLE_ROYALE)
		return (APP_SCREEN_WAITING_ROOM);
	return (screen);
}

/**
 * @brief Returns the reader-facing name for one application screen.
 */
const char	*app_screen_name(t_app_screen screen)
{
	static const char	*names[APP_SCREEN_COUNT] = {
		"Entry",
		"Login",
		"Sign Up",
		"Home",
		"Solo",
		"Marketplace",
		"Settings",
		"Leaderboard",
		"Multiplayer",
		"Multiplayer Lobby",
		"Create Room",
		"Waiting Room",
		"Double",
		"Battle Royale",
		"Quit"
	};

	if (screen < APP_SCREEN_ENTRY || screen >= APP_SCREEN_COUNT)
		return ("Unknown");
	return (names[screen]);
}

/**
 * @brief Handles universal keyboard navigation without rendering concerns.
 */
t_app_screen	app_handle_key(t_app_screen current, uint32_t key)
{
	if (key == 'q' || key == 'Q')
		return (APP_SCREEN_QUIT);
	if (key == NCKEY_ESC)
		return (app_screen_parent(current));
	return (current);
}

/**
 * @brief Moves the menu selection up or down, wrapping at the ends.
 */
void	menu_move_selection(t_menu_selection *m, uint32_t key)
{
	if (m == NULL)
		return ;
	if (key == NCKEY_UP)
		m->selected = (m->selected + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
	else if (key == NCKEY_DOWN)
		m->selected = (m->selected + 1) % MENU_ITEM_COUNT;
}

/**
 * @brief Returns the fixed-order label for a menu item.
 */
const char	*menu_item_label(int index)
{
	static const char	*labels[MENU_ITEM_COUNT] = {
		"Single Player",
		"Multiplayer",
		"Marketplace",
		"Leaderboard",
		"Settings"
	};

	if (index < 0 || index >= MENU_ITEM_COUNT)
		return ("");
	return (labels[index]);
}

/**
 * @brief Builds a temporary message for a scaffolded home destination.
 */
const char	*menu_stub_text(int selected_index)
{
	snprintf(g_stub_buf, sizeof(g_stub_buf), "[%s] screen ready",
		menu_item_label(selected_index));
	return (g_stub_buf);
}

/**
 * @brief Returns whether the explicit local UI preview gate is enabled.
 *
 * The environment is intentionally strict: only TETRISU_UI_PREVIEW=1
 * exposes the fixture sign-in action. This keeps the unavailable real server
 * path and the offline path unchanged for normal runs.
 */
bool	app_ui_preview_enabled(void)
{
	const char	*value;

	value = getenv("TETRISU_UI_PREVIEW");
	return (value != NULL && strcmp(value, "1") == 0);
}

/**
 * @brief Resolves only routes allowed by the item-15 screen graph.
 */
static bool	navigation_target(const t_app_navigation *navigation,
	t_app_nav_action action, t_app_screen *target)
{
	t_app_screen	current;

	current = navigation->current;
	if (action == APP_NAV_QUIT && current != APP_SCREEN_QUIT)
		*target = APP_SCREEN_QUIT;
	else if (action == APP_NAV_BACK && app_screen_parent(current) != current)
		*target = app_screen_parent(current);
	else if (action == APP_NAV_OPEN_LOGIN && current == APP_SCREEN_ENTRY)
		*target = APP_SCREEN_LOGIN;
	else if (action == APP_NAV_OPEN_SIGN_UP
		&& (current == APP_SCREEN_ENTRY || current == APP_SCREEN_LOGIN))
		*target = APP_SCREEN_SIGN_UP;
	else if (action == APP_NAV_PLAY_OFFLINE
		&& (current == APP_SCREEN_ENTRY || current == APP_SCREEN_LOGIN
			|| current == APP_SCREEN_SIGN_UP))
		*target = APP_SCREEN_HOME;
	else if (action == APP_NAV_AUTHENTICATED
		&& (current == APP_SCREEN_LOGIN || current == APP_SCREEN_SIGN_UP))
		*target = APP_SCREEN_HOME;
	else if (action == APP_NAV_OPEN_SOLO && current == APP_SCREEN_HOME)
		*target = APP_SCREEN_SOLO;
	else if (action == APP_NAV_OPEN_MARKETPLACE
		&& !navigation->offline
		&& (current == APP_SCREEN_HOME || current == APP_SCREEN_SETTINGS))
		*target = APP_SCREEN_MARKETPLACE;
	else if (action == APP_NAV_OPEN_SETTINGS && current == APP_SCREEN_HOME)
		*target = APP_SCREEN_SETTINGS;
	else if (action == APP_NAV_OPEN_LEADERBOARD
		&& current == APP_SCREEN_HOME)
		*target = APP_SCREEN_LEADERBOARD;
	else if (action == APP_NAV_OPEN_MULTIPLAYER_MODE
		&& !navigation->offline && current == APP_SCREEN_HOME)
		*target = APP_SCREEN_MULTIPLAYER_MODE;
	else if (action == APP_NAV_OPEN_LOBBY
		&& current == APP_SCREEN_MULTIPLAYER_MODE)
		*target = APP_SCREEN_LOBBY;
	else if (action == APP_NAV_OPEN_CREATE_ROOM
		&& current == APP_SCREEN_LOBBY)
		*target = APP_SCREEN_CREATE_ROOM_MODAL;
	else if (action == APP_NAV_OPEN_WAITING_ROOM
		&& (current == APP_SCREEN_LOBBY
			|| current == APP_SCREEN_CREATE_ROOM_MODAL))
		*target = APP_SCREEN_WAITING_ROOM;
	else if (action == APP_NAV_START_DOUBLE
		&& current == APP_SCREEN_WAITING_ROOM)
		*target = APP_SCREEN_DOUBLE;
	else if (action == APP_NAV_START_BATTLE_ROYALE
		&& current == APP_SCREEN_WAITING_ROOM)
		*target = APP_SCREEN_BATTLE_ROYALE;
	else
		return (false);
	return (true);
}
