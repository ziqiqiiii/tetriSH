#include "tetrisu.h"

// Static Functions
static int	reflow_home(render_ctx_t *ctx, const menu_selection_t *menu);

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
	/* The intro plays at the default level; only the looping theme starts
	 * soft. Later +/- presses adjust from wherever the user left it. */
	audio_set_music_volume(&audio, HOME_BGM_START_VOLUME);
	audio_play_music(&audio, HOME_BGM_PATH);
	audio_load_menu_sfx(&audio, MENU_MOVE_SFX_PATH, MENU_SELECT_SFX_PATH);
	state = APP_MAIN_MENU;
	render_menu_create(&ctx);
	while (state != APP_QUIT)
	{
		key = render_wait_key(&ctx);
		if (key == (uint32_t)-1)
			state = APP_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (reflow_home(&ctx, &menu) < 0)
				state = APP_QUIT;
		}
		else if (key == NCKEY_UP || key == NCKEY_DOWN)
		{
			menu_move_selection(&menu, key);
			audio_play_menu_move(&audio);
			render_menu_move_bunny(&ctx, &menu);
		}
		else if (key == NCKEY_ENTER || key == '\n')
		{
			audio_play_menu_select(&audio);
			if (menu.selected == 0)
			{
				if (solo_mode_run(&ctx, &audio) < 0
					&& reflow_home(&ctx, &menu) < 0)
					state = APP_QUIT;
			}
			else
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
	render_menu_destroy(&ctx);
	render_background_destroy(&ctx);
	render_teardown(&ctx);
	return (0);
}

/**
 * @brief Rebuilds the home screen after terminal geometry changes.
 *
 * The background is regenerated before the selector so both planes use the
 * same refreshed cell and pixel geometry.
 *
 * @param ctx Active render context.
 * @param menu Current menu selection to restore.
 * @return 0 on success, -1 when geometry or background refresh fails.
 */
static int	reflow_home(render_ctx_t *ctx, const menu_selection_t *menu)
{
	render_menu_destroy(ctx);
	if (render_geometry_refresh(ctx, true) < 0
		|| render_background_replace(ctx, SPLASH_ASSET_PATH, false) < 0)
		return (-1);
	render_menu_create(ctx);
	render_menu_move_bunny(ctx, menu);
	return (0);
}
