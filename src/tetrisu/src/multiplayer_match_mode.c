#include "tetrisu.h"

// Static Functions
static uint64_t	match_now_ms(void);
static uint32_t	match_seed(void);
static int	preview_player_count(t_app_game_mode mode);
static void	load_match_identity(const t_app_data_provider *provider,
				t_app_profile_view_model *profile,
				t_app_catalogue_view_model *characters);
static bool	handle_match_key(t_mp_match_state *state, t_render_ctx *ctx,
				t_audio_ctx *audio, uint32_t key, const ncinput *input,
				bool *rebuild, t_solo_handling_state *handling,
				const t_solo_handling_config *handling_config);
static bool	apply_game_key(t_mp_match_state *state, uint32_t key);
static bool	apply_handling_actions(t_mp_match_state *state,
				t_solo_handling_state *handling,
				const t_solo_handling_config *config, int elapsed_ms);
static void	select_power(t_mp_match_state *state, t_audio_ctx *audio,
				uint32_t key);
static void	play_match_events(t_audio_ctx *audio, uint32_t events);
static bool	match_mouse_pixel_position(const t_render_ctx *ctx,
				const ncinput *input, int *x, int *y);

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
	const char *room_id, const t_app_room_view_model *room)
{
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
	int						elapsed_ms;
	bool					leave;
	bool					changed;
	bool					rebuild;
	bool					mouse_enabled;
	int					input_batch;

	if (ctx == NULL || audio == NULL)
		return (-1);
	load_match_identity(provider, &profile, &characters);
	mp_match_state_init(&state, mode, room_id, &profile, &characters,
		match_seed());
	mp_match_apply_room(&state, room, preview_player_count(mode));
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
	previous_ms = match_now_ms();
	while (!leave)
	{
		if (!render_multiplayer_match_show(ctx, &state, rebuild))
		{
			render_multiplayer_match_destroy(ctx);
			return (-1);
		}
		rebuild = false;
		memset(&input, 0, sizeof(input));
		key = render_wait_input_timeout(ctx, &input, MP_MATCH_FRAME_MS);
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
		if (state.phase == MP_MATCH_CHARACTER_SELECT)
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
			changed = solo_game_update(&state.local_game, elapsed_ms) || changed;
			changed = solo_game_update(&state.opponent_game, elapsed_ms) || changed;
			if (!state.local_game.paused && !state.local_game.countdown_active
				&& state.local_game.phase == SOLO_ACTIVE)
				changed = apply_handling_actions(&state, &handling,
						&handling_config, elapsed_ms) || changed;
			play_match_events(audio, solo_game_take_events(&state.local_game));
			(void)solo_game_take_events(&state.opponent_game);
			if (state.local_game.phase == SOLO_GAME_OVER)
			{
				mp_match_finish(&state, false,
					state.mode == APP_GAME_MODE_BATTLE_ROYALE
					? state.players_alive : 2);
				changed = true;
			}
		}
		if (key == (uint32_t)-1)
			leave = true;
		else if (key != 0)
			leave = handle_match_key(&state, ctx, audio, key, &input, &rebuild,
					&handling, &handling_config);
		input_batch = 0;
		while (!leave && key != (uint32_t)-1
			&& input_batch < MP_MATCH_INPUT_BATCH_MAX)
		{
			memset(&input, 0, sizeof(input));
			key = notcurses_get_nblock(ctx->nc, &input);
			if (key == 0)
				break ;
			leave = handle_match_key(&state, ctx, audio, key, &input, &rebuild,
					&handling, &handling_config);
			input_batch++;
		}
		if (changed)
			continue ;
	}
	if (mouse_enabled)
		(void)notcurses_mice_disable(ctx->nc);
	render_multiplayer_match_destroy(ctx);
	return (0);
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

static bool	handle_match_key(t_mp_match_state *state, t_render_ctx *ctx,
	t_audio_ctx *audio, uint32_t key, const ncinput *input, bool *rebuild,
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
			select_power(state, audio, (uint32_t)('0' + ability));
		return (false);
	}
	if ((key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_DOWN)
		&& state->phase == MP_MATCH_PLAYING)
	{
		if (mp_match_movement_event(state, handling, handling_config, key,
				input != NULL ? input->evtype : NCTYPE_UNKNOWN, &action))
		{
			(void)solo_game_apply_action(&state->local_game, action);
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
		select_power(state, audio, key);
		return (false);
	}
	if (key == 'p' || key == 'P')
	{
		solo_game_toggle_pause(&state->local_game);
		solo_handling_reset(handling);
		return (false);
	}
	if (apply_game_key(state, key))
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

static bool	apply_game_key(t_mp_match_state *state, uint32_t key)
{
	if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (solo_game_apply_action(&state->local_game, SOLO_ROTATE_CW));
	if (key == 'z' || key == 'Z')
		return (solo_game_apply_action(&state->local_game, SOLO_ROTATE_CCW));
	if (key == ' ')
		return (solo_game_apply_action(&state->local_game, SOLO_HARD_DROP));
	if (key == 'c' || key == 'C')
		return (solo_game_apply_action(&state->local_game, SOLO_HOLD));
	return (false);
}

static bool	apply_handling_actions(t_mp_match_state *state,
	t_solo_handling_state *handling, const t_solo_handling_config *config,
	int elapsed_ms)
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
		changed = solo_game_apply_action(&state->local_game, actions[index])
			|| changed;
		index++;
	}
	return (changed);
}

static void	select_power(t_mp_match_state *state, t_audio_ctx *audio,
	uint32_t key)
{
	const t_app_catalogue_item_view_model	*character;
	int								index;

	character = mp_match_selected_character(state);
	index = (int)(key - '1');
	if (character == NULL || index < 0 || index >= APP_CHARACTER_ABILITY_COUNT)
		return ;
	snprintf(state->status, sizeof(state->status),
		"%s SELECTED - AWAITING SERVER TARGET AUTHORITY",
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
