#include "tetrisu.h"

// Static Functions
static int	restore_home(t_render_ctx *ctx);
static uint32_t	new_game_seed(void);
static uint64_t	monotonic_ms(void);
static int	milliseconds_until_render(uint64_t now_ms,
	uint64_t last_render_ms);
static bool	solo_display_ready(const t_solo_render *solo);
static uint32_t	wait_solo_input(t_render_ctx *ctx,
	const t_solo_authority *authority, int timeout_ms,
	ncinput *input, int *input_errno);
static bool	handle_solo_key(t_solo_authority *authority, t_solo_game *game,
	t_audio_ctx *audio, t_render_ctx *ctx, t_solo_render *solo, uint32_t key,
	const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed,
	t_solo_handling_state *handling,
	const t_solo_handling_config *handling_config);
static bool	handle_solo_mouse(t_solo_authority *authority, t_render_ctx *ctx,
	t_solo_render *solo, t_solo_game *game, uint32_t key,
	const ncinput *input, bool display_ready, bool resize_pending,
	bool *state_changed);
static bool	dispatch_game_key(t_solo_authority *authority,
				t_solo_game *game, uint32_t key);
static bool	apply_handling_actions(t_solo_authority *authority,
				t_solo_game *game, t_solo_handling_state *handling,
				const t_solo_handling_config *config, int elapsed_ms);
static void	play_solo_events(t_audio_ctx *audio, uint32_t events);
static uint64_t	load_personal_best(const t_solo_authority *authority);

/**
 * @brief Runs the Solo game loop.
 *
 * This loop owns input timing and rendering, and nothing else: who owns the
 * board is solo_authority.c's question, and it answers either "tetrisd" or
 * "the local rules" depending on whether the app has a session. The render
 * lifecycle is the same either way, which is what the migration was for.
 *
 * The wait is over the terminal *and* the session when there is one, so a
 * pushed snapshot wakes the loop as promptly as a keypress does rather than
 * waiting out a poll interval.
 *
 * @param ctx Pointer to the initialized render context.
 * @param audio Pointer to the initialized audio context.
 * @param net The app's session, or NULL to play offline.
 * @return 0 after restoring the home screen, or -1 when restoration fails.
 */
int	solo_mode_run(t_render_ctx *ctx, t_audio_ctx *audio, t_net_client *net)
{
	t_solo_authority	authority;
	t_solo_game	game;
	t_solo_render	solo;
	t_solo_handling_config	handling_config;
	t_solo_handling_state	handling;
	ncinput			input;
	uint32_t		key;
	uint64_t		previous_ms;
	uint64_t		now_ms;
	uint64_t		last_render_ms;
	int				elapsed_ms;
	int				input_errno;
	int				input_batch;
	int				wake_ms;
	int				wait_ms;
	int				render_wait_ms;
	int				handling_wake_ms;
	int				notification_wake_ms;
	int				popover_wake_ms;
	int				audio_wake_ms;
	bool			leave;
	bool			resize_pending;
	bool			display_ready;
	bool			needs_draw;
	bool			render_pending;
	bool			input_backlog;
	bool			force_render;
	bool			mouse_enabled;

	render_menu_destroy(ctx);
	render_background_destroy(ctx);
	ncplane_erase(ctx->std);
	if (render_geometry_refresh(ctx, false) < 0)
	{
		(void)restore_home(ctx);
		return (-1);
	}
	solo_authority_open(&authority, net, &game, new_game_seed());
	solo_game_set_personal_best(&game, load_personal_best(&authority));
	solo_game_start_countdown(&game);
	play_solo_events(audio, solo_game_take_events(&game));
	handling_config = solo_handling_default_config();
	solo_handling_reset(&handling);
	render_solo_create(ctx, &solo);
	render_solo_draw(ctx, &solo, &game);
	mouse_enabled = notcurses_mice_enable(ctx->nc,
		NCMICE_ALL_EVENTS) == 0;
	previous_ms = monotonic_ms();
	last_render_ms = previous_ms;
	render_pending = false;
	input_backlog = false;
	leave = false;
	while (!leave)
	{
		display_ready = solo_display_ready(&solo);
		wake_ms = display_ready ? solo_game_next_wake_ms(&game) : -1;
		if (display_ready && !game.paused && !game.countdown_active
			&& game.phase == SOLO_ACTIVE)
		{
			handling_wake_ms = solo_handling_next_wake_ms(&handling,
					&handling_config, gravity_interval_ms(game.level));
			if (wake_ms < 0 || (handling_wake_ms >= 0
					&& handling_wake_ms < wake_ms))
				wake_ms = handling_wake_ms;
		}
		notification_wake_ms = render_notification_next_wake_ms(ctx);
		if (wake_ms < 0 || (notification_wake_ms >= 0
				&& notification_wake_ms < wake_ms))
			wake_ms = notification_wake_ms;
		popover_wake_ms = solo_popover_next_wake_ms(&solo);
		if (wake_ms < 0 || (popover_wake_ms >= 0
				&& popover_wake_ms < wake_ms))
			wake_ms = popover_wake_ms;
		audio_wake_ms = audio_next_wake_ms(audio);
		if (wake_ms < 0 || (audio_wake_ms >= 0
				&& audio_wake_ms < wake_ms))
			wake_ms = audio_wake_ms;
		wait_ms = wake_ms;
		if (wait_ms < 0 || wait_ms > RENDER_RESIZE_POLL_MS)
			wait_ms = RENDER_RESIZE_POLL_MS;
		if (render_pending)
		{
			render_wait_ms = milliseconds_until_render(monotonic_ms(),
					last_render_ms);
			if (render_wait_ms < wait_ms)
				wait_ms = render_wait_ms;
		}
		/*
		 * A snapshot already in hand is not something to sleep on. The reply to
		 * a move and the snapshot it caused arrive together often enough that
		 * the request read both, and the socket then has nothing left to make
		 * the poll return - so the frame the player is waiting for sat here for
		 * a whole poll interval before anything looked at it.
		 */
		if (solo_authority_pending(&authority))
			wait_ms = 0;
		if (input_backlog)
		{
			errno = 0;
			key = notcurses_get_nblock(ctx->nc, &input);
			input_errno = errno;
		}
		else
			key = wait_solo_input(ctx, &authority, wait_ms, &input,
					&input_errno);
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		now_ms = monotonic_ms();
		if (now_ms < previous_ms)
			elapsed_ms = 0;
		else if (now_ms - previous_ms > SOLO_MAX_CATCHUP_MS)
			elapsed_ms = SOLO_MAX_CATCHUP_MS;
		else
			elapsed_ms = (int)(now_ms - previous_ms);
		previous_ms = now_ms;
		(void)audio_update(audio, elapsed_ms);
		needs_draw = false;
		force_render = false;
		if (display_ready)
			needs_draw = solo_authority_update(&authority, &game,
					elapsed_ms);
		if (display_ready && solo_game_finish_personal_best(&game))
		{
			/* Online the record is the account's, and tetrisd already wrote
			** it when it recorded the game; writing the file too would leave
			** this machine claiming a score nobody playing offline set. */
			if (!solo_authority_is_online(&authority))
				(void)solo_best_store(game.personal_best);
			needs_draw = true;
		}
		if (display_ready && solo_game_update_danger(&game, elapsed_ms))
		{
			if (game.danger_active)
				audio_transition_music(audio, ctx->theme_assets.danger_music,
					AUDIO_MUSIC_TRANSITION_MS);
			else
				audio_transition_music(audio, ctx->theme_assets.music,
					AUDIO_MUSIC_TRANSITION_MS);
			needs_draw = true;
		}
		needs_draw = solo_popover_update(&solo, &game, elapsed_ms)
			|| needs_draw;
		if (display_ready && !game.paused && !game.countdown_active
			&& game.phase == SOLO_ACTIVE)
			needs_draw = apply_handling_actions(&authority, &game, &handling,
					&handling_config, elapsed_ms) || needs_draw;
		resize_pending = render_terminal_geometry_changed(ctx);
		if (key == (uint32_t)-1)
		{
			if (input_errno == EINTR)
				key = 0;
			else
				break ;
		}
		input_backlog = false;
		input_batch = 0;
		while (key != 0)
		{
			if (handle_solo_key(&authority, &game, audio, ctx, &solo, key,
					&input, display_ready,
					&resize_pending, &needs_draw, &handling,
					&handling_config))
			{
				leave = true;
				break ;
			}
			input_batch++;
			if (input_batch >= SOLO_INPUT_BATCH_MAX)
			{
				input_backlog = true;
				break ;
			}
			errno = 0;
			key = notcurses_get_nblock(ctx->nc, &input);
			if (key == (uint32_t)-1)
			{
				if (errno != EINTR)
					leave = true;
				break ;
			}
		}
		if (leave)
			break ;
		play_solo_events(audio, solo_game_take_events(&game));
		if (resize_pending)
		{
			solo_handling_reset(&handling);
			render_solo_resize(ctx, &solo);
			render_notification_reflow(ctx);
			needs_draw = true;
			force_render = true;
			/* Reflow is an explicit pause; protocol negotiation time must not
			 * be charged to gameplay gravity. */
			previous_ms = monotonic_ms();
		}
		render_pending = render_pending || needs_draw;
		now_ms = monotonic_ms();
		if (render_pending && (force_render
				|| milliseconds_until_render(now_ms, last_render_ms) == 0))
		{
			render_solo_draw(ctx, &solo, &game);
			last_render_ms = monotonic_ms();
			render_pending = false;
		}
	}
	if (mouse_enabled)
		(void)notcurses_mice_disable(ctx->nc);
	solo_authority_close(&authority);
	render_solo_destroy(&solo);
	audio_transition_music(audio, ctx->theme_assets.music, 0);
	return (restore_home(ctx));
}

/**
 * @brief Restores the background and selector after leaving Solo mode.
 *
 * @param ctx Pointer to the render context.
 * @return 0 on success, or -1 when the homepage image cannot be restored.
 */
static int	restore_home(t_render_ctx *ctx)
{
	ncplane_erase(ctx->std);
	if (render_background_replace(ctx, ctx->theme_assets.homepage, false) < 0)
		return (-1);
	render_menu_create(ctx);
	return (0);
}

/**
 * @brief Creates a process-local seed for a new seven-bag sequence.
 *
 * @return Seed mixed from monotonic time and process identifier.
 */
static uint32_t	new_game_seed(void)
{
	return ((uint32_t)(monotonic_ms() ^ (uint64_t)getpid()));
}

/**
 * @brief Reads the monotonic clock in whole milliseconds.
 *
 * @return Milliseconds from the platform monotonic epoch.
 */
static uint64_t	monotonic_ms(void)
{
	struct timespec	now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
}

/**
 * @brief Returns the remaining delay in the bounded Solo presentation rate.
 *
 * AI-assisted: gameplay and input remain immediate, while coalescing visual
 * updates prevents pixel-protocol frames from outrunning the terminal parser.
 * Clock rollback is treated as immediately due rather than underflowing.
 *
 * @param now_ms Current monotonic time.
 * @param last_render_ms Time of the previous terminal presentation.
 * @return Milliseconds until another frame may be presented.
 */
static int	milliseconds_until_render(uint64_t now_ms,
	uint64_t last_render_ms)
{
	uint64_t	elapsed_ms;

	if (now_ms < last_render_ms)
		return (0);
	elapsed_ms = now_ms - last_render_ms;
	if (elapsed_ms >= SOLO_RENDER_INTERVAL_MS)
		return (0);
	return (SOLO_RENDER_INTERVAL_MS - (int)elapsed_ms);
}

/**
 * @brief Reports whether the renderer can accept gameplay updates.
 *
 * @param solo Pointer to the Solo render state.
 * @return true when layout, assets, and planes are ready.
 */
static bool	solo_display_ready(const t_solo_render *solo)
{
	return (solo->layout_valid && solo->assets_ready && solo->planes_ready);
}

/**
 * @brief Waits for input without using Notcurses' timed condition wait.
 *
 * Notcurses 3.0.17 on macOS can consume a core in its timed wait path. Its
 * documented input-ready descriptor integrates with `poll()` and preserves
 * the same state-timer deadline without busy waiting.
 *
 * @param ctx Pointer to the render context.
 * @param timeout_ms Maximum number of milliseconds to wait.
 * @param input Output metadata for the received key.
 * @param input_errno Output errno captured beside the result.
 * @return A key code, 0 on timeout, or `(uint32_t)-1` on failure.
 */
static uint32_t	wait_solo_input(t_render_ctx *ctx,
	const t_solo_authority *authority, int timeout_ms,
	ncinput *input, int *input_errno)
{
	struct pollfd	poll_fd[2];
	uint32_t		key;
	int				count;
	int				result;

	poll_fd[0].fd = notcurses_inputready_fd(ctx->nc);
	poll_fd[0].events = POLLIN;
	poll_fd[0].revents = 0;
	if (poll_fd[0].fd < 0)
	{
		*input_errno = EIO;
		return ((uint32_t)-1);
	}
	count = 1;
	poll_fd[1].fd = solo_authority_fd(authority);
	poll_fd[1].events = POLLIN;
	poll_fd[1].revents = 0;
	if (poll_fd[1].fd >= 0)
		count = 2;
	errno = 0;
	result = poll(poll_fd, (nfds_t)count, timeout_ms);
	*input_errno = errno;
	if (result < 0)
		return ((uint32_t)-1);
	if (result == 0 || (poll_fd[0].revents & POLLIN) == 0)
		return (0);
	errno = 0;
	key = notcurses_get_nblock(ctx->nc, input);
	*input_errno = errno;
	return (key);
}

/**
 * @brief Handles one Solo input event.
 *
 * @param game Pointer to the local Solo state.
 * @param audio Pointer to the audio context.
 * @param ctx Active render context used for mouse coordinate conversion.
 * @param solo Solo renderer containing layout and hover state.
 * @param key Notcurses key code or Unicode code point.
 * @param input Pointer to the input metadata.
 * @param display_ready Whether gameplay rendering is currently available.
 * @param resize_pending In/out resize request flag.
 * @param state_changed In/out redraw request flag.
 * @return true when the Solo loop should return to the home screen.
 */
static bool	handle_solo_key(t_solo_authority *authority, t_solo_game *game,
	t_audio_ctx *audio, t_render_ctx *ctx, t_solo_render *solo, uint32_t key,
	const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed,
	t_solo_handling_state *handling,
	const t_solo_handling_config *handling_config)
{
	t_solo_action	action;

	if (nckey_mouse_p(key))
		return (handle_solo_mouse(authority, ctx, solo, game, key, input,
				display_ready, *resize_pending, state_changed));
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_DOWN)
	{
		if (game->paused || game->countdown_active
			|| game->phase != SOLO_ACTIVE
			|| !display_ready || *resize_pending)
		{
			if (input->evtype == NCTYPE_RELEASE)
				(void)solo_handling_event(handling, handling_config, key,
					input->evtype, &action);
			return (false);
		}
		if (solo_handling_event(handling, handling_config, key,
				input->evtype, &action))
			*state_changed = solo_authority_action(authority, game, action)
				|| *state_changed;
		return (false);
	}
	if (input->evtype == NCTYPE_RELEASE)
		return (false);
	if (key == NCKEY_EOF)
		return (true);
	if (key == NCKEY_ESC || key == 'q' || key == 'Q')
	{
		if (confirmation_prompt_run(ctx, audio, CONFIRM_LEAVE_MATCH))
			return (true);
		*state_changed = true;
		return (false);
	}
	if (key == NCKEY_RESIZE || key == 12u)
	{
		*resize_pending = true;
		return (false);
	}
	/*
	 * The card is a bitmap over the board's bitmap, and notcurses wipes the
	 * sprixel underneath rather than overlapping it - so the frame it covered
	 * has to be drawn again, which nothing else here would ask for.
	 */
	if (key == '+' || key == '=')
	{
		audio_volume_up(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		*state_changed = true;
		return (false);
	}
	if (key == '-' || key == '_')
	{
		audio_volume_down(audio);
		render_notification_show_volume(ctx, audio->music_volume);
		*state_changed = true;
		return (false);
	}
	if ((key == 'r' || key == 'R') && game->phase == SOLO_GAME_OVER)
	{
		*state_changed = solo_authority_restart(authority, game,
				new_game_seed()) || *state_changed;
		audio_transition_music(audio, ctx->theme_assets.music,
			AUDIO_MUSIC_TRANSITION_MS);
		solo_handling_reset(handling);
		return (false);
	}
	if (key == 'p' || key == 'P')
	{
		*state_changed = solo_authority_pause(authority, game)
			|| *state_changed;
		solo_handling_reset(handling);
		return (false);
	}
	if (display_ready && !*resize_pending)
		*state_changed = dispatch_game_key(authority, game, key)
			|| *state_changed;
	return (false);
}

/**
 * @brief Handles hover and primary-button activation over the ability meter.
 *
 * AI-assisted: absolute terminal coordinates are transformed through the
 * current fitted canvas before hit-testing, so resizes and differing cell
 * pixel sizes cannot desynchronize the visible circle and click target.
 *
 * @param ctx Active render context.
 * @param solo Solo renderer containing fitted canvas geometry.
 * @param game Local Solo state receiving an activation.
 * @param key Notcurses mouse event identifier.
 * @param input Mouse coordinates and event type.
 * @param display_ready Whether the Solo bitmap is currently usable.
 * @param resize_pending Whether a geometry reflow is pending.
 * @param state_changed In/out redraw request flag.
 * @return false; mouse input never exits Solo mode.
 */
static bool	handle_solo_mouse(t_solo_authority *authority, t_render_ctx *ctx,
	t_solo_render *solo, t_solo_game *game, uint32_t key,
	const ncinput *input, bool display_ready, bool resize_pending,
	bool *state_changed)
{
	t_solo_ability	ability;
	int				canvas_x;
	int				canvas_y;

	ability = SOLO_ABILITY_NONE;
	if (display_ready && !resize_pending
		&& solo_mouse_canvas_position(ctx, solo, input, &canvas_x, &canvas_y))
		ability = solo_ability_at_canvas(canvas_x, canvas_y);
	if (solo_popover_set_hover(solo, ability))
		*state_changed = true;
	if (key != NCKEY_BUTTON1 || ability == SOLO_ABILITY_NONE
		|| (input->evtype != NCTYPE_PRESS
			&& input->evtype != NCTYPE_UNKNOWN))
		return (false);
	if (game->countdown_active)
		return (false);
	if (solo_authority_ability(authority, game, ability))
		*state_changed = true;
	return (false);
}

/**
 * @brief Maps a gameplay key to one action, whoever ends up applying it.
 *
 * @param authority Whoever owns the board.
 * @param game Solo view model.
 * @param key Notcurses key code or Unicode code point.
 * @return true when the renderer has something new to draw.
 */
static bool	dispatch_game_key(t_solo_authority *authority,
	t_solo_game *game, uint32_t key)
{
	if (game->countdown_active)
		return (false);
	if (key >= '1' && key <= '4')
		return (solo_authority_ability(authority, game,
				(t_solo_ability)(SOLO_ABILITY_MIRURUN + key - '1')));
	if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (solo_authority_action(authority, game, SOLO_ROTATE_CW));
	else if (key == 'z' || key == 'Z')
		return (solo_authority_action(authority, game, SOLO_ROTATE_CCW));
	else if (key == ' ')
		return (solo_authority_action(authority, game, SOLO_HARD_DROP));
	else if (key == 'c' || key == 'C')
		return (solo_authority_action(authority, game, SOLO_HOLD));
	return (false);
}

/**
 * @brief Applies every repeat due on the shared monotonic loop clock.
 *
 * @param game Active Solo state.
 * @param handling Mutable key-repeat state.
 * @param config DAS, ARR, and soft-drop configuration.
 * @param elapsed_ms Elapsed monotonic time.
 * @return true when at least one action changed visible game state.
 */
static bool	apply_handling_actions(t_solo_authority *authority,
	t_solo_game *game, t_solo_handling_state *handling,
	const t_solo_handling_config *config, int elapsed_ms)
{
	t_solo_action	actions[SOLO_HANDLING_ACTION_CAP];
	int				count;
	int				index;
	bool			changed;

	count = solo_handling_update(handling, config,
			gravity_interval_ms(game->level), elapsed_ms, actions,
			SOLO_HANDLING_ACTION_CAP);
	changed = false;
	index = 0;
	while (index < count)
	{
		changed = solo_authority_action(authority, game, actions[index])
			|| changed;
		index++;
	}
	return (changed);
}

/**
 * @brief Plays the supplied WAV mapped to each accumulated gameplay event.
 *
 * Hard drops suppress the simultaneous landing clip, and perfect clears
 * suppress the ordinary line-count clip, keeping layered feedback readable.
 *
 * @param audio Active audio context.
 * @param events Bitmask of t_solo_event values.
 */
static void	play_solo_events(t_audio_ctx *audio, uint32_t events)
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
	if ((events & SOLO_EVENT_ABILITY_READY) != 0)
		audio_play_sfx(audio, AUDIO_SFX_ABILITY_READY);
	if ((events & SOLO_EVENT_ABILITY_ACTIVATED) != 0)
		audio_play_sfx(audio, AUDIO_SFX_ABILITY_ACTIVATED);
	if ((events & SOLO_EVENT_ABILITY_REJECTED) != 0)
		audio_play_sfx(audio, AUDIO_SFX_ABILITY_REJECTED);
	if ((events & SOLO_EVENT_PAUSE) != 0)
		audio_play_sfx(audio, AUDIO_SFX_PAUSE);
	if ((events & SOLO_EVENT_LEVEL_UP) != 0)
		audio_play_sfx(audio, AUDIO_SFX_LEVEL_UP);
	if ((events & SOLO_EVENT_PERSONAL_BEST) != 0)
		audio_play_sfx(audio, AUDIO_SFX_PERSONAL_BEST);
	if ((events & SOLO_EVENT_COUNTDOWN_TICK) != 0)
		audio_play_sfx(audio, AUDIO_SFX_COUNTDOWN_TICK);
	if ((events & SOLO_EVENT_COUNTDOWN_GO) != 0)
		audio_play_sfx(audio, AUDIO_SFX_COUNTDOWN_GO);
}

/**
 * @brief Loads the best score this game is to be measured against.
 *
 * Online the record belongs to the account, so it comes out of the same
 * PROFILE the Marketplace reads and follows the player to whatever terminal
 * they sign in at. Offline there is no account to ask and the machine's own
 * file stands in, which is also the fallback when the server will not answer -
 * starting a signed-in player at zero would announce their next bad game as a
 * personal best.
 *
 * @param authority Authority running this game.
 * @return The best score to compare this game against.
 */
static uint64_t	load_personal_best(const t_solo_authority *authority)
{
	t_body_profile	profile;

	if (solo_authority_is_online(authority)
		&& net_profile(authority->net, &profile) == 0)
		return (profile.score);
	return (solo_best_load());
}
