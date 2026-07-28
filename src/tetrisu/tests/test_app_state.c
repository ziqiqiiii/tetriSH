#include "tetrisu.h"

static void	test_screen_names_and_parents(void);
static void	test_entry_and_account_navigation(void);
static void	test_home_destinations_and_back(void);
static void	test_multiplayer_navigation_chain(void);
static void	test_invalid_transitions_are_rejected(void);
static void	test_five_item_menu_labels(void);
static void	test_menu_navigation_wraps(void);

int	main(void)
{
	test_screen_names_and_parents();
	test_entry_and_account_navigation();
	test_home_destinations_and_back();
	test_multiplayer_navigation_chain();
	test_invalid_transitions_are_rejected();
	test_five_item_menu_labels();
	test_menu_navigation_wraps();
	return (0);
}

static void	test_screen_names_and_parents(void)
{
	int	screen;

	screen = APP_SCREEN_ENTRY;
	while (screen < APP_SCREEN_COUNT)
	{
		assert(strcmp(app_screen_name((app_screen_t)screen), "Unknown") != 0);
		screen++;
	}
	assert(app_screen_parent(APP_SCREEN_LOGIN) == APP_SCREEN_LOGIN);
	assert(app_screen_parent(APP_SCREEN_SIGN_UP) == APP_SCREEN_LOGIN);
	assert(app_screen_parent(APP_SCREEN_HOME) == APP_SCREEN_LOGIN);
	assert(app_screen_parent(APP_SCREEN_SOLO) == APP_SCREEN_HOME);
	assert(app_screen_parent(APP_SCREEN_MARKETPLACE) == APP_SCREEN_HOME);
	assert(app_screen_parent(APP_SCREEN_CREATE_ROOM_MODAL) == APP_SCREEN_LOBBY);
	assert(app_screen_parent(APP_SCREEN_WAITING_ROOM) == APP_SCREEN_LOBBY);
	assert(app_screen_parent(APP_SCREEN_DOUBLE) == APP_SCREEN_WAITING_ROOM);
	assert(app_screen_parent(APP_SCREEN_BATTLE_ROYALE)
		== APP_SCREEN_WAITING_ROOM);
	printf("PASS test_screen_names_and_parents\n");
}

static void	test_entry_and_account_navigation(void)
{
	app_navigation_t	navigation;

	app_navigation_init(&navigation, APP_SCREEN_ENTRY);
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_LOGIN));
	assert(navigation.current == APP_SCREEN_LOGIN);
	assert(navigation.previous == APP_SCREEN_ENTRY);
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_SIGN_UP));
	assert(navigation.current == APP_SCREEN_SIGN_UP);
	assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
	assert(navigation.current == APP_SCREEN_LOGIN);
	assert(app_navigation_dispatch(&navigation, APP_NAV_AUTHENTICATED));
	assert(navigation.current == APP_SCREEN_HOME && !navigation.offline);
	assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
	assert(navigation.current == APP_SCREEN_LOGIN);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	assert(app_navigation_dispatch(&navigation, APP_NAV_PLAY_OFFLINE));
	assert(navigation.current == APP_SCREEN_HOME && navigation.offline);
	assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
	assert(navigation.current == APP_SCREEN_LOGIN && !navigation.offline);
	printf("PASS test_entry_and_account_navigation\n");
}

static void	test_home_destinations_and_back(void)
{
	static const app_nav_action_t	actions[] = {
		APP_NAV_OPEN_SOLO,
		APP_NAV_OPEN_MARKETPLACE,
		APP_NAV_OPEN_SETTINGS,
		APP_NAV_OPEN_LEADERBOARD,
		APP_NAV_OPEN_LOBBY
	};
	static const app_screen_t		screens[] = {
		APP_SCREEN_SOLO,
		APP_SCREEN_MARKETPLACE,
		APP_SCREEN_SETTINGS,
		APP_SCREEN_LEADERBOARD,
		APP_SCREEN_LOBBY
	};
	app_navigation_t				navigation;
	size_t							index;

	index = 0;
	while (index < sizeof(actions) / sizeof(actions[0]))
	{
		app_navigation_init(&navigation, APP_SCREEN_HOME);
		assert(app_navigation_dispatch(&navigation, actions[index]));
		assert(navigation.current == screens[index]);
		assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
		assert(navigation.current == APP_SCREEN_HOME);
		index++;
	}
	assert(app_handle_key(APP_SCREEN_MARKETPLACE, NCKEY_ESC)
		== APP_SCREEN_HOME);
	assert(app_handle_key(APP_SCREEN_HOME, 'q') == APP_SCREEN_QUIT);
	printf("PASS test_home_destinations_and_back\n");
}

static void	test_multiplayer_navigation_chain(void)
{
	app_navigation_t	navigation;

	app_navigation_init(&navigation, APP_SCREEN_HOME);
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_LOBBY));
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_CREATE_ROOM));
	assert(navigation.current == APP_SCREEN_CREATE_ROOM_MODAL);
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_WAITING_ROOM));
	assert(navigation.current == APP_SCREEN_WAITING_ROOM);
	assert(app_navigation_dispatch(&navigation, APP_NAV_START_DOUBLE));
	assert(navigation.current == APP_SCREEN_DOUBLE);
	assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
	assert(navigation.current == APP_SCREEN_WAITING_ROOM);
	assert(app_navigation_dispatch(&navigation,
			APP_NAV_START_BATTLE_ROYALE));
	assert(navigation.current == APP_SCREEN_BATTLE_ROYALE);
	assert(app_navigation_dispatch(&navigation, APP_NAV_QUIT));
	assert(navigation.current == APP_SCREEN_QUIT);
	printf("PASS test_multiplayer_navigation_chain\n");
}

static void	test_invalid_transitions_are_rejected(void)
{
	app_navigation_t	navigation;

	app_navigation_init(&navigation, APP_SCREEN_ENTRY);
	assert(!app_navigation_dispatch(&navigation, APP_NAV_OPEN_SOLO));
	assert(navigation.current == APP_SCREEN_ENTRY);
	assert(!app_navigation_dispatch(&navigation, APP_NAV_BACK));
	app_navigation_init(&navigation, APP_SCREEN_HOME);
	assert(!app_navigation_dispatch(&navigation, APP_NAV_START_DOUBLE));
	assert(!app_navigation_dispatch(&navigation, APP_NAV_OPEN_WAITING_ROOM));
	assert(navigation.current == APP_SCREEN_HOME);
	printf("PASS test_invalid_transitions_are_rejected\n");
}

static void	test_five_item_menu_labels(void)
{
	assert(MENU_ITEM_COUNT == 5);
	assert(strcmp(menu_item_label(0), "Single Player") == 0);
	assert(strcmp(menu_item_label(1), "Multiplayer") == 0);
	assert(strcmp(menu_item_label(2), "Marketplace") == 0);
	assert(strcmp(menu_item_label(3), "Leaderboard") == 0);
	assert(strcmp(menu_item_label(4), "Settings") == 0);
	assert(strcmp(menu_item_label(-1), "") == 0);
	assert(strcmp(menu_item_label(MENU_ITEM_COUNT), "") == 0);
	printf("PASS test_five_item_menu_labels\n");
}

static void	test_menu_navigation_wraps(void)
{
	menu_selection_t	menu;

	menu.selected = 0;
	menu_move_selection(&menu, NCKEY_UP);
	assert(menu.selected == MENU_ITEM_COUNT - 1);
	menu_move_selection(&menu, NCKEY_DOWN);
	assert(menu.selected == 0);
	menu_move_selection(&menu, NCKEY_DOWN);
	assert(menu.selected == 1);
	printf("PASS test_menu_navigation_wraps\n");
}
