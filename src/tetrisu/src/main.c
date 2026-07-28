#include "tetrisu.h"

// Static Functions
static int	reflow_home(render_ctx_t *ctx, const menu_selection_t *menu);
static int	run_auth_flow(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, auth_form_t *form,
				const menu_selection_t *menu);
static auth_action_t	auth_pointer_action(render_ctx_t *ctx,
				audio_ctx_t *audio, auth_form_t *form,
				const ncinput *input, uint32_t key);
static bool	apply_auth_action(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, auth_form_t *form,
				auth_action_t action);
static bool	activate_menu_selection(audio_ctx_t *audio,
				app_navigation_t *navigation,
				const menu_selection_t *menu);
static app_nav_action_t	menu_navigation_action(int selected);
static int	run_scaffold_step(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, const menu_selection_t *menu);
static app_nav_action_t	scaffold_navigation_action(app_screen_t screen,
				uint32_t key);
static void	enable_home_mouse(render_ctx_t *ctx);

/**
 * @brief Entry point for the screen-navigation and rendering loop.
 */
int	main(void)
{
	app_navigation_t	navigation;
	app_data_provider_t	provider;
	menu_selection_t	menu;
	auth_form_t		auth_form;
	render_ctx_t		ctx;
	audio_ctx_t			audio;
	ncinput				input;
	uint32_t			key;
	int					hovered;

	menu.selected = 0;
	ctx = render_init(SPLASH_ASSET_PATH);
	audio_init(&audio);
	render_intro_play(&ctx, &audio, INTRO_VIDEO_PATH, INTRO_AUDIO_PATH);
	audio_set_music_volume(&audio, HOME_BGM_START_VOLUME);
	audio_play_music(&audio, HOME_BGM_PATH);
	audio_load_menu_sfx(&audio, MENU_MOVE_SFX_PATH, MENU_SELECT_SFX_PATH);
	audio_load_game_sfx(&audio);
	app_fixture_provider_init(&provider);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	auth_form_init(&auth_form, AUTH_FORM_LOGIN);
	while (navigation.current != APP_SCREEN_QUIT)
	{
		if (navigation.current == APP_SCREEN_ENTRY)
		{
			(void)app_navigation_dispatch(&navigation, APP_NAV_OPEN_LOGIN);
			continue ;
		}
		if (navigation.current == APP_SCREEN_LOGIN
			|| navigation.current == APP_SCREEN_SIGN_UP)
		{
			if (run_auth_flow(&ctx, &audio, &provider, &navigation,
					&auth_form, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_SOLO)
		{
			if (solo_mode_run(&ctx, &audio) < 0)
			{
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
				continue ;
			}
			(void)app_navigation_dispatch(&navigation, APP_NAV_BACK);
			enable_home_mouse(&ctx);
			continue ;
		}
		if (navigation.current != APP_SCREEN_HOME)
		{
			if (run_scaffold_step(&ctx, &audio, &provider,
					&navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		key = render_wait_input(&ctx, &input);
		if (key == (uint32_t)-1)
			(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (reflow_home(&ctx, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
		}
		else if (key == NCKEY_UP || key == NCKEY_DOWN)
		{
			menu_move_selection(&menu, key);
			audio_play_menu_move(&audio);
			render_menu_move_bunny(&ctx, &menu);
		}
		else if (nckey_mouse_p(key)
			&& render_menu_hit_test(&ctx, &input, &hovered))
		{
			if (menu.selected != hovered)
			{
				menu.selected = hovered;
				audio_play_menu_move(&audio);
				render_menu_move_bunny(&ctx, &menu);
			}
			if (key == NCKEY_BUTTON1
				&& (input.evtype == NCTYPE_PRESS
					|| input.evtype == NCTYPE_UNKNOWN))
				(void)activate_menu_selection(&audio, &navigation, &menu);
		}
		else if (key == NCKEY_ENTER || key == '\n')
			(void)activate_menu_selection(&audio, &navigation, &menu);
		else if (key == '+' || key == '=')
		{
			audio_volume_up(&audio);
			render_notification_show_volume(&ctx, audio.music_volume);
		}
		else if (key == '-' || key == '_')
		{
			audio_volume_down(&audio);
			render_notification_show_volume(&ctx, audio.music_volume);
		}
		else if (key == 'q' || key == 'Q')
			(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
		else if (key == NCKEY_ESC)
		{
			if (app_navigation_dispatch(&navigation, APP_NAV_BACK))
				render_menu_destroy(&ctx);
		}
	}
	audio_teardown(&audio);
	render_screen_destroy(&ctx);
	render_menu_destroy(&ctx);
	render_background_destroy(&ctx);
	render_teardown(&ctx);
	return (0);
}

/**
 * @brief Runs the complete login/sign-up/offline entry experience.
 */
static int	run_auth_flow(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	auth_form_t *form, const menu_selection_t *menu)
{
	ncinput			input;
	uint32_t		key;
	auth_action_t	action;
	bool			rebuild;

	if (navigation->current == APP_SCREEN_SIGN_UP
		&& form->mode != AUTH_FORM_SIGN_UP)
		auth_form_set_mode(form, AUTH_FORM_SIGN_UP);
	else if (navigation->current == APP_SCREEN_LOGIN
		&& form->mode != AUTH_FORM_LOGIN)
		auth_form_set_mode(form, AUTH_FORM_LOGIN);
	(void)notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS);
	rebuild = true;
	while (navigation->current == APP_SCREEN_LOGIN
		|| navigation->current == APP_SCREEN_SIGN_UP)
	{
		if (!render_auth_show(ctx, form, rebuild))
			return (-1);
		rebuild = false;
		key = render_wait_input(ctx, &input);
		action = AUTH_ACTION_NONE;
		if (key == (uint32_t)-1)
			action = AUTH_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0)
				return (-1);
			rebuild = true;
		}
		else if (nckey_mouse_p(key))
			action = auth_pointer_action(ctx, audio, form, &input, key);
		else
			action = auth_form_handle_key(form, key);
		if (action != AUTH_ACTION_NONE
			&& !apply_auth_action(ctx, audio, provider, navigation,
				form, action))
			return (-1);
	}
	render_auth_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Turns pointer hover and click into the shared form focus/action path.
 */
static auth_action_t	auth_pointer_action(render_ctx_t *ctx,
	audio_ctx_t *audio, auth_form_t *form, const ncinput *input, uint32_t key)
{
	auth_focus_t	focus;

	if (!render_auth_hit_test(ctx, form, input, &focus))
		return (AUTH_ACTION_NONE);
	if (form->focus != focus)
	{
		form->focus = focus;
		audio_play_menu_move(audio);
	}
	if (key == NCKEY_BUTTON1 && (input->evtype == NCTYPE_PRESS
			|| input->evtype == NCTYPE_UNKNOWN))
		return (auth_form_handle_key(form, NCKEY_ENTER));
	return (AUTH_ACTION_NONE);
}

/**
 * @brief Applies one semantic auth action to providers and screen navigation.
 */
static bool	apply_auth_action(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	auth_form_t *form, auth_action_t action)
{
	app_auth_view_model_t	view;
	app_provider_result_t	result;

	if (action == AUTH_ACTION_CHECK_SERVER)
	{
		audio_play_menu_select(audio);
		if (!render_auth_show(ctx, form, false))
			return (false);
		/*
		 * The network adapter is intentionally not connected yet. Keeping the
		 * check as a semantic action lets it become asynchronous later without
		 * changing the form, focus, or rendering contract.
		 */
		auth_form_finish_server_check(form, false);
		return (true);
	}
	if (action == AUTH_ACTION_SUBMIT_LOGIN
		|| action == AUTH_ACTION_SUBMIT_SIGN_UP)
	{
		audio_play_menu_select(audio);
		if (!render_auth_show(ctx, form, false))
			return (false);
		memset(&view, 0, sizeof(view));
		result = auth_form_submit(form, provider, &view);
		if (result != APP_PROVIDER_OK)
			return (true);
		if (action == AUTH_ACTION_SUBMIT_LOGIN)
			return (app_navigation_dispatch(navigation,
					APP_NAV_AUTHENTICATED));
		(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		auth_form_set_mode(form, AUTH_FORM_LOGIN);
		form->feedback = AUTH_FEEDBACK_SUCCESS;
		snprintf(form->status, sizeof(form->status),
			"ACCOUNT CREATED - PLEASE SIGN IN");
		return (true);
	}
	audio_play_menu_select(audio);
	if (action == AUTH_ACTION_OPEN_SIGN_UP)
	{
		if (!app_navigation_dispatch(navigation, APP_NAV_OPEN_SIGN_UP))
			return (false);
		auth_form_set_mode(form, AUTH_FORM_SIGN_UP);
	}
	else if (action == AUTH_ACTION_OPEN_LOGIN)
	{
		if (!app_navigation_dispatch(navigation, APP_NAV_BACK))
			return (false);
		auth_form_set_mode(form, AUTH_FORM_LOGIN);
	}
	else if (action == AUTH_ACTION_PLAY_OFFLINE)
		return (app_navigation_dispatch(navigation, APP_NAV_PLAY_OFFLINE));
	else if (action == AUTH_ACTION_QUIT)
		return (app_navigation_dispatch(navigation, APP_NAV_QUIT));
	return (true);
}

/**
 * @brief Routes one home selection into the screen graph.
 */
static bool	activate_menu_selection(audio_ctx_t *audio,
	app_navigation_t *navigation, const menu_selection_t *menu)
{
	app_nav_action_t	action;

	if (navigation == NULL || menu == NULL)
		return (false);
	action = menu_navigation_action(menu->selected);
	if (action == APP_NAV_NONE)
		return (false);
	audio_play_menu_select(audio);
	return (app_navigation_dispatch(navigation, action));
}

/**
 * @brief Maps the fixed five-item home order to typed navigation actions.
 */
static app_nav_action_t	menu_navigation_action(int selected)
{
	if (selected == 0)
		return (APP_NAV_OPEN_SOLO);
	if (selected == 1)
		return (APP_NAV_OPEN_LOBBY);
	if (selected == 2)
		return (APP_NAV_OPEN_MARKETPLACE);
	if (selected == 3)
		return (APP_NAV_OPEN_LEADERBOARD);
	if (selected == 4)
		return (APP_NAV_OPEN_SETTINGS);
	return (APP_NAV_NONE);
}

/**
 * @brief Presents and advances one scaffolded item-15 screen.
 */
static int	run_scaffold_step(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	const menu_selection_t *menu)
{
	app_screen_view_model_t	view;
	app_nav_action_t		action;
	ncinput					input;
	uint32_t				key;

	if (app_screen_view_load(provider, navigation->current, &view)
		== APP_PROVIDER_INVALID || !render_screen_show(ctx, &view))
		return (-1);
	key = render_wait_input(ctx, &input);
	if (key == (uint32_t)-1)
		action = APP_NAV_QUIT;
	else if (key == NCKEY_RESIZE || key == 12u)
	{
		if (render_geometry_refresh(ctx, true) < 0)
			return (-1);
		return (0);
	}
	else if (key == '+' || key == '=')
	{
		audio_volume_up(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		return (0);
	}
	else if (key == '-' || key == '_')
	{
		audio_volume_down(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		return (0);
	}
	else
		action = scaffold_navigation_action(navigation->current, key);
	if (action == APP_NAV_NONE)
		return (0);
	audio_play_menu_select(audio);
	if (!app_navigation_dispatch(navigation, action))
		return (0);
	if (navigation->current == APP_SCREEN_HOME)
	{
		render_screen_destroy(ctx);
		if (reflow_home(ctx, menu) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Maps temporary scaffold controls to the validated state graph.
 */
static app_nav_action_t	scaffold_navigation_action(app_screen_t screen,
	uint32_t key)
{
	if (key == 'q' || key == 'Q')
		return (APP_NAV_QUIT);
	if (key == NCKEY_ESC)
		return (APP_NAV_BACK);
	if (screen == APP_SCREEN_ENTRY && (key == 'l' || key == 'L'))
		return (APP_NAV_OPEN_LOGIN);
	if (screen == APP_SCREEN_ENTRY && (key == 's' || key == 'S'))
		return (APP_NAV_OPEN_SIGN_UP);
	if (screen == APP_SCREEN_ENTRY && (key == 'o' || key == 'O'))
		return (APP_NAV_PLAY_OFFLINE);
	if ((screen == APP_SCREEN_LOGIN || screen == APP_SCREEN_SIGN_UP)
		&& (key == NCKEY_ENTER || key == '\n'))
		return (APP_NAV_AUTHENTICATED);
	if (screen == APP_SCREEN_LOBBY
		&& (key == NCKEY_ENTER || key == '\n'))
		return (APP_NAV_OPEN_CREATE_ROOM);
	if (screen == APP_SCREEN_CREATE_ROOM_MODAL
		&& (key == NCKEY_ENTER || key == '\n'))
		return (APP_NAV_OPEN_WAITING_ROOM);
	if (screen == APP_SCREEN_WAITING_ROOM && (key == 'd' || key == 'D'))
		return (APP_NAV_START_DOUBLE);
	if (screen == APP_SCREEN_WAITING_ROOM && (key == 'b' || key == 'B'))
		return (APP_NAV_START_BATTLE_ROYALE);
	return (APP_NAV_NONE);
}

/**
 * @brief Enables pointer movement and click reporting for the home menu.
 */
static void	enable_home_mouse(render_ctx_t *ctx)
{
	(void)notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS);
}

/**
 * @brief Rebuilds the home screen after resize or scaffold navigation.
 */
static int	reflow_home(render_ctx_t *ctx, const menu_selection_t *menu)
{
	render_screen_destroy(ctx);
	render_menu_destroy(ctx);
	if (render_geometry_refresh(ctx, true) < 0
		|| render_background_replace(ctx, SPLASH_ASSET_PATH, false) < 0)
		return (-1);
	render_menu_create(ctx);
	render_menu_move_bunny(ctx, menu);
	render_notification_reflow(ctx);
	return (0);
}
