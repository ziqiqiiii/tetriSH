#include "tetrisu.h"

/**
 * @brief Entry point: background image, splash keywait, then the menu loop.
 *
 * @return 0 on clean exit.
 */
int	main(void)
{
	app_state_t			state;
	menu_selection_t	menu;
	render_ctx_t		ctx;
	uint32_t			key;

	menu.selected = 0;
	ctx = render_init(SPLASH_ASSET_PATH);
	key = render_wait_key(&ctx);
	if (key == (uint32_t)-1)
		state = APP_QUIT;
	else
		state = app_handle_key(APP_SPLASH, key);
	render_menu_create(&ctx);
	while (state != APP_QUIT)
	{
		key = render_wait_key(&ctx);
		if (key == (uint32_t)-1)
			state = APP_QUIT;
		else if (key == NCKEY_UP || key == NCKEY_DOWN)
		{
			menu_move_selection(&menu, key);
			render_menu_move_bunny(&ctx, &menu);
		}
		else if (key == NCKEY_ENTER || key == '\n')
			render_menu_show_message(&ctx, menu_stub_text(menu.selected));
		else
			state = app_handle_key(state, key);
	}
	render_teardown(&ctx);
	return (0);
}
