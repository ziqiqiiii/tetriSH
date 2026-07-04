#include "tetrisu.h"

#define BACKGROUND_SOURCE_PIXELS_Y	1086
#define BACKGROUND_SOURCE_PIXELS_X	1448

static int	max_int(int a, int b)
{
	if (a > b)
		return (a);
	return (b);
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

/**
 * @brief Starts notcurses and blits image_path across the standard plane.
 *
 * The standard plane is notcurses' full-screen base plane; blitting the
 * image directly onto it (rather than a separate plane) makes it the
 * backdrop everything else renders on top of. notcurses installs its own
 * signal handlers by default (restores the screen on SIGINT/SIGTERM/etc.
 * before chaining to the previous handler), so no custom SIGINT handling
 * is needed here.
 *
 * @param image_path Path to the image file to render as the background.
 * @return A render_ctx_t with nc/std populated; menu_plane and bunny_plane
 * are NULL until render_menu_create() is called.
 */
render_ctx_t	render_init(const char *image_path)
{
	render_ctx_t			ctx;
	notcurses_options		opts;
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	ncplane_options			bg_opts;
	unsigned				std_rows;
	unsigned				std_cols;
	unsigned				cell_px_y;
	unsigned				cell_px_x;

	memset(&opts, 0, sizeof(opts));
	ctx.nc = notcurses_init(&opts, NULL);
	if (ctx.nc == NULL)
	{
		fprintf(stderr, "tetrisu: notcurses_core_init failed (TERM=%s) — "
			"check terminfo for this terminal type\n", getenv("TERM"));
		exit(1);
	}
	ctx.std = notcurses_stdplane(ctx.nc);
	ctx.bg_plane = NULL;
	ctx.menu_plane = NULL;
	ctx.bunny_plane = NULL;
	ncplane_dim_yx(ctx.std, &std_rows, &std_cols);
	cell_px_y = 2;
	cell_px_x = 1;
	ncplane_pixel_geom(ctx.std, NULL, NULL, &cell_px_y, &cell_px_x,
		NULL, NULL);
	ctx.cell_px_y = (int)cell_px_y;
	ctx.cell_px_x = (int)cell_px_x;
	fit_background_to_terminal(&ctx, (int)std_rows, (int)std_cols);
	ctx.menu_row = 0;
	ctx.menu_col = 0;
	ctx.bunny_rows = 0;
	ctx.bunny_cols = 0;
	ncv = ncvisual_from_file(image_path);
	if (ncv == NULL)
	{
		notcurses_stop(ctx.nc);
		fprintf(stderr, "tetrisu: failed to load image %s\n", image_path);
		exit(1);
	}
	memset(&bg_opts, 0, sizeof(bg_opts));
	bg_opts.y = ctx.bg_row;
	bg_opts.x = ctx.bg_col;
	bg_opts.rows = ctx.bg_rows;
	bg_opts.cols = ctx.bg_cols;
	ctx.bg_plane = ncplane_create(ctx.std, &bg_opts);
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = ctx.bg_plane;
	vopts.scaling = NCSCALE_STRETCH;
	vopts.blitter = NCBLIT_4x2;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	ncvisual_blit(ctx.nc, ncv, &vopts);
	ncvisual_destroy(ncv);
	notcurses_render(ctx.nc);
	return (ctx);
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
	notcurses_stop(ctx->nc);
}
