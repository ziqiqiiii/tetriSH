#include "tetrisu.h"

// Static Functions
static uint64_t	match_now_ms(void);
static uint32_t	match_seed(void);
static int	preview_player_count(t_app_game_mode mode);
static void	load_match_identity(const t_app_data_provider *provider,
				t_app_profile_view_model *profile,
				t_app_catalogue_view_model *characters);
static bool	handle_match_key(t_match_authority *authority,
				t_mp_match_state *state, t_render_ctx *ctx,
				t_audio_ctx *audio, uint32_t key, const ncinput *input,
				bool *rebuild, t_solo_handling_state *handling,
				const t_solo_handling_config *handling_config);
static bool	apply_game_key(t_match_authority *authority,
				t_mp_match_state *state, uint32_t key);
static bool	apply_handling_actions(t_match_authority *authority,
				t_mp_match_state *state, t_solo_handling_state *handling,
				const t_solo_handling_config *config, int elapsed_ms);
static void	select_power(t_match_authority *authority,
				t_mp_match_state *state, t_audio_ctx *audio, uint32_t key);
static bool	selection_online_update(const t_app_data_provider *provider,
				const char *room_id, t_mp_match_state *state,
				t_render_ctx *ctx, t_audio_ctx *audio, int elapsed_ms,
				uint64_t *poll_due_ms);
static void	selection_send_lock(const t_app_data_provider *provider,
				const char *room_id, const t_mp_match_state *state);
static void	play_match_events(t_audio_ctx *audio, uint32_t events);
static bool	announce_effects(t_match_authority *authority,
				const t_mp_match_state *state, t_render_ctx *ctx,
				t_audio_ctx *audio);
static bool	match_mouse_pixel_position(const t_render_ctx *ctx,
				const ncinput *input, int *x, int *y);
static int	next_match_wake_ms(t_render_ctx *ctx, t_audio_ctx *audio,
				const t_mp_match_state *state,
				const t_solo_handling_state *handling,
				const t_solo_handling_config *config);
static uint32_t	wait_match_input(t_render_ctx *ctx,
					const t_match_authority *authority, int timeout_ms,
					ncinput *input);

/**
 * @brief Runs the client-only Double/Battle Royale presentation.
 *
 * This loop intentionally owns no room synchronization. local_game and the
 * deterministic opponent are preview snapshots until tetrisd exposes the
 * multiplayer state stream; the renderer and controls already consume the
 * same shape that stream can populate.
 */
int	multiplayer_match_mode_run(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_app_data_provider *provider, t_app_game_mode mode,
	const char *room_id, const t_app_room_view_model *room, t_net_client *net)
{
	t_match_authority		authority;
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_mp_match_state		state;
	t_solo_handling_config	handling_config;
	t_solo_handling_state	handling;
	ncinput					input;
	uint32_t				key;
	uint32_t				selection_events;
	uint64_t				previous_ms;
	uint64_t				now_ms;
	uint64_t				select_poll_ms;
	bool					lock_sent;
	int						elapsed_ms;
	int						wait_ms;
	bool					leave;
	bool					changed;
	bool					rebuild;
	bool					input_backlog;
	bool					mouse_enabled;
	int					input_batch;

	if (ctx == NULL || audio == NULL)
		return (-1);
	load_match_identity(provider, &profile, &characters);
	mp_match_state_init(&state, mode, room_id, &profile, &characters,
		match_seed());
	mp_match_apply_room(&state, room, preview_player_count(mode));
	match_authority_open(&authority, net, &state);
	if (match_authority_is_online(&authority))
	{
		/*
		 * Two ways to arrive online. If the room is still SELECTING the
		 * server is holding its window open and the roster is exactly where
		 * this screen should start; the clock is the room's, so the local
		 * timer is left at zero and refreshed from the room instead. If the
		 * boards are already dealt there is no selection left to run, and the
		 * countdown in the first snapshot is what the player sees.
		 */
		if (room != NULL && room->state == APP_ROOM_STATE_SELECTING)
		{
			state.selection.locked = false;
			state.selection.remaining_ms = room->select_ms;
			state.phase = MP_MATCH_CHARACTER_SELECT;
		}
		else
		{
			state.selection.locked = true;
			state.selection.remaining_ms = 0;
			state.phase = MP_MATCH_PLAYING;
		}
	}
	handling_config = solo_handling_default_config();
	solo_handling_reset(&handling);
	if (getenv("TETRISU_MATCH_PREVIEW_SKIP_SELECTION") != NULL)
	{
		const t_app_catalogue_item_view_model	*character;

		state.selection.locked = true;
		state.selection.remaining_ms = 0;
		state.phase = MP_MATCH_PLAYING;
		solo_game_start_countdown(&state.local_game);
		solo_game_start_countdown(&state.opponent_game);
		character = mp_match_selected_character(&state);
		if (character != NULL)
			tetrisu_character_apply(ctx, character->name);
	}
	rebuild = true;
	leave = false;
	mouse_enabled = notcurses_mice_enable(ctx->nc, NCMICE_ALL_EVENTS) == 0;
	if (!render_multiplayer_match_show(ctx, &state, rebuild))
	{
		render_multiplayer_match_destroy(ctx);
		return (-1);
	}
	rebuild = false;
	previous_ms = match_now_ms();
	select_poll_ms = previous_ms;
	lock_sent = false;
	input_backlog = false;
	while (!leave)
	{
		/*
		 * The loop sleeps until something is actually due - gravity, the lock
		 * delay, a DAS or ARR repeat, the selection countdown, a notification,
		 * the music cross-fade - instead of spinning at a fixed frame rate.
		 * Polling every frame and repainting unconditionally is what made two
		 * pixel boards stutter: the terminal was being handed a full bitmap
		 * far faster than it could parse one, and input queued up behind it.
		 */
		wait_ms = next_match_wake_ms(ctx, audio, &state, &handling,
				&handling_config);
		if (wait_ms < 0 || wait_ms > RENDER_RESIZE_POLL_MS)
			wait_ms = RENDER_RESIZE_POLL_MS;
		if (match_authority_pending(&authority))
			wait_ms = 0;
		memset(&input, 0, sizeof(input));
		if (input_backlog)
			key = notcurses_get_nblock(ctx->nc, &input);
		else
			key = wait_match_input(ctx, &authority, wait_ms, &input);
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		now_ms = match_now_ms();
		if (now_ms < previous_ms)
			elapsed_ms = 0;
		else if (now_ms - previous_ms > MP_MATCH_MAX_CATCHUP_MS)
			elapsed_ms = MP_MATCH_MAX_CATCHUP_MS;
		else
			elapsed_ms = (int)(now_ms - previous_ms);
		previous_ms = now_ms;
		(void)audio_update(audio, elapsed_ms);
		changed = false;
		if (state.phase == MP_MATCH_CHARACTER_SELECT
			&& match_authority_is_online(&authority))
		{
			/*
			 * Online the window belongs to the room, so the local timer is
			 * only ever an interpolation and the decision to start is never
			 * this client's. Locking in is sent the moment it happens: it is
			 * what lets the room stop waiting out its clock.
			 */
			if (state.selection.locked && !lock_sent)
			{
				selection_send_lock(provider, room_id, &state);
				lock_sent = true;
			}
			changed = selection_online_update(provider, room_id, &state, ctx,
					audio, elapsed_ms, &select_poll_ms) || changed;
		}
		else if (state.phase == MP_MATCH_CHARACTER_SELECT)
		{
			selection_events = mp_match_character_update(&state, elapsed_ms);
			if ((selection_events & MP_SELECTION_EVENT_SECOND) != 0)
				audio_play_sfx(audio, AUDIO_SFX_COUNTDOWN_TICK);
			if ((selection_events & MP_SELECTION_EVENT_FINISHED) != 0)
			{
				const t_app_catalogue_item_view_model	*character;

				character = mp_match_selected_character(&state);
				if (character != NULL)
					tetrisu_character_apply(ctx, character->name);
				changed = true;
			}
			changed = selection_events != MP_SELECTION_EVENT_NONE || changed;
		}
		else if (state.phase == MP_MATCH_PLAYING)
		{
			changed = match_authority_update(&authority, &state, elapsed_ms)
				|| changed;
			if (!state.local_game.paused && !state.local_game.countdown_active
				&& state.local_game.phase == SOLO_ACTIVE)
				changed = apply_handling_actions(&authority, &state, &handling,
						&handling_config, elapsed_ms) || changed;
			play_match_events(audio, solo_game_take_events(&state.local_game));
			(void)solo_game_take_events(&state.opponent_game);
			changed = announce_effects(&authority, &state, ctx, audio)
				|| changed;
			/*
			 * A frame the opponent's presentation floor held back is due now,
			 * and nothing else this turn is going to ask for a draw.
			 */
			if (render_multiplayer_match_deferred_ms(ctx) == 0)
				changed = true;
			/*
			 * Only the fixture ends a match on a local top-out. Online the
			 * verdict is the server's and arrives in a snapshot, because a
			 * player who tops out has not necessarily lost yet - somebody else
			 * may be about to do the same, and in Battle Royale the placing is
			 * not known until they do.
			 */
			if (!match_authority_is_online(&authority)
				&& state.local_game.phase == SOLO_GAME_OVER)
			{
				mp_match_finish(&state, false,
					state.mode == APP_GAME_MODE_BATTLE_ROYALE
					? state.players_alive : 2);
				changed = true;
			}
		}
		if (render_terminal_geometry_changed(ctx))
		{
			solo_handling_reset(&handling);
			if (render_geometry_refresh(ctx, true) < 0)
				break ;
			render_multiplayer_match_destroy(ctx);
			rebuild = true;
			/* Reflow is an explicit pause; protocol negotiation time must not
			 * be charged to gameplay gravity. */
			previous_ms = match_now_ms();
		}
		if (key == (uint32_t)-1)
			leave = true;
		else if (key != 0)
		{
			/*
			 * Anything the player pressed is a reason to present: the frame it
			 * belongs to is the one it has to appear in. Waiting for the next
			 * gravity tick to notice would put up to a whole gravity interval
			 * between a keypress and the piece moving on screen. Repainting on
			 * a key that changed nothing costs nothing - the renderer compares
			 * signatures and returns early.
			 */
			changed = true;
			leave = handle_match_key(&authority, &state, ctx, audio, key,
					&input, &rebuild, &handling, &handling_config);
		}
		input_backlog = false;
		input_batch = 0;
		while (!leave && key != 0 && key != (uint32_t)-1)
		{
			input_batch++;
			if (input_batch >= MP_MATCH_INPUT_BATCH_MAX)
			{
				input_backlog = true;
				break ;
			}
			memset(&input, 0, sizeof(input));
			key = notcurses_get_nblock(ctx->nc, &input);
			if (key == 0 || key == (uint32_t)-1)
				break ;
			leave = handle_match_key(&authority, &state, ctx, audio, key,
					&input, &rebuild, &handling, &handling_config);
		}
		if (leave)
			break ;
		/*
		 * Present the moment something changed, with no rate of our own on top
		 * of the terminal's. The renderer compares signatures and repaints
		 * only the region planes that actually moved, so the terminal is
		 * already the thing setting the pace - adding a frame budget here just
		 * put a delay between a keypress and the piece.
		 */
		if (changed || rebuild)
		{
			if (!render_multiplayer_match_show(ctx, &state, rebuild))
			{
				render_multiplayer_match_destroy(ctx);
				return (-1);
			}
			rebuild = false;
		}
	}
	if (mouse_enabled)
		(void)notcurses_mice_disable(ctx->nc);
	match_authority_close(&authority);
	render_multiplayer_match_destroy(ctx);
	return (0);
}

/**
 * @brief Waits for one terminal event, key-up events included.
 *
 * A match cannot use render_wait_input_timeout(). That helper is the menus'
 * and it deliberately drops NCTYPE_RELEASE so a tapped arrow moves a selection
 * exactly once - correct for a menu, fatal for a board: the press takes the
 * key held and charges DAS, and if the release is swallowed on the way in
 * nothing ever hands the axis back, so one tap slides the piece to the wall.
 *
 * Solo reads the input descriptor itself for the same reason. This is that
 * function, waiting on the session beside the terminal so a pushed snapshot
 * wakes the loop as promptly as a keypress does - and a snapshot is now both
 * boards, so sleeping through one stops the opponent as well.
 *
 * @param ctx Active render context.
 * @param authority Authority whose session is waited on, if it has one.
 * @param timeout_ms Maximum wait in milliseconds.
 * @param input Output metadata for the received event.
 * @return A key code, 0 on timeout, or (uint32_t)-1 on failure.
 */
static uint32_t	wait_match_input(t_render_ctx *ctx,
	const t_match_authority *authority, int timeout_ms, ncinput *input)
{
	struct pollfd	poll_fd[2];
	int				count;
	int				result;

	memset(poll_fd, 0, sizeof(poll_fd));
	poll_fd[0].fd = notcurses_inputready_fd(ctx->nc);
	poll_fd[0].events = POLLIN;
	if (poll_fd[0].fd < 0)
		return ((uint32_t)-1);
	count = 1;
	poll_fd[1].fd = match_authority_fd(authority);
	poll_fd[1].events = POLLIN;
	if (poll_fd[1].fd >= 0)
		count = 2;
	errno = 0;
	result = poll(poll_fd, (nfds_t)count, timeout_ms);
	if (result < 0)
	{
		if (errno == EINTR)
			return (0);
		return ((uint32_t)-1);
	}
	if (result == 0 || (poll_fd[0].revents & POLLIN) == 0)
		return (0);
	errno = 0;
	return (notcurses_get_nblock(ctx->nc, input));
}

/**
 * @brief Returns the earliest deadline the match loop must wake for.
 *
 * Every clock in the screen gets a vote, so the loop can sleep between them
 * rather than poll: both boards' gravity and lock delay, the DAS and ARR
 * repeats the player is holding, the character-select countdown, a
 * notification's dismissal and the music cross-fade.
 */
static int	next_match_wake_ms(t_render_ctx *ctx, t_audio_ctx *audio,
	const t_mp_match_state *state, const t_solo_handling_state *handling,
	const t_solo_handling_config *config)
{
	int	wake_ms;
	int	candidate;

	wake_ms = -1;
	if (state->phase == MP_MATCH_CHARACTER_SELECT)
		wake_ms = MP_MATCH_FRAME_MS;
	else if (state->phase == MP_MATCH_PLAYING)
	{
		wake_ms = solo_game_next_wake_ms(&state->local_game);
		candidate = solo_game_next_wake_ms(&state->opponent_game);
		if (wake_ms < 0 || (candidate >= 0 && candidate < wake_ms))
			wake_ms = candidate;
		if (!state->local_game.paused && !state->local_game.countdown_active
			&& state->local_game.phase == SOLO_ACTIVE)
		{
			candidate = solo_handling_next_wake_ms(handling, config,
					gravity_interval_ms(state->local_game.level));
			if (wake_ms < 0 || (candidate >= 0 && candidate < wake_ms))
				wake_ms = candidate;
		}
	}
	candidate = render_notification_next_wake_ms(ctx);
	if (wake_ms < 0 || (candidate >= 0 && candidate < wake_ms))
		wake_ms = candidate;
	candidate = render_multiplayer_match_deferred_ms(ctx);
	if (wake_ms < 0 || (candidate >= 0 && candidate < wake_ms))
		wake_ms = candidate;
	candidate = audio_next_wake_ms(audio);
	if (wake_ms < 0 || (candidate >= 0 && candidate < wake_ms))
		wake_ms = candidate;
	return (wake_ms);
}

static int	preview_player_count(t_app_game_mode mode)
{
	const char	*value;
	char		*end;
	long		count;

	if (mode == APP_GAME_MODE_DOUBLE)
		return (2);
	value = getenv("TETRISU_PREVIEW_PLAYERS");
	if (value == NULL || value[0] == '\0')
		return (12);
	errno = 0;
	count = strtol(value, &end, 10);
	if (errno != 0 || *end != '\0')
		return (12);
	if (count < WAITING_ROOM_ROYALE_MIN_PLAYERS)
		return (WAITING_ROOM_ROYALE_MIN_PLAYERS);
	if (count > APP_ROOM_MAX_PLAYERS)
		return (APP_ROOM_MAX_PLAYERS);
	return ((int)count);
}

static uint64_t	match_now_ms(void)
{
	struct timespec	now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
}

/* TEMPORARY: frame cost instrumentation, removed once the lag is pinned. */
static uint32_t	match_seed(void)
{
	return ((uint32_t)(match_now_ms() ^ (uint64_t)getpid()));
}

static void	load_match_identity(const t_app_data_provider *provider,
	t_app_profile_view_model *profile,
	t_app_catalogue_view_model *characters)
{
	t_app_data_provider	fallback;
	bool				profile_ready;
	bool				characters_ready;

	memset(profile, 0, sizeof(*profile));
	memset(characters, 0, sizeof(*characters));
	profile_ready = provider != NULL && provider->load_profile != NULL
		&& provider->load_profile(provider->userdata, profile) == APP_PROVIDER_OK;
	characters_ready = provider != NULL && provider->load_catalogue != NULL
		&& provider->load_catalogue(provider->userdata,
			APP_CATALOGUE_CHARACTERS, characters) == APP_PROVIDER_OK;
	if (profile_ready && characters_ready)
		return ;
	app_fixture_provider_init(&fallback);
	if (!profile_ready)
		(void)fallback.load_profile(fallback.userdata, profile);
	if (!characters_ready)
		(void)fallback.load_catalogue(fallback.userdata,
			APP_CATALOGUE_CHARACTERS, characters);
}

static bool	handle_match_key(t_match_authority *authority,
	t_mp_match_state *state, t_render_ctx *ctx, t_audio_ctx *audio,
	uint32_t key, const ncinput *input, bool *rebuild,
	t_solo_handling_state *handling,
	const t_solo_handling_config *handling_config)
{
	t_mp_match_pixel_layout layout;
	t_solo_action action;
	int mouse_x;
	int mouse_y;
	int ability;

	if (nckey_mouse_p(key))
	{
		ability = 0;
		if (state->phase == MP_MATCH_PLAYING
			&& match_mouse_pixel_position(ctx, input, &mouse_x, &mouse_y))
		{
			mp_match_pixel_layout_build(state->mode,
				ctx->bg_cols * ctx->cell_px_x,
				ctx->bg_rows * ctx->cell_px_y, &layout);
			ability = mp_match_ability_at_pixel(&layout, mouse_x, mouse_y);
		}
		state->hovered_ability = ability;
		if (key == NCKEY_BUTTON1 && ability > 0
			&& input->evtype == NCTYPE_PRESS)
			select_power(authority, state, audio,
				(uint32_t)('0' + ability));
		return (false);
	}
	if ((key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_DOWN)
		&& state->phase == MP_MATCH_PLAYING)
	{
		if (mp_match_movement_event(state, handling, handling_config, key,
				input != NULL ? input->evtype : NCTYPE_UNKNOWN, &action))
		{
			(void)match_authority_action(authority, state, action);
			play_match_events(audio,
				solo_game_take_events(&state->local_game));
		}
		return (false);
	}
	if ((key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_DOWN)
		&& input != NULL && input->evtype == NCTYPE_RELEASE)
	{
		(void)mp_match_movement_event(state, handling, handling_config, key,
			input->evtype, &action);
		return (false);
	}
	if (input != NULL && input->evtype == NCTYPE_RELEASE)
		return (false);
	if (key == NCKEY_RESIZE || key == 12u)
	{
		if (render_geometry_refresh(ctx, true) < 0)
			return (true);
		render_multiplayer_match_destroy(ctx);
		*rebuild = true;
		return (false);
	}
	if (key == NCKEY_ESC || key == 'q' || key == 'Q')
		return (true);
	if (key == '+' || key == '=')
	{
		audio_volume_up(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		return (false);
	}
	if (key == '-' || key == '_')
	{
		audio_volume_down(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		return (false);
	}
	if (state->phase == MP_MATCH_FINISHED)
		return (key == NCKEY_ENTER || key == '\n' || key == '\r');
	if (state->phase == MP_MATCH_CHARACTER_SELECT)
	{
		if (mp_match_character_handle_key(state, key))
		{
			if (state->selection.locked)
				audio_play_menu_select(audio);
			else
				audio_play_menu_move(audio);
		}
		return (false);
	}
	if (mp_match_target_handle_key(state, key))
	{
		audio_play_sfx(audio, AUDIO_SFX_MOVE);
		snprintf(state->status, sizeof(state->status), "TARGETING: %s",
			mp_match_target_name(state->target_mode));
		return (false);
	}
	if (key >= '1' && key <= '4')
	{
		select_power(authority, state, audio, key);
		return (false);
	}
	/*
	 * There is deliberately no pause in a match. Solo can stop its own clock
	 * because nobody else is waiting on it; a match cannot, and a key that
	 * froze only the local board while rivals kept playing would be worse than
	 * no key at all. P falls through to the game keys, where it means nothing.
	 */
	if (apply_game_key(authority, state, key))
		play_match_events(audio, solo_game_take_events(&state->local_game));
	return (false);
}

static bool	match_mouse_pixel_position(const t_render_ctx *ctx,
	const ncinput *input, int *x, int *y)
{
	int cell_x;
	int cell_y;
	int sub_x;
	int sub_y;

	if (ctx == NULL || input == NULL || x == NULL || y == NULL
		|| ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0)
		return (false);
	cell_x = input->x - ctx->bg_col;
	cell_y = input->y - ctx->bg_row;
	if (cell_x < 0 || cell_y < 0 || cell_x >= ctx->bg_cols
		|| cell_y >= ctx->bg_rows)
		return (false);
	sub_x = input->xpx;
	sub_y = input->ypx;
	if (sub_x < 0 || sub_x >= ctx->cell_px_x)
		sub_x = ctx->cell_px_x / 2;
	if (sub_y < 0 || sub_y >= ctx->cell_px_y)
		sub_y = ctx->cell_px_y / 2;
	*x = cell_x * ctx->cell_px_x + sub_x;
	*y = cell_y * ctx->cell_px_y + sub_y;
	return (true);
}

static bool	apply_game_key(t_match_authority *authority,
	t_mp_match_state *state, uint32_t key)
{
	if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (match_authority_action(authority, state, SOLO_ROTATE_CW));
	if (key == 'z' || key == 'Z')
		return (match_authority_action(authority, state, SOLO_ROTATE_CCW));
	if (key == ' ')
		return (match_authority_action(authority, state, SOLO_HARD_DROP));
	if (key == 'c' || key == 'C')
		return (match_authority_action(authority, state, SOLO_HOLD));
	return (false);
}

static bool	apply_handling_actions(t_match_authority *authority,
	t_mp_match_state *state, t_solo_handling_state *handling,
	const t_solo_handling_config *config, int elapsed_ms)
{
	t_solo_action	actions[SOLO_HANDLING_ACTION_CAP];
	int				count;
	int				index;
	bool			changed;

	count = solo_handling_update(handling, config,
		gravity_interval_ms(state->local_game.level), elapsed_ms, actions,
		SOLO_HANDLING_ACTION_CAP);
	changed = false;
	index = 0;
	while (index < count)
	{
		changed = match_authority_action(authority, state, actions[index])
			|| changed;
		index++;
	}
	return (changed);
}

/**
 * @brief Runs one frame of a roster screen whose clock belongs to the room.
 *
 * The local number is spent between polls so the seconds move smoothly, and
 * corrected to the room's whenever one comes back - the room is the only thing
 * that can know a rival has locked in and cut the window short. When the room
 * stops selecting it has dealt the boards, and this screen has nothing left to
 * decide: it becomes the match, and the countdown the server is already
 * pushing is what the player sees next.
 *
 * @param provider Provider the room is read through.
 * @param room_id The room being played in.
 * @param state Match model whose selection and phase are advanced.
 * @param ctx Render context, so a settled fighter can theme the screen.
 * @param audio Audio context for the closing seconds.
 * @param elapsed_ms Milliseconds since the previous frame.
 * @param poll_due_ms In/out deadline for the next room read.
 * @return true when something the renderer shows has changed.
 */
static bool	selection_online_update(const t_app_data_provider *provider,
	const char *room_id, t_mp_match_state *state, t_render_ctx *ctx,
	t_audio_ctx *audio, int elapsed_ms, uint64_t *poll_due_ms)
{
	const t_app_catalogue_item_view_model	*character;
	t_app_screen_view_model					view;
	uint64_t								now;
	int										before;
	bool									changed;

	changed = false;
	before = mp_match_character_seconds(state);
	if (state->selection.remaining_ms > elapsed_ms)
		state->selection.remaining_ms -= elapsed_ms;
	else
		state->selection.remaining_ms = 0;
	if (mp_match_character_seconds(state) != before)
	{
		if (mp_match_character_seconds(state) <= MP_CHARACTER_TICK_AUDIO_SECONDS)
			audio_play_sfx(audio, AUDIO_SFX_COUNTDOWN_TICK);
		changed = true;
	}
	now = match_now_ms();
	if (now < *poll_due_ms)
		return (changed);
	*poll_due_ms = now + MP_MATCH_SELECT_POLL_MS;
	memset(&view, 0, sizeof(view));
	view.screen = APP_SCREEN_WAITING_ROOM;
	if (app_room_view_refresh(provider, room_id, &view) != APP_PROVIDER_OK)
		return (changed);
	state->selection.remaining_ms = view.data.room.select_ms;
	if (view.data.room.state == APP_ROOM_STATE_SELECTING)
		return (true);
	state->selection.locked = true;
	state->selection.remaining_ms = 0;
	state->phase = MP_MATCH_PLAYING;
	character = mp_match_selected_character(state);
	if (character != NULL)
		tetrisu_character_apply(ctx, character->name);
	return (true);
}

/**
 * @brief Tells the room which fighter this seat has settled on.
 *
 * It travels as a readiness because that is the route that carries a
 * character: the seat is already ready, and re-declaring is how it names one.
 * A failure is not reported to the player - the window has a clock behind it,
 * so a lock that never arrived costs them the early start and nothing else.
 *
 * @param provider Provider the declaration is sent through.
 * @param room_id The room being played in.
 * @param state Match model holding the highlighted fighter.
 */
static void	selection_send_lock(const t_app_data_provider *provider,
	const char *room_id, const t_mp_match_state *state)
{
	const t_app_catalogue_item_view_model	*character;
	t_app_room_view_model					room;

	character = mp_match_selected_character(state);
	if (provider == NULL || provider->ready_room == NULL || character == NULL)
		return ;
	memset(&room, 0, sizeof(room));
	(void)provider->ready_room(provider->userdata, room_id, true,
		character->item_id, &room);
}

static void	select_power(t_match_authority *authority,
	t_mp_match_state *state, t_audio_ctx *audio, uint32_t key)
{
	const t_app_catalogue_item_view_model	*character;
	int								index;

	character = mp_match_selected_character(state);
	index = (int)(key - '1');
	if (character == NULL || index < 0 || index >= APP_CHARACTER_ABILITY_COUNT)
		return ;
	if (match_authority_is_online(authority))
	{
		/*
		 * The level goes up, never the ability: which four a level selects
		 * from is decided by the character the account has equipped, and the
		 * server reads that itself rather than taking a client's word for it.
		 */
		(void)match_authority_ability(authority, state,
			(t_solo_ability)(SOLO_ABILITY_MIRURUN + index));
		if (state->local_game.ability_result == SOLO_ABILITY_RESULT_ACTIVATED)
			audio_play_sfx(audio, AUDIO_SFX_ABILITY_ACTIVATED);
		else
			audio_play_sfx(audio, AUDIO_SFX_ABILITY_REJECTED);
		return ;
	}
	/*
	 * The popover is hover-driven and the pointer leaving is what dismisses it,
	 * so a keypress deliberately does not raise it: nothing would ever take it
	 * back down, and it would sit over HOLD and NEXT for the rest of the match.
	 */
	/*
	 * The name is bounded by APP_TEXT_MAX and the banner by
	 * MP_MATCH_STATUS_MAX, which is the shorter of the two, so the name is
	 * cut to what is left after the suffix rather than letting snprintf
	 * decide where the sentence stops.
	 */
	snprintf(state->status, sizeof(state->status),
		"%.*s SELECTED - AWAITING SERVER TARGET AUTHORITY",
		(int)(MP_MATCH_STATUS_MAX - MP_MATCH_SELECTED_SUFFIX_LEN - 1),
		character->abilities[index].name);
	audio_play_sfx(audio, AUDIO_SFX_ABILITY_ACTIVATED);
}

static void	play_match_events(t_audio_ctx *audio, uint32_t events)
{
	if ((events & SOLO_EVENT_MOVE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_MOVE);
	if ((events & SOLO_EVENT_ROTATE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_ROTATE);
	if ((events & SOLO_EVENT_SOFT_DROP) != 0)
		audio_play_sfx(audio, AUDIO_SFX_SOFT_DROP);
	if ((events & SOLO_EVENT_HARD_DROP) != 0)
		audio_play_sfx(audio, AUDIO_SFX_HARD_DROP);
	else if ((events & SOLO_EVENT_LOCK) != 0)
		audio_play_sfx(audio, AUDIO_SFX_LANDING);
	if ((events & SOLO_EVENT_HOLD) != 0)
		audio_play_sfx(audio, AUDIO_SFX_HOLD);
	if ((events & SOLO_EVENT_PERFECT_CLEAR) != 0)
		audio_play_sfx(audio, AUDIO_SFX_PERFECT_CLEAR);
	else if ((events & SOLO_EVENT_TETRIS) != 0)
		audio_play_sfx(audio, AUDIO_SFX_TETRIS);
	else if ((events & SOLO_EVENT_TRIPLE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_TRIPLE);
	else if ((events & SOLO_EVENT_DOUBLE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_DOUBLE);
	else if ((events & SOLO_EVENT_SINGLE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_SINGLE);
}

/**
 * @brief Puts up a card for any ability that has just landed on this player.
 *
 * The effects themselves are the server's and were always enforced; what was
 * missing was any way to tell. Paralysis and a rotate key that had stopped
 * responding looked identical from this side, and the player had no reason to
 * suspect the first.
 *
 * A card and a sound rather than a screen shake: the board is a terminal
 * bitmap with no partial update, so moving it means re-encoding both boards
 * for every frame of the shake - which would cost exactly the input latency
 * the banding work went to remove, at the moment the player can least afford
 * it. The card is drawn on its own plane over the top and costs one region.
 *
 * @param authority Authority holding what was last reported.
 * @param state Match model carrying the server's latest counts.
 * @param ctx Render context to raise the card on.
 * @param audio Audio context, for the sting that goes with it.
 * @return true when a card went up and the frame should be presented.
 */
static bool	announce_effects(t_match_authority *authority,
		const t_mp_match_state *state, t_render_ctx *ctx, t_audio_ctx *audio)
{
	char	title[UI_NOTIFICATION_TITLE_MAX + 1];
	char	message[UI_NOTIFICATION_MESSAGE_MAX + 1];

	if (!match_authority_is_online(authority))
		return (false);
	if (!solo_effects_take_arrival(&state->local_game.effects,
			&authority->reported, title, sizeof(title), message,
			sizeof(message)))
		return (false);
	render_notification_show_effect(ctx, title, message);
	audio_play_sfx(audio, AUDIO_SFX_ABILITY_REJECTED);
	return (true);
}
