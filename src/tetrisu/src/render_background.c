#include "tetrisu.h"

// Static Functions
static void	refresh_cell_geometry(render_ctx_t *ctx);
static void	fit_background_to_terminal(render_ctx_t *ctx,
	int std_rows, int std_cols);
static int	max_int(int a, int b);
static ncblitter_e	preferred_blitter(const render_ctx_t *ctx,
	int rows, int cols);
static void	set_opaque_backdrop(struct ncplane *plane);

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
 * @brief Reports whether movable bitmap planes are safe for this backend.
 *
 * Kitty and iTerm2 preserve overlapping bitmap movement; Sixel and the Linux
 * framebuffer use the stationary composite-board fallback.
 *
 * @param ctx Active render context.
 * @return true for backends that preserve movable bitmap planes.
 */
bool	render_pixel_planes_reliable(const render_ctx_t *ctx)
{
	ncpixelimpl_e	backend;

	/* AI-assisted compatibility exception: Notcurses calls this enum
	 * informational, but moving overlapping planes visibly tears on the tested
	 * Sixel/Linux framebuffer paths. Match named backends instead of relying on
	 * enum ordering, which is not an API stability promise. */
	backend = notcurses_check_pixel_support(ctx->nc);
	return (backend == NCPIXEL_ITERM2 || backend == NCPIXEL_KITTY_STATIC
		|| backend == NCPIXEL_KITTY_ANIMATED
		|| backend == NCPIXEL_KITTY_SELFREF);
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
	struct ncvisual_options	vopts;
	ncplane_options			bg_opts;
	struct ncplane			*new_plane;
	struct ncplane			*old_plane;
	unsigned				std_rows;
	unsigned				std_cols;

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
	memset(&bg_opts, 0, sizeof(bg_opts));
	bg_opts.y = ctx->bg_row;
	bg_opts.x = ctx->bg_col;
	bg_opts.rows = ctx->bg_rows;
	bg_opts.cols = ctx->bg_cols;
	new_plane = ncplane_create(ctx->std, &bg_opts);
	if (new_plane == NULL)
	{
		ncvisual_destroy(ncv);
		return (-1);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = new_plane;
	vopts.scaling = NCSCALE_STRETCH;
	vopts.blitter = preferred_blitter(ctx, ctx->bg_rows, ctx->bg_cols);
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		ncplane_destroy(new_plane);
		return (-1);
	}
	ncvisual_destroy(ncv);
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
	uint32_t	key;

	event = input;
	if (event == NULL)
		event = &local;
	while (1)
	{
		memset(event, 0, sizeof(*event));
		key = notcurses_get(ctx->nc, NULL, event);
		if (key == (uint32_t)-1)
			return (key);
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
		notcurses_stop(ctx->nc);
		ctx->nc = NULL;
		ctx->std = NULL;
		ctx->bg_plane = NULL;
		ctx->menu_plane = NULL;
		ctx->menu_labels_plane = NULL;
		ctx->bunny_plane = NULL;
	}
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
