#include "tetrisu.h"

/**
 * @brief Decides the routing action for a home menu selection.
 *
 * Pure policy function with no I/O or side effects. Determines whether a
 * home menu press should navigate, show the sign-in-required modal, or do
 * nothing. Tested without launching an interactive terminal.
 *
 * @param selected Menu index 0..4.
 * @param offline true when the user entered via Play Offline.
 * @return The resolved home menu route.
 */
t_home_route	home_menu_route(int selected, bool offline)
{
	t_home_route	route;

	route.action = HOME_ROUTE_BLOCKED;
	route.nav_action = APP_NAV_NONE;
	route.label = "";
	if (selected < 0 || selected >= MENU_ITEM_COUNT)
		return (route);
	if (selected == 0)
	{
		route.action = HOME_ROUTE_NAVIGATE;
		route.nav_action = APP_NAV_OPEN_SOLO;
		route.label = "Single Player";
		return (route);
	}
	if (selected == 4)
	{
		route.action = HOME_ROUTE_NAVIGATE;
		route.nav_action = APP_NAV_OPEN_SETTINGS;
		route.label = "Settings";
		return (route);
	}
	if (!offline)
	{
		if (selected == 1)
		{
			route.action = HOME_ROUTE_NAVIGATE;
			route.nav_action = APP_NAV_OPEN_MULTIPLAYER_MODE;
			route.label = "Multiplayer";
		}
		else if (selected == 2)
		{
			route.action = HOME_ROUTE_NAVIGATE;
			route.nav_action = APP_NAV_OPEN_MARKETPLACE;
			route.label = "Marketplace";
		}
		else if (selected == 3)
		{
			route.action = HOME_ROUTE_NAVIGATE;
			route.nav_action = APP_NAV_OPEN_LEADERBOARD;
			route.label = "Leaderboard";
		}
		return (route);
	}
	route.action = HOME_ROUTE_SIGN_IN_REQUIRED;
	if (selected == 1)
		route.label = "Multiplayer";
	else if (selected == 2)
		route.label = "Marketplace";
	else if (selected == 3)
		route.label = "Leaderboard";
	return (route);
}

/**
 * @brief Returns line 1 of the sign-in-required body: identifies the feature.
 *
 * Pure helper, testable without a terminal. Output fits well within any
 * reasonable modal width.
 *
 * @param label The action label (e.g. "Multiplayer").
 * @param line1 Output buffer for the first line.
 * @param size Buffer capacity.
 */
void	home_sign_in_body_line1(const char *label, char *line1, size_t size)
{
	if (label == NULL || line1 == NULL || size == 0)
		return ;
	snprintf(line1, size, "%s is an online-only feature.", label);
}

/**
 * @brief Returns line 2 of the sign-in-required body: the call to action.
 *
 * @param line2 Output buffer for the second line.
 * @param size Buffer capacity.
 */
void	home_sign_in_body_line2(char *line2, size_t size)
{
	if (line2 == NULL || size == 0)
		return ;
	snprintf(line2, size, "Sign in to connect and continue.");
}

/**
 * @brief Initializes the sign-in modal to a hidden idle state.
 */
void	sign_in_modal_init(t_sign_in_modal *modal)
{
	if (modal == NULL)
		return ;
	memset(modal, 0, sizeof(*modal));
	modal->visible = false;
	modal->focus = SIGN_IN_FOCUS_DISMISS;
	modal->label = "";
}

/**
 * @brief Handles keyboard input for the sign-in modal.
 *
 * Pure logic: no rendering, no side effects beyond updating the modal state.
 *
 * @return SIGN_IN_RESULT_DISMISS, SIGN_IN_RESULT_LOGIN, or _NONE.
 */
t_sign_in_result	sign_in_modal_handle_key(t_sign_in_modal *modal,
	uint32_t key)
{
	if (modal == NULL || !modal->visible)
		return (SIGN_IN_RESULT_NONE);
	if (key == NCKEY_ESC)
		return (SIGN_IN_RESULT_DISMISS);
	if (key == NCKEY_ENTER || key == '\n' || key == '\r')
	{
		if (modal->focus == SIGN_IN_FOCUS_LOGIN)
			return (SIGN_IN_RESULT_LOGIN);
		return (SIGN_IN_RESULT_DISMISS);
	}
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT || key == '\t')
	{
		if (modal->focus == SIGN_IN_FOCUS_DISMISS)
			modal->focus = SIGN_IN_FOCUS_LOGIN;
		else
			modal->focus = SIGN_IN_FOCUS_DISMISS;
	}
	return (SIGN_IN_RESULT_NONE);
}
