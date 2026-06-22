#include "tetrisu.h"

/**
 * @brief Creates the menu-text and bunny overlay planes on top of the
 * background image already blitted onto ctx->std, and draws them once.
 *
 * Position is computed as a fraction of the background's cell dimensions
 * (roughly where the original artwork's menu text sits: ~72% down, ~45%
 * across), so it lands in the same relative spot at any terminal size.
 *
 * @param ctx Pointer to the render context; std must already be set by
 * render_init().
 */
void	render_menu_create(render_ctx_t *ctx)
{
	unsigned		rows;
	unsigned		cols;
	ncplane_options	menu_opts;
	ncplane_options	bunny_opts;
	int				i;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	ctx->menu_row = (int)(rows * 0.72);
	ctx->menu_col = (int)(cols * 0.45);
	memset(&menu_opts, 0, sizeof(menu_opts));
	menu_opts.y = ctx->menu_row;
	menu_opts.x = ctx->menu_col;
	menu_opts.rows = MENU_ITEM_COUNT;
	menu_opts.cols = cols - (unsigned)ctx->menu_col;
	ctx->menu_plane = ncplane_create(ctx->std, &menu_opts);
	ncplane_set_fg_rgb8(ctx->menu_plane, 200, 170, 20);
	i = 0;
	while (i < MENU_ITEM_COUNT)
	{
		ncplane_putstr_yx(ctx->menu_plane, i, 2, menu_item_label(i));
		i++;
	}
	memset(&bunny_opts, 0, sizeof(bunny_opts));
	bunny_opts.y = ctx->menu_row;
	bunny_opts.x = ctx->menu_col - 3;
	bunny_opts.rows = 1;
	bunny_opts.cols = 3;
	ctx->bunny_plane = ncplane_create(ctx->std, &bunny_opts);
	ncplane_set_fg_rgb8(ctx->bunny_plane, 255, 150, 200);
	ncplane_putstr_yx(ctx->bunny_plane, 0, 0, "\xf0\x9f\x90\x87");
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
	ncplane_move_yx(ctx->bunny_plane, ctx->menu_row + m->selected,
		ctx->menu_col - 3);
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
