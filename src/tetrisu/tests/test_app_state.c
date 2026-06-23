#include "tetrisu.h"

/**
 * @brief Verifies any key during APP_SPLASH advances to APP_MAIN_MENU.
 */
void	test_splash_advances_on_any_key(void)
{
	assert(app_handle_key(APP_SPLASH, 'x') == APP_MAIN_MENU);
	assert(app_handle_key(APP_SPLASH, NCKEY_UP) == APP_MAIN_MENU);
	assert(app_handle_key(APP_SPLASH, -1) == APP_MAIN_MENU);
	printf("PASS test_splash_advances_on_any_key\n");
}

/**
 * @brief Verifies 'q' during APP_MAIN_MENU transitions to APP_QUIT.
 */
void	test_menu_q_quits(void)
{
	assert(app_handle_key(APP_MAIN_MENU, 'q') == APP_QUIT);
	printf("PASS test_menu_q_quits\n");
}

/**
 * @brief Verifies arrow keys during APP_MAIN_MENU do not change app_state_t.
 *
 * Selection movement is handled separately by menu_move_selection(); this
 * function must leave the top-level state untouched for those keys.
 */
void	test_menu_arrows_do_not_change_state(void)
{
	assert(app_handle_key(APP_MAIN_MENU, NCKEY_UP) == APP_MAIN_MENU);
	assert(app_handle_key(APP_MAIN_MENU, NCKEY_DOWN) == APP_MAIN_MENU);
	printf("PASS test_menu_arrows_do_not_change_state\n");
}

/**
 * @brief Verifies NCKEY_UP wraps from item 0 to the last item.
 */
void	test_up_wraps_from_zero(void)
{
	menu_selection_t	m;

	m.selected = 0;
	menu_move_selection(&m, NCKEY_UP);
	assert(m.selected == MENU_ITEM_COUNT - 1);
	printf("PASS test_up_wraps_from_zero\n");
}

/**
 * @brief Verifies NCKEY_DOWN wraps from the last item to item 0.
 */
void	test_down_wraps_from_last(void)
{
	menu_selection_t	m;

	m.selected = MENU_ITEM_COUNT - 1;
	menu_move_selection(&m, NCKEY_DOWN);
	assert(m.selected == 0);
	printf("PASS test_down_wraps_from_last\n");
}

/**
 * @brief Verifies a normal NCKEY_DOWN step advances by one without wrapping.
 */
void	test_down_steps_by_one(void)
{
	menu_selection_t	m;

	m.selected = 1;
	menu_move_selection(&m, NCKEY_DOWN);
	assert(m.selected == 2);
	printf("PASS test_down_steps_by_one\n");
}

/**
 * @brief Verifies an unrelated key leaves the selection unchanged.
 */
void	test_unrelated_key_is_noop(void)
{
	menu_selection_t	m;

	m.selected = 2;
	menu_move_selection(&m, 'x');
	assert(m.selected == 2);
	printf("PASS test_unrelated_key_is_noop\n");
}

/**
 * @brief Verifies the menu item labels match the fixed menu order.
 */
void	test_item_labels_are_in_order(void)
{
	assert(strcmp(menu_item_label(0), "Solo Battle") == 0);
	assert(strcmp(menu_item_label(1), "Multiplayer Battle") == 0);
	assert(strcmp(menu_item_label(2), "Marketplace") == 0);
	assert(strcmp(menu_item_label(3), "Options") == 0);
	printf("PASS test_item_labels_are_in_order\n");
}

/**
 * @brief Verifies the stub text names the selected item and the right phrase.
 */
void	test_stub_text_names_selected_item(void)
{
	assert(strcmp(menu_stub_text(0), "[Solo Battle] not wired up yet") == 0);
	assert(strcmp(menu_stub_text(3), "[Options] not wired up yet") == 0);
	printf("PASS test_stub_text_names_selected_item\n");
}

int	main(void)
{
	test_splash_advances_on_any_key();
	test_menu_q_quits();
	test_menu_arrows_do_not_change_state();
	test_up_wraps_from_zero();
	test_down_wraps_from_last();
	test_down_steps_by_one();
	test_unrelated_key_is_noop();
	test_item_labels_are_in_order();
	test_stub_text_names_selected_item();
	return (0);
}
