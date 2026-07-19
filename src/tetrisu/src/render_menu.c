#include "tetrisu.h"

// Static Functions
static int	clamp_int(int value, int min, int max);
static int	bunny_cols_for_rows(const render_ctx_t *ctx);
static int	scale_from_bg(int origin, int size, double ratio);
static int	bunny_x(const render_ctx_t *ctx);
static int	bunny_y_for_selection(const render_ctx_t *ctx,
	const menu_selection_t *m);
static int	menu_step_y(const render_ctx_t *ctx);
static struct ncplane	*create_bunny_sprite(render_ctx_t *ctx, int y, int x);
static struct ncplane	*create_bunny_fallback(render_ctx_t *ctx,
	int y, int x);
static void	set_transparent_base(struct ncplane *plane);

/**
 * @brief Creates the bunny selector plane on top of the background image.
 *
 * The menu labels live inside homepage.png. We only draw the selector, using
 * the rendered background geometry so its row spacing follows image scaling.
 *
 * @param ctx Pointer to the initialized render context.
 */
void	render_menu_create(render_ctx_t *ctx)
{
	menu_selection_t	initial;
	int					y;
	int					x;

	render_menu_destroy(ctx);
	initial.selected = 0;
	ctx->bunny_rows = clamp_int((int)((double)ctx->bg_rows * BUNNY_ROWS_RATIO
		+ 0.5), BUNNY_MIN_ROWS, BUNNY_MAX_ROWS);
	ctx->bunny_cols = bunny_cols_for_rows(ctx);
	ctx->menu_row = scale_from_bg(ctx->bg_row, ctx->bg_rows,
		MENU_FIRST_Y_RATIO);
	ctx->menu_col = bunny_x(ctx);
	y = bunny_y_for_selection(ctx, &initial);
	x = bunny_x(ctx);
	ctx->bunny_plane = create_bunny_sprite(ctx, y, x);
	if (ctx->bunny_plane != NULL)
		ncplane_move_top(ctx->bunny_plane);
	if (notcurses_render(ctx->nc) != 0 && ctx->bunny_plane != NULL)
	{
		ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = create_bunny_fallback(ctx, y, x);
		if (ctx->bunny_plane != NULL)
		{
			ncplane_move_top(ctx->bunny_plane);
			(void)notcurses_render(ctx->nc);
		}
	}
}

/**
 * @brief Moves the bunny plane to line up with the currently selected item.
 *
 * @param ctx Pointer to the render context.
 * @param m Pointer to the current selection state.
 */
void	render_menu_move_bunny(render_ctx_t *ctx, const menu_selection_t *m)
{
	int	y;
	int	x;

	if (ctx->bunny_plane == NULL)
		return ;
	y = bunny_y_for_selection(ctx, m);
	x = bunny_x(ctx);
	/* Sixel-class terminals cannot relocate a bitmap without leaving trails,
	 * so the sprite is rebuilt at the new spot instead of moved. */
	if (notcurses_canpixel(ctx->nc) && !render_pixel_planes_reliable(ctx))
	{
		ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = create_bunny_sprite(ctx, y, x);
		if (ctx->bunny_plane != NULL)
			ncplane_move_top(ctx->bunny_plane);
	}
	else if (ncplane_move_yx(ctx->bunny_plane, y, x) != 0)
		return ;
	if (notcurses_render(ctx->nc) != 0)
	{
		if (ctx->bunny_plane != NULL)
			ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = create_bunny_fallback(ctx, y, x);
		if (ctx->bunny_plane != NULL)
			(void)notcurses_render(ctx->nc);
	}
}

/**
 * @brief Prints a one-line message near the bottom of the background plane.
 *
 * @param ctx Pointer to the render context.
 * @param msg Message text to display.
 */
void	render_menu_show_message(render_ctx_t *ctx, const char *msg)
{
	ncplane_options	opts;
	unsigned	rows;
	unsigned	cols;
	int			x;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (ctx->menu_plane != NULL)
	{
		ncplane_destroy(ctx->menu_plane);
		ctx->menu_plane = NULL;
	}
	if (rows == 0 || cols == 0)
		return ;
	memset(&opts, 0, sizeof(opts));
	opts.y = (int)rows - 1;
	opts.x = 0;
	opts.rows = 1;
	opts.cols = (int)cols;
	ctx->menu_plane = ncplane_create(ctx->std, &opts);
	if (ctx->menu_plane == NULL)
		return ;
	set_transparent_base(ctx->menu_plane);
	ncplane_set_fg_rgb8(ctx->menu_plane, 255, 255, 255);
	x = ((int)cols - (int)strlen(msg)) / 2;
	if (x < 0)
		x = 0;
	ncplane_putstr_yx(ctx->menu_plane, 0, x, msg);
	ncplane_move_top(ctx->menu_plane);
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Destroys every menu-owned plane and clears the context handles.
 *
 * @param ctx Pointer to the render context.
 */
void	render_menu_destroy(render_ctx_t *ctx)
{
	if (ctx->bunny_plane != NULL)
	{
		ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = NULL;
	}
	if (ctx->menu_plane != NULL)
	{
		ncplane_destroy(ctx->menu_plane);
		ctx->menu_plane = NULL;
	}
}

/**
 * @brief Clamps a value to an inclusive range.
 *
 * @param value Candidate value.
 * @param min Lowest accepted value.
 * @param max Highest accepted value.
 * @return The bounded value.
 */
static int	clamp_int(int value, int min, int max)
{
	if (value < min)
		return (min);
	if (value > max)
		return (max);
	return (value);
}

/**
 * @brief Preserves the bunny's pixel aspect ratio at the selected row count.
 *
 * @param ctx Pointer to the render context.
 * @return Selector width in terminal columns.
 */
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

/**
 * @brief Converts a normalized background coordinate to a terminal cell.
 *
 * @param origin First terminal cell occupied by the background.
 * @param size Background extent in terminal cells.
 * @param ratio Normalized coordinate in the authored image.
 * @return Rounded terminal-cell coordinate.
 */
static int	scale_from_bg(int origin, int size, double ratio)
{
	return (origin + (int)((double)size * ratio + 0.5));
}

/**
 * @brief Calculates the bunny's column beside the baked menu labels.
 *
 * @param ctx Pointer to the render context.
 * @return Clamped terminal column for the selector plane.
 */
static int	bunny_x(const render_ctx_t *ctx)
{
	int	label_x;
	int	x;
	int	max_x;

	label_x = scale_from_bg(ctx->bg_col, ctx->bg_cols,
		BUNNY_LABEL_X_RATIO);
	x = label_x - ctx->bunny_cols - BUNNY_TEXT_GAP_COLS;
	max_x = ctx->bg_col + ctx->bg_cols - ctx->bunny_cols;
	if (max_x < ctx->bg_col)
		max_x = ctx->bg_col;
	return (clamp_int(x, ctx->bg_col, max_x));
}

/**
 * @brief Calculates the bunny's row for the current menu selection.
 *
 * @param ctx Pointer to the render context.
 * @param m Pointer to the current selection.
 * @return Clamped terminal row for the selector plane.
 */
static int	bunny_y_for_selection(const render_ctx_t *ctx,
	const menu_selection_t *m)
{
	int	center_y;
	int	y;
	int	max_y;

	center_y = scale_from_bg(ctx->bg_row, ctx->bg_rows, MENU_FIRST_Y_RATIO);
	y = center_y + (m->selected * menu_step_y(ctx)) - (ctx->bunny_rows / 2);
	max_y = ctx->bg_row + ctx->bg_rows - ctx->bunny_rows;
	if (max_y < ctx->bg_row)
		max_y = ctx->bg_row;
	return (clamp_int(y, ctx->bg_row, max_y));
}

/**
 * @brief Calculates the vertical distance between adjacent menu entries.
 *
 * @param ctx Pointer to the render context.
 * @return Menu spacing in terminal rows, always at least one.
 */
static int	menu_step_y(const render_ctx_t *ctx)
{
	int	step_y;

	step_y = (int)((double)ctx->bg_rows * MENU_STEP_Y_RATIO + 0.5);
	if (step_y < 1)
		step_y = 1;
	return (step_y);
}

/**
 * @brief Creates the highest-quality bunny selector supported by the terminal.
 *
 * AI-assisted: the homepage is a cell backdrop, so the selector can use one
 * non-overlapping pixel plane. A cell-blitted sprite and then a text fallback
 * keep the menu usable when the terminal rejects pixel graphics.
 *
 * @param ctx Pointer to the render context.
 * @param y Target terminal row.
 * @param x Target terminal column.
 * @return Owned selector plane, or NULL when every renderer fails.
 */
static struct ncplane	*create_bunny_sprite(render_ctx_t *ctx, int y, int x)
{
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	struct ncplane			*plane;
	int						target_pixels_y;
	int						target_pixels_x;

	ncv = ncvisual_from_file(BUNNY_ASSET_PATH);
	if (ncv == NULL)
		return (create_bunny_fallback(ctx, y, x));
	plane = NULL;
	target_pixels_y = ctx->bunny_rows * ctx->cell_px_y;
	target_pixels_x = ctx->bunny_cols * ctx->cell_px_x;
	if (notcurses_canpixel(ctx->nc)
		&& ncvisual_resize_noninterpolative(ncv,
			target_pixels_y, target_pixels_x) == 0)
	{
		memset(&vopts, 0, sizeof(vopts));
		vopts.n = ctx->std;
		vopts.scaling = NCSCALE_NONE;
		vopts.y = y;
		vopts.x = x;
		vopts.blitter = NCBLIT_PIXEL;
		vopts.flags = NCVISUAL_OPTION_CHILDPLANE
			| NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
		plane = ncvisual_blit(ctx->nc, ncv, &vopts);
	}
	if (plane != NULL)
	{
		ncvisual_destroy(ncv);
		return (plane);
	}
	ncvisual_destroy(ncv);
	ncv = ncvisual_from_file(BUNNY_ASSET_PATH);
	if (ncv == NULL)
		return (create_bunny_fallback(ctx, y, x));
	if (ncvisual_resize_noninterpolative(ncv, ctx->bunny_rows * 4,
			ctx->bunny_cols * 2) == 0)
	{
		memset(&vopts, 0, sizeof(vopts));
		vopts.n = ctx->std;
		vopts.scaling = NCSCALE_NONE;
		vopts.y = y;
		vopts.x = x;
		vopts.blitter = NCBLIT_4x2;
		vopts.flags = NCVISUAL_OPTION_CHILDPLANE
			| NCVISUAL_OPTION_NOINTERPOLATE;
		plane = ncvisual_blit(ctx->nc, ncv, &vopts);
	}
	if (plane == NULL
		&& ncvisual_resize_noninterpolative(ncv, ctx->bunny_rows * 2,
			ctx->bunny_cols) == 0)
	{
		memset(&vopts, 0, sizeof(vopts));
		vopts.n = ctx->std;
		vopts.scaling = NCSCALE_NONE;
		vopts.y = y;
		vopts.x = x;
		vopts.blitter = NCBLIT_2x1;
		vopts.flags = NCVISUAL_OPTION_CHILDPLANE
			| NCVISUAL_OPTION_NOINTERPOLATE;
		plane = ncvisual_blit(ctx->nc, ncv, &vopts);
	}
	ncvisual_destroy(ncv);
	if (plane == NULL)
		plane = create_bunny_fallback(ctx, y, x);
	return (plane);
}

/**
 * @brief Creates a terminal-cell selector when pixel rendering is unavailable.
 *
 * @param ctx Pointer to the render context.
 * @param y Target terminal row.
 * @param x Target terminal column.
 * @return Owned selector plane, or NULL when allocation fails.
 */
static struct ncplane	*create_bunny_fallback(render_ctx_t *ctx,
	int y, int x)
{
	ncplane_options	opts;
	struct ncplane	*plane;

	memset(&opts, 0, sizeof(opts));
	opts.y = y;
	opts.x = x;
	opts.rows = ctx->bunny_rows;
	opts.cols = ctx->bunny_cols;
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
		return (NULL);
	set_transparent_base(plane);
	ncplane_set_fg_rgb8(plane, 220, 175, 255);
	ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
	ncplane_putstr_yx(plane, ctx->bunny_rows / 2,
		ctx->bunny_cols > 2 ? ctx->bunny_cols - 2 : 0, ">");
	return (plane);
}

/**
 * @brief Makes an ncplane fully transparent before drawing onto it.
 *
 * @param plane Plane whose base cell and contents are reset.
 */
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
