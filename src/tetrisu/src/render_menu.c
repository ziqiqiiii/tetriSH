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
static struct ncplane	*create_bunny_pixel(render_ctx_t *ctx, int y, int x);
static struct ncplane	*create_bunny_fallback(render_ctx_t *ctx,
	int y, int x);
static struct ncplane	*create_compatibility_selector(render_ctx_t *ctx,
	int y, int x);
static void	set_transparent_base(struct ncplane *plane);

/**
 * @brief Creates the bunny selector plane on top of the background image.
 *
 * Five exact labels are rasterized from the shared pixel-font mask over the
 * clean homepage art. The selector uses the same background-relative geometry
 * so both layers stay aligned while the terminal resizes.
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
	if (render_compatibility_mode(ctx))
	{
		ctx->bunny_rows = COMPAT_SELECTOR_ROWS;
		ctx->bunny_cols = COMPAT_SELECTOR_COLS;
	}
	ctx->menu_row = scale_from_bg(ctx->bg_row, ctx->bg_rows,
		MENU_FIRST_Y_RATIO);
	ctx->menu_col = bunny_x(ctx);
	y = bunny_y_for_selection(ctx, &initial);
	x = bunny_x(ctx);
	ctx->menu_labels_plane = render_menu_labels_create(ctx);
	ctx->bunny_plane = create_bunny_sprite(ctx, y, x);
	if (ctx->bunny_plane != NULL)
		ncplane_move_top(ctx->bunny_plane);
	render_compatibility_badge_refresh(ctx);
	if (notcurses_render(ctx->nc) != 0 && ctx->bunny_plane != NULL)
	{
		ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = create_bunny_fallback(ctx, y, x);
		if (ctx->bunny_plane != NULL)
		{
			ncplane_move_top(ctx->bunny_plane);
			render_compatibility_badge_refresh(ctx);
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
	if (render_compatibility_mode(ctx))
	{
		ncplane_destroy(ctx->bunny_plane);
		ctx->bunny_plane = create_compatibility_selector(ctx, y, x);
		if (ctx->bunny_plane != NULL)
		{
			ncplane_move_top(ctx->bunny_plane);
			render_compatibility_badge_refresh(ctx);
			(void)notcurses_render(ctx->nc);
		}
		return ;
	}
	if (ncplane_move_yx(ctx->bunny_plane, y, x) != 0)
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
 * @brief Maps a mouse position over the visible home list to a menu item.
 *
 * The hit area includes the bunny and the complete label row. Empty space
 * outside the five rows does not change the keyboard selection.
 *
 * @param ctx Pointer to the active render context.
 * @param input Full Notcurses mouse event containing terminal coordinates.
 * @param selected Destination for the zero-based item index.
 * @return true when the pointer is over one of the five menu rows.
 */
bool	render_menu_hit_test(const render_ctx_t *ctx, const ncinput *input,
	int *selected)
{
	int	first_y;
	int	last_y;
	int	step_y;
	int	half_step;
	int	left_x;
	int	right_x;
	int	index;
	int	center_y;

	if (ctx == NULL || input == NULL || selected == NULL)
		return (false);
	step_y = menu_step_y(ctx);
	half_step = clamp_int(step_y / 2, 1, step_y);
	first_y = scale_from_bg(ctx->bg_row, ctx->bg_rows, MENU_FIRST_Y_RATIO);
	last_y = first_y + ((MENU_ITEM_COUNT - 1) * step_y);
	left_x = bunny_x(ctx);
	right_x = scale_from_bg(ctx->bg_col, ctx->bg_cols,
		MENU_PANEL_X_RATIO + MENU_PANEL_WIDTH_RATIO);
	if (input->x < left_x || input->x > right_x
		|| input->y < first_y - half_step
		|| input->y > last_y + half_step)
		return (false);
	index = (input->y - first_y + half_step) / step_y;
	index = clamp_int(index, 0, MENU_ITEM_COUNT - 1);
	center_y = first_y + (index * step_y);
	if (input->y < center_y - half_step
		|| input->y > center_y + half_step)
		return (false);
	*selected = index;
	return (true);
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
	if (ctx->menu_labels_plane != NULL)
	{
		ncplane_destroy(ctx->menu_labels_plane);
		ctx->menu_labels_plane = NULL;
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

	if (render_compatibility_mode(ctx))
		center_y = render_menu_label_y(ctx, m->selected);
	else
	{
		center_y = scale_from_bg(ctx->bg_row, ctx->bg_rows,
				MENU_FIRST_Y_RATIO);
		center_y += m->selected * menu_step_y(ctx);
	}
	y = center_y - (ctx->bunny_rows / 2);
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
 * @brief Blits the bunny as a crisp pixel sprite, scaled to its cell box.
 *
 * Used only where render_pixels_leak_safe() confirms the terminal will not
 * retain the placement when the selector moves on key repeat. The source is
 * resized to the selector's cell box in pixels using the terminal's cell
 * geometry, so it reads as smoothly as the authored HUD art.
 *
 * @param ctx Pointer to the render context.
 * @param y Target terminal row.
 * @param x Target terminal column.
 * @return Owned selector plane, or NULL when the pixel blit is unavailable.
 */
static struct ncplane	*create_bunny_pixel(render_ctx_t *ctx, int y, int x)
{
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	struct ncplane			*plane;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0)
		return (NULL);
	ncv = ncvisual_from_file(BUNNY_ASSET_PATH);
	if (ncv == NULL)
		return (NULL);
	plane = NULL;
	if (ncvisual_resize(ncv, ctx->bunny_rows * ctx->cell_px_y,
			ctx->bunny_cols * ctx->cell_px_x) == 0)
	{
		memset(&vopts, 0, sizeof(vopts));
		vopts.n = ctx->std;
		vopts.scaling = NCSCALE_NONE;
		vopts.y = y;
		vopts.x = x;
		vopts.blitter = NCBLIT_PIXEL;
		vopts.flags = NCVISUAL_OPTION_CHILDPLANE;
		plane = ncvisual_blit(ctx->nc, ncv, &vopts);
	}
	ncvisual_destroy(ncv);
	return (plane);
}

/**
 * @brief Creates the highest-quality bunny selector supported by the terminal.
 *
 * Leak-safe pixel terminals (Kitty, Ghostty) get a crisp pixel sprite. Where a
 * Compatibility mode always uses a native terminal marker. This avoids moving
 * a cell-blitted visual plane, which some terminals retain at its old position.
 * Other bitmap-unsafe terminals use the densest cell blitter available.
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

	if (render_compatibility_mode(ctx))
		return (create_compatibility_selector(ctx, y, x));
	if (!render_compatibility_mode(ctx) && render_pixels_leak_safe(ctx))
	{
		plane = create_bunny_pixel(ctx, y, x);
		if (plane != NULL)
			return (plane);
	}
	ncv = ncvisual_from_file(BUNNY_ASSET_PATH);
	if (ncv == NULL)
		return (create_bunny_fallback(ctx, y, x));
	plane = NULL;
	if (ncvisual_resize(ncv, ctx->bunny_rows * 4,
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
		&& ncvisual_resize(ncv, ctx->bunny_rows * 2,
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
 * @brief Creates the compact native selector used by compatibility mode.
 *
 * ASCII-only text keeps the marker equally readable in terminals with
 * different Unicode width tables.
 *
 * @param ctx Pointer to the render context.
 * @param y Target terminal row.
 * @param x Target terminal column.
 * @return Owned selector plane, or NULL when allocation fails.
 */
static struct ncplane	*create_compatibility_selector(render_ctx_t *ctx,
	int y, int x)
{
	ncplane_options	opts;
	struct ncplane	*plane;
	uint64_t		channels;

	memset(&opts, 0, sizeof(opts));
	opts.y = y;
	opts.x = x;
	opts.rows = COMPAT_SELECTOR_ROWS;
	opts.cols = COMPAT_SELECTOR_COLS;
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
		return (NULL);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 255, 203, 102);
	(void)ncchannels_set_bg_rgb8(&channels, 63, 23, 78);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	(void)ncplane_set_fg_rgb8(plane, 255, 203, 102);
	(void)ncplane_set_bg_rgb8(plane, 63, 23, 78);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_aligned(plane, 0, NCALIGN_CENTER, "[>]");
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
