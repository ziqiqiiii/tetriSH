#include "tetrisu.h"

#define BACKGROUND_SOURCE_PIXELS_Y	1086
#define BACKGROUND_SOURCE_PIXELS_X	1448

static int	max_int(int a, int b)
{
	if (a > b)
		return (a);
	return (b);
}

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

static ncblitter_e	preferred_blitter(const render_ctx_t *ctx,
	int rows, int cols)
{
	(void)ctx;
	(void)rows;
	(void)cols;
	/* Decorative backdrops are intentionally cell-rendered. This leaves the
	 * terminal's bitmap layer for crisp characters, pieces, text, and bunny. */
	return (NCBLIT_4x2);
}

static void	set_opaque_backdrop(struct ncplane *plane)
{
	uint64_t	channels;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 7, 13, 23);
	(void)ncchannels_set_bg_rgb8(&channels, 7, 13, 23);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
}

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

void	render_background_destroy(render_ctx_t *ctx)
{
	if (ctx->bg_plane != NULL)
	{
		ncplane_destroy(ctx->bg_plane);
		ctx->bg_plane = NULL;
	}
}

/**
 * @brief Starts notcurses and blits image_path across the standard plane.
 *
 * The image lives on one child plane below menu/game overlays so modes can
 * replace their backdrop without restarting notcurses. notcurses installs
 * its own signal handlers by default, so no custom SIGINT handler is needed.
 *
 * @param image_path Path to the image file to render as the background.
 * @return A render_ctx_t with nc/std populated; menu_plane and bunny_plane
 * are NULL until render_menu_create() is called.
 */
render_ctx_t	render_init(const char *image_path)
{
	render_ctx_t			ctx;
	notcurses_options		opts;
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
	ctx.bg_plane = NULL;
	ctx.menu_plane = NULL;
	ctx.bunny_plane = NULL;
	if (render_geometry_refresh(&ctx, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: could not read terminal geometry\n");
		exit(1);
	}
	ctx.menu_row = 0;
	ctx.menu_col = 0;
	ctx.bunny_rows = 0;
	ctx.bunny_cols = 0;
	if (render_background_replace(&ctx, image_path, false) < 0)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: failed to load image %s\n", image_path);
		exit(1);
	}
	return (ctx);
}

/* AI-assisted: replaces only the backdrop plane, allowing menu and Solo mode
 * to share one notcurses session without leaking image or plane resources. */
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
		ncplane_move_top(new_plane);
	set_opaque_backdrop(ctx->std);
	if (notcurses_render(ctx->nc) != 0)
	{
		ncplane_destroy(new_plane);
		if (old_plane != NULL)
		{
			ncplane_move_top(old_plane);
			(void)notcurses_render(ctx->nc);
		}
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
	ncinput	ni;
	uint32_t	key;

	while (1)
	{
		key = notcurses_get(ctx->nc, NULL, &ni);
		if (key == (uint32_t)-1)
			return (key);
		/* Ignore key-up events: terminals/notcurses can report both press and
		 * release for one arrow tap, and selection should move once per tap. */
		if (ni.evtype != NCTYPE_RELEASE)
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
		ctx->bunny_plane = NULL;
	}
}
