#include "tetrisu.h"

# define SETTINGS_INPUT_BATCH_MAX	64
# define LEADERBOARD_INPUT_BATCH_MAX	64
# define MARKETPLACE_INPUT_BATCH_MAX	64
# define MULTIPLAYER_INPUT_BATCH_MAX	64
# define DISCARD_INPUT_BATCH_MAX		256

/*
 * What the four multiplayer screens hand to each other. The picker chooses the
 * mode, the lobby or the create-room panel chooses the room, and the waiting
 * room is the only one that holds a model across keystrokes because its ready
 * flags and its transcript are edited in place rather than reloaded.
 */
typedef struct s_mp_session
{
	t_app_game_mode			mode;
	char					room_id[LOBBY_ROOM_ID_MAX];
	bool					create_pending;
	t_app_screen_view_model	room_view;
	t_waiting_room_state	room_state;
}	t_mp_session;

// Static Functions
static int	reflow_home(t_render_ctx *ctx, const t_menu_selection *menu,
				bool refresh_geometry, bool replace_background);
static int	run_auth_flow(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_auth_form *form,
				const t_menu_selection *menu);
static t_auth_action	auth_pointer_action(t_render_ctx *ctx,
				t_audio_ctx *audio, t_auth_form *form,
				const ncinput *input, uint32_t key);
static bool	apply_auth_action(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_auth_form *form,
				t_auth_action action);
static int	activate_menu_selection(t_render_ctx *ctx, t_audio_ctx *audio,
				t_app_navigation *navigation,
				const t_menu_selection *menu,
				t_sign_in_modal *modal);
static int	run_sign_in_modal(t_render_ctx *ctx, t_audio_ctx *audio,
				t_app_navigation *navigation,
				const t_menu_selection *menu,
				t_sign_in_modal *modal);
static int	run_leaderboard_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, const t_menu_selection *menu);
static int	run_settings_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, const t_menu_selection *menu);
static int	run_marketplace_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, const t_menu_selection *menu);
static bool	apply_marketplace_purchase(t_render_ctx *ctx, t_audio_ctx *audio,
				t_app_screen_view_model *view, t_marketplace_state *state);
static bool	apply_marketplace_equip(t_render_ctx *ctx, t_audio_ctx *audio,
				t_app_screen_view_model *view, t_marketplace_state *state);
static int	run_multiplayer_mode_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_mp_session *session);
static int	run_lobby_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_mp_session *session);
static void	apply_lobby_join(t_app_screen_view_model *view,
				t_lobby_state *state, t_app_navigation *navigation,
				t_mp_session *session, bool by_id);
static int	run_create_room_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_mp_session *session);
static int	run_waiting_room_screen(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, t_mp_session *session);
static bool	apply_room_action(t_render_ctx *ctx, t_audio_ctx *audio,
				t_app_navigation *navigation, t_mp_session *session,
				t_room_action action);
static bool	load_room_view(const t_app_data_provider *provider,
				t_mp_session *session);
static bool	is_multiplayer_screen(t_app_screen screen);
static int	leave_multiplayer(t_render_ctx *ctx, t_app_navigation *navigation,
				const t_menu_selection *menu);
static void	discard_queued_input(t_render_ctx *ctx);
static void	leaderboard_loading_view(const t_app_data_provider *provider,
				t_app_screen_view_model *view);
static int	run_scaffold_step(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
				t_app_navigation *navigation, const t_menu_selection *menu);
static t_app_nav_action	scaffold_navigation_action(t_app_screen screen,
				uint32_t key);
static void	enable_home_mouse(t_render_ctx *ctx);
static void	apply_domain_to_config(t_net_config *cfg, const char *domain);

/**
 * @brief Entry point for the screen-navigation and rendering loop.
 */
int	main(void)
{
	t_app_navigation	navigation;
	t_app_data_provider	provider;
	t_menu_selection	menu;
	t_auth_form		auth_form;
	t_sign_in_modal	sign_in;
	t_mp_session		mp_session;
	t_app_net_session	net_session;
	t_render_ctx		ctx;
	t_audio_ctx			audio;
	ncinput				input;
	const char			*match_preview;
	uint32_t			key;
	int					hovered;
	bool				direct_match_preview;

	menu.selected = 0;
	sign_in_modal_init(&sign_in);
	memset(&mp_session, 0, sizeof(mp_session));
	memset(&net_session, 0, sizeof(net_session));
	mp_session.mode = APP_GAME_MODE_DOUBLE;
	ctx = render_init(SPLASH_ASSET_PATH);
	audio_init(&audio);
	match_preview = getenv("TETRISU_MATCH_PREVIEW");
	direct_match_preview = match_preview != NULL
		&& (strcmp(match_preview, "double") == 0
			|| strcmp(match_preview, "battle") == 0);
	if (!direct_match_preview)
		render_intro_play(&ctx, &audio, INTRO_VIDEO_PATH, INTRO_AUDIO_PATH);
	audio_set_music_volume(&audio, HOME_BGM_START_VOLUME);
	audio_play_music(&audio, ctx.theme_assets.music);
	audio_load_menu_sfx(&audio, MENU_MOVE_SFX_PATH, MENU_SELECT_SFX_PATH);
	audio_load_game_sfx(&audio);
	if (getenv("TETRISU_NET") != NULL && getenv("TETRISU_NET")[0] != '\0')
		app_net_provider_init(&provider, &net_session);
	else
		app_fixture_provider_init(&provider);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	if (direct_match_preview)
	{
		navigation.current = strcmp(match_preview, "battle") == 0
			? APP_SCREEN_BATTLE_ROYALE : APP_SCREEN_DOUBLE;
		navigation.previous = APP_SCREEN_WAITING_ROOM;
		snprintf(mp_session.room_id, sizeof(mp_session.room_id),
			"KITTY-UI-PREVIEW");
	}
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
			t_net_client	*solo_net;

			solo_net = (!provider.local_fixtures && net_session.connected
					&& net_session.net.state >= NET_AUTHED)
				? &net_session.net : NULL;
			if (solo_mode_run(&ctx, &audio, solo_net) < 0)
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
		if (navigation.current == APP_SCREEN_MULTIPLAYER_MODE)
		{
			if (run_multiplayer_mode_screen(&ctx, &audio, &provider,
					&navigation, &mp_session) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			if (leave_multiplayer(&ctx, &navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_LOBBY)
		{
			if (run_lobby_screen(&ctx, &audio, &provider, &navigation,
					&mp_session) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			if (leave_multiplayer(&ctx, &navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_CREATE_ROOM_MODAL)
		{
			if (run_create_room_screen(&ctx, &audio, &provider, &navigation,
					&mp_session) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			if (leave_multiplayer(&ctx, &navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_WAITING_ROOM)
		{
			if (run_waiting_room_screen(&ctx, &audio, &provider, &navigation,
					&mp_session) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			if (leave_multiplayer(&ctx, &navigation, &menu) < 0)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			continue ;
		}
		if (navigation.current == APP_SCREEN_DOUBLE
			|| navigation.current == APP_SCREEN_BATTLE_ROYALE)
		{
			if (multiplayer_match_mode_run(&ctx, &audio, &provider,
					navigation.current == APP_SCREEN_DOUBLE
					? APP_GAME_MODE_DOUBLE : APP_GAME_MODE_BATTLE_ROYALE,
					mp_session.room_id,
					mp_session.room_view.screen == APP_SCREEN_WAITING_ROOM
					? &mp_session.room_view.data.room : NULL) < 0)
			{
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
				continue ;
			}
			if (direct_match_preview)
				(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
			else
				(void)app_navigation_dispatch(&navigation, APP_NAV_BACK);
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
			if (reflow_home(&ctx, &menu, true, true) < 0)
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
		else if ((key == 'q' || key == 'Q')
			&& confirmation_prompt_run(&ctx, &audio, CONFIRM_QUIT_APP))
			(void)app_navigation_dispatch(&navigation, APP_NAV_QUIT);
		else if (key == NCKEY_ESC)
		{
			if (app_navigation_dispatch(&navigation, APP_NAV_BACK))
				render_menu_destroy(&ctx);
		}
	}
	audio_teardown(&audio);
	render_sign_in_destroy(&ctx, &sign_in);
	render_multiplayer_destroy(&ctx);
	render_marketplace_destroy(&ctx);
	render_settings_destroy(&ctx);
	render_screen_destroy(&ctx);
	render_menu_destroy(&ctx);
	render_background_destroy(&ctx);
	render_teardown(&ctx);
	if (net_session.connected)
		net_disconnect(&net_session.net);
	return (0);
}

/**
 * @brief Runs the complete login/sign-up/offline entry experience.
 */
static int	run_auth_flow(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_auth_form *form, const t_menu_selection *menu)
{
	ncinput			input;
	uint32_t		key;
	t_auth_action	action;
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
		if (action == AUTH_ACTION_QUIT && (key == 'q' || key == 'Q')
			&& !confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP))
			action = AUTH_ACTION_NONE;
		if (action != AUTH_ACTION_NONE
			&& !apply_auth_action(ctx, audio, provider, navigation,
				form, action))
			return (-1);
	}
	render_auth_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu, false, true) < 0)
			return (-1);
		/*
		 * Signing in is the one transition a player waits on, so it is also the
		 * one they are most likely to have typed into. Anything queued was
		 * aimed at the form, not at the menu that replaces it; delivering it
		 * would open whatever Home item happened to be selected.
		 */
		discard_queued_input(ctx);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Turns pointer hover and click into the shared form focus/action path.
 */
static t_auth_action	auth_pointer_action(t_render_ctx *ctx,
	t_audio_ctx *audio, t_auth_form *form, const ncinput *input, uint32_t key)
{
	t_auth_focus	focus;

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
static bool	apply_auth_action(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_auth_form *form, t_auth_action action)
{
	t_app_auth_view_model	view;
	t_app_provider_result	result;

	if (action == AUTH_ACTION_CHECK_SERVER)
	{
		t_app_net_session	*session;

		audio_play_menu_select(audio);
		if (!render_auth_show(ctx, form, false))
			return (false);
		if (provider->local_fixtures || provider->userdata == NULL)
		{
			auth_form_finish_server_check(form, false);
			return (true);
		}
		session = (t_app_net_session *)provider->userdata;
		net_config_load(&session->cfg);
		if (form->domain[0] != '\0')
			apply_domain_to_config(&session->cfg, form->domain);
		if (net_connect(&session->net, &session->cfg) == 0)
		{
			session->connected = true;
			auth_form_finish_server_check(form, true);
		}
		else
		{
			session->connected = false;
			auth_form_finish_server_check(form, false);
		}
		return (true);
	}
	if (action == AUTH_ACTION_SUBMIT_LOGIN
		|| action == AUTH_ACTION_SUBMIT_SIGN_UP)
	{
		audio_play_menu_select(audio);
		/*
		 * Validating here rather than inside the submit is what makes the
		 * in-progress status visible: it is auth_form_validate() that sets
		 * "SIGNING IN...", and the only paint before the provider call happens
		 * on this side of it. Submitting first left the message written to a
		 * form nobody drew again until the call had already returned. The
		 * submit skips its own validation while the form reads as loading.
		 */
		if (!auth_form_validate(form))
			return (render_auth_show(ctx, form, false));
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
			"ACCOUNT CREATED - SIGN IN");
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
				"SET TETRISU_UI_PREVIEW=1");
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

/*
** Parses the typed auth domain field as "host" or "host:port" and overrides
** the resolved net config. An empty or whitespace-only value is ignored so
** env defaults stay in charge.
*/
static void	apply_domain_to_config(t_net_config *cfg, const char *domain)
{
	const char	*colon;
	size_t		host_len;

	if (cfg == NULL || domain == NULL || domain[0] == '\0')
		return ;
	colon = strchr(domain, ':');
	if (colon != NULL)
	{
		host_len = (size_t)(colon - domain);
		if (host_len == 0 || host_len >= sizeof(cfg->host))
			return ;
		memcpy(cfg->host, domain, host_len);
		cfg->host[host_len] = '\0';
		cfg->port = (int)strtol(colon + 1, NULL, 10);
		if (cfg->port < 1 || cfg->port > 65535)
			cfg->port = NET_DEFAULT_PORT;
		return ;
	}
	if (strnlen(domain, sizeof(cfg->host)) >= sizeof(cfg->host))
		return ;
	snprintf(cfg->host, sizeof(cfg->host), "%s", domain);
}

/**
 * @brief Routes one home selection using the pure routing policy.
 *
 * Returns 0 on success (including showing the modal), -1 on fatal error.
 * When the route is HOME_ROUTE_SIGN_IN_REQUIRED the sign-in modal loop runs
 * blocking until the user dismisses or goes to Login.
 */
static int	activate_menu_selection(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_navigation *navigation, const t_menu_selection *menu,
	t_sign_in_modal *modal)
{
	t_home_route	route;

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
static int	run_sign_in_modal(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_navigation *navigation, const t_menu_selection *menu,
	t_sign_in_modal *modal)
{
	ncinput			input;
	uint32_t		key;
	t_sign_in_result	result;
	t_sign_in_focus		old_focus;

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
			if (reflow_home(ctx, menu, false, true) < 0)
				return (-1);
			modal->visible = true;
			if (!render_sign_in_show(ctx, modal))
				return (-1);
			continue ;
		}
		if (key == 'q' || key == 'Q')
		{
			if (confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP))
			{
				render_sign_in_destroy(ctx, modal);
				(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
				return (0);
			}
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
static int	run_leaderboard_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	const t_menu_selection *menu)
{
	t_app_screen_view_model	view;
	t_leaderboard_state		state;
	t_leaderboard_focus		hovered;
	t_leaderboard_focus		old_focus;
	t_leaderboard_action	action;
	t_app_provider_result	result;
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
		if (leaderboard_action_leaves_screen(action))
			discard_queued_input(ctx);
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
		else if (action == LEADERBOARD_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_leaderboard_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu, false, true) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Creates the visible loading model before a provider refresh.
 */
static void	leaderboard_loading_view(const t_app_data_provider *provider,
	t_app_screen_view_model *view)
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
static int	run_settings_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	const t_menu_selection *menu)
{
	t_app_screen_view_model	view;
	t_settings_state		state;
	t_settings_state		previous;
	t_settings_action		action;
	t_settings_equip_result	equip_result;
	t_app_provider_result	result;
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
	tetrisu_visual_selection_sync(ctx, &view.data.settings);
	audio_transition_music(audio, ctx->theme_assets.music, 0);
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
				action == SETTINGS_ACTION_CHARACTER_NEXT ? 1 : -1))
			{
				tetrisu_character_apply(ctx,
					view.data.settings.profile.character);
				tetrisu_visual_selection_sync(ctx, &view.data.settings);
				if (!render_settings_show(ctx, &view, &state, false))
					return (-1);
			}
		}
		else if (action == SETTINGS_ACTION_EQUIP_CHARACTER)
		{
			audio_play_menu_select(audio);
			equip_result = settings_equip_character_slot(&view.data.settings,
					state.character_slot);
			if (equip_result == SETTINGS_EQUIP_CHANGED)
			{
				tetrisu_character_apply(ctx,
					view.data.settings.profile.character);
				tetrisu_visual_selection_sync(ctx, &view.data.settings);
			}
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
			if (equip_result == SETTINGS_EQUIP_CHANGED)
			{
				tetrisu_theme_apply(ctx, view.data.settings.profile.theme);
				render_background_cache_reset(ctx);
				tetrisu_visual_selection_sync(ctx, &view.data.settings);
				audio_transition_music(audio, ctx->theme_assets.music, 0);
			}
			if (equip_result == SETTINGS_EQUIP_LOCKED)
				render_notification_queue_ownership(ctx);
			if ((equip_result == SETTINGS_EQUIP_CHANGED
					|| equip_result == SETTINGS_EQUIP_LOCKED)
				&& !render_settings_show(ctx, &view, &state,
					equip_result == SETTINGS_EQUIP_CHANGED))
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
		else if (action == SETTINGS_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_settings_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu, false, true) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Runs the Marketplace: browse the shelves, spend points, equip stock.
 */
static int	run_marketplace_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	const t_menu_selection *menu)
{
	t_app_screen_view_model	view;
	t_marketplace_state		state;
	t_marketplace_state		previous;
	t_marketplace_action	action;
	t_app_provider_result	result;
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
	tetrisu_visual_selection_sync(ctx, &view.data.marketplace);
	audio_transition_music(audio, ctx->theme_assets.music, 0);
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
		else if (action == MARKETPLACE_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	render_marketplace_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
		if (reflow_home(ctx, menu, false, true) < 0)
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
static bool	apply_marketplace_purchase(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_screen_view_model *view, t_marketplace_state *state)
{
	const t_app_catalogue_item_view_model	*item;
	t_marketplace_purchase_result			result;

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
static bool	apply_marketplace_equip(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_screen_view_model *view, t_marketplace_state *state)
{
	t_settings_equip_result	result;
	bool					theme_changed;

	theme_changed = false;
	if (marketplace_focused_item(&view->data.marketplace, state) == NULL)
		return (true);
	result = marketplace_equip_focused(&view->data.marketplace, state);
	if (result != SETTINGS_EQUIP_CHANGED && result != SETTINGS_EQUIP_LOCKED)
		return (true);
	if (result == SETTINGS_EQUIP_CHANGED)
	{
		if (marketplace_focused_is_character(state))
			tetrisu_character_apply(ctx,
				view->data.marketplace.profile.character);
		else
		{
			tetrisu_theme_apply(ctx, view->data.marketplace.profile.theme);
			render_background_cache_reset(ctx);
			audio_transition_music(audio, ctx->theme_assets.music, 0);
			theme_changed = true;
		}
		tetrisu_visual_selection_sync(ctx, &view->data.marketplace);
	}
	audio_play_menu_select(audio);
	if (result == SETTINGS_EQUIP_CHANGED)
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_EQUIPPED, 0);
	else
		marketplace_set_feedback(state, MARKETPLACE_FEEDBACK_LOCKED, 0);
	return (render_marketplace_show(ctx, view, state, theme_changed));
}

/**
 * @brief Runs the multiplayer mode picker over the home artwork.
 *
 * The chosen mode becomes the lobby's list filter rather than a restriction, so
 * this screen narrows the browser without closing anything off.
 */
static int	run_multiplayer_mode_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session)
{
	t_app_screen_view_model	view;
	t_mp_mode_state			state;
	t_mp_mode_state			previous;
	t_mp_mode_action		action;
	ncinput					input;
	ncinput					queued_input;
	ncinput					pending_input;
	uint32_t				key;
	uint32_t				queued_key;
	uint32_t				pending_key;
	int						drained;
	bool					has_pending;

	if (app_screen_view_load_for_session(provider, APP_SCREEN_MULTIPLAYER_MODE,
			navigation->offline, &view) == APP_PROVIDER_INVALID)
		return (-1);
	mp_mode_state_init(&state);
	if (session->mode == APP_GAME_MODE_BATTLE_ROYALE)
		state.focus = MP_MODE_FOCUS_BATTLE_ROYALE;
	/*
	 * Every multiplayer screen is keyboard-only, and every one of them clears
	 * the notification stack on entry: a card left over from another screen is
	 * a plane raised over these regions, and on a stationary protocol raising
	 * or dropping one blanks whatever it covers.
	 */
	(void)notcurses_mice_disable(ctx->nc);
	render_notification_destroy(ctx);
	if (!render_mp_mode_show(ctx, &view, &state, true))
		return (-1);
	has_pending = false;
	while (navigation->current == APP_SCREEN_MULTIPLAYER_MODE)
	{
		if (has_pending)
		{
			input = pending_input;
			key = pending_key;
			has_pending = false;
		}
		else
			key = render_wait_input(ctx, &input);
		action = MP_MODE_ACTION_NONE;
		previous = state;
		if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = MP_MODE_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_mp_mode_show(ctx, &view, &state, true))
				return (-1);
			continue ;
		}
		else
			action = mp_mode_handle_key(&state, key);
		drained = 0;
		while (action == MP_MODE_ACTION_NONE
			&& mp_mode_navigation_keys_coalesce(key, key)
			&& drained < MULTIPLAYER_INPUT_BATCH_MAX)
		{
			memset(&queued_input, 0, sizeof(queued_input));
			queued_key = notcurses_get_nblock(ctx->nc, &queued_input);
			if (queued_key == 0)
				break ;
			drained++;
			if (queued_input.evtype == NCTYPE_RELEASE
				|| nckey_mouse_p(queued_key))
				continue ;
			if (!mp_mode_navigation_keys_coalesce(key, queued_key))
			{
				pending_input = queued_input;
				pending_key = queued_key;
				has_pending = true;
				break ;
			}
			(void)mp_mode_handle_key(&state, queued_key);
		}
		if (mp_mode_state_view_changed(&previous, &state))
		{
			audio_play_menu_move(audio);
			if (!render_mp_mode_show(ctx, &view, &state, false))
				return (-1);
		}
		if (mp_mode_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (action == MP_MODE_ACTION_VOLUME_UP
			|| action == MP_MODE_ACTION_VOLUME_DOWN)
		{
			audio_play_menu_select(audio);
			if (action == MP_MODE_ACTION_VOLUME_UP)
				audio_volume_up(audio);
			else
				audio_volume_down(audio);
			state.feedback = MP_MODE_FEEDBACK_VOLUME;
			state.feedback_value
				= ui_notification_volume_percent(audio->music_volume);
			if (!render_mp_mode_show(ctx, &view, &state, false))
				return (-1);
		}
		else if (action == MP_MODE_ACTION_SELECT)
		{
			audio_play_menu_select(audio);
			session->mode = mp_mode_focused_mode(&state);
			(void)app_navigation_dispatch(navigation, APP_NAV_OPEN_LOBBY);
		}
		else if (action == MP_MODE_ACTION_BACK)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == MP_MODE_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	return (0);
}

/**
 * @brief Runs the room browser: pick a room, type an id, or open a new one.
 */
static int	run_lobby_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session)
{
	t_app_screen_view_model	view;
	t_lobby_state			state;
	t_lobby_state			previous;
	t_lobby_action			action;
	ncinput					input;
	ncinput					queued_input;
	ncinput					pending_input;
	uint32_t				key;
	uint32_t				queued_key;
	uint32_t				pending_key;
	int						drained;
	bool					has_pending;
	bool					refresh;

	if (app_screen_view_load_for_session(provider, APP_SCREEN_LOBBY,
			navigation->offline, &view) == APP_PROVIDER_INVALID)
		return (-1);
	lobby_state_init(&state, session->mode, &view.data.lobby);
	(void)notcurses_mice_disable(ctx->nc);
	render_notification_destroy(ctx);
	if (!render_lobby_show(ctx, &view, &state, true))
		return (-1);
	has_pending = false;
	while (navigation->current == APP_SCREEN_LOBBY)
	{
		if (has_pending)
		{
			input = pending_input;
			key = pending_key;
			has_pending = false;
		}
		else
			key = render_wait_input(ctx, &input);
		action = LOBBY_ACTION_NONE;
		previous = state;
		refresh = false;
		if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = LOBBY_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_lobby_show(ctx, &view, &state, true))
				return (-1);
			continue ;
		}
		else
			action = lobby_handle_key(&state, key);
		drained = 0;
		while (action == LOBBY_ACTION_NONE
			&& lobby_navigation_keys_coalesce(key, key)
			&& drained < MULTIPLAYER_INPUT_BATCH_MAX)
		{
			memset(&queued_input, 0, sizeof(queued_input));
			queued_key = notcurses_get_nblock(ctx->nc, &queued_input);
			if (queued_key == 0)
				break ;
			drained++;
			if (queued_input.evtype == NCTYPE_RELEASE
				|| nckey_mouse_p(queued_key))
				continue ;
			if (!lobby_navigation_keys_coalesce(key, queued_key))
			{
				pending_input = queued_input;
				pending_key = queued_key;
				has_pending = true;
				break ;
			}
			(void)lobby_handle_key(&state, queued_key);
		}
		if (state.filter != previous.filter)
			lobby_state_sync(&state, &view.data.lobby);
		if (action == LOBBY_ACTION_REFRESH)
		{
			audio_play_menu_select(audio);
			if (app_screen_view_load_for_session(provider, APP_SCREEN_LOBBY,
					navigation->offline, &view) == APP_PROVIDER_INVALID)
				return (-1);
			lobby_state_sync(&state, &view.data.lobby);
			lobby_set_feedback(&state, LOBBY_FEEDBACK_REFRESHED, 0);
			refresh = true;
		}
		else if (action == LOBBY_ACTION_VOLUME_UP
			|| action == LOBBY_ACTION_VOLUME_DOWN)
		{
			audio_play_menu_select(audio);
			if (action == LOBBY_ACTION_VOLUME_UP)
				audio_volume_up(audio);
			else
				audio_volume_down(audio);
			render_notification_show_volume(ctx, audio->music_volume);
		}
		else if (action == LOBBY_ACTION_JOIN
			|| action == LOBBY_ACTION_JOIN_BY_ID)
		{
			audio_play_menu_select(audio);
			apply_lobby_join(&view, &state, navigation, session,
				action == LOBBY_ACTION_JOIN_BY_ID);
			refresh = true;
		}
		else if (action == LOBBY_ACTION_CREATE)
		{
			audio_play_menu_select(audio);
			session->mode = state.filter == APP_GAME_MODE_BATTLE_ROYALE
				? APP_GAME_MODE_BATTLE_ROYALE : APP_GAME_MODE_DOUBLE;
			(void)app_navigation_dispatch(navigation, APP_NAV_OPEN_CREATE_ROOM);
		}
		else if (action == LOBBY_ACTION_BACK)
		{
			audio_play_menu_select(audio);
			session->mode = state.filter;
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == LOBBY_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
		if (lobby_action_leaves_screen(action))
			discard_queued_input(ctx);
		if ((refresh || lobby_state_view_changed(&previous, &state))
			&& navigation->current == APP_SCREEN_LOBBY)
		{
			if (!refresh)
				audio_play_menu_move(audio);
			if (!render_lobby_show(ctx, &view, &state, false))
				return (-1);
		}
	}
	return (0);
}

/**
 * @brief Resolves one join request into a room, or explains why it failed.
 *
 * Joining by id ignores the list filter deliberately: a room id is how a friend
 * shares a room, and a filter the player happens to have set must not hide the
 * room they were invited to.
 */
static void	apply_lobby_join(t_app_screen_view_model *view,
	t_lobby_state *state, t_app_navigation *navigation, t_mp_session *session,
	bool by_id)
{
	const t_app_room_summary_view_model	*room;
	t_lobby_feedback					blocker;

	if (by_id)
	{
		if (state->room_id_length == 0)
		{
			lobby_set_feedback(state, LOBBY_FEEDBACK_EMPTY_ID, 0);
			return ;
		}
		room = lobby_room_by_id(&view->data.lobby, state->room_id);
		if (room == NULL)
		{
			lobby_set_feedback(state, LOBBY_FEEDBACK_UNKNOWN_ID, 0);
			return ;
		}
	}
	else
		room = lobby_selected_room(&view->data.lobby, state);
	blocker = lobby_join_blocker(room);
	if (blocker != LOBBY_FEEDBACK_NONE)
	{
		lobby_set_feedback(state, blocker, 0);
		return ;
	}
	/*
	 * The session id field is the one the join box types into, and it is
	 * shorter than the model's. The explicit precision keeps the copy inside it
	 * and lets the compiler prove nothing is truncated.
	 */
	snprintf(session->room_id, sizeof(session->room_id), "%.*s",
		(int)sizeof(session->room_id) - 1, room->id);
	session->mode = room->mode;
	session->create_pending = false;
	(void)app_navigation_dispatch(navigation, APP_NAV_OPEN_WAITING_ROOM);
}

/**
 * @brief Runs the create-room panel and hands the chosen mode to the room.
 */
static int	run_create_room_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session)
{
	t_app_screen_view_model	view;
	t_create_room_state		state;
	t_create_room_state		previous;
	t_create_room_action	action;
	ncinput					input;
	uint32_t				key;

	if (app_screen_view_load_for_session(provider,
			APP_SCREEN_CREATE_ROOM_MODAL, navigation->offline, &view)
		== APP_PROVIDER_INVALID)
		return (-1);
	create_room_state_init(&state, session->mode);
	(void)notcurses_mice_disable(ctx->nc);
	render_notification_destroy(ctx);
	if (!render_create_room_show(ctx, &view, &state, true))
		return (-1);
	while (navigation->current == APP_SCREEN_CREATE_ROOM_MODAL)
	{
		key = render_wait_input(ctx, &input);
		action = CREATE_ROOM_ACTION_NONE;
		previous = state;
		if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		if (key == (uint32_t)-1)
			action = CREATE_ROOM_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_create_room_show(ctx, &view, &state, true))
				return (-1);
			continue ;
		}
		else
			action = create_room_handle_key(&state, key);
		if (action == CREATE_ROOM_ACTION_VOLUME_UP
			|| action == CREATE_ROOM_ACTION_VOLUME_DOWN)
		{
			audio_play_menu_select(audio);
			if (action == CREATE_ROOM_ACTION_VOLUME_UP)
				audio_volume_up(audio);
			else
				audio_volume_down(audio);
			state.feedback = MP_MODE_FEEDBACK_VOLUME;
			state.feedback_value
				= ui_notification_volume_percent(audio->music_volume);
		}
		if (create_room_state_view_changed(&previous, &state))
		{
			audio_play_menu_move(audio);
			if (!render_create_room_show(ctx, &view, &state, false))
				return (-1);
		}
		if (create_room_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (action == CREATE_ROOM_ACTION_CREATE)
		{
			audio_play_menu_select(audio);
			session->mode = state.mode;
			session->create_pending = true;
			session->room_id[0] = '\0';
			(void)app_navigation_dispatch(navigation,
				APP_NAV_OPEN_WAITING_ROOM);
		}
		else if (action == CREATE_ROOM_ACTION_CANCEL)
		{
			audio_play_menu_select(audio);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else if (action == CREATE_ROOM_ACTION_QUIT
			&& ((key != 'q' && key != 'Q')
				|| confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP)))
			(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	return (0);
}

/**
 * @brief Runs the waiting room, including chat and the pre-match countdown.
 *
 * The countdown is the only thing in the client that advances without input, so
 * this is the only loop that waits on a deadline as well as on a keystroke. It
 * repaints the status region alone once a second; everything else on the screen
 * is left untouched.
 */
static int	run_waiting_room_screen(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session)
{
	t_waiting_room_state	previous;
	t_room_action			action;
	ncinput					input;
	uint32_t				key;
	uint64_t				deadline;
	uint64_t				now;
	int						wait_ms;
	bool					prompted;

	if (!load_room_view(provider, session))
		return (-1);
	(void)waiting_room_sync_state(&session->room_view.data.room);
	waiting_room_state_init(&session->room_state);
	if (waiting_room_auto_start_allowed(&session->room_view.data.room))
		(void)waiting_room_begin_countdown(&session->room_state);
	(void)notcurses_mice_disable(ctx->nc);
	render_notification_destroy(ctx);
	if (!render_waiting_room_show(ctx, &session->room_view,
			&session->room_state, true))
		return (-1);
	audio_play_room_entry(audio);
	deadline = 0;
	if (session->room_state.counting_down)
		deadline = ui_notification_now_ms()
			+ WAITING_ROOM_COUNTDOWN_STEP_MS;
	while (navigation->current == APP_SCREEN_WAITING_ROOM)
	{
		wait_ms = -1;
		if (session->room_state.counting_down)
		{
			now = ui_notification_now_ms();
			wait_ms = deadline > now ? (int)(deadline - now) : 0;
		}
		key = render_wait_input_timeout(ctx, &input, wait_ms);
		action = ROOM_ACTION_NONE;
		prompted = false;
		previous = session->room_state;
		if (key == 0)
		{
			deadline += WAITING_ROOM_COUNTDOWN_STEP_MS;
			if (waiting_room_tick(&session->room_state))
				action = ROOM_ACTION_LAUNCH;
		}
		else if (input.evtype == NCTYPE_RELEASE || nckey_mouse_p(key))
			continue ;
		else if (key == (uint32_t)-1)
			action = ROOM_ACTION_QUIT;
		else if (key == NCKEY_RESIZE || key == 12u)
		{
			if (render_geometry_refresh(ctx, true) < 0
				|| !render_waiting_room_show(ctx, &session->room_view,
					&session->room_state, true))
				return (-1);
			continue ;
		}
		else
			action = waiting_room_handle_key(&session->room_state,
					&session->room_view.data.room, key);
		if (action == ROOM_ACTION_LEAVE)
		{
			prompted = true;
			if (!confirmation_prompt_run(ctx, audio, CONFIRM_LEAVE_ROOM))
				action = ROOM_ACTION_NONE;
		}
		else if (action == ROOM_ACTION_QUIT && (key == 'q' || key == 'Q'))
		{
			prompted = true;
			if (!confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP))
				action = ROOM_ACTION_NONE;
		}
		if (prompted && session->room_state.counting_down)
			deadline = ui_notification_now_ms()
				+ WAITING_ROOM_COUNTDOWN_STEP_MS;
		if (!apply_room_action(ctx, audio, navigation, session, action))
			return (-1);
		/* Double auto-starts; Battle Royale can only enter here through the
		 * owner's explicit Start action handled above. */
		if (navigation->current == APP_SCREEN_WAITING_ROOM
			&& waiting_room_auto_start_allowed(
				&session->room_view.data.room))
			(void)waiting_room_begin_countdown(&session->room_state);
		if (session->room_state.counting_down && previous.counting_down == false)
			deadline = ui_notification_now_ms()
				+ WAITING_ROOM_COUNTDOWN_STEP_MS;
		if (waiting_room_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (navigation->current == APP_SCREEN_WAITING_ROOM
			&& waiting_room_state_view_changed(&previous,
				&session->room_state)
			&& !render_waiting_room_show(ctx, &session->room_view,
				&session->room_state, false))
			return (-1);
	}
	return (0);
}

/**
 * @brief Applies one waiting-room action to the room model and navigation.
 *
 * Readying and starting are resolved locally because there is no server yet;
 * both go through the same policy helpers a server-driven build will call, so
 * only the source of the room snapshot changes later.
 */
static bool	apply_room_action(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_navigation *navigation, t_mp_session *session, t_room_action action)
{
	t_app_room_view_model	*room;
	t_room_feedback			blocker;

	room = &session->room_view.data.room;
	if (action == ROOM_ACTION_TOGGLE_READY)
	{
		audio_play_menu_select(audio);
		if (waiting_room_toggle_ready(room))
		{
			(void)waiting_room_sync_state(room);
			session->room_state.feedback = waiting_room_local_ready(room)
				? ROOM_FEEDBACK_READY : ROOM_FEEDBACK_NOT_READY;
		}
		/* Un-readying mid-countdown stops it: the room is no longer eligible. */
		if (session->room_state.counting_down && !waiting_room_can_start(room))
			(void)waiting_room_cancel_countdown(&session->room_state);
		return (true);
	}
	if (action == ROOM_ACTION_START)
	{
		audio_play_menu_select(audio);
		if (session->room_state.counting_down)
			return (true);
		blocker = waiting_room_start_blocker(room);
		if (blocker != ROOM_FEEDBACK_NONE)
			session->room_state.feedback = blocker;
		else
			(void)waiting_room_begin_countdown(&session->room_state);
		return (true);
	}
	if (action == ROOM_ACTION_SEND_CHAT)
	{
		if (waiting_room_send_chat(room, &session->room_state))
			audio_play_menu_select(audio);
		return (true);
	}
	if (action == ROOM_ACTION_VOLUME_UP || action == ROOM_ACTION_VOLUME_DOWN)
	{
		audio_play_menu_select(audio);
		if (action == ROOM_ACTION_VOLUME_UP)
			audio_volume_up(audio);
		else
			audio_volume_down(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		return (true);
	}
	if (action == ROOM_ACTION_LAUNCH)
	{
		audio_play_menu_select(audio);
		room->state = APP_ROOM_STATE_IN_GAME;
		(void)app_navigation_dispatch(navigation,
			waiting_room_launch_action(room));
		return (true);
	}
	if (action == ROOM_ACTION_LEAVE)
	{
		audio_play_menu_select(audio);
		(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
	}
	else if (action == ROOM_ACTION_QUIT)
		(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	return (true);
}

/**
 * @brief Loads the room the previous screen chose, creating it when asked to.
 */
static bool	load_room_view(const t_app_data_provider *provider,
	t_mp_session *session)
{
	t_app_provider_result	result;

	if (session->create_pending)
		result = app_room_view_create(provider, session->mode,
				&session->room_view);
	else
		result = app_room_view_load(provider, session->room_id,
				&session->room_view);
	session->create_pending = false;
	if (result != APP_PROVIDER_INVALID)
		snprintf(session->room_id, sizeof(session->room_id), "%.*s",
			(int)sizeof(session->room_id) - 1,
			session->room_view.data.room.id);
	return (result != APP_PROVIDER_INVALID);
}

/**
 * @brief Reports whether a screen belongs to the multiplayer group.
 *
 * The four screens share one set of region planes and one static cache, so the
 * teardown that frees them only runs when the whole group is left rather than
 * on every step between them.
 */
static bool	is_multiplayer_screen(t_app_screen screen)
{
	return (screen == APP_SCREEN_MULTIPLAYER_MODE
		|| screen == APP_SCREEN_LOBBY
		|| screen == APP_SCREEN_CREATE_ROOM_MODAL
		|| screen == APP_SCREEN_WAITING_ROOM);
}

/**
 * @brief Releases the multiplayer planes once the group is actually left.
 */
static int	leave_multiplayer(t_render_ctx *ctx, t_app_navigation *navigation,
	const t_menu_selection *menu)
{
	if (is_multiplayer_screen(navigation->current))
		return (0);
	render_multiplayer_destroy(ctx);
	if (navigation->current != APP_SCREEN_HOME)
		return (0);
	if (reflow_home(ctx, menu, false, true) < 0)
		return (-1);
	enable_home_mouse(ctx);
	return (0);
}

/**
 * @brief Drops Settings input already buffered across a screen transition.
 */
static void	discard_queued_input(t_render_ctx *ctx)
{
	ncinput		input;
	uint32_t	key;
	int			drained;

	drained = 0;
	while (drained < DISCARD_INPUT_BATCH_MAX)
	{
		memset(&input, 0, sizeof(input));
		key = notcurses_get_nblock(ctx->nc, &input);
		if (key == 0 || key == (uint32_t)-1)
			return ;
		drained++;
	}
}

/**
 * @brief Presents and advances one scaffolded item-15 screen.
 */
static int	run_scaffold_step(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	const t_menu_selection *menu)
{
	t_app_screen_view_model	view;
	t_app_nav_action		action;
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
	if (action == APP_NAV_QUIT && (key == 'q' || key == 'Q')
		&& !confirmation_prompt_run(ctx, audio, CONFIRM_QUIT_APP))
		return (0);
	audio_play_menu_select(audio);
	if (!app_navigation_dispatch(navigation, action))
		return (0);
	if (navigation->current == APP_SCREEN_HOME)
	{
		render_screen_destroy(ctx);
		if (reflow_home(ctx, menu, false, true) < 0)
			return (-1);
		enable_home_mouse(ctx);
	}
	return (0);
}

/**
 * @brief Maps temporary scaffold controls to the validated state graph.
 */
static t_app_nav_action	scaffold_navigation_action(t_app_screen screen,
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
static void	enable_home_mouse(t_render_ctx *ctx)
{
	(void)notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS);
}

/**
 * @brief Rebuilds the home screen after resize or scaffold navigation.
 */
static int	reflow_home(t_render_ctx *ctx, const t_menu_selection *menu,
	bool refresh_geometry, bool replace_background)
{
	render_screen_destroy(ctx);
	render_menu_destroy(ctx);
	if ((refresh_geometry && render_geometry_refresh(ctx, true) < 0)
		|| (replace_background && render_background_replace(ctx,
				ctx->theme_assets.homepage, false) < 0))
		return (-1);
	render_menu_create(ctx);
	render_menu_move_bunny(ctx, menu);
	render_notification_reflow(ctx);
	return (0);
}
