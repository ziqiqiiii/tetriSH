#include "tetrisu.h"

static void	test_single_player_always_navigates(void);
static void	test_settings_always_navigates(void);
static void	test_multiplayer_offline_requires_sign_in(void);
static void	test_marketplace_offline_requires_sign_in(void);
static void	test_leaderboard_offline_requires_sign_in(void);
static void	test_multiplayer_fixture_navigates(void);
static void	test_marketplace_fixture_navigates(void);
static void	test_leaderboard_fixture_navigates(void);
static void	test_invalid_index_blocked(void);
static void	test_sign_in_body_message(void);
static void	test_modal_escape_dismisses(void);
static void	test_modal_enter_dismiss_focus(void);
static void	test_modal_enter_login_focus(void);
static void	test_modal_carriage_return_confirms(void);
static void	test_modal_tab_toggles_focus(void);
static void	test_modal_arrows_toggle_focus(void);

int	main(void)
{
	test_single_player_always_navigates();
	test_settings_always_navigates();
	test_multiplayer_offline_requires_sign_in();
	test_marketplace_offline_requires_sign_in();
	test_leaderboard_offline_requires_sign_in();
	test_multiplayer_fixture_navigates();
	test_marketplace_fixture_navigates();
	test_leaderboard_fixture_navigates();
	test_invalid_index_blocked();
	test_sign_in_body_message();
	test_modal_escape_dismisses();
	test_modal_enter_dismiss_focus();
	test_modal_enter_login_focus();
	test_modal_carriage_return_confirms();
	test_modal_tab_toggles_focus();
	test_modal_arrows_toggle_focus();
	return (0);
}

/* --- Routing policy tests ------------------------------------------------- */

static void	test_single_player_always_navigates(void)
{
	home_route_t	route;

	route = home_menu_route(0, true);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_SOLO);
	route = home_menu_route(0, false);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_SOLO);
	printf("PASS test_single_player_always_navigates\n");
}

static void	test_settings_always_navigates(void)
{
	home_route_t	route;

	route = home_menu_route(4, true);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_SETTINGS);
	route = home_menu_route(4, false);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_SETTINGS);
	printf("PASS test_settings_always_navigates\n");
}

static void	test_multiplayer_offline_requires_sign_in(void)
{
	home_route_t	route;

	route = home_menu_route(1, true);
	assert(route.action == HOME_ROUTE_SIGN_IN_REQUIRED);
	assert(strcmp(route.label, "Multiplayer") == 0);
	printf("PASS test_multiplayer_offline_requires_sign_in\n");
}

static void	test_marketplace_offline_requires_sign_in(void)
{
	home_route_t	route;

	route = home_menu_route(2, true);
	assert(route.action == HOME_ROUTE_SIGN_IN_REQUIRED);
	assert(strcmp(route.label, "Marketplace") == 0);
	printf("PASS test_marketplace_offline_requires_sign_in\n");
}

static void	test_leaderboard_offline_requires_sign_in(void)
{
	home_route_t	route;

	route = home_menu_route(3, true);
	assert(route.action == HOME_ROUTE_SIGN_IN_REQUIRED);
	assert(strcmp(route.label, "Leaderboard") == 0);
	printf("PASS test_leaderboard_offline_requires_sign_in\n");
}

static void	test_multiplayer_fixture_navigates(void)
{
	home_route_t	route;

	route = home_menu_route(1, false);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_MULTIPLAYER_MODE);
	printf("PASS test_multiplayer_fixture_navigates\n");
}

static void	test_marketplace_fixture_navigates(void)
{
	home_route_t	route;

	route = home_menu_route(2, false);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_MARKETPLACE);
	printf("PASS test_marketplace_fixture_navigates\n");
}

static void	test_leaderboard_fixture_navigates(void)
{
	home_route_t	route;

	route = home_menu_route(3, false);
	assert(route.action == HOME_ROUTE_NAVIGATE);
	assert(route.nav_action == APP_NAV_OPEN_LEADERBOARD);
	printf("PASS test_leaderboard_fixture_navigates\n");
}

static void	test_invalid_index_blocked(void)
{
	home_route_t	route;

	route = home_menu_route(-1, false);
	assert(route.action == HOME_ROUTE_BLOCKED);
	route = home_menu_route(5, false);
	assert(route.action == HOME_ROUTE_BLOCKED);
	route = home_menu_route(100, true);
	assert(route.action == HOME_ROUTE_BLOCKED);
	printf("PASS test_invalid_index_blocked\n");
}

static void	test_sign_in_body_message(void)
{
	char	line1[80];
	char	line2[80];

	home_sign_in_body_line1("Multiplayer", line1, sizeof(line1));
	home_sign_in_body_line2(line2, sizeof(line2));
	assert(strstr(line1, "Multiplayer") != NULL);
	assert(strstr(line1, "online-only") != NULL);
	assert(strstr(line2, "Sign in") != NULL);
	home_sign_in_body_line1("Marketplace", line1, sizeof(line1));
	assert(strstr(line1, "Marketplace") != NULL);
	home_sign_in_body_line1("Leaderboard", line1, sizeof(line1));
	assert(strstr(line1, "Leaderboard") != NULL);
	printf("PASS test_sign_in_body_message\n");
}

/* --- Modal input handling tests ------------------------------------------- */

static void	test_modal_escape_dismisses(void)
{
	sign_in_modal_t		modal;
	sign_in_result_t	result;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Multiplayer";
	result = sign_in_modal_handle_key(&modal, NCKEY_ESC);
	assert(result == SIGN_IN_RESULT_DISMISS);
	printf("PASS test_modal_escape_dismisses\n");
}

static void	test_modal_enter_dismiss_focus(void)
{
	sign_in_modal_t		modal;
	sign_in_result_t	result;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Marketplace";
	modal.focus = SIGN_IN_FOCUS_DISMISS;
	result = sign_in_modal_handle_key(&modal, NCKEY_ENTER);
	assert(result == SIGN_IN_RESULT_DISMISS);
	printf("PASS test_modal_enter_dismiss_focus\n");
}

static void	test_modal_enter_login_focus(void)
{
	sign_in_modal_t		modal;
	sign_in_result_t	result;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Leaderboard";
	modal.focus = SIGN_IN_FOCUS_LOGIN;
	result = sign_in_modal_handle_key(&modal, NCKEY_ENTER);
	assert(result == SIGN_IN_RESULT_LOGIN);
	printf("PASS test_modal_enter_login_focus\n");
}

static void	test_modal_carriage_return_confirms(void)
{
	sign_in_modal_t		modal;
	sign_in_result_t	result;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Multiplayer";
	modal.focus = SIGN_IN_FOCUS_LOGIN;
	result = sign_in_modal_handle_key(&modal, '\r');
	assert(result == SIGN_IN_RESULT_LOGIN);
	printf("PASS test_modal_carriage_return_confirms\n");
}

static void	test_modal_tab_toggles_focus(void)
{
	sign_in_modal_t	modal;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Multiplayer";
	modal.focus = SIGN_IN_FOCUS_DISMISS;
	(void)sign_in_modal_handle_key(&modal, '\t');
	assert(modal.focus == SIGN_IN_FOCUS_LOGIN);
	(void)sign_in_modal_handle_key(&modal, '\t');
	assert(modal.focus == SIGN_IN_FOCUS_DISMISS);
	printf("PASS test_modal_tab_toggles_focus\n");
}

static void	test_modal_arrows_toggle_focus(void)
{
	sign_in_modal_t	modal;

	sign_in_modal_init(&modal);
	modal.visible = true;
	modal.label = "Multiplayer";
	modal.focus = SIGN_IN_FOCUS_DISMISS;
	(void)sign_in_modal_handle_key(&modal, NCKEY_RIGHT);
	assert(modal.focus == SIGN_IN_FOCUS_LOGIN);
	(void)sign_in_modal_handle_key(&modal, NCKEY_LEFT);
	assert(modal.focus == SIGN_IN_FOCUS_DISMISS);
	printf("PASS test_modal_arrows_toggle_focus\n");
}
