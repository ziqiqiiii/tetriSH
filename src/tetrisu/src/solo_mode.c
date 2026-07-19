#include "tetrisu.h"

// Static Functions
static int	restore_home(render_ctx_t *ctx);
static uint32_t	new_game_seed(void);
static uint64_t	monotonic_ms(void);
static bool	solo_display_ready(const solo_render_t *solo);
static uint32_t	wait_solo_input(render_ctx_t *ctx, int timeout_ms,
	ncinput *input, int *input_errno);
static bool	terminal_geometry_changed(const render_ctx_t *ctx);
static bool	handle_solo_key(solo_game_t *game, audio_ctx_t *audio,
	uint32_t key, const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed);
static bool	dispatch_game_key(solo_game_t *game, uint32_t key);

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
	ncinput			input;
	uint32_t		key;
	uint64_t		previous_ms;
	uint64_t		now_ms;
	int				elapsed_ms;
	int				input_errno;
	int				wake_ms;
	int				wait_ms;
	bool			leave;
	bool			resize_pending;
	bool			display_ready;
	bool			needs_draw;

	render_menu_destroy(ctx);
	render_background_destroy(ctx);
	ncplane_erase(ctx->std);
	if (render_geometry_refresh(ctx, false) < 0)
	{
		(void)restore_home(ctx);
		return (-1);
	}
	solo_game_init(&game, new_game_seed());
	render_solo_create(ctx, &solo);
	render_solo_draw(ctx, &solo, &game);
	previous_ms = monotonic_ms();
	leave = false;
	while (!leave)
	{
		display_ready = solo_display_ready(&solo);
		wake_ms = display_ready ? solo_game_next_wake_ms(&game) : -1;
		wait_ms = wake_ms;
		if (wait_ms < 0 || wait_ms > SOLO_RESIZE_POLL_MS)
			wait_ms = SOLO_RESIZE_POLL_MS;
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
		if (display_ready)
			needs_draw = solo_game_update(&game, elapsed_ms);
		resize_pending = terminal_geometry_changed(ctx);
		if (key == (uint32_t)-1)
		{
			if (input_errno == EINTR)
				key = 0;
			else
				break ;
		}
		while (key != 0)
		{
			if (handle_solo_key(&game, audio, key, &input, display_ready,
					&resize_pending, &needs_draw))
			{
				leave = true;
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
			render_solo_resize(ctx, &solo);
			needs_draw = true;
			/* Reflow is an explicit pause; protocol negotiation time must not
			 * be charged to gameplay gravity. */
			previous_ms = monotonic_ms();
		}
		if (needs_draw)
			render_solo_draw(ctx, &solo, &game);
	}
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
 * @brief Detects terminal resize changes not delivered as input events.
 *
 * @param ctx Pointer to the render context.
 * @return true when the tty and standard-plane dimensions differ.
 */
static bool	terminal_geometry_changed(const render_ctx_t *ctx)
{
	struct winsize	terminal;
	unsigned		plane_rows;
	unsigned		plane_cols;

	memset(&terminal, 0, sizeof(terminal));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal) != 0
		|| terminal.ws_row == 0 || terminal.ws_col == 0)
		return (false);
	ncplane_dim_yx(ctx->std, &plane_rows, &plane_cols);
	return (terminal.ws_row != plane_rows || terminal.ws_col != plane_cols);
}

/**
 * @brief Handles one non-release Solo input event.
 *
 * @param game Pointer to the local Solo state.
 * @param audio Pointer to the audio context.
 * @param key Notcurses key code or Unicode code point.
 * @param input Pointer to the input metadata.
 * @param display_ready Whether gameplay rendering is currently available.
 * @param resize_pending In/out resize request flag.
 * @param state_changed In/out redraw request flag.
 * @return true when the Solo loop should return to the home screen.
 */
static bool	handle_solo_key(solo_game_t *game, audio_ctx_t *audio,
	uint32_t key, const ncinput *input, bool display_ready,
	bool *resize_pending, bool *state_changed)
{
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
		*state_changed = true;
		return (false);
	}
	if (key == 'p' || key == 'P')
	{
		solo_game_toggle_pause(game);
		*state_changed = true;
		return (false);
	}
	if (display_ready && !*resize_pending)
		*state_changed = dispatch_game_key(game, key) || *state_changed;
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
	if (key == NCKEY_LEFT)
		return (solo_game_apply_action(game, SOLO_MOVE_LEFT));
	else if (key == NCKEY_RIGHT)
		return (solo_game_apply_action(game, SOLO_MOVE_RIGHT));
	else if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (solo_game_apply_action(game, SOLO_ROTATE_CW));
	else if (key == 'z' || key == 'Z')
		return (solo_game_apply_action(game, SOLO_ROTATE_CCW));
	else if (key == NCKEY_DOWN)
		return (solo_game_apply_action(game, SOLO_SOFT_DROP));
	else if (key == ' ')
		return (solo_game_apply_action(game, SOLO_HARD_DROP));
	return (false);
}
