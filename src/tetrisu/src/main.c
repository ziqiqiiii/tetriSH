#include "tetrisu.h"

# define SETTINGS_INPUT_BATCH_MAX	64
# define LEADERBOARD_INPUT_BATCH_MAX	64
# define MARKETPLACE_INPUT_BATCH_MAX	64

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
static int	activate_menu_selection(render_ctx_t *ctx, audio_ctx_t *audio,
				app_navigation_t *navigation,
				const menu_selection_t *menu,
				sign_in_modal_t *modal);
static int	run_sign_in_modal(render_ctx_t *ctx, audio_ctx_t *audio,
				app_navigation_t *navigation,
				const menu_selection_t *menu,
				sign_in_modal_t *modal);
static int	run_leaderboard_screen(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, const menu_selection_t *menu);
static int	run_settings_screen(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, const menu_selection_t *menu);
static int	run_marketplace_screen(render_ctx_t *ctx, audio_ctx_t *audio,
				const app_data_provider_t *provider,
				app_navigation_t *navigation, const menu_selection_t *menu);
static bool	apply_marketplace_purchase(render_ctx_t *ctx, audio_ctx_t *audio,
				app_screen_view_model_t *view, marketplace_state_t *state);
static bool	apply_marketplace_equip(render_ctx_t *ctx, audio_ctx_t *audio,
				app_screen_view_model_t *view, marketplace_state_t *state);
static void	discard_queued_input(render_ctx_t *ctx);
static void	leaderboard_loading_view(const app_data_provider_t *provider,
				app_screen_view_model_t *view);
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
	sign_in_modal_t	sign_in;
	render_ctx_t		ctx;
	audio_ctx_t			audio;
	ncinput				input;
	uint32_t			key;
	int					hovered;

	menu.selected = 0;
	sign_in_modal_init(&sign_in);
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
		if (navigation.current == APP_SCREEN_LEADERBOARD)
		{
			if (run_leaderboard_screen(&ctx, &audio, &provider,
					&navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_SETTINGS)
		{
			if (run_settings_screen(&ctx, &audio, &provider,
					&navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_MARKETPLACE)
		{
			if (run_marketplace_screen(&ctx, &audio, &provider,
					&navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
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
		if (input.evtype == NCTYPE_RELEASE && !nckey_mouse_p(key))
			continue ;
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
			{
				if (activate_menu_selection(&ctx, &audio, &navigation,
						&menu, &sign_in) < 0)
					(void)app_navigation_dispatch(&navigation,
						APP_NAV_QUIT);
			}
		}
		else if (key == NCKEY_ENTER || key == '\n' || key == '\r')
		{
			if (activate_menu_selection(&ctx, &audio, &navigation,
					&menu, &sign_in) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
		}
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
	render_sign_in_destroy(&ctx, &sign_in);
	render_marketplace_destroy(&ctx);
	render_settings_destroy(&ctx);
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
	int				drained;

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
		drained = 0;
		while (action == AUTH_ACTION_NONE && !rebuild && drained < 64)
		{
			memset(&input, 0, sizeof(input));
			key = notcurses_get_nblock(ctx->nc, &input);
			if (key == 0)
				break ;
			if (key == (uint32_t)-1)
				action = AUTH_ACTION_QUIT;
			else if (input.evtype == NCTYPE_RELEASE
				&& !nckey_mouse_p(key))
			{
				drained++;
				continue ;
			}
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
			drained++;
		}
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
	if (action == AUTH_ACTION_PREVIEW_LOGIN)
	{
		audio_play_menu_select(audio);
		memset(&view, 0, sizeof(view));
		result = app_provider_preview_sign_in(provider, &view);
		if (result != APP_PROVIDER_OK)
		{
			form->feedback = AUTH_FEEDBACK_ERROR;
			snprintf(form->status, sizeof(form->status),
				"PREVIEW GATE UNAVAILABLE - SET TETRISU_UI_PREVIEW=1");
			return (true);
		}
		form->feedback = AUTH_FEEDBACK_SUCCESS;
		snprintf(form->status, sizeof(form->status),
			"LOCAL UI PREVIEW SIGNED IN");
		return (app_navigation_dispatch(navigation, APP_NAV_AUTHENTICATED));
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
 * @brief Routes one home selection using the pure routing policy.
 *
 * Returns 0 on success (including showing the modal), -1 on fatal error.
 * When the route is HOME_ROUTE_SIGN_IN_REQUIRED the sign-in modal loop runs
 * blocking until the user dismisses or goes to Login.
 */
static int	activate_menu_selection(render_ctx_t *ctx, audio_ctx_t *audio,
	app_navigation_t *navigation, const menu_selection_t *menu,
	sign_in_modal_t *modal)
{
	home_route_t	route;

	if (navigation == NULL || menu == NULL)
		return (0);
	route = home_menu_route(menu->selected, navigation->offline);
	if (route.action == HOME_ROUTE_BLOCKED)
		return (0);
	audio_play_menu_select(audio);
	if (route.action == HOME_ROUTE_NAVIGATE)
	{
		(void)app_navigation_dispatch(navigation, route.nav_action);
		return (0);
	}
	modal->label = route.label;
	modal->focus = SIGN_IN_FOCUS_DISMISS;
	modal->visible = true;
	return (run_sign_in_modal(ctx, audio, navigation, menu, modal));
}

/**
 * @brief Runs the blocking sign-in-required modal loop.
 *
 * Blocks on input until the user dismisses or navigates to Login. Handles
 * resize by destroying and recreating modal planes. Returns 0 on normal
 * exit, -1 on fatal error.
 */
static int	run_sign_in_modal(render_ctx_t *ctx, audio_ctx_t *audio,
	app_navigation_t *navigation, const menu_selection_t *menu,
	sign_in_modal_t *modal)
{
	ncinput			input;
	uint32_t		key;
	sign_in_result_t	result;
	sign_in_focus_t		old_focus;

	if (!render_sign_in_show(ctx, modal))
		return (-1);
	while (modal->visible)
	{
		key = render_wait_input(ctx, &input);
		result = SIGN_IN_RESULT_NONE;
		old_focus = modal->focus;
		if (key == (uint32_t)-1)
		{
			render_sign_in_destroy(ctx, modal);
			return (-1);
		}
		if (key == NCKEY_RESIZE || key == 12u)
		{
			render_sign_in_destroy(ctx, modal);
			if (reflow_home(ctx, menu) < 0)
				return (-1);
			modal->visible = true;
			if (!render_sign_in_show(ctx, modal))
				return (-1);
			continue ;
		}
		if (nckey_mouse_p(key))
			result = sign_in_modal_handle_mouse(modal, ctx, &input, key);
		else
			result = sign_in_modal_handle_key(modal, key);
		if (result == SIGN_IN_RESULT_DISMISS)
		{
			render_sign_in_destroy(ctx, modal);
			(void)notcurses_render(ctx->nc);
			return (0);
		}
		if (result == SIGN_IN_RESULT_LOGIN)
		{
			render_sign_in_destroy(ctx, modal);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
			return (0);
		}
		if (modal->focus != old_focus)
		{
			audio_play_menu_move(audio);
			if (!render_sign_in_refresh(ctx, modal))
				return (-1);
		}
	}
	return (0);
}

/**
 * @brief Runs the dedicated leaderboard, including refresh and pointer input.
 */
static int	run_leaderboard_screen(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	const menu_selection_t *menu)
{
	app_screen_view_model_t	view;
	leaderboard_state_t		state;
	leaderboard_focus_t		hovered;
	leaderboard_focus_t		old_focus;
	leaderboard_action_t	action;
	app_provider_result_t	result;
	ncinput					input;
	ncinput					queued_input;
	ncinput					pending_input;
	uint32_t				key;
	uint32_t				queued_key;
	uint32_t				pending_key;
	int						drained;
	bool					has_pending;
	bool					refresh;
	bool					rebuild_background;

	leaderboard_state_init(&state);
	(void)notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS);
	refresh = true;
	rebuild_background = true;
	has_pending = false;
	while (navigation->current == APP_SCREEN_LEADERBOARD)
	{
		if (refresh)
		{
			leaderboard_loading_view(provider, &view);
			if (!render_leaderboard_show(ctx, &view, &state,
					rebuild_background))
				return (-1);
			rebuild_background = false;
			result = app_screen_view_load(provider, APP_SCREEN_LEADERBOARD,
					&view);
			if (result == APP_PROVIDER_INVALID
				|| !render_leaderboard_show(ctx, &view, &state, false))
				return (-1);
			refresh = false;
		}
		if (has_pending)
		{
			input = pending_input;
			key = pending_key;
			has_pending = false;
		}
		else
			key = render_wait_input(ctx, &input);
		action = LEADERBOARD_ACTION_NONE;
		old_focus = state.focus;
		if (input.evtype == NCTYPE_RELEASE && !nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = LEADERBOARD_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_leaderboard_show(ctx, &view, &state, true))
				return (-1);
			render_notification_reflow(ctx);
			continue ;
		}
		else if (key == '+' || key == '=')
		{
			audio_volume_up(audio);
			render_notification_show_volume(ctx, audio->music_volume);
			continue ;
		}
		else if (key == '-' || key == '_')
		{
			audio_volume_down(audio);
			render_notification_show_volume(ctx, audio->music_volume);
			continue ;
		}
		else if (nckey_mouse_p(key)
			&& render_leaderboard_hit_test(ctx, &input, &hovered))
		{
			leaderboard_set_focus(&state, hovered);
			if (key == NCKEY_BUTTON1 && (input.evtype == NCTYPE_PRESS
					|| input.evtype == NCTYPE_UNKNOWN))
				action = leaderboard_handle_key(&state, NCKEY_ENTER);
		}
		else if (!nckey_mouse_p(key))
			action = leaderboard_handle_key(&state, key);
		/* Collapse a held navigation key into one repaint, but keep the first
		 * different command for the next loop so its target state is painted
		 * before it is acted on. */
		drained = 0;
		while (action == LEADERBOARD_ACTION_NONE
			&& leaderboard_navigation_keys_coalesce(key, key)
			&& drained < LEADERBOARD_INPUT_BATCH_MAX)
		{
			memset(&queued_input, 0, sizeof(queued_input));
			queued_key = notcurses_get_nblock(ctx->nc, &queued_input);
			if (queued_key == 0)
				break ;
			drained++;
			if (queued_input.evtype == NCTYPE_RELEASE
				|| nckey_mouse_p(queued_key))
				continue ;
			if (!leaderboard_navigation_keys_coalesce(key, queued_key))
			{
				pending_input = queued_input;
				pending_key = queued_key;
				has_pending = true;
				break ;
			}
			(void)leaderboard_handle_key(&state, queued_key);
		}
		if (state.focus != old_focus)
		{
			audio_play_menu_move(audio);
			if (!render_leaderboard_show(ctx, &view, &state, false))
				return (-1);
		}
		if (action == LEADERBOARD_ACTION_REFRESH)
		{
			audio_play_menu_select(audio);
			refresh = true;
		}
		else if (action == LEADERBOARD_ACTION_BACK)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == LEADERBOARD_ACTION_QUIT)
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_leaderboard_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Creates the visible loading model before a provider refresh.
 */
static void	leaderboard_loading_view(const app_data_provider_t *provider,
	app_screen_view_model_t *view)
{
	memset(view, 0, sizeof(*view));
	view->screen = APP_SCREEN_LEADERBOARD;
	view->status = APP_DATA_LOADING;
	view->local_preview = provider != NULL && provider->local_fixtures;
	snprintf(view->title, sizeof(view->title), "Leaderboard");
	snprintf(view->subtitle, sizeof(view->subtitle),
		"Fetching the latest scores");
}

/**
 * @brief Runs the responsive Settings/Profile surface and local controls.
 */
static int	run_settings_screen(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	const menu_selection_t *menu)
{
	app_screen_view_model_t	view;
	settings_state_t		state;
	settings_state_t		previous;
	settings_action_t		action;
	settings_equip_result_t	equip_result;
	app_provider_result_t	result;
	ncinput				input;
	ncinput				queued_input;
	ncinput				pending_input;
	uint32_t			key;
	uint32_t			queued_key;
	uint32_t			batch_key;
	uint32_t			pending_key;
	int					drained;
	bool				repaint;
	bool				has_pending;

	result = app_screen_view_load_for_session(provider, APP_SCREEN_SETTINGS,
		navigation->offline, &view);
	if (result == APP_PROVIDER_INVALID)
		return (-1);
	app_settings_apply_local_controls(&view.data.settings,
		audio->music_volume, tetrisu_renderer_mode_requested());
	settings_state_init(&state, view.data.settings.signed_in,
		settings_catalogue_count(&view.data.settings.characters,
			SETTINGS_CHARACTER_SLOTS),
		settings_catalogue_count(&view.data.settings.themes,
			SETTINGS_THEME_SLOTS));
	/*
	 * Settings is keyboard-only: pointer reporting is switched off for the
	 * whole screen so no motion, drag, or click stream can reach it, and the
	 * screens that do use the pointer re-enable it when they are entered.
	 */
	(void)notcurses_mice_disable(ctx->nc);
	if (!render_settings_show(ctx, &view, &state, true))
		return (-1);
	has_pending = false;
	while (navigation->current == APP_SCREEN_SETTINGS)
	{
		if (has_pending)
		{
			input = pending_input;
			key = pending_key;
			has_pending = false;
		}
		else
			key = render_wait_input(ctx, &input);
		action = SETTINGS_ACTION_NONE;
		previous = state;
		repaint = false;
		/*
		 * Pointer reporting is disabled here, but a terminal can still deliver
		 * events queued before the disable sequence was written, so mouse keys
		 * are dropped rather than routed anywhere.
		 */
		if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = SETTINGS_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_settings_show(ctx, &view, &state, false))
				return (-1);
			continue ;
		}
		else
			action = settings_handle_key(&state, key);
		/* Coalesce one held navigation key, but preserve the first different
		 * command for the next loop so its target state is painted first. */
		batch_key = key;
		drained = 0;
		while (action == SETTINGS_ACTION_NONE
			&& settings_navigation_keys_coalesce(batch_key, batch_key)
			&& drained < SETTINGS_INPUT_BATCH_MAX)
		{
			memset(&queued_input, 0, sizeof(queued_input));
			queued_key = notcurses_get_nblock(ctx->nc, &queued_input);
			if (queued_key == 0)
				break ;
			drained++;
			if (queued_input.evtype == NCTYPE_RELEASE
				|| nckey_mouse_p(queued_key))
				continue ;
			if (!settings_navigation_keys_coalesce(batch_key, queued_key))
			{
				pending_input = queued_input;
				pending_key = queued_key;
				has_pending = true;
				break ;
			}
			(void)settings_handle_key(&state, queued_key);
		}
		if (settings_state_view_changed(&previous, &state))
		{
			audio_play_menu_move(audio);
			repaint = true;
		}
		if (repaint && !render_settings_show(ctx, &view, &state, false))
			return (-1);
		if (settings_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (action == SETTINGS_ACTION_VOLUME_UP)
		{
			audio_play_menu_select(audio);
			audio_volume_up(audio);
			view.data.settings.music_volume = audio->music_volume;
			render_notification_queue_volume(ctx, audio->music_volume);
			if (!render_settings_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == SETTINGS_ACTION_VOLUME_DOWN)
		{
			audio_play_menu_select(audio);
			audio_volume_down(audio);
			view.data.settings.music_volume = audio->music_volume;
			render_notification_queue_volume(ctx, audio->music_volume);
			if (!render_settings_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == SETTINGS_ACTION_CHARACTER_PREVIOUS
			|| action == SETTINGS_ACTION_CHARACTER_NEXT)
		{
			audio_play_menu_select(audio);
			if (settings_select_character(&view.data.settings,
					action == SETTINGS_ACTION_CHARACTER_NEXT ? 1 : -1)
				&& !render_settings_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == SETTINGS_ACTION_EQUIP_CHARACTER)
		{
			audio_play_menu_select(audio);
			equip_result = settings_equip_character_slot(&view.data.settings,
					state.character_slot);
			if (equip_result == SETTINGS_EQUIP_LOCKED)
				render_notification_queue_ownership(ctx);
			if ((equip_result == SETTINGS_EQUIP_CHANGED
					|| equip_result == SETTINGS_EQUIP_LOCKED)
				&& !render_settings_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == SETTINGS_ACTION_EQUIP_THEME)
		{
			audio_play_menu_select(audio);
			equip_result = settings_equip_theme_slot(&view.data.settings,
					state.theme_slot);
			if (equip_result == SETTINGS_EQUIP_LOCKED)
				render_notification_queue_ownership(ctx);
			if ((equip_result == SETTINGS_EQUIP_CHANGED
					|| equip_result == SETTINGS_EQUIP_LOCKED)
				&& !render_settings_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == SETTINGS_ACTION_BACK)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == SETTINGS_ACTION_MARKETPLACE)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation,
				APP_NAV_OPEN_MARKETPLACE);
		}
		else if (action == SETTINGS_ACTION_QUIT)
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_settings_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Runs the Marketplace: browse the shelves, spend points, equip stock.
 */
static int	run_marketplace_screen(render_ctx_t *ctx, audio_ctx_t *audio,
	const app_data_provider_t *provider, app_navigation_t *navigation,
	const menu_selection_t *menu)
{
	app_screen_view_model_t	view;
	marketplace_state_t		state;
	marketplace_state_t		previous;
	marketplace_action_t	action;
	app_provider_result_t	result;
	ncinput					input;
	ncinput					queued_input;
	ncinput					pending_input;
	uint32_t				key;
	uint32_t				queued_key;
	uint32_t				pending_key;
	int						drained;
	bool					repaint;
	bool					has_pending;

	result = app_screen_view_load_for_session(provider, APP_SCREEN_MARKETPLACE,
			navigation->offline, &view);
	if (result == APP_PROVIDER_INVALID)
		return (-1);
	marketplace_state_init(&state, view.data.marketplace.signed_in
		&& !view.data.marketplace.offline,
		settings_catalogue_count(&view.data.marketplace.characters,
			MARKETPLACE_CHARACTER_SLOTS),
		settings_catalogue_count(&view.data.marketplace.themes,
			MARKETPLACE_THEME_SLOTS));
	/*
	 * The Marketplace is keyboard-only for the same reason Settings is:
	 * pointer reporting is switched off for the whole screen so no motion,
	 * drag, or stray click can reach a control that spends points.
	 */
	(void)notcurses_mice_disable(ctx->nc);
	if (!render_marketplace_show(ctx, &view, &state, true))
		return (-1);
	has_pending = false;
	while (navigation->current == APP_SCREEN_MARKETPLACE)
	{
		if (has_pending)
		{
			input = pending_input;
			key = pending_key;
			has_pending = false;
		}
		else
			key = render_wait_input(ctx, &input);
		action = MARKETPLACE_ACTION_NONE;
		previous = state;
		repaint = false;
		if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = MARKETPLACE_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_marketplace_show(ctx, &view, &state, false))
				return (-1);
			continue ;
		}
		else
			action = marketplace_handle_key(&state, key);
		/* Coalesce one held navigation key, but preserve the first different
		 * command for the next loop so its target state is painted first. */
		drained = 0;
		while (action == MARKETPLACE_ACTION_NONE
			&& marketplace_navigation_keys_coalesce(key, key)
			&& drained < MARKETPLACE_INPUT_BATCH_MAX)
		{
			memset(&queued_input, 0, sizeof(queued_input));
			queued_key = notcurses_get_nblock(ctx->nc, &queued_input);
			if (queued_key == 0)
				break ;
			drained++;
			if (queued_input.evtype == NCTYPE_RELEASE
				|| nckey_mouse_p(queued_key))
				continue ;
			if (!marketplace_navigation_keys_coalesce(key, queued_key))
			{
				pending_input = queued_input;
				pending_key = queued_key;
				has_pending = true;
				break ;
			}
			(void)marketplace_handle_key(&state, queued_key);
		}
		if (marketplace_state_view_changed(&previous, &state))
		{
			audio_play_menu_move(audio);
			repaint = true;
		}
		if (repaint && !render_marketplace_show(ctx, &view, &state, false))
			return (-1);
		if (marketplace_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (action == MARKETPLACE_ACTION_VOLUME_UP
			|| action == MARKETPLACE_ACTION_VOLUME_DOWN)
		{
			audio_play_menu_select(audio);
			if (action == MARKETPLACE_ACTION_VOLUME_UP)
				audio_volume_up(audio);
			else
				audio_volume_down(audio);
			view.data.marketplace.music_volume = audio->music_volume;
			/*
			 * Reported on the control row rather than through the shared
			 * volume card. That card is a plane raised over the screen, and
			 * on a stationary protocol raising one forces the full-screen
			 * bitmap to be retransmitted over every region plane here.
			 */
			marketplace_set_feedback(&state, MARKETPLACE_FEEDBACK_VOLUME,
				ui_notification_volume_percent(audio->music_volume));
			if (!render_marketplace_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == MARKETPLACE_ACTION_BUY)
		{
			if (!apply_marketplace_purchase(ctx, audio, &view, &state))
				return (-1);
		}
		else if (action == MARKETPLACE_ACTION_EQUIP)
		{
			if (!apply_marketplace_equip(ctx, audio, &view, &state))
				return (-1);
		}
		else if (action == MARKETPLACE_ACTION_BACK)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == MARKETPLACE_ACTION_QUIT)
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_marketplace_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Applies one purchase and reports its outcome on the notification card.
 *
 * Enter inside a grid means "act on this item", so an already-owned item is
 * equipped rather than refused: that is what the shelf tile and the Buy button
 * caption both promise.
 */
static bool	apply_marketplace_purchase(render_ctx_t *ctx, audio_ctx_t *audio,
	app_screen_view_model_t *view, marketplace_state_t *state)
{
	const app_catalogue_item_view_model_t	*item;
	marketplace_purchase_result_t			result;

	item = marketplace_focused_item(&view->data.marketplace, state);
	if (item == NULL)
		return (true);
	if (item->owned)
		return (apply_marketplace_equip(ctx, audio, view, state));
	result = marketplace_buy_focused(&view->data.marketplace, state);
	if (result == MARKETPLACE_PURCHASE_INVALID)
		return (true);
	audio_play_menu_select(audio);
	if (result == MARKETPLACE_PURCHASE_BOUGHT)
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_BOUGHT, 0);
	else if (result == MARKETPLACE_PURCHASE_INSUFFICIENT)
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_INSUFFICIENT, 0);
	else
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_OWNED, 0);
	return (render_marketplace_show(ctx, view, state, false));
}

/**
 * @brief Equips the focused item when it is owned, or explains why it is not.
 */
static bool	apply_marketplace_equip(render_ctx_t *ctx, audio_ctx_t *audio,
	app_screen_view_model_t *view, marketplace_state_t *state)
{
	settings_equip_result_t	result;

	if (marketplace_focused_item(&view->data.marketplace, state) == NULL)
		return (true);
	result = marketplace_equip_focused(&view->data.marketplace, state);
	if (result != SETTINGS_EQUIP_CHANGED && result != SETTINGS_EQUIP_LOCKED)
		return (true);
	audio_play_menu_select(audio);
	if (result == SETTINGS_EQUIP_CHANGED)
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_EQUIPPED, 0);
	else
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_LOCKED, 0);
	return (render_marketplace_show(ctx, view, state, false));
}

/**
 * @brief Drops Settings input already buffered across a screen transition.
 */
static void	discard_queued_input(render_ctx_t *ctx)
{
	ncinput		input;
	uint32_t	key;

	while (true)
	{
		memset(&input, 0, sizeof(input));
		key = notcurses_get_nblock(ctx->nc, &input);
		if (key == 0 || key == (uint32_t)-1)
			return ;
	}
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

	if (app_screen_view_load_for_session(provider, navigation->current,
		navigation->offline, &view)
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
		&& (key == NCKEY_ENTER || key == '\n' || key == '\r'))
		return (APP_NAV_AUTHENTICATED);
	if (screen == APP_SCREEN_LOBBY
		&& (key == NCKEY_ENTER || key == '\n' || key == '\r'))
		return (APP_NAV_OPEN_CREATE_ROOM);
	if (screen == APP_SCREEN_CREATE_ROOM_MODAL
		&& (key == NCKEY_ENTER || key == '\n' || key == '\r'))
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
