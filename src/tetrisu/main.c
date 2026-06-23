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
	audio_ctx_t		audio;
	uint32_t			key;

	menu.selected = 0;
	ctx = render_init(SPLASH_ASSET_PATH);
	audio_init(&audio);
	render_intro_play(&ctx, &audio, INTRO_VIDEO_PATH, INTRO_AUDIO_PATH);
	audio_play_music(&audio, HOME_BGM_PATH);
	audio_load_menu_sfx(&audio, MENU_MOVE_SFX_PATH, MENU_SELECT_SFX_PATH);
	state = APP_MAIN_MENU;
	render_menu_create(&ctx);
	while (state != APP_QUIT)
	{
		key = render_wait_key(&ctx);
		if (key == (uint32_t)-1)
			state = APP_QUIT;
		else if (key == NCKEY_UP || key == NCKEY_DOWN)
		{
			menu_move_selection(&menu, key);
			audio_play_menu_move(&audio);
			render_menu_move_bunny(&ctx, &menu);
		}
		else if (key == NCKEY_ENTER || key == '\n')
		{
			audio_play_menu_select(&audio);
			render_menu_show_message(&ctx, menu_stub_text(menu.selected));
		}
		else if (key == '+' || key == '=')
			audio_volume_up(&audio);
		else if (key == '-' || key == '_')
			audio_volume_down(&audio);
		else
			state = app_handle_key(state, key);
	}
	audio_teardown(&audio);
	render_teardown(&ctx);
	return (0);
}
