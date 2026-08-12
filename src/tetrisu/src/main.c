#include "tetrisu.h"
#include "tetrisu_bot.h"

# define SETTINGS_INPUT_BATCH_MAX	64
# define LEADERBOARD_INPUT_BATCH_MAX	64
# define MARKETPLACE_INPUT_BATCH_MAX	64
# define MULTIPLAYER_INPUT_BATCH_MAX	64
# define DISCARD_INPUT_BATCH_MAX		256

/*
 * Why the loop ended, when it ended because a screen could not be drawn
 * rather than because the player asked to leave. notcurses owns the terminal
 * until render_teardown, so the reason cannot be printed where it is found -
 * it waits here and is printed once the terminal belongs to the shell again.
 * Without this the client restores the terminal and exits 0 on a failed draw,
 * which from the outside is indistinguishable from a crash.
 */
static const char	*g_exit_reason;

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
	/*
	 * The character catalogue, held so the waiting room can offer a choice of
	 * fighter. Only owned entries are offered - the roster is what the player
	 * bought, and the Marketplace is where buying happens.
	 */
	t_app_catalogue_view_model	characters;
	/*
	 * The bots this player has added to this room, as child processes. They
	 * live on the session rather than on the room model because they are this
	 * client's processes and not the room's members: the server knows them
	 * only as four more clients that logged in.
	 */
	t_bot_farm					bots;
}	t_mp_session;

// Static Functions
static int	reflow_home(t_render_ctx *ctx, const t_menu_selection *menu,
				bool refresh_geometry, bool replace_background);
static void	restore_after_notification(t_render_ctx *ctx,
				const t_menu_selection *menu);
static bool	toggle_ready(const t_app_data_provider *provider,
				t_app_room_view_model *room, t_mp_session *session);
static void	add_bot(t_mp_session *session);
static void	kick_bot(t_mp_session *session);
static t_bot_level	default_bot_level(void);
static t_room_feedback	start_blocker_for(t_mp_session *session,
					const t_app_room_view_model *room);
static void	load_room_characters(const t_app_data_provider *provider,
				t_mp_session *session);
static void	cycle_character(t_mp_session *session, int delta);
static uint32_t	chosen_character(const t_mp_session *session);
static void	name_character(t_mp_session *session);
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
static void	adopt_account_loadout(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider);
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
				const t_app_data_provider *provider,
				t_app_screen_view_model *view, t_marketplace_state *state);
static bool	apply_marketplace_equip(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_app_data_provider *provider,
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
					const t_app_data_provider *provider,
					t_app_navigation *navigation, t_mp_session *session,
					t_room_action action);
static bool	launch_match(t_render_ctx *ctx, t_audio_ctx *audio,
					const t_app_data_provider *provider,
					t_app_navigation *navigation, t_mp_session *session);
static bool	waiting_room_counts_down_here(
					const t_app_data_provider *provider);
static bool	network_dropped(const t_app_data_provider *provider,
					const t_app_net_session *session);
static bool	handle_network_drop(t_render_ctx *ctx, t_audio_ctx *audio,
					t_app_data_provider *provider,
					t_app_navigation *navigation, t_mp_session *mp_session,
					t_app_net_session *session);
static void	network_heartbeat(const t_app_data_provider *provider,
					t_app_net_session *session, uint64_t *due_ms);
static bool	send_room_chat(const t_app_data_provider *provider,
					t_app_room_view_model *room,
					t_waiting_room_state *state);
static void	refresh_waiting_room(const t_app_data_provider *provider,
					t_mp_session *session, bool *changed);
static int	waiting_room_wait_ms(const t_waiting_room_state *state,
					uint64_t countdown_deadline, uint64_t refresh_deadline,
					bool polling);
static bool	load_room_view(const t_app_data_provider *provider,
				t_mp_session *session);
static bool	apply_start_screen_override(t_app_navigation *navigation,
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
static t_net_client	*match_session(const t_app_data_provider *provider,
						t_app_net_session *session);
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
	uint64_t			heartbeat_due_ms;
	uint32_t			key;
	int					hovered;
	bool				direct_match_preview;
	bool				direct_screen;

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
	direct_screen = direct_match_preview
		|| app_start_screen_override() != APP_SCREEN_COUNT;
	if (!direct_screen)
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
	else if (apply_start_screen_override(&navigation, &mp_session))
		direct_match_preview = navigation.current == APP_SCREEN_DOUBLE
			|| navigation.current == APP_SCREEN_BATTLE_ROYALE;
	auth_form_init(&auth_form, AUTH_FORM_LOGIN);
	heartbeat_due_ms = ui_notification_now_ms() + NET_HEARTBEAT_MS;
	while (navigation.current != APP_SCREEN_QUIT)
	{
		/*
		 * Every screen comes back through here, so this is the one place a
		 * drop has to be noticed however it was discovered - a failed
		 * request inside a screen, a poll that stopped answering, or the
		 * heartbeat below.
		 */
		if (handle_network_drop(&ctx, &audio, &provider, &navigation,
				&mp_session, &net_session))
			continue ;
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
					? &mp_session.room_view.data.room : NULL,
					match_session(&provider, &net_session)) < 0)
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
		memset(&input, 0, sizeof(input));
		key = render_wait_input_timeout(&ctx, &input, NET_HEARTBEAT_MS);
		if (key == 0)
		{
			/*
			 * The home screen is where a player leaves the client sitting,
			 * so it is where a silent drop has to be found. Everything else
			 * either polls the server already or is over in seconds.
			 */
			network_heartbeat(&provider, &net_session, &heartbeat_due_ms);
			continue ;
		}
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
			restore_after_notification(&ctx, &menu);
		}
		else if (key == '-' || key == '_')
		{
			audio_volume_down(&audio);
			render_notification_show_volume(&ctx, audio.music_volume);
			restore_after_notification(&ctx, &menu);
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
	/*
	 * The last chance to collect the bots. Every ordinary way out of a room
	 * has already done it, so reaching here with children still running means
	 * the client is exiting from somewhere that did not - and a bot whose
	 * parent is gone would notice through its deadman pipe anyway, but a
	 * process this one started is this one's to wait for.
	 */
	bot_farm_clear(&mp_session.bots);
	if (net_session.connected)
		net_disconnect(&net_session.net);
	if (g_exit_reason != NULL)
	{
		fprintf(stderr, "tetrisu: %s\n", g_exit_reason);
		return (1);
	}
	return (0);
}

/**
 * @brief Puts the account's own loadout on screen, before the screen is drawn.
 *
 * The renderer is born wearing Classic and Mirurun (render_init), and until
 * this existed nothing replaced them on the way in: tetrisu_visual_selection_
 * bind() was reachable only from Settings and the Marketplace, so a returning
 * player landed on home in the default theme and had to open Settings and
 * re-equip a theme they already owned to get it back. The loadout is a fact
 * about the account, so signing in is when it arrives.
 *
 * The Settings model is the carrier because it is the one the bind already
 * takes - it is where the equipped ids come back from the server. Nothing is
 * drawn here and nothing is written back; the caller reflows home immediately
 * after, and it reads theme_assets.homepage.
 *
 * A provider that cannot answer, or an offline/preview session, leaves the
 * defaults standing - which is what they are for.
 *
 * @param ctx Render context whose theme and character are being set.
 * @param audio Audio context, moved to the theme's own music.
 * @param provider Data provider the account is read through.
 */
static void	adopt_account_loadout(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider)
{
	t_app_screen_view_model	view;

	if (provider == NULL)
		return ;
	memset(&view, 0, sizeof(view));
	if (app_screen_view_load_for_session(provider, APP_SCREEN_SETTINGS, false,
			&view) == APP_PROVIDER_INVALID)
		return ;
	tetrisu_visual_selection_bind(ctx, provider, &view.data.settings);
	audio_transition_music(audio, ctx->theme_assets.music, 0);
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
		{
			g_exit_reason = "the sign-in screen could not be drawn";
			return (-1);
		}
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
		adopt_account_loadout(ctx, audio, provider);
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
			auth_form_finish_server_check(form, false, NULL);
			return (true);
		}
		session = (t_app_net_session *)provider->userdata;
		net_config_load(&session->cfg);
		if (form->domain[0] != '\0')
			apply_domain_to_config(&session->cfg, form->domain);
		if (net_connect(&session->net, &session->cfg) == 0)
		{
			session->connected = true;
			auth_form_finish_server_check(form, true, NULL);
		}
		else
		{
			session->connected = false;
			/*
			** net.error, not a generic failure: it separates nothing
			** listening from a server that answered and was not trusted,
			** and those send you looking in different places.
			*/
			auth_form_finish_server_check(form, false, session->net.error);
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
	tetrisu_visual_selection_bind(ctx, provider, &view.data.settings);
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
				tetrisu_visual_selection_bind(ctx, provider,
					&view.data.settings);
				if (!render_settings_show(ctx, &view, &state, false))
					return (-1);
			}
		}
		else if (action == SETTINGS_ACTION_EQUIP_CHARACTER)
		{
			audio_play_menu_select(audio);
			equip_result = app_settings_equip_character(provider,
					&view.data.settings, state.character_slot);
			if (equip_result == SETTINGS_EQUIP_CHANGED)
				tetrisu_visual_selection_bind(ctx, provider,
					&view.data.settings);
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
			equip_result = app_settings_equip_theme(provider,
					&view.data.settings, state.theme_slot);
			if (equip_result == SETTINGS_EQUIP_CHANGED)
			{
				tetrisu_visual_selection_bind(ctx, provider,
					&view.data.settings);
				render_background_cache_reset(ctx);
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
	tetrisu_visual_selection_bind(ctx, provider, &view.data.marketplace);
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
			if (!apply_marketplace_purchase(ctx, audio, provider, &view, &state))
				return (-1);
		}
		else if (action == MARKETPLACE_ACTION_EQUIP)
		{
			if (!apply_marketplace_equip(ctx, audio, provider, &view, &state))
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
	const t_app_data_provider *provider, t_app_screen_view_model *view,
	t_marketplace_state *state)
{
	const t_app_catalogue_item_view_model	*item;
	t_marketplace_purchase_result			result;

	item = marketplace_focused_item(&view->data.marketplace, state);
	if (item == NULL)
		return (true);
	if (item->owned)
		return (apply_marketplace_equip(ctx, audio, provider, view, state));
	result = app_marketplace_buy(provider, &view->data.marketplace, state);
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
	const t_app_data_provider *provider, t_app_screen_view_model *view,
	t_marketplace_state *state)
{
	t_settings_equip_result	result;
	bool					theme_changed;

	theme_changed = false;
	if (marketplace_focused_item(&view->data.marketplace, state) == NULL)
		return (true);
	result = app_marketplace_equip(provider, &view->data.marketplace,
			state);
	if (result != SETTINGS_EQUIP_CHANGED && result != SETTINGS_EQUIP_LOCKED)
		return (true);
	if (result == SETTINGS_EQUIP_CHANGED)
	{
		tetrisu_visual_selection_bind(ctx, provider, &view->data.marketplace);
		if (!marketplace_focused_is_character(state))
		{
			render_background_cache_reset(ctx);
			audio_transition_music(audio, ctx->theme_assets.music, 0);
			theme_changed = true;
		}
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
			/*
			 * The filter cycles through APP_GAME_MODE_NONE for "all rooms",
			 * which is not a mode anything downstream can play; carrying it out
			 * of here left the mode picker silently falling back to Double.
			 */
			if (state.filter != APP_GAME_MODE_NONE)
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
	bool					cued;

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
		cued = false;
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
			cued = true;
		}
		if (create_room_state_view_changed(&previous, &state))
		{
			/*
			 * A volume keypress already played its own cue, and it also moves
			 * the feedback fields - so the view-changed repaint used to play a
			 * second cue for the same key.
			 */
			if (!cued)
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
	uint64_t				refresh_deadline;
	uint64_t				now;
	int						wait_ms;
	bool					prompted;
	bool					auto_start;
	bool					polling;
	bool					room_changed;

	/*
	 * A room that is not there is not a reason to close the game, and this is
	 * the ordinary way to arrive at one: tetrisd destroys a room when its
	 * match ends, so the room a player is returning *from* has already gone by
	 * the time they get back to it. Answering that with -1 sent main.c
	 * APP_NAV_QUIT, which is why pressing Enter or Escape on the results
	 * screen closed the whole application.
	 *
	 * The lobby is where a player with no room belongs, so that is where they
	 * go. Whether a finished room should instead be kept for a rematch is a
	 * separate question, and a bigger one - until it is answered, going back a
	 * screen is the honest thing to do rather than the last thing to do.
	 */
	if (!load_room_view(provider, session))
	{
		session->room_id[0] = '\0';
		(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		return (0);
	}
	(void)waiting_room_sync_state(&session->room_view.data.room);
	waiting_room_state_init(&session->room_state);
	load_room_characters(provider, session);
	/*
	 * Arriving from a finished match must not re-arm the countdown, for this
	 * whole visit rather than only on entry. Reloading the room hands back a
	 * ready room with everyone ready, so a Double room auto-started again the
	 * instant the player left the match, relaunched it, and left Escape looking
	 * like it did nothing forever. The player asked to be back in the room;
	 * starting the next match is their call, and S still does it.
	 *
	 * A server-backed room never arms it at all - the room decides when it is
	 * under way and this screen learns that from a snapshot, so there is
	 * nothing here to count down towards.
	 */
	auto_start = navigation->previous != APP_SCREEN_DOUBLE
		&& navigation->previous != APP_SCREEN_BATTLE_ROYALE
		&& waiting_room_counts_down_here(provider);
	if (auto_start
		&& waiting_room_local_is_owner(&session->room_view.data.room)
		&& waiting_room_auto_start_allowed(&session->room_view.data.room))
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
	polling = provider != NULL && provider->refresh_room != NULL
		&& !provider->local_fixtures;
	refresh_deadline = ui_notification_now_ms() + WAITING_ROOM_REFRESH_MS;
	while (navigation->current == APP_SCREEN_WAITING_ROOM)
	{
		wait_ms = waiting_room_wait_ms(&session->room_state, deadline,
				refresh_deadline, polling);
		key = render_wait_input_timeout(ctx, &input, wait_ms);
		action = ROOM_ACTION_NONE;
		prompted = false;
		room_changed = false;
		previous = session->room_state;
		if (key == 0)
		{
			now = ui_notification_now_ms();
			if (session->room_state.counting_down && now >= deadline)
			{
				deadline = now + WAITING_ROOM_COUNTDOWN_STEP_MS;
				if (waiting_room_tick(&session->room_state))
					action = ROOM_ACTION_LAUNCH;
			}
			if (polling && now >= refresh_deadline)
			{
				refresh_deadline = now + WAITING_ROOM_REFRESH_MS;
				refresh_waiting_room(provider, session, &room_changed);
				/*
				 * A poll that found nothing at the other end is the room's
				 * heartbeat failing. Staying here would leave the player
				 * watching a roster that has stopped updating; leaving lets
				 * the main loop say so.
				 */
				if (provider != NULL && !provider->local_fixtures
					&& provider->userdata != NULL
					&& ((t_app_net_session *)provider->userdata)->net.state
					== NET_OFFLINE)
				{
					(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
					continue ;
				}
				/*
				 * SELECTING launches too. The room commits before it deals,
				 * and the moment it does both players belong on the roster
				 * screen - waiting for IN_GAME would leave them staring at
				 * the waiting room through the whole select window and drop
				 * them into a match already counting down.
				 */
				if (session->room_view.data.room.state == APP_ROOM_STATE_IN_GAME
					|| session->room_view.data.room.state
					== APP_ROOM_STATE_SELECTING)
					action = ROOM_ACTION_LAUNCH;
				else if (session->room_state.counting_down
					&& !waiting_room_can_start(&session->room_view.data.room))
					(void)waiting_room_cancel_countdown(&session->room_state);
			}
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
		if (!apply_room_action(ctx, audio, provider, navigation, session, action))
			return (-1);
		/* Double auto-starts; Battle Royale can only enter here through the
		 * owner's explicit Start action handled above. */
		if (auto_start && action != ROOM_ACTION_LAUNCH
			&& navigation->current == APP_SCREEN_WAITING_ROOM
			&& waiting_room_local_is_owner(&session->room_view.data.room)
			&& waiting_room_auto_start_allowed(
				&session->room_view.data.room))
			(void)waiting_room_begin_countdown(&session->room_state);
		if (session->room_state.counting_down && previous.counting_down == false)
			deadline = ui_notification_now_ms()
				+ WAITING_ROOM_COUNTDOWN_STEP_MS;
		if (waiting_room_action_leaves_screen(action))
			discard_queued_input(ctx);
		if (navigation->current == APP_SCREEN_WAITING_ROOM
			&& (room_changed || render_notification_repaint_pending(ctx)
				|| waiting_room_state_view_changed(&previous,
					&session->room_state))
			&& !render_waiting_room_show(ctx, &session->room_view,
				&session->room_state, false))
			return (-1);
	}
	return (0);
}

/**
 * @brief Applies one waiting-room action to the room model and navigation.
 *
 * Fixture readiness remains local. Leave and Start cross the provider seam so
 * a network session cannot navigate away from a room state the server still
 * owns.
 */
/**
 * @brief Declares this player ready, through the server when there is one.
 *
 * A provider that owns the room owns readiness too: the declaration goes up
 * and the roster that comes back is what gets drawn. Toggling locally as well
 * would put the screen a step ahead of every other player, and the next
 * refresh would take it away again - which is exactly what withdrawing
 * readiness used to look like.
 *
 * With no such provider - an offline room - the local toggle is the whole of
 * it, which is what the waiting room has always done.
 *
 * @param provider Data provider, possibly serving no room.
 * @param room Room model, replaced with the server's answer on success.
 * @param session Multiplayer screen session, whose feedback line is set.
 * @return true when the declaration stood.
 */
static bool	toggle_ready(const t_app_data_provider *provider,
	t_app_room_view_model *room, t_mp_session *session)
{
	bool	wanted;

	wanted = !waiting_room_local_ready(room);
	if (provider == NULL || provider->ready_room == NULL)
	{
		if (!waiting_room_toggle_ready(room))
			return (true);
		(void)waiting_room_sync_state(room);
		session->room_state.feedback = waiting_room_local_ready(room)
			? ROOM_FEEDBACK_READY : ROOM_FEEDBACK_NOT_READY;
		return (true);
	}
	if (provider->ready_room(provider->userdata, room->id, wanted,
			chosen_character(session), room) != APP_PROVIDER_OK)
		return (false);
	session->room_state.feedback = wanted
		? ROOM_FEEDBACK_READY : ROOM_FEEDBACK_NOT_READY;
	return (true);
}

/**
 * @brief Add one bot to this room, as a child of this process.
 *
 * The bot joins over its own socket, from the server's own account pool, so
 * nothing here names an account and nothing here seats anybody: the roster
 * grows when the room's next refresh shows one more member, exactly as it
 * would for a person who walked in.
 *
 * @param session The multiplayer session, whose farm and feedback are set.
 */
static void	add_bot(t_mp_session *session)
{
	if (session->bots.count >= BOT_FARM_MAX)
	{
		session->room_state.feedback = ROOM_FEEDBACK_BOT_LIMIT;
		return ;
	}
	if (bot_farm_add(&session->bots, session->room_id,
			default_bot_level()) != 0)
	{
		session->room_state.feedback = ROOM_FEEDBACK_BOT_UNAVAILABLE;
		return ;
	}
	session->room_state.feedback = ROOM_FEEDBACK_BOT_ADDED;
}

/**
 * @brief Kick the most recently added bot, and never anybody else.
 *
 * There is no authorisation question and no route, because a bot is this
 * client's own child: kicking is a signal, and a client can only signal the
 * processes it started. A room with people in it and no bots answers that
 * there is nothing to kick rather than doing something to a person.
 *
 * @param session The multiplayer session, whose farm and feedback are set.
 */
static void	kick_bot(t_mp_session *session)
{
	if (bot_farm_drop(&session->bots) != 0)
	{
		session->room_state.feedback = ROOM_FEEDBACK_BOT_NONE;
		return ;
	}
	session->room_state.feedback = ROOM_FEEDBACK_BOT_KICKED;
}

/**
 * @brief The difficulty a new bot is added at.
 *
 * TETRISU_BOT_LEVEL for now, defaulting to normal. It belongs in Settings -
 * one setting for the room rather than a prompt per bot, because four prompts
 * to add four bots is worse than one choice made once.
 *
 * @return The level to spawn at.
 */
static t_bot_level	default_bot_level(void)
{
	t_bot_level	level;

	level = BOT_NORMAL;
	bot_level_parse(getenv("TETRISU_BOT_LEVEL"), &level);
	return (level);
}

/**
 * @brief Why this room will not start, and what the player can do about it.
 *
 * A Battle Royale needs four and a player with two laptops has two, so "not
 * enough players" on its own is a dead end - the room cannot be started and
 * the screen has not said that anything can be done. When the shortfall is
 * one a bot could fill, the message carries the way out with it.
 *
 * Only when pressing B would actually work: start_blocker has already turned
 * a non-owner away with ROOM_FEEDBACK_NOT_OWNER, and the farm is checked here
 * so a player who has already added four is told the plain fact rather than
 * pointed at a key that will refuse them.
 *
 * @param session The multiplayer session, whose farm and feedback value are read.
 * @param room Room snapshot to test.
 * @return The blocker to show, or ROOM_FEEDBACK_NONE when the start may go on.
 */
static t_room_feedback	start_blocker_for(t_mp_session *session,
	const t_app_room_view_model *room)
{
	t_room_feedback	blocker;

	blocker = waiting_room_start_blocker(room);
	if (blocker != ROOM_FEEDBACK_NEED_PLAYERS)
		return (blocker);
	session->room_state.feedback_value = waiting_room_players_needed(room);
	if (session->bots.count < BOT_FARM_MAX)
		return (ROOM_FEEDBACK_NEED_PLAYERS_BOT);
	return (blocker);
}

static bool	apply_room_action(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session, t_room_action action)
{
	t_app_room_view_model	*room;
	t_room_feedback			blocker;

	room = &session->room_view.data.room;
	if (action == ROOM_ACTION_TOGGLE_READY)
	{
		audio_play_menu_select(audio);
		if (!toggle_ready(provider, room, session))
			session->room_state.feedback = ROOM_FEEDBACK_UNAVAILABLE;
		/* Un-readying mid-countdown stops it: the room is no longer eligible. */
		if (session->room_state.counting_down && !waiting_room_can_start(room))
			(void)waiting_room_cancel_countdown(&session->room_state);
		return (true);
	}
	if (action == ROOM_ACTION_CHARACTER_PREV
		|| action == ROOM_ACTION_CHARACTER_NEXT)
	{
		audio_play_menu_move(audio);
		cycle_character(session,
			action == ROOM_ACTION_CHARACTER_NEXT ? 1 : -1);
		/*
		 * A player who had already declared is re-declared, because the
		 * declaration is what carries the choice - otherwise changing fighter
		 * after pressing R would change nothing the server ever heard.
		 */
		if (waiting_room_local_ready(room) && provider != NULL
			&& provider->ready_room != NULL)
			(void)provider->ready_room(provider->userdata, room->id, true,
				chosen_character(session), room);
		return (true);
	}
	if (action == ROOM_ACTION_START)
	{
		audio_play_menu_select(audio);
		if (session->room_state.counting_down)
			return (true);
		blocker = start_blocker_for(session, room);
		if (blocker != ROOM_FEEDBACK_NONE)
			session->room_state.feedback = blocker;
		else if (waiting_room_counts_down_here(provider))
			(void)waiting_room_begin_countdown(&session->room_state);
		else
			return (launch_match(ctx, audio, provider, navigation, session));
		return (true);
	}
	if (action == ROOM_ACTION_ADD_BOT || action == ROOM_ACTION_KICK_BOT)
	{
		audio_play_menu_select(audio);
		if (!waiting_room_local_is_owner(room))
			session->room_state.feedback = ROOM_FEEDBACK_NOT_OWNER;
		else if (action == ROOM_ACTION_ADD_BOT)
			add_bot(session);
		else
			kick_bot(session);
		return (true);
	}
	if (action == ROOM_ACTION_SEND_CHAT)
	{
		if (send_room_chat(provider, room, &session->room_state))
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
		return (launch_match(ctx, audio, provider, navigation, session));
	if (action == ROOM_ACTION_LEAVE)
	{
		if (app_room_view_leave(provider, room->id) == APP_PROVIDER_OK)
		{
			audio_play_menu_select(audio);
			/*
			 * The bots go with the player who added them. Leaving them behind
			 * would strand seats nobody can reach: the only client that can
			 * stop them is the one that started them, and it has just walked
			 * out of the room they are in.
			 */
			bot_farm_clear(&session->bots);
			(void)app_navigation_dispatch(navigation, APP_NAV_BACK);
		}
		else
			session->room_state.feedback = ROOM_FEEDBACK_UNAVAILABLE;
	}
	else if (action == ROOM_ACTION_QUIT)
	{
		bot_farm_clear(&session->bots);
		(void)app_navigation_dispatch(navigation, APP_NAV_QUIT);
	}
	return (true);
}

/**
 * @brief Leaves the waiting room for the match, starting it if it needs it.
 *
 * A room already under way is one to walk into, not one to start. SELECTING
 * counts as under way: the server opened that window itself and will deal the
 * boards once both players have chosen, so a START sent from here would be
 * asking for a match that is already being set up.
 *
 * @param ctx Render context.
 * @param audio Audio context.
 * @param provider Data provider that may own the start.
 * @param navigation Navigation state to dispatch through.
 * @param session Multiplayer screen session.
 * @return true unless the caller must tear the screen down.
 */
static bool	launch_match(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *session)
{
	t_app_room_view_model	*room;

	(void)ctx;
	room = &session->room_view.data.room;
	if (!waiting_room_is_under_way(room)
		&& !waiting_room_local_is_owner(room))
	{
		(void)waiting_room_cancel_countdown(&session->room_state);
		return (true);
	}
	if (!waiting_room_is_under_way(room)
		&& app_room_view_start(provider, room->id, &session->room_view)
			!= APP_PROVIDER_OK)
	{
		(void)waiting_room_cancel_countdown(&session->room_state);
		session->room_state.feedback = ROOM_FEEDBACK_UNAVAILABLE;
		return (true);
	}
	audio_play_menu_select(audio);
	(void)app_navigation_dispatch(navigation,
		waiting_room_launch_action(&session->room_view.data.room));
	return (true);
}

/**
 * @brief Reports whether the link to the server has gone since it was up.
 *
 * The two halves matter separately. `connected` is the client's own record
 * that it once reached the server, and NET_OFFLINE is where net_client parks
 * a socket whose send or receive failed - so together they name a link that
 * existed and does not any more, which is the only case worth interrupting
 * the player over. A session that never connected is a sign-in that has not
 * happened yet, not a drop.
 *
 * @param provider The app's data provider; fixtures have nothing to drop.
 * @param session The app's network session.
 * @return true when a live connection has been lost.
 */
static bool	network_dropped(const t_app_data_provider *provider,
	const t_app_net_session *session)
{
	if (provider == NULL || provider->local_fixtures || session == NULL)
		return (false);
	return (session->connected && session->net.state == NET_OFFLINE);
}

/**
 * @brief Tells the player the connection is gone and takes their answer.
 *
 * Falling back silently is what this replaces. Both authorities already swap
 * themselves to the local rules when the server stops answering, so a dropped
 * player kept playing - against a board that had quietly stopped being the
 * server's, with a result nobody would record and a room that no longer knew
 * them. The fallback is the right behaviour; being told is the missing half.
 *
 * Offline is the safe answer because it takes nothing away: the provider
 * becomes the local fixture, which is the same one a player who never signed
 * in uses, and the screens that need a server hide themselves behind the
 * navigation's offline flag. Signing in again goes back to the auth screen,
 * whose CHECK SERVER redials - a dead session is redialled there already.
 *
 * @param ctx Render context.
 * @param audio Audio context.
 * @param provider The app's data provider, replaced when offline is chosen.
 * @param navigation Navigation state, moved to wherever the answer leads.
 * @param mp_session Multiplayer session, whose room is gone with the link.
 * @param session The network session being abandoned.
 * @return true when a drop was handled and the caller must re-dispatch.
 */
static bool	handle_network_drop(t_render_ctx *ctx, t_audio_ctx *audio,
	t_app_data_provider *provider, t_app_navigation *navigation,
	t_mp_session *mp_session, t_app_net_session *session)
{
	bool	sign_in_again;

	if (!network_dropped(provider, session))
		return (false);
	session->connected = false;
	session->has_catalogue = false;
	memset(mp_session->room_id, 0, sizeof(mp_session->room_id));
	mp_session->create_pending = false;
	sign_in_again = confirmation_prompt_run(ctx, audio,
			CONFIRM_CONNECTION_LOST);
	navigation->previous = navigation->current;
	if (sign_in_again)
	{
		navigation->offline = false;
		navigation->current = APP_SCREEN_LOGIN;
	}
	else
	{
		app_fixture_provider_init(provider);
		navigation->offline = true;
		navigation->current = APP_SCREEN_HOME;
	}
	return (true);
}

/**
 * @brief Asks the server whether it is still there, at most so often.
 *
 * PROFILE rather than a method of its own: it is the cheapest thing the
 * server already answers, and inventing a route for this would put a keepalive
 * in the protocol to serve one client's idle screen. What matters is the
 * round trip, not the body - net_request parks a failed socket at NET_OFFLINE
 * on its way out, and that is the whole result this reads.
 *
 * @param provider The app's data provider; fixtures never ask.
 * @param session The session to probe.
 * @param due_ms When the next probe is due; advanced by one interval here.
 */
static void	network_heartbeat(const t_app_data_provider *provider,
	t_app_net_session *session, uint64_t *due_ms)
{
	t_body_profile	profile;
	uint64_t		now;

	if (provider == NULL || provider->local_fixtures || session == NULL
		|| !session->connected || session->net.state < NET_AUTHED)
		return ;
	now = ui_notification_now_ms();
	if (now < *due_ms)
		return ;
	*due_ms = now + NET_HEARTBEAT_MS;
	(void)net_profile(&session->net, &profile);
}

/**
 * @brief Reports whether the pre-match countdown belongs to this client.
 *
 * It does when there is no server to own one. A room the server is running
 * counts itself down - it opens the select window, holds its own clock, and
 * deals both players in on the same tick - and a second countdown here ran
 * three seconds in the waiting room before the request that starts any of
 * that had even been sent. Both players then watched a number that meant
 * nothing, and the one the match actually begins on arrived after it.
 *
 * @param provider Data provider backing the room.
 * @return true when the local countdown is the only one there is.
 */
static bool	waiting_room_counts_down_here(const t_app_data_provider *provider)
{
	return (provider == NULL || provider->local_fixtures);
}

/**
 * @brief Refreshes the room model without re-entering it.
 *
 * Local chat is retained until the server owns that stream; the authoritative
 * snapshot replaces every roster and room-state field.
 *
 * @param provider Provider carrying the read-only refresh hook.
 * @param session Multiplayer screen session.
 * @param changed Receives whether anything renderable changed.
 */
/**
 * @brief Sends the composed line, through the server when there is one.
 *
 * A provider that serves a feed owns it entirely: the line goes up, and the
 * copy that comes back down is what gets drawn. Appending locally as well
 * would put the message on screen twice, and the local copy would be the one
 * without the server's ordering.
 *
 * With no such provider - an offline room - the local append is the feed,
 * which is what waiting_room_send_chat has always done.
 *
 * @param provider Data provider, possibly serving no feed.
 * @param room Room model, updated with the server's answer on success.
 * @param state Waiting-room state holding the composed line.
 * @return true when the line was sent and the composer should clear.
 */
static bool	send_room_chat(const t_app_data_provider *provider,
	t_app_room_view_model *room, t_waiting_room_state *state)
{
	if (provider == NULL || provider->send_chat == NULL)
		return (waiting_room_send_chat(room, state));
	if (state->compose_length == 0)
	{
		state->feedback = ROOM_FEEDBACK_CHAT_EMPTY;
		state->feedback_value = 0;
		return (false);
	}
	if (provider->send_chat(provider->userdata, room->id, state->compose,
			room) != APP_PROVIDER_OK)
	{
		state->feedback = ROOM_FEEDBACK_UNAVAILABLE;
		state->feedback_value = 0;
		return (false);
	}
	state->compose[0] = '\0';
	state->compose_length = 0;
	state->feedback = ROOM_FEEDBACK_CHAT_SENT;
	state->feedback_value = 0;
	return (true);
}

static void	refresh_waiting_room(const t_app_data_provider *provider,
	t_mp_session *session, bool *changed)
{
	t_app_room_view_model	before;
	t_app_room_chat_view_model	chat[APP_ROOM_CHAT_MAX];
	int					chat_count;
	t_app_provider_result	result;

	before = session->room_view.data.room;
	chat_count = before.chat_count;
	memcpy(chat, before.chat, sizeof(chat));
	result = app_room_view_refresh(provider, session->room_id,
			&session->room_view);
	if (result == APP_PROVIDER_OK)
	{
		if (session->room_view.data.room.chat_count == 0 && chat_count > 0)
		{
			session->room_view.data.room.chat_count = chat_count;
			memcpy(session->room_view.data.room.chat, chat, sizeof(chat));
		}
		*changed = memcmp(&before, &session->room_view.data.room,
				sizeof(before)) != 0;
		return ;
	}
	if (result == APP_PROVIDER_INVALID)
		session->room_state.feedback = ROOM_FEEDBACK_UNAVAILABLE;
	*changed = false;
}

/**
 * @brief Computes the next waiting-room deadline without busy-spinning.
 *
 * @param state Current countdown state.
 * @param countdown_deadline Next countdown tick time.
 * @param refresh_deadline Next live snapshot time.
 * @param polling Whether the provider supports network refresh.
 * @return Milliseconds to wait, or -1 when only input can wake the loop.
 */
static int	waiting_room_wait_ms(const t_waiting_room_state *state,
	uint64_t countdown_deadline, uint64_t refresh_deadline, bool polling)
{
	uint64_t	next;
	uint64_t	now;

	next = 0;
	if (state->counting_down)
		next = countdown_deadline;
	if (polling && (next == 0 || refresh_deadline < next))
		next = refresh_deadline;
	if (next == 0)
		return (-1);
	now = ui_notification_now_ms();
	if (next <= now)
		return (0);
	if (next - now > INT_MAX)
		return (INT_MAX);
	return ((int)(next - now));
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
 * @brief Boots straight into the screen TETRISU_START_SCREEN names.
 *
 * Only the entry point moves. The screen still loads its model through the
 * provider and still leaves through its own Back route, so this exercises the
 * real screen rather than a preview of it. The multiplayer screens below the
 * lobby also need the room the lobby would have chosen, which is why the mode
 * and the room id are seeded here rather than left zeroed.
 *
 * @return Whether an override was named and applied.
 */
static bool	apply_start_screen_override(t_app_navigation *navigation,
	t_mp_session *session)
{
	t_app_screen	screen;

	screen = app_start_screen_override();
	if (screen == APP_SCREEN_COUNT)
		return (false);
	navigation->current = screen;
	navigation->previous = app_screen_parent(screen);
	navigation->offline = false;
	if (screen == APP_SCREEN_BATTLE_ROYALE)
		session->mode = APP_GAME_MODE_BATTLE_ROYALE;
	else if (screen == APP_SCREEN_DOUBLE)
		session->mode = APP_GAME_MODE_DOUBLE;
	if (screen == APP_SCREEN_WAITING_ROOM || screen == APP_SCREEN_DOUBLE
		|| screen == APP_SCREEN_BATTLE_ROYALE)
		snprintf(session->room_id, sizeof(session->room_id), "%s",
			session->mode == APP_GAME_MODE_BATTLE_ROYALE
			? "arena-88" : "duel-42");
	return (true);
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
	/*
	 * The scaffold owns a full-screen plane, so it has to be destroyed whenever
	 * the step leaves this screen - not only on the way home. Destroying it only
	 * for APP_SCREEN_HOME left it covering whatever came next, which is what
	 * made leaving a match look like it had done nothing at all.
	 */
	render_screen_destroy(ctx);
	if (navigation->current == APP_SCREEN_HOME)
	{
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
/**
 * @brief Hands a match the session it is to be played on, if there is one.
 *
 * A match needs more of a session than Solo does. Solo takes its own room on
 * the way in, so being signed in is enough; a match is played in a room the
 * waiting room already seated this client in, and the play path bound with
 * that seat is what every pushed snapshot is matched against. Without it the
 * client would connect, render, and silently discard every frame the server
 * sent - so a session that has not got that far is not one to play on, and
 * the fixture is the honest answer instead.
 *
 * @param provider The app's data provider; fixtures never carry a session.
 * @param session The app's network session.
 * @return The session to play against, or NULL to play the local fixture.
 */
static t_net_client	*match_session(const t_app_data_provider *provider,
					t_app_net_session *session)
{
	if (provider->local_fixtures || !session->connected)
		return (NULL);
	if (session->net.state < NET_IN_ROOM || session->net.play_path[0] == '\0')
		return (NULL);
	return (&session->net);
}

static void	enable_home_mouse(t_render_ctx *ctx)
{
	(void)notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS);
}

/**
 * @brief Rebuilds the home screen after resize or scaffold navigation.
 */
/**
 * @brief Redraws the home screen a notification card has just covered.
 *
 * The card is a bitmap and so is the menu under it. Notcurses wipes the
 * sprixel underneath rather than overlapping it, and the home loop redraws
 * only on a keypress that changes the selection - so a volume press left the
 * menu erased until something else happened to repaint it.
 *
 * @param ctx Active render context.
 * @param menu Current selection, redrawn where it was.
 */
static void	restore_after_notification(t_render_ctx *ctx,
	const t_menu_selection *menu)
{
	if (!render_notification_take_repaint(ctx))
		return ;
	(void)reflow_home(ctx, menu, false, false);
}

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

/**
 * @brief Fetches the character catalogue the waiting room offers a choice from.
 *
 * A provider that serves no catalogue leaves the roster empty, and an empty
 * roster is not a failure: it means the client has nothing to offer and sends
 * no character, which the server reads as "whatever the account has equipped".
 * That is exactly what a Single game does and what a client with no selector
 * has always done.
 *
 * The index is started on the equipped character, so the default choice is the
 * one the player already made in the Marketplace.
 *
 * @param provider Data provider, possibly serving no catalogue.
 * @param session Multiplayer session whose roster is filled.
 */
static void	load_room_characters(const t_app_data_provider *provider,
		t_mp_session *session)
{
	int	index;

	memset(&session->characters, 0, sizeof(session->characters));
	session->room_state.character_index = 0;
	if (provider == NULL || provider->load_catalogue == NULL)
		return ;
	if (provider->load_catalogue(provider->userdata, APP_CATALOGUE_CHARACTERS,
			&session->characters) != APP_PROVIDER_OK)
	{
		memset(&session->characters, 0, sizeof(session->characters));
		return ;
	}
	index = 0;
	while (index < session->characters.count)
	{
		if (session->characters.items[index].owned
			&& session->characters.items[index].equipped)
		{
			session->room_state.character_index = index;
			break ;
		}
		index++;
	}
	name_character(session);
}

/**
 * @brief Moves the choice to the next owned character in either direction.
 *
 * Unowned entries are stepped over rather than refused, so the roster reads as
 * the fighters this player has rather than as a catalogue with gaps in it. A
 * player who owns exactly one lands back on it, which is the correct no-op.
 *
 * @param session Multiplayer session holding the roster and the index.
 * @param delta +1 for the next fighter, -1 for the previous.
 */
static void	cycle_character(t_mp_session *session, int delta)
{
	int	count;
	int	index;
	int	tried;

	count = session->characters.count;
	if (count <= 0)
		return ;
	index = session->room_state.character_index;
	tried = 0;
	while (tried < count)
	{
		index = (index + delta + count) % count;
		tried++;
		if (session->characters.items[index].owned)
		{
			session->room_state.character_index = index;
			break ;
		}
	}
	name_character(session);
}

/**
 * @brief The character id this player will take into the match.
 *
 * @param session Multiplayer session holding the roster and the index.
 * @return The chosen item id, or 0 when there is no roster to choose from.
 */
static uint32_t	chosen_character(const t_mp_session *session)
{
	int	index;

	index = session->room_state.character_index;
	if (index < 0 || index >= session->characters.count)
		return (0);
	if (!session->characters.items[index].owned)
		return (0);
	return (session->characters.items[index].item_id);
}

/**
 * @brief Copies the chosen fighter's name where the renderers can read it.
 *
 * @param session Multiplayer session whose roster and index are read.
 */
static void	name_character(t_mp_session *session)
{
	int	index;

	session->room_state.character_name[0] = '\0';
	index = session->room_state.character_index;
	if (index < 0 || index >= session->characters.count
		|| !session->characters.items[index].owned)
		return ;
	snprintf(session->room_state.character_name,
		sizeof(session->room_state.character_name), "%s",
		session->characters.items[index].name);
}
