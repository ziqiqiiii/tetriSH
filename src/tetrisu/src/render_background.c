#include "tetrisu.h"

// Static Functions
static tetrisu_pixel_policy_t	detect_pixel_policy(const render_ctx_t *ctx);
static void	refresh_cell_geometry(render_ctx_t *ctx);
static void	fit_background_to_terminal(render_ctx_t *ctx,
	int std_rows, int std_cols);
static int	max_int(int a, int b);
static ncblitter_e	preferred_blitter(const render_ctx_t *ctx,
	int rows, int cols);
static void	set_opaque_backdrop(struct ncplane *plane);
static int	replace_visual_scaled(render_ctx_t *ctx, struct ncvisual *ncv,
				bool stretch, ncscale_e scaling, ncblitter_e blitter,
				uint64_t flags);

/**
 * @brief Starts notcurses and renders the initial background image.
 *
 * The background owns a child plane below menu/game overlays so later modes
 * can replace it without restarting the terminal session.
 *
 * @param image_path Image rendered behind the home screen.
 * @return Fully initialised render context; exits when setup cannot recover.
 */
render_ctx_t	render_init(const char *image_path)
{
	render_ctx_t		ctx;
	notcurses_options	opts;
	const char			*term;

	memset(&ctx, 0, sizeof(ctx));
	memset(&opts, 0, sizeof(opts));
	ctx.nc = notcurses_init(&opts, NULL);
	if (ctx.nc == NULL)
	{
		term = getenv("TERM");
		fprintf(stderr, "tetrisu: notcurses_core_init failed (TERM=%s) — "
			"check terminfo for this terminal type\n",
			term != NULL ? term : "unset");
		exit(1);
	}
	ctx.std = notcurses_stdplane(ctx.nc);
	ctx.pixels = detect_pixel_policy(&ctx);
	if (render_geometry_refresh(&ctx, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: could not read terminal geometry\n");
		exit(1);
	}
	if (render_background_replace(&ctx, image_path, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: failed to load image %s\n", image_path);
		exit(1);
	}
	return (ctx);
}

/**
 * @brief Reports whether bitmap planes may be moved and overlapped freely.
 *
 * Only the Kitty-protocol tier can restack a sprixel or slide it a cell without
 * tearing, so per-piece board planes and the animated selector are limited to
 * it. Sixel and the Linux framebuffer still draw bitmaps, but stationary ones.
 *
 * @param ctx Active render context.
 * @return true when bitmap planes may move, overlap, and restack.
 */
bool	render_pixel_planes_reliable(const render_ctx_t *ctx)
{
	return (ctx != NULL && ctx->pixels == TETRISU_PIXELS_MOVABLE);
}

/**
 * @brief Reports whether this session may draw bitmaps at all.
 *
 * @param ctx Active render context.
 * @return true for every tier above the terminal-cell renderer.
 */
bool	render_pixels_available(const render_ctx_t *ctx)
{
	return (ctx != NULL && ctx->pixels != TETRISU_PIXELS_NONE);
}

/**
 * @brief Reports whether this session uses the terminal-native renderer.
 *
 * Automatic mode only falls back to cells when the terminal reports no bitmap
 * support. TETRISU_RENDERER=cell makes the same polished path deterministic.
 *
 * @param ctx Active render context.
 * @return true when all changing UI surfaces must remain terminal cells.
 */
bool	render_compatibility_mode(const render_ctx_t *ctx)
{
	return (ctx != NULL && ctx->pixels == TETRISU_PIXELS_NONE);
}

/**
 * @brief Shows the compact terminal-native mode badge at the top of the grid.
 *
 * The badge is intentionally opaque and high-contrast so users understand why
 * the presentation differs from bitmap-capable screenshots.
 *
 * @param ctx Active render context.
 */
void	render_compatibility_badge_refresh(render_ctx_t *ctx)
{
	ncplane_options	opts;
	const char		*text;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;
	int				width;

	render_compatibility_badge_hide(ctx);
	if (!render_compatibility_mode(ctx) || ctx->std == NULL)
		return ;
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows == 0 || cols < 12)
		return ;
	text = COMPATIBILITY_BADGE_TEXT;
	if (cols < strlen(text) + 4)
		text = COMPATIBILITY_BADGE_SHORT;
	width = (int)strlen(text) + 4;
	if (width > (int)cols)
		width = (int)cols;
	memset(&opts, 0, sizeof(opts));
	opts.y = 0;
	opts.x = ((int)cols - width) / 2;
	opts.rows = 1;
	opts.cols = width;
	ctx->compatibility_plane = ncplane_create(ctx->std, &opts);
	if (ctx->compatibility_plane == NULL)
		return ;
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 255, 203, 102);
	(void)ncchannels_set_bg_rgb8(&channels, 28, 13, 39);
	(void)ncplane_set_base(ctx->compatibility_plane, " ", 0, channels);
	ncplane_erase(ctx->compatibility_plane);
	(void)ncplane_set_fg_rgb8(ctx->compatibility_plane, 255, 203, 102);
	(void)ncplane_set_bg_rgb8(ctx->compatibility_plane, 28, 13, 39);
	(void)ncplane_on_styles(ctx->compatibility_plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_aligned(ctx->compatibility_plane, 0,
		NCALIGN_CENTER, text);
	ncplane_move_top(ctx->compatibility_plane);
}

/**
 * @brief Removes the compatibility badge when a screen has no spare top row.
 *
 * @param ctx Active render context.
 */
void	render_compatibility_badge_hide(render_ctx_t *ctx)
{
	if (ctx != NULL && ctx->compatibility_plane != NULL)
	{
		ncplane_destroy(ctx->compatibility_plane);
		ctx->compatibility_plane = NULL;
	}
}

/**
 * @brief Refreshes terminal and background geometry.
 *
 * @param ctx Context whose dimensions are updated.
 * @param repaint Whether notcurses must query and repaint the terminal first.
 * @return 0 on success, -1 when notcurses refresh fails.
 */
int	render_geometry_refresh(render_ctx_t *ctx, bool repaint)
{
	unsigned	rows;
	unsigned	cols;

	if (repaint)
	{
		if (notcurses_refresh(ctx->nc, &rows, &cols) != 0)
			return (-1);
	}
	else
		ncplane_dim_yx(ctx->std, &rows, &cols);
	refresh_cell_geometry(ctx);
	fit_background_to_terminal(ctx, (int)rows, (int)cols);
	return (0);
}

/**
 * @brief Detects terminal geometry changes missing from the input stream.
 *
 * Ghostty can resize its drawable grid without emitting NCKEY_RESIZE. Comparing
 * the tty geometry with Notcurses' current standard plane lets every screen
 * recover without requiring a key press.
 *
 * @param ctx Pointer to the render context.
 * @return true when the tty and standard-plane dimensions differ.
 */
bool	render_terminal_geometry_changed(const render_ctx_t *ctx)
{
	struct winsize	terminal;
	unsigned		plane_rows;
	unsigned		plane_cols;

	if (ctx == NULL || ctx->std == NULL)
		return (false);
	memset(&terminal, 0, sizeof(terminal));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal) != 0
		|| terminal.ws_row == 0 || terminal.ws_col == 0)
		return (false);
	ncplane_dim_yx(ctx->std, &plane_rows, &plane_cols);
	return (terminal.ws_row != plane_rows || terminal.ws_col != plane_cols);
}

/**
 * @brief Destroys the currently owned background plane.
 *
 * @param ctx Context whose background pointer is cleared.
 */
void	render_background_destroy(render_ctx_t *ctx)
{
	if (ctx->bg_plane != NULL)
	{
		ncplane_destroy(ctx->bg_plane);
		ctx->bg_plane = NULL;
	}
}

/**
 * @brief Replaces only the backdrop within the active notcurses session.
 *
 * AI-assisted: the replacement is rendered before the old plane is destroyed,
 * so a failed image transfer leaves the previous screen recoverable.
 *
 * @param ctx Active render context.
 * @param image_path Image to load into the replacement plane.
 * @param stretch Whether to fill the entire terminal instead of letterboxing.
 * @return 0 on success, -1 when loading, allocation, or rendering fails.
 */
int	render_background_replace(render_ctx_t *ctx, const char *image_path,
	bool stretch)
{
	struct ncvisual			*ncv;
	int						result;

	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
		return (-1);
	result = render_background_replace_visual(ctx, ncv, stretch);
	ncvisual_destroy(ncv);
	return (result);
}

/**
 * @brief Replaces the background through an exact-size bitmap plane.
 *
 * Auth screens contain small static lettering that cannot survive conversion
 * to a 4x2 terminal-cell mosaic. The visual is resized once to the physical
 * pixel geometry of its fitted plane, then transferred without another scale.
 * NODEGRADE keeps this path honest: unsupported terminals fall back through
 * the caller's native renderer instead of quietly degrading the artwork.
 *
 * @param ctx Active render context.
 * @param image_path Image to load into the replacement plane.
 * @param stretch Whether to fill the terminal instead of letterboxing.
 * @return 0 on success, -1 when exact bitmap rendering is unavailable.
 */
int	render_background_replace_exact(render_ctx_t *ctx,
	const char *image_path, bool stretch)
{
	struct ncvisual	*ncv;
	unsigned		std_rows;
	unsigned		std_cols;
	int				pixel_rows;
	int				pixel_cols;
	int				result;

	if (ctx == NULL || image_path == NULL || !render_pixels_available(ctx))
		return (-1);
	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
		return (-1);
	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	refresh_cell_geometry(ctx);
	if (stretch)
	{
		ctx->bg_row = 0;
		ctx->bg_col = 0;
		ctx->bg_rows = (int)std_rows;
		ctx->bg_cols = (int)std_cols;
	}
	else
		fit_background_to_terminal(ctx, (int)std_rows, (int)std_cols);
	if (ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x)
	{
		ncvisual_destroy(ncv);
		return (-1);
	}
	pixel_rows = ctx->bg_rows * ctx->cell_px_y;
	pixel_cols = ctx->bg_cols * ctx->cell_px_x;
	if (ncvisual_resize(ncv, pixel_rows, pixel_cols) != 0)
	{
		ncvisual_destroy(ncv);
		return (-1);
	}
	result = replace_visual_scaled(ctx, ncv, stretch, NCSCALE_NONE,
			NCBLIT_PIXEL, NCVISUAL_OPTION_NODEGRADE);
	ncvisual_destroy(ncv);
	return (result);
}

/**
 * @brief Replaces the background with an already composed visual.
 *
 * The caller retains ownership of ncv. Keeping the sizing and replacement
 * logic here ensures synthesized visuals obey the same exact plane contract as
 * file-backed artwork.
 */
int	render_background_replace_visual(render_ctx_t *ctx,
	struct ncvisual *ncv, bool stretch)
{
	return (replace_visual_scaled(ctx, ncv, stretch, NCSCALE_STRETCH,
			preferred_blitter(ctx, 0, 0), NCVISUAL_OPTION_NOINTERPOLATE));
}

static int	replace_visual_scaled(render_ctx_t *ctx, struct ncvisual *ncv,
	bool stretch, ncscale_e scaling, ncblitter_e blitter, uint64_t flags)
{
	struct ncvisual_options	vopts;
	ncplane_options			bg_opts;
	struct ncplane			*new_plane;
	struct ncplane			*old_plane;
	unsigned				std_rows;
	unsigned				std_cols;

	if (ctx == NULL || ncv == NULL)
		return (-1);
	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	refresh_cell_geometry(ctx);
	if (stretch)
	{
		ctx->bg_row = 0;
		ctx->bg_col = 0;
		ctx->bg_rows = (int)std_rows;
		ctx->bg_cols = (int)std_cols;
	}
	else
		fit_background_to_terminal(ctx, (int)std_rows, (int)std_cols);
	memset(&bg_opts, 0, sizeof(bg_opts));
	bg_opts.y = ctx->bg_row;
	bg_opts.x = ctx->bg_col;
	bg_opts.rows = ctx->bg_rows;
	bg_opts.cols = ctx->bg_cols;
	new_plane = ncplane_create(ctx->std, &bg_opts);
	if (new_plane == NULL)
		return (-1);
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = new_plane;
	vopts.scaling = scaling;
	vopts.blitter = blitter;
	vopts.flags = flags;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncplane_destroy(new_plane);
		return (-1);
	}
	old_plane = ctx->bg_plane;
	if (old_plane != NULL)
		(void)ncplane_move_above(new_plane, old_plane);
	else
		(void)ncplane_move_above(new_plane, ctx->std);
	set_opaque_backdrop(ctx->std);
	if (notcurses_render(ctx->nc) != 0)
	{
		ncplane_destroy(new_plane);
		if (old_plane != NULL)
			(void)notcurses_render(ctx->nc);
		return (-1);
	}
	ctx->bg_plane = new_plane;
	if (old_plane != NULL)
		ncplane_destroy(old_plane);
	return (0);
}

/**
 * @brief Blocks until one input event is available, returning its key id.
 *
 * @param ctx Pointer to the render context.
 * @return The Unicode codepoint or NCKEY_* constant for the event, or
 * (uint32_t)-1 on input error.
 */
uint32_t	render_wait_key(render_ctx_t *ctx)
{
	return (render_wait_input(ctx, NULL));
}

/**
 * @brief Blocks until one non-release input event is available.
 *
 * Supplying the full event lets screens use mouse coordinates and, later,
 * terminal protocols with distinct press/repeat/release states. Callers which
 * only need a key id can continue using render_wait_key().
 *
 * @param ctx Pointer to the render context.
 * @param input Optional destination for the complete Notcurses input event.
 * @return The Unicode codepoint or NCKEY_* constant for the event, or
 * (uint32_t)-1 on input error.
 */
uint32_t	render_wait_input(render_ctx_t *ctx, ncinput *input)
{
	ncinput		local;
	ncinput		*event;
	struct pollfd	input_fd;
	int			poll_result;
	int			wait_ms;
	int			notification_wait_ms;
	uint32_t	key;

	event = input;
	if (event == NULL)
		event = &local;
	memset(&input_fd, 0, sizeof(input_fd));
	input_fd.fd = notcurses_inputready_fd(ctx->nc);
	input_fd.events = POLLIN;
	while (1)
	{
		memset(event, 0, sizeof(*event));
		wait_ms = RENDER_RESIZE_POLL_MS;
		notification_wait_ms = render_notification_next_wake_ms(ctx);
		if (notification_wait_ms >= 0 && notification_wait_ms < wait_ms)
			wait_ms = notification_wait_ms;
		poll_result = poll(&input_fd, 1, wait_ms);
		if (poll_result < 0)
		{
			if (errno == EINTR)
				continue ;
			return ((uint32_t)-1);
		}
		if (render_notification_next_wake_ms(ctx) == 0)
			render_notification_tick(ctx);
		if (poll_result == 0)
		{
			if (render_terminal_geometry_changed(ctx))
				return (NCKEY_RESIZE);
			continue ;
		}
		if ((input_fd.revents & POLLIN) == 0)
			return ((uint32_t)-1);
		errno = 0;
		key = notcurses_get_nblock(ctx->nc, event);
		if (key == 0)
			continue ;
		if (key == (uint32_t)-1)
		{
			if (errno == EINTR)
				continue ;
			return (key);
		}
		/* Ignore keyboard key-up events so one arrow tap moves once. Mouse
		 * motion can legitimately arrive with release/no-button state and
		 * must still reach the menu for hover selection. */
		if (event->evtype != NCTYPE_RELEASE || nckey_mouse_p(key))
			return (key);
	}
}

/**
 * @brief Stops notcurses, restoring the terminal to normal mode.
 *
 * @param ctx Pointer to the render context to tear down.
 */
void	render_teardown(render_ctx_t *ctx)
{
	if (ctx->nc != NULL)
	{
		render_notification_destroy(ctx);
		render_compatibility_badge_hide(ctx);
		render_auth_pixel_overlay_destroy(ctx);
		render_auth_pixel_background_reset(ctx);
		render_settings_pixel_destroy(ctx);
		if (ctx->auth_font_visual != NULL)
		{
			ncvisual_destroy(ctx->auth_font_visual);
			ctx->auth_font_visual = NULL;
		}
		/* Runs before notcurses_stop() because the menu owns a decoded sprite
		 * that no plane teardown would release. */
		render_menu_destroy(ctx);
		notcurses_stop(ctx->nc);
		ctx->nc = NULL;
		ctx->std = NULL;
		ctx->bg_plane = NULL;
		ctx->menu_plane = NULL;
		ctx->menu_labels_plane = NULL;
		ctx->auth_background_signature = 0;
		ctx->bunny_plane = NULL;
		ctx->pixels = TETRISU_PIXELS_NONE;
	}
}

/**
 * @brief Probes the terminal once and resolves the renderer capability tier.
 *
 * @param ctx Render context holding a started notcurses instance.
 * @return The tier every later render decision is derived from.
 */
static tetrisu_pixel_policy_t	detect_pixel_policy(const render_ctx_t *ctx)
{
	tetrisu_pixel_policy_t	policy;
	char					*term;

	term = notcurses_detected_terminal(ctx->nc);
	policy = tetrisu_pixel_policy_for(notcurses_check_pixel_support(ctx->nc),
			term, tetrisu_renderer_mode_requested());
	free(term);
	return (policy);
}

/**
 * @brief Records the terminal cell dimensions in physical pixels.
 *
 * @param ctx Render context updated with a portable 2:1 fallback when the
 * terminal exposes no bitmap geometry.
 */
static void	refresh_cell_geometry(render_ctx_t *ctx)
{
	unsigned	cell_px_y;
	unsigned	cell_px_x;

	cell_px_y = 0;
	cell_px_x = 0;
	ncplane_pixel_geom(ctx->std, NULL, NULL, &cell_px_y, &cell_px_x,
		NULL, NULL);
	/* A non-bitmap terminal may not report pixels. A 2:1 cell is the safest
	 * portable fallback for keeping the 4:3 art physically proportional. */
	if (cell_px_y == 0)
		cell_px_y = 2;
	if (cell_px_x == 0)
		cell_px_x = 1;
	ctx->cell_px_y = (int)cell_px_y;
	ctx->cell_px_x = (int)cell_px_x;
}

/**
 * @brief Fits the authored background inside terminal geometry.
 *
 * @param ctx Context receiving fitted origin and dimensions.
 * @param std_rows Available terminal rows.
 * @param std_cols Available terminal columns.
 */
static void	fit_background_to_terminal(render_ctx_t *ctx,
	int std_rows, int std_cols)
{
	double	image_ratio;
	int		rows;
	int		cols;

	image_ratio = (double)BACKGROUND_SOURCE_PIXELS_X
		/ BACKGROUND_SOURCE_PIXELS_Y;
	cols = std_cols;
	rows = (int)((double)cols * ctx->cell_px_x
		/ (image_ratio * ctx->cell_px_y) + 0.5);
	if (rows > std_rows)
	{
		rows = std_rows;
		cols = (int)((double)rows * ctx->cell_px_y * image_ratio
			/ ctx->cell_px_x + 0.5);
	}
	ctx->bg_rows = max_int(rows, 1);
	ctx->bg_cols = max_int(cols, 1);
	ctx->bg_row = (std_rows - ctx->bg_rows) / 2;
	ctx->bg_col = (std_cols - ctx->bg_cols) / 2;
}

/**
 * @brief Returns the greater of two integers.
 *
 * @param a First value.
 * @param b Second value.
 * @return The greater value.
 */
static int	max_int(int a, int b)
{
	if (a > b)
		return (a);
	return (b);
}

/**
 * @brief Selects the low-bandwidth blitter used for decorative backgrounds.
 *
 * @param ctx Active render context (unused).
 * @param rows Destination row count (unused).
 * @param cols Destination column count (unused).
 * @return The 4x2 cell blitter.
 */
static ncblitter_e	preferred_blitter(const render_ctx_t *ctx,
	int rows, int cols)
{
	(void)ctx;
	(void)rows;
	(void)cols;
	/* Decorative backdrops are intentionally cell-rendered. This leaves the
	 * bitmap layer for stationary crisp characters, pieces, and text. */
	return (NCBLIT_4x2);
}

/**
 * @brief Clears a plane to the playfield fallback colour.
 *
 * @param plane Plane whose base cell and contents are replaced.
 */
static void	set_opaque_backdrop(struct ncplane *plane)
{
	uint64_t	channels;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 7, 13, 23);
	(void)ncchannels_set_bg_rgb8(&channels, 7, 13, 23);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
}
