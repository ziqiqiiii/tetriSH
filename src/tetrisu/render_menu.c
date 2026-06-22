#include "tetrisu.h"

#define MENU_FIRST_Y_RATIO	0.748
#define MENU_STEP_Y_RATIO	0.066
#define BUNNY_LEFT_X_RATIO	0.382
#define BUNNY_ROWS_RATIO	0.058
#define BUNNY_SOURCE_PIXELS_Y	160
#define BUNNY_SOURCE_PIXELS_X	150
#define BUNNY_MIN_ROWS		3
#define BUNNY_MAX_ROWS		8

static int	clamp_int(int value, int min, int max)
{
	if (value < min)
		return (min);
	if (value > max)
		return (max);
	return (value);
}

static int	scale_from_bg(int origin, int size, double ratio)
{
	return (origin + (int)((double)size * ratio + 0.5));
}

static int	bunny_y_for_selection(const render_ctx_t *ctx,
	const menu_selection_t *m)
{
	int	center_y;
	int	step_y;

	center_y = scale_from_bg(ctx->bg_row, ctx->bg_rows, MENU_FIRST_Y_RATIO);
	step_y = (int)((double)ctx->bg_rows * MENU_STEP_Y_RATIO + 0.5);
	if (step_y < 1)
		step_y = 1;
	return (center_y + (m->selected * step_y) - (ctx->bunny_rows / 2));
}

static int	bunny_x(const render_ctx_t *ctx)
{
	return (scale_from_bg(ctx->bg_col, ctx->bg_cols, BUNNY_LEFT_X_RATIO));
}

static int	bunny_cols_for_rows(const render_ctx_t *ctx)
{
	double	source_ratio;
	double	cell_ratio;

	source_ratio = (double)BUNNY_SOURCE_PIXELS_X / BUNNY_SOURCE_PIXELS_Y;
	cell_ratio = 2.0;
	if (ctx->cell_px_x > 0 && ctx->cell_px_y > 0)
		cell_ratio = (double)ctx->cell_px_y / ctx->cell_px_x;
	return (clamp_int((int)((double)ctx->bunny_rows * source_ratio
		* cell_ratio + 0.5), 1, ctx->bg_cols));
}

static void	set_transparent_base(struct ncplane *plane)
{
	nccell	base;

	nccell_init(&base);
	if (nccell_load(plane, &base, " ") >= 0)
	{
		nccell_set_fg_alpha(&base, NCALPHA_TRANSPARENT);
		nccell_set_bg_alpha(&base, NCALPHA_TRANSPARENT);
		ncplane_set_base_cell(plane, &base);
		nccell_release(plane, &base);
	}
	ncplane_erase(plane);
}

static void	draw_bunny_sprite(render_ctx_t *ctx)
{
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;

	ncv = ncvisual_from_file(BUNNY_ASSET_PATH);
	if (ncv == NULL)
	{
		ncplane_set_fg_rgb8(ctx->bunny_plane, 255, 150, 200);
		ncplane_set_bg_alpha(ctx->bunny_plane, NCALPHA_TRANSPARENT);
		ncplane_putstr_yx(ctx->bunny_plane, 1, 1, ">>");
		return ;
	}
	/* AI-assisted: the background image already owns the labels; this selector
	 * keeps PNG alpha and dense cell blitting so the bunny stays crisp. */
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = ctx->bunny_plane;
	vopts.scaling = NCSCALE_STRETCH;
	vopts.blitter = NCBLIT_4x2;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	ncvisual_blit(ctx->nc, ncv, &vopts);
	ncvisual_destroy(ncv);
}

/**
 * @brief Creates the bunny selector plane on top of the background image.
 *
 * The menu labels live inside homepage.png. We only draw the selector, using
 * the rendered background geometry so its row spacing follows image scaling.
 *
 * @param ctx Pointer to the render context; std must already be set by
 * render_init().
 */
void	render_menu_create(render_ctx_t *ctx)
{
	ncplane_options	bunny_opts;
	menu_selection_t	initial;

	initial.selected = 0;
	ctx->bunny_rows = clamp_int((int)((double)ctx->bg_rows * BUNNY_ROWS_RATIO
		+ 0.5), BUNNY_MIN_ROWS, BUNNY_MAX_ROWS);
	ctx->bunny_cols = bunny_cols_for_rows(ctx);
	ctx->menu_row = scale_from_bg(ctx->bg_row, ctx->bg_rows,
		MENU_FIRST_Y_RATIO);
	ctx->menu_col = scale_from_bg(ctx->bg_col, ctx->bg_cols,
		BUNNY_LEFT_X_RATIO);
	memset(&bunny_opts, 0, sizeof(bunny_opts));
	bunny_opts.y = bunny_y_for_selection(ctx, &initial);
	bunny_opts.x = bunny_x(ctx);
	bunny_opts.rows = ctx->bunny_rows;
	bunny_opts.cols = ctx->bunny_cols;
	ctx->bunny_plane = ncplane_create(ctx->std, &bunny_opts);
	set_transparent_base(ctx->bunny_plane);
	draw_bunny_sprite(ctx);
	notcurses_render(ctx->nc);
}

/**
 * @brief Moves the bunny plane to line up with the currently selected item.
 *
 * @param ctx Pointer to the render context.
 * @param m Pointer to the current selection state.
 */
void	render_menu_move_bunny(render_ctx_t *ctx, const menu_selection_t *m)
{
	ncplane_move_yx(ctx->bunny_plane, bunny_y_for_selection(ctx, m),
		bunny_x(ctx));
	notcurses_render(ctx->nc);
}

/**
 * @brief Prints a one-line message near the bottom of the background plane.
 *
 * @param ctx Pointer to the render context.
 * @param msg Message text to display.
 */
void	render_menu_show_message(render_ctx_t *ctx, const char *msg)
{
	unsigned	rows;
	unsigned	cols;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	(void)cols;
	ncplane_set_fg_rgb8(ctx->std, 255, 255, 255);
	ncplane_putstr_yx(ctx->std, (int)rows - 1, 2, msg);
	notcurses_render(ctx->nc);
}
