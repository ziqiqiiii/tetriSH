#include "tetrisu.h"

// Static Functions
static int	restore_home(render_ctx_t *ctx);
static uint32_t	new_game_seed(void);
static uint64_t	monotonic_ms(void);
static int	milliseconds_until_render(uint64_t now_ms,
	uint64_t last_render_ms);
static bool	solo_display_ready(const solo_render_t *solo);
static uint32_t	wait_solo_input(render_ctx_t *ctx, int timeout_ms,
	ncinput *input, int *input_errno);
static bool	handle_solo_key(solo_game_t *game, audio_ctx_t *audio,
	render_ctx_t *ctx, solo_render_t *solo, uint32_t key,
	const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed,
	solo_handling_state_t *handling,
	const solo_handling_config_t *handling_config);
static bool	handle_solo_mouse(render_ctx_t *ctx, solo_render_t *solo,
	solo_game_t *game, uint32_t key, const ncinput *input,
	bool display_ready, bool resize_pending, bool *state_changed);
static bool	dispatch_game_key(solo_game_t *game, uint32_t key);
static bool	apply_handling_actions(solo_game_t *game,
				solo_handling_state_t *handling,
				const solo_handling_config_t *config, int elapsed_ms);

/**
 * @brief Runs the temporary local-authority Solo game loop.
 *
 * AI-assisted: this loop owns input timing and rendering only. Migration to
 * `tetrisd` replaces local apply/update calls with HTTTP actions and STATE
 * responses without changing the render lifecycle.
 *
 * @param ctx Pointer to the initialized render context.
 * @param audio Pointer to the initialized audio context.
 * @return 0 after restoring the home screen, or -1 when restoration fails.
 */
int	solo_mode_run(render_ctx_t *ctx, audio_ctx_t *audio)
{
	solo_game_t	game;
	solo_render_t	solo;
	solo_handling_config_t	handling_config;
	solo_handling_state_t	handling;
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
	solo_game_init(&game, new_game_seed());
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
		if (display_ready && !game.paused && game.phase == SOLO_ACTIVE)
		{
			handling_wake_ms = solo_handling_next_wake_ms(&handling,
					&handling_config, gravity_interval_ms(game.level));
			if (wake_ms < 0 || (handling_wake_ms >= 0
					&& handling_wake_ms < wake_ms))
				wake_ms = handling_wake_ms;
		}
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
		if (input_backlog)
		{
			errno = 0;
			key = notcurses_get_nblock(ctx->nc, &input);
			input_errno = errno;
		}
		else
			key = wait_solo_input(ctx, wait_ms, &input, &input_errno);
		now_ms = monotonic_ms();
		if (now_ms < previous_ms)
			elapsed_ms = 0;
		else if (now_ms - previous_ms > SOLO_MAX_CATCHUP_MS)
			elapsed_ms = SOLO_MAX_CATCHUP_MS;
		else
			elapsed_ms = (int)(now_ms - previous_ms);
		previous_ms = now_ms;
		needs_draw = false;
		force_render = false;
		if (display_ready)
			needs_draw = solo_game_update(&game, elapsed_ms);
		if (display_ready && !game.paused && game.phase == SOLO_ACTIVE)
			needs_draw = apply_handling_actions(&game, &handling,
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
			if (handle_solo_key(&game, audio, ctx, &solo, key, &input,
					display_ready,
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
		if (resize_pending)
		{
			solo_handling_reset(&handling);
			render_solo_resize(ctx, &solo);
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
	render_solo_destroy(&solo);
	return (restore_home(ctx));
}

/**
 * @brief Restores the background and selector after leaving Solo mode.
 *
 * @param ctx Pointer to the render context.
 * @return 0 on success, or -1 when the homepage image cannot be restored.
 */
static int	restore_home(render_ctx_t *ctx)
{
	ncplane_erase(ctx->std);
	if (render_background_replace(ctx, SPLASH_ASSET_PATH, false) < 0)
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
static bool	solo_display_ready(const solo_render_t *solo)
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
static uint32_t	wait_solo_input(render_ctx_t *ctx, int timeout_ms,
	ncinput *input, int *input_errno)
{
	struct pollfd	poll_fd;
	uint32_t		key;
	int				result;

	poll_fd.fd = notcurses_inputready_fd(ctx->nc);
	poll_fd.events = POLLIN;
	poll_fd.revents = 0;
	if (poll_fd.fd < 0)
	{
		*input_errno = EIO;
		return ((uint32_t)-1);
	}
	errno = 0;
	result = poll(&poll_fd, 1, timeout_ms);
	*input_errno = errno;
	if (result < 0)
		return ((uint32_t)-1);
	if (result == 0)
		return (0);
	if ((poll_fd.revents & POLLIN) == 0)
	{
		*input_errno = EIO;
		return ((uint32_t)-1);
	}
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
static bool	handle_solo_key(solo_game_t *game, audio_ctx_t *audio,
	render_ctx_t *ctx, solo_render_t *solo, uint32_t key,
	const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed,
	solo_handling_state_t *handling,
	const solo_handling_config_t *handling_config)
{
	solo_action_t	action;

	if (nckey_mouse_p(key))
		return (handle_solo_mouse(ctx, solo, game, key, input,
				display_ready, *resize_pending, state_changed));
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_DOWN)
	{
		if (game->paused || game->phase != SOLO_ACTIVE
			|| !display_ready || *resize_pending)
		{
			if (input->evtype == NCTYPE_RELEASE)
				(void)solo_handling_event(handling, handling_config, key,
					input->evtype, &action);
			return (false);
		}
		if (solo_handling_event(handling, handling_config, key,
				input->evtype, &action))
			*state_changed = solo_game_apply_action(game, action)
				|| *state_changed;
		return (false);
	}
	if (input->evtype == NCTYPE_RELEASE)
		return (false);
	if (key == NCKEY_ESC || key == NCKEY_EOF || key == 'q' || key == 'Q')
		return (true);
	if (key == NCKEY_RESIZE || key == 12u)
	{
		*resize_pending = true;
		return (false);
	}
	if (key == '+' || key == '=')
	{
		audio_volume_up(audio);
		return (false);
	}
	if (key == '-' || key == '_')
	{
		audio_volume_down(audio);
		return (false);
	}
	if ((key == 'r' || key == 'R') && game->phase == SOLO_GAME_OVER)
	{
		solo_game_init(game, new_game_seed());
		solo_handling_reset(handling);
		*state_changed = true;
		return (false);
	}
	if (key == 'p' || key == 'P')
	{
		solo_game_toggle_pause(game);
		solo_handling_reset(handling);
		*state_changed = true;
		return (false);
	}
	if (display_ready && !*resize_pending)
		*state_changed = dispatch_game_key(game, key) || *state_changed;
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
static bool	handle_solo_mouse(render_ctx_t *ctx, solo_render_t *solo,
	solo_game_t *game, uint32_t key, const ncinput *input,
	bool display_ready, bool resize_pending, bool *state_changed)
{
	solo_ability_result_t	result;
	solo_ability_t			ability;
	int						canvas_x;
	int						canvas_y;

	ability = SOLO_ABILITY_NONE;
	if (display_ready && !resize_pending
		&& solo_mouse_canvas_position(ctx, solo, input, &canvas_x, &canvas_y))
		ability = solo_ability_at_canvas(canvas_x, canvas_y);
	if (ability != solo->hovered_ability)
	{
		solo->hovered_ability = ability;
		*state_changed = true;
	}
	if (key != NCKEY_BUTTON1 || ability == SOLO_ABILITY_NONE
		|| (input->evtype != NCTYPE_PRESS
			&& input->evtype != NCTYPE_UNKNOWN))
		return (false);
	result = solo_game_activate_ability(game, ability);
	if (result != SOLO_ABILITY_RESULT_INVALID)
		*state_changed = true;
	return (false);
}

/**
 * @brief Maps a gameplay key to one local game action.
 *
 * @param game Pointer to the local Solo state.
 * @param key Notcurses key code or Unicode code point.
 * @return true when the action changed the game state, otherwise false.
 */
static bool	dispatch_game_key(solo_game_t *game, uint32_t key)
{
	solo_ability_result_t	result;

	if (key >= '1' && key <= '4')
	{
		result = solo_game_activate_ability(game,
			(solo_ability_t)(SOLO_ABILITY_MIRURUN + key - '1'));
		return (result != SOLO_ABILITY_RESULT_INVALID);
	}
	if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (solo_game_apply_action(game, SOLO_ROTATE_CW));
	else if (key == 'z' || key == 'Z')
		return (solo_game_apply_action(game, SOLO_ROTATE_CCW));
	else if (key == ' ')
		return (solo_game_apply_action(game, SOLO_HARD_DROP));
	else if (key == 'c' || key == 'C')
		return (solo_game_apply_action(game, SOLO_HOLD));
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
static bool	apply_handling_actions(solo_game_t *game,
	solo_handling_state_t *handling,
	const solo_handling_config_t *config, int elapsed_ms)
{
	solo_action_t	actions[SOLO_HANDLING_ACTION_CAP];
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
		changed = solo_game_apply_action(game, actions[index]) || changed;
		index++;
	}
	return (changed);
}
