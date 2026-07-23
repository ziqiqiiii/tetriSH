#include "tetrisu.h"

static void	test_five_item_menu_labels(void);
static void	test_menu_navigation_wraps(void);

int	main(void)
{
	test_five_item_menu_labels();
	test_menu_navigation_wraps();
	return (0);
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
