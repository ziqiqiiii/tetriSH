#include "tetrisu.h"

// Static Variables
static const color_t	g_white = {250, 245, 250};

_Static_assert(HUD_BOARD_X % HUD_TILE_SIZE == 0
	&& HUD_BOARD_Y % HUD_TILE_SIZE == 0
	&& SOLO_NEXT_X % HUD_TILE_SIZE == 0
	&& SOLO_NEXT_Y % HUD_TILE_SIZE == 0
	&& SOLO_METER_X % HUD_TILE_SIZE == 0
	&& SOLO_METER_Y % HUD_TILE_SIZE == 0
	&& SOLO_MIRURUN_X % HUD_TILE_SIZE == 0
	&& SOLO_MIRURUN_Y % HUD_TILE_SIZE == 0,
	"pixel region origins must align to the 16px HUD grid");
/* Adjacent score planes keep each dynamic pixel in one refreshable region. */
_Static_assert(HUD_SCORE_X % HUD_TILE_SIZE == 0
	&& HUD_SCORE_WIDTH % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_HEADER_Y % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_HEADER_HEIGHT % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_VALUE_Y % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_VALUE_HEIGHT % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_STATS_Y % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_STATS_HEIGHT % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_EVENT_Y % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_EVENT_HEIGHT % HUD_TILE_SIZE == 0
	&& SOLO_SCORE_HEADER_Y + SOLO_SCORE_HEADER_HEIGHT == SOLO_SCORE_VALUE_Y
	&& SOLO_SCORE_VALUE_Y + SOLO_SCORE_VALUE_HEIGHT == SOLO_SCORE_STATS_Y
	&& SOLO_SCORE_STATS_Y + SOLO_SCORE_STATS_HEIGHT == SOLO_SCORE_EVENT_Y,
	"score pixel regions must be aligned and non-overlapping");
_Static_assert(SOLO_CONTROLS_X % HUD_TILE_SIZE == 0
	&& SOLO_CONTROLS_WIDTH % HUD_TILE_SIZE == 0,
	"controls cell region must align horizontally to the HUD grid");

// Static Functions
static void	reset_render_signatures(solo_render_t *solo);
static void	calculate_solo_layout(render_ctx_t *ctx, solo_render_t *solo);
static bool	create_solo_planes(render_ctx_t *ctx, solo_render_t *solo);
static bool	composite_board_required(const render_ctx_t *ctx);
static void	set_standard_backdrop(render_ctx_t *ctx);
static bool	create_background_plane(render_ctx_t *ctx, solo_render_t *solo);
static struct ncplane	*create_plane(render_ctx_t *ctx, int y, int x,
	int rows, int cols);
static bool	blit_surface(render_ctx_t *ctx, struct ncplane *plane,
	const uint32_t *pixels, int width, int height, int row_stride,
	ncblitter_e blitter);
static void	destroy_plane(struct ncplane **plane);
static bool	draw_status_message(render_ctx_t *ctx, solo_render_t *solo,
	const char *message);
static void	set_transparent_base(struct ncplane *plane);
static int	update_foreground_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game);
static int	update_hud_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game);
static uint64_t	next_frame_signature(const solo_game_t *game);
static uint64_t	hash_value(uint64_t hash, uint64_t value);
static uint64_t	meter_frame_signature(const solo_render_t *solo,
	const solo_game_t *game);
static uint64_t	score_stats_signature(const solo_game_t *game);
static uint64_t	score_event_signature(const solo_render_t *solo,
	const solo_game_t *game);
static bool	update_pixel_region(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, int source_x, int source_y, int source_width,
	int source_height);
static bool	update_hud_pixel_region(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, int source_x, int source_y, int source_width,
	int source_height, const char *region_name);
static bool	create_controls_plane(render_ctx_t *ctx, solo_render_t *solo);
static int	update_board_region(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game);
static bool	update_composite_board(render_ctx_t *ctx,
	solo_render_t *solo, bool use_cells);
static color_t	sample_board_pixel(const solo_render_t *solo, int x, int y);
static int	color_distance(color_t first, color_t second);
static bool	put_quadrant_cell(struct ncplane *plane, int y, int x,
	const color_t samples[4]);
static uint64_t	board_overlay_signature(const solo_game_t *game);
static uint64_t	settled_row_signature(const solo_game_t *game, int row);
static int	board_tile_index(const solo_game_t *game, int col, int row);
static void	destroy_board_tiles(solo_render_t *solo);
static void	destroy_settled_row(solo_render_t *solo, int row);
static int	update_settled_rows(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game);
static bool	rebuild_settled_row(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game, int row, uint64_t signature);
static void	copy_tile_pixels(const solo_render_t *solo, int tile_index,
	uint32_t *destination, int destination_stride, bool ghost);
static struct ncplane	*create_pixel_plane_at(render_ctx_t *ctx,
	solo_render_t *solo, const uint32_t *pixels, int width, int height,
	int row_stride, int source_x, int source_y);
static int	update_piece_planes(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game);
static bool	piece_geometry(const t_piece *piece, piece_geometry_t *geometry);
static bool	piece_rectangles_overlap(const piece_geometry_t *first,
	const piece_geometry_t *second);
static int	position_piece_pair(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, uint64_t *cached_signature,
	const piece_geometry_t *active, const piece_geometry_t *ghost,
	int tile_index);
static void	piece_pair_bounds(const piece_geometry_t *active,
	const piece_geometry_t *ghost, piece_bounds_t *bounds);
static uint64_t	piece_pair_signature(const piece_geometry_t *active,
	const piece_geometry_t *ghost, const piece_bounds_t *bounds,
	int tile_index);
static struct ncplane	*create_piece_pair_plane(render_ctx_t *ctx,
	solo_render_t *solo, const piece_geometry_t *active,
	const piece_geometry_t *ghost, const piece_bounds_t *bounds,
	int tile_index);
static int	position_atomic_piece(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, uint64_t *cached_signature,
	const piece_geometry_t *geometry, int tile_index, bool ghost);
static uint64_t	piece_shape_signature(const piece_geometry_t *geometry,
	int tile_index, bool ghost);
static struct ncplane	*create_atomic_piece_plane(render_ctx_t *ctx,
	solo_render_t *solo, const piece_geometry_t *geometry, int tile_index,
	bool ghost);
static void	compose_atomic_piece_pixels(uint32_t *pixels,
	const solo_render_t *solo, const piece_geometry_t *geometry,
	int tile_index, bool ghost);
static void	destroy_solo_planes(solo_render_t *solo);
static void	destroy_board_planes(solo_render_t *solo);

/**
 * @brief Creates all state required by the Solo renderer.
 *
 * The fixed-resolution canvas is loaded once, scaled to terminal geometry, and
 *   backed by capability-appropriate planes.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 */
void	render_solo_create(render_ctx_t *ctx, solo_render_t *solo)
{
	memset(solo, 0, sizeof(*solo));
	reset_render_signatures(solo);
	calculate_solo_layout(ctx, solo);
	if (!notcurses_canpixel(ctx->nc))
	{
		solo_canvas_set_error(solo,
			"Solo requires Kitty, Sixel, or another pixel-graphics terminal",
			NULL);
		return ;
	}
	solo->assets_ready = solo_canvas_load(solo);
	solo->composite_board = composite_board_required(ctx);
	solo->cell_board = solo->composite_board;
	if (solo->layout_valid && solo->assets_ready
		&& !create_solo_planes(ctx, solo))
		solo_canvas_set_error(solo,
			"Terminal rejected the Solo foreground bitmap",
			NULL);
}

/**
 * @brief Presents changed Solo regions to the terminal.
 *
 * Dirty signatures prevent unchanged HUD and board regions from being
 *   regenerated or rendered.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
void	render_solo_draw(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	int	changed;
	int	result;

	if (!solo->layout_valid)
	{
		if (draw_status_message(ctx, solo,
				"Game paused - resize the terminal to at least 64 x 24"))
			(void)notcurses_render(ctx->nc);
		return ;
	}
	if (!solo->assets_ready || !solo->planes_ready)
	{
		if (draw_status_message(ctx, solo, solo->asset_error))
			(void)notcurses_render(ctx->nc);
		return ;
	}
	changed = 0;
	if (solo->status_plane != NULL)
	{
		destroy_plane(&solo->status_plane);
		changed = 1;
	}
	result = update_foreground_regions(ctx, solo, game);
	if (result < 0)
	{
		if (result == -2 && solo->asset_error[0] == '\0')
			solo_canvas_set_error(solo,
				"Notcurses rejected a Solo HUD image", NULL);
		else if (result != -2)
			solo_canvas_set_error(solo,
				"Notcurses rejected the Solo board cells", NULL);
		goto render_failure;
	}
	changed |= result;
	if (changed > 0 && notcurses_render(ctx->nc) != 0)
	{
		solo_canvas_set_error(solo,
			"Notcurses could not present the Solo frame", NULL);
		goto render_failure;
	}
	return ;
render_failure:
	destroy_solo_planes(solo);
	set_standard_backdrop(ctx);
	if (draw_status_message(ctx, solo, solo->asset_error))
		(void)notcurses_render(ctx->nc);
}

/**
 * @brief Releases every plane and pixel buffer owned by Solo rendering.
 *
 * Plane destruction precedes heap release, then the state is zeroed for safe
 *   reuse.
 *
 * @param solo Pointer to the Solo render state.
 */
void	render_solo_destroy(solo_render_t *solo)
{
	destroy_solo_planes(solo);
	free(solo->static_pixels);
	free(solo->frame_pixels);
	free(solo->tile_pixels);
	free(solo->font_pixels);
	free(solo->number_pixels);
	memset(solo, 0, sizeof(*solo));
}

/**
 * @brief Reflows Solo planes after a terminal geometry change.
 *
 * Assets remain cached while terminal-sized planes are destroyed and recreated
 *   from refreshed cell geometry.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 */
void	render_solo_resize(render_ctx_t *ctx, solo_render_t *solo)
{
	destroy_solo_planes(solo);
	if (render_geometry_refresh(ctx, true) < 0)
	{
		solo->layout_valid = false;
		return ;
	}
	calculate_solo_layout(ctx, solo);
	if (!notcurses_canpixel(ctx->nc))
	{
		solo_canvas_set_error(solo,
			"Solo requires Kitty, Sixel, or another pixel-graphics terminal",
			NULL);
		return ;
	}
	solo->composite_board = composite_board_required(ctx);
	solo->cell_board = solo->composite_board;
	if (solo->layout_valid && solo->assets_ready
		&& !create_solo_planes(ctx, solo))
		solo_canvas_set_error(solo,
			"Terminal rejected the Solo foreground bitmap",
			NULL);
}

/**
 * @brief Invalidates all cached region signatures.
 *
 * The next draw treats every dynamic region as dirty and repopulates its
 *   plane.
 *
 * @param solo Pointer to the Solo render state.
 */
static void	reset_render_signatures(solo_render_t *solo)
{
	int	row;

	row = 0;
	while (row < BOARD_HEIGHT)
	{
		solo->row_signatures[row] = UINT64_MAX;
		row++;
	}
	solo->next_signature = UINT64_MAX;
	solo->meter_signature = UINT64_MAX;
	solo->score_value_signature = UINT64_MAX;
	solo->score_stats_signature = UINT64_MAX;
	solo->score_event_signature = UINT64_MAX;
	solo->overlay_signature = UINT64_MAX;
	solo->active_shape_signature = UINT64_MAX;
	solo->ghost_shape_signature = UINT64_MAX;
	solo->piece_planes_combined = false;
}

/**
 * @brief Selects the cell-composited path for unsafe movable-image backends.
 *
 * AI-assisted: terminals without reliable movable pixel planes use one
 * cell-composited board. Native Kitty/iTerm2/WezTerm backends retain the exact
 * authored tile sprites; the Solo loop bounds their presentation rate.
 *
 * @param ctx Active render context.
 * @return true when Solo should refresh one stationary board plane.
 */
static bool	composite_board_required(const render_ctx_t *ctx)
{
	return (!render_pixel_planes_reliable(ctx));
}

/**
 * @brief Calculates an aspect-correct integer-grid Solo layout.
 *
 * Tile rows and columns use whole terminal cells so every authored 16-pixel
 *   tile remains aligned.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 */
static void	calculate_solo_layout(render_ctx_t *ctx, solo_render_t *solo)
{
	unsigned	std_rows;
	unsigned	std_cols;
	unsigned	max_bitmap_y;
	unsigned	max_bitmap_x;
	int			candidate_cols;
	int			candidate_rows;
	int			pixel_width;
	int			pixel_height;
	int			error;
	int			minimum;
	int			best_cols;
	int			best_rows;
	int			best_error;
	int			best_minimum;
	bool			has_square_candidate;
	bool			pixel_capable;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	max_bitmap_y = 0;
	max_bitmap_x = 0;
	ncplane_pixel_geom(ctx->std, NULL, NULL, NULL, NULL,
		&max_bitmap_y, &max_bitmap_x);
	pixel_capable = notcurses_canpixel(ctx->nc);
	solo->tile_cols = 0;
	solo->tile_rows = 0;
	best_cols = 0;
	best_rows = 0;
	best_error = INT32_MAX;
	best_minimum = 1;
	has_square_candidate = false;
	candidate_cols = 1;
	while (candidate_cols * 32 <= (int)std_cols)
	{
		candidate_rows = (candidate_cols * ctx->cell_px_x
			+ ctx->cell_px_y / 2) / ctx->cell_px_y;
		if (candidate_rows < 1)
			candidate_rows = 1;
		pixel_width = candidate_cols * ctx->cell_px_x;
		pixel_height = candidate_rows * ctx->cell_px_y;
		error = abs(pixel_width - pixel_height);
		minimum = pixel_width < pixel_height ? pixel_width : pixel_height;
		if (candidate_rows * 24 <= (int)std_rows
			&& (!pixel_capable || max_bitmap_x == 0
				|| candidate_cols * 14 * ctx->cell_px_x
				<= (int)max_bitmap_x)
			&& (!pixel_capable || max_bitmap_y == 0
				|| candidate_rows * 23 * ctx->cell_px_y
				<= (int)max_bitmap_y))
		{
			if (error * 10 <= minimum)
			{
				has_square_candidate = true;
				solo->tile_cols = candidate_cols;
				solo->tile_rows = candidate_rows;
			}
			else if (!has_square_candidate
				&& (best_cols == 0
					|| error * best_minimum < best_error * minimum
					|| (error * best_minimum == best_error * minimum
						&& candidate_cols > best_cols)))
			{
				best_cols = candidate_cols;
				best_rows = candidate_rows;
				best_error = error;
				best_minimum = minimum;
			}
		}
		candidate_cols++;
	}
	if (!has_square_candidate)
	{
		solo->tile_cols = best_cols;
		solo->tile_rows = best_rows;
	}
	solo->canvas_cols = solo->tile_cols * 32;
	solo->canvas_rows = solo->tile_rows * 24;
	solo->layout_valid = solo->canvas_cols >= SOLO_MIN_CANVAS_COLS
		&& solo->canvas_rows >= SOLO_MIN_CANVAS_ROWS;
	if (!solo->layout_valid)
		return ;
	solo->canvas_col = ((int)std_cols - solo->canvas_cols) / 2;
	solo->canvas_row = ((int)std_rows - solo->canvas_rows) / 2;
}

/**
 * @brief Creates the terminal planes for a loaded Solo canvas.
 *
 * Static background creation establishes the base layer before dynamic planes
 *   are added.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @return true on success, otherwise false.
 */
static bool	create_solo_planes(render_ctx_t *ctx, solo_render_t *solo)
{
	set_standard_backdrop(ctx);
	if (!create_background_plane(ctx, solo))
		return (false);
	solo->planes_ready = true;
	return (true);
}

/**
 * @brief Applies the Solo backdrop color to the standard plane.
 *
 * Erasing the standard plane prevents stale homepage cells around the centered
 *   canvas.
 *
 * @param ctx Pointer to the active render context.
 */
static void	set_standard_backdrop(render_ctx_t *ctx)
{
	uint64_t	channels;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 7, 13, 23);
	(void)ncchannels_set_bg_rgb8(&channels, 7, 13, 23);
	(void)ncplane_set_base(ctx->std, " ", 0, channels);
	ncplane_erase(ctx->std);
}

/**
 * @brief Creates and fills the static Solo background plane.
 *
 * The background uses a low-resolution cell blitter while foreground assets
 *   remain pixel-rendered.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @return true on success, otherwise false.
 */
static bool	create_background_plane(render_ctx_t *ctx, solo_render_t *solo)
{
	solo->background_plane = create_plane(ctx, solo->canvas_row,
		solo->canvas_col, solo->canvas_rows, solo->canvas_cols);
	if (solo->background_plane == NULL)
		return (false);
	if (!blit_surface(ctx, solo->background_plane, solo->static_pixels,
			SOLO_CANVAS_WIDTH, SOLO_CANVAS_HEIGHT,
			SOLO_CANVAS_WIDTH, NCBLIT_4x2))
	{
		destroy_plane(&solo->background_plane);
		return (false);
	}
	return (true);
}

/**
 * @brief Creates a child plane with validated dimensions.
 *
 * Zero or negative geometry is rejected before calling Notcurses.
 *
 * @param ctx Pointer to the active render context.
 * @param y Terminal or canvas row coordinate.
 * @param x Terminal or canvas column coordinate.
 * @param rows Plane height in terminal rows.
 * @param cols Plane width in terminal columns.
 * @return Owned child plane, or NULL when geometry or allocation fails.
 */
static struct ncplane	*create_plane(render_ctx_t *ctx, int y, int x,
	int rows, int cols)
{
	ncplane_options	opts;

	if (rows <= 0 || cols <= 0)
		return (NULL);
	memset(&opts, 0, sizeof(opts));
	opts.y = y;
	opts.x = x;
	opts.rows = rows;
	opts.cols = cols;
	return (ncplane_create(ctx->std, &opts));
}

/**
 * @brief Blits one RGBA surface into an existing plane.
 *
 * A temporary visual owns only the supplied pixel view; the destination plane
 *   keeps terminal ownership.
 *
 * @param ctx Pointer to the active render context.
 * @param plane Pointer to an owned plane handle.
 * @param pixels Pointer to the source RGBA pixels.
 * @param width Pixel or terminal width.
 * @param height Pixel or terminal height.
 * @param row_stride Number of source pixels per row.
 * @param blitter Notcurses blitter used for the surface.
 * @return true when Notcurses accepts the surface, otherwise false.
 */
static bool	blit_surface(render_ctx_t *ctx, struct ncplane *plane,
	const uint32_t *pixels, int width, int height, int row_stride,
	ncblitter_e blitter)
{
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	bool					ok;

	if (width <= 0 || height <= 0 || row_stride < width
		|| row_stride > INT32_MAX / (int)sizeof(*pixels))
		return (false);
	ncv = ncvisual_from_rgba(pixels, height,
		row_stride * (int)sizeof(*pixels), width);
	if (ncv == NULL)
		return (false);
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_STRETCH;
	vopts.blitter = blitter;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	if (blitter == NCBLIT_PIXEL)
		vopts.flags |= NCVISUAL_OPTION_NODEGRADE;
	ok = ncvisual_blit(ctx->nc, ncv, &vopts) != NULL;
	ncvisual_destroy(ncv);
	return (ok);
}

/**
 * @brief Destroys one owned plane and clears its handle.
 *
 * The handle is nulled before destruction so reentrant cleanup cannot reuse
 *   stale ownership.
 *
 * @param plane Pointer to an owned plane handle.
 */
static void	destroy_plane(struct ncplane **plane)
{
	struct ncplane	*old;

	if (*plane != NULL)
	{
		old = *plane;
		*plane = NULL;
		ncplane_destroy(old);
	}
}

/**
 * @brief Draws a centered fallback or resize message.
 *
 * One reusable transparent plane reports capability and asset failures without
 *   entering gameplay rendering.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param message Human-readable fallback message.
 * @return true when the message plane is ready, otherwise false.
 */
static bool	draw_status_message(render_ctx_t *ctx, solo_render_t *solo,
	const char *message)
{
	unsigned	rows;
	unsigned	cols;
	int			x;

	if (solo->status_plane == NULL)
	{
		ncplane_dim_yx(ctx->std, &rows, &cols);
		solo->status_plane = create_plane(ctx, 0, 0, (int)rows, (int)cols);
		if (solo->status_plane == NULL)
			return (false);
		set_transparent_base(solo->status_plane);
	}
	ncplane_erase(solo->status_plane);
	ncplane_set_fg_rgb8(solo->status_plane, 255, 225, 242);
	ncplane_set_bg_alpha(solo->status_plane, NCALPHA_TRANSPARENT);
	ncplane_dim_yx(solo->status_plane, &rows, &cols);
	x = ((int)cols - (int)strlen(message)) / 2;
	if (x < 0)
		x = 0;
	ncplane_putstr_yx(solo->status_plane, (int)rows / 2, x, message);
	ncplane_move_top(solo->status_plane);
	return (true);
}

/**
 * @brief Resets a plane to a fully transparent base cell.
 *
 * Transparent child planes can then overlay the authored background without
 *   opaque blank cells.
 *
 * @param plane Pointer to an owned plane handle.
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

/**
 * @brief Updates all dirty foreground regions for one frame.
 *
 * HUD and board changes are accumulated so Notcurses renders at most once per
 *   state update.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	update_foreground_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	int	result;
	int	changed;

	changed = update_hud_regions(ctx, solo, game);
	if (changed < 0)
		return (-2);
	result = update_board_region(ctx, solo, game);
	if (result < 0)
		return (-3);
	return (changed | result);
}

/**
 * @brief Refreshes only dirty high-resolution HUD regions.
 *
 * The full canvas is recomposed only when a HUD signature changes, avoiding
 *   work on ordinary rotations.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	update_hud_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	next_signature;
	uint64_t	meter_signature;
	uint64_t	stats_signature;
	uint64_t	event_signature;
	int			changed;
	bool		hud_dirty;

	next_signature = next_frame_signature(game);
	meter_signature = meter_frame_signature(solo, game);
	stats_signature = score_stats_signature(game);
	event_signature = score_event_signature(solo, game);
	hud_dirty = next_signature != solo->next_signature
		|| meter_signature != solo->meter_signature
		|| solo->mirurun_plane == NULL || solo->score_header_plane == NULL
		|| game->scoring.total != solo->score_value_signature
		|| stats_signature != solo->score_stats_signature
		|| event_signature != solo->score_event_signature;
	if (hud_dirty)
		solo_canvas_compose_hud(solo, game);
	changed = 0;
	if (next_signature != solo->next_signature)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->next_plane,
				SOLO_NEXT_X, SOLO_NEXT_Y, SOLO_NEXT_WIDTH, SOLO_NEXT_HEIGHT,
				"next queue"))
			return (-1);
		solo->next_signature = next_signature;
		changed = 1;
	}
	if (meter_signature != solo->meter_signature)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->meter_plane,
				SOLO_METER_X, SOLO_METER_Y,
				SOLO_METER_WIDTH, SOLO_METER_HEIGHT, "ability meter"))
			return (-1);
		solo->meter_signature = meter_signature;
		changed = 1;
	}
	if (solo->mirurun_plane == NULL)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->mirurun_plane,
				SOLO_MIRURUN_X, SOLO_MIRURUN_Y,
				SOLO_MIRURUN_WIDTH, SOLO_MIRURUN_HEIGHT, "Mirurun portrait"))
			return (-1);
		changed = 1;
	}
	if (solo->score_header_plane == NULL)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->score_header_plane,
				HUD_SCORE_X, SOLO_SCORE_HEADER_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_HEADER_HEIGHT, "score header"))
			return (-1);
		changed = 1;
	}
	if (game->scoring.total != solo->score_value_signature)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->score_value_plane,
				HUD_SCORE_X, SOLO_SCORE_VALUE_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_VALUE_HEIGHT, "score value"))
			return (-1);
		solo->score_value_signature = game->scoring.total;
		changed = 1;
	}
	if (stats_signature != solo->score_stats_signature)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->score_stats_plane,
				HUD_SCORE_X, SOLO_SCORE_STATS_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_STATS_HEIGHT, "score statistics"))
			return (-1);
		solo->score_stats_signature = stats_signature;
		changed = 1;
	}
	if (event_signature != solo->score_event_signature)
	{
		if (!update_hud_pixel_region(ctx, solo, &solo->score_event_plane,
				HUD_SCORE_X, SOLO_SCORE_EVENT_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_EVENT_HEIGHT, "score event"))
			return (-1);
		solo->score_event_signature = event_signature;
		changed = 1;
	}
	if (solo->controls_plane == NULL)
	{
		if (!create_controls_plane(ctx, solo))
		{
			solo_canvas_set_error(solo,
				"Solo controls do not fit this terminal width", NULL);
			return (-1);
		}
		changed = 1;
	}
	return (changed);
}

/**
 * @brief Hashes the three-piece preview queue.
 *
 * The next panel changes only after a piece locks and the queue advances.
 *
 * @param game Pointer to the current Solo game state.
 * @return Signature for the preview queue.
 */
static uint64_t	next_frame_signature(const solo_game_t *game)
{
	uint64_t	hash;
	int			index;

	hash = UINT64_C(1469598103934665603);
	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		hash = hash_value(hash, (uint64_t)game->next[index]);
		index++;
	}
	return (hash);
}

/**
 * @brief Mixes one 64-bit value into an FNV-style signature.
 *
 * Byte-wise mixing gives stable dirty-region keys without storing duplicate
 *   game snapshots.
 *
 * @param hash Current 64-bit signature.
 * @param value Value to mix into the signature.
 * @return Updated 64-bit signature.
 */
static uint64_t	hash_value(uint64_t hash, uint64_t value)
{
	int	byte;

	byte = 0;
	while (byte < 8)
	{
		hash ^= (value >> (byte * 8)) & 0xffu;
		hash *= UINT64_C(1099511628211);
		byte++;
	}
	return (hash);
}

/**
 * @brief Hashes charge and interactive marker state for the meter plane.
 *
 * @param solo Pointer to the Solo renderer containing hover state.
 * @param game Pointer to the current Solo game state.
 * @return Signature for the complete interactive ability meter.
 */
static uint64_t	meter_frame_signature(const solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	hash;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, (uint64_t)game->crystal_charge);
	hash = hash_value(hash, (uint64_t)solo->hovered_ability);
	hash = hash_value(hash, (uint64_t)game->last_ability);
	hash = hash_value(hash, (uint64_t)game->ability_result);
	return (hash);
}

/**
 * @brief Hashes level, lines, and combo HUD values.
 *
 * Packing the small integer fields avoids redrawing unchanged score
 *   statistics.
 *
 * @param game Pointer to the current Solo game state.
 * @return Signature for level, lines, and combo.
 */
static uint64_t	score_stats_signature(const solo_game_t *game)
{
	uint64_t	hash;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, (uint64_t)(game->scoring.combo + 1));
	hash = hash_value(hash, (uint64_t)game->level);
	hash = hash_value(hash, (uint64_t)game->total_lines);
	return (hash);
}

/**
 * @brief Hashes the latest scoring-award label state.
 *
 * Line, spin, perfect-clear, back-to-back, and award values share one dirty
 *   key.
 *
 * @param game Pointer to the current Solo game state.
 * @return Signature for the last scoring event.
 */
static uint64_t	score_event_signature(const solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	hash;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, game->scoring.back_to_back);
	hash = hash_value(hash, (uint64_t)game->last_lines);
	hash = hash_value(hash, (uint64_t)game->last_spin);
	hash = hash_value(hash, game->last_perfect_clear);
	hash = hash_value(hash, game->last_score.total_awarded);
	hash = hash_value(hash, (uint64_t)solo->hovered_ability);
	hash = hash_value(hash, (uint64_t)game->last_ability);
	hash = hash_value(hash, (uint64_t)game->ability_result);
	return (hash);
}

/**
 * @brief Replaces one aligned high-resolution foreground region.
 *
 * Regions are mapped from the 512x384 canvas to terminal cells and blitted as
 *   independent pixel planes.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param plane Pointer to an owned plane handle.
 * @param source_x Left edge of the source canvas region.
 * @param source_y Top edge of the source canvas region.
 * @param source_width Source-region width in pixels.
 * @param source_height Source-region height in pixels.
 * @return true on success, otherwise false.
 */
static bool	update_pixel_region(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, int source_x, int source_y, int source_width,
	int source_height)
{
	const uint32_t	*pixels;
	bool			created;
	int				y;
	int				x;
	int				rows;
	int				cols;

	if (source_x < 0 || source_y < 0 || source_width <= 0 || source_height <= 0
		|| source_x + source_width > SOLO_CANVAS_WIDTH
		|| source_y + source_height > SOLO_CANVAS_HEIGHT
		|| source_x % HUD_TILE_SIZE != 0 || source_y % HUD_TILE_SIZE != 0
		|| source_width % HUD_TILE_SIZE != 0
		|| source_height % HUD_TILE_SIZE != 0)
		return (false);
	y = solo->canvas_row + source_y / HUD_TILE_SIZE * solo->tile_rows;
	x = solo->canvas_col + source_x / HUD_TILE_SIZE * solo->tile_cols;
	rows = source_height / HUD_TILE_SIZE * solo->tile_rows;
	cols = source_width / HUD_TILE_SIZE * solo->tile_cols;
	created = false;
	if (*plane == NULL)
	{
		*plane = create_plane(ctx, y, x, rows, cols);
		if (*plane == NULL)
			return (false);
		created = true;
	}
	pixels = &solo->frame_pixels[(size_t)source_y * SOLO_CANVAS_WIDTH
		+ source_x];
	if (!blit_surface(ctx, *plane, pixels, source_width, source_height,
			SOLO_CANVAS_WIDTH, NCBLIT_PIXEL))
	{
		if (created)
			destroy_plane(plane);
		return (false);
	}
	ncplane_move_top(*plane);
	return (true);
}

/**
 * @brief Refreshes a named HUD bitmap and records actionable fallback text.
 *
 * Keeping the failed region name lets users distinguish an asset-size/backend
 * limit from a board or terminal-presentation failure.
 */
static bool	update_hud_pixel_region(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, int source_x, int source_y, int source_width,
	int source_height, const char *region_name)
{
	char	message[120];

	if (update_pixel_region(ctx, solo, plane, source_x, source_y,
			source_width, source_height))
		return (true);
	snprintf(message, sizeof(message),
		"Notcurses rejected the Solo %s image", region_name);
	solo_canvas_set_error(solo, message, NULL);
	return (false);
}

/**
 * @brief Creates the compact terminal-font controls strip.
 *
 * Controls are cell text rather than a scaled mask so they remain readable at
 *   every supported size.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @return true on success, otherwise false.
 */
static bool	create_controls_plane(render_ctx_t *ctx, solo_render_t *solo)
{
	const char	*text;
	unsigned	rows;
	unsigned	cols;
	int			x;
	int			width;

	x = solo->canvas_col
		+ SOLO_CONTROLS_X / HUD_TILE_SIZE * solo->tile_cols;
	width = SOLO_CONTROLS_WIDTH / HUD_TILE_SIZE * solo->tile_cols;
	solo->controls_plane = create_plane(ctx,
		solo->canvas_row + solo->canvas_rows - 1, x, 1, width);
	if (solo->controls_plane == NULL)
		return (false);
	set_transparent_base(solo->controls_plane);
	ncplane_set_fg_rgb8(solo->controls_plane,
		g_white.r, g_white.g, g_white.b);
	ncplane_set_bg_alpha(solo->controls_plane, NCALPHA_TRANSPARENT);
	ncplane_dim_yx(solo->controls_plane, &rows, &cols);
	(void)rows;
	text = "ARROWS MOVE | UP/X CW | Z CCW | SPACE DROP | 1-4 ABILITY | "
		"P PAUSE | ESC HOME";
	if (cols < strlen(text))
		text = "ARROWS MOVE | X CW | SPACE DROP | 1-4 ABILITY | ESC HOME";
	if (cols < strlen(text))
		text = "ARROWS MOVE | SPACE DROP | 1-4 ABILITY | ESC HOME";
	if (cols < strlen(text))
		text = "ARROWS | X/Z ROT | SPACE | 1-4 | ESC";
	if (cols < strlen(text))
		text = "ARROWS X/Z SPACE 1-4 ESC";
	if (ncplane_putstr_aligned(solo->controls_plane, 0,
			NCALIGN_CENTER, text) < 0)
	{
		ncplane_destroy(solo->controls_plane);
		solo->controls_plane = NULL;
		return (false);
	}
	ncplane_move_top(solo->controls_plane);
	return (true);
}

/**
 * @brief Refreshes the capability-specific board representation.
 *
 * Reliable terminals update row and piece planes; composited terminals redraw
 *   one opaque board surface.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	update_board_region(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	signature;
	bool		use_cells;
	int			result;
	int			changed;

	if (solo->composite_board || game->paused
		|| game->phase == SOLO_GAME_OVER)
	{
		signature = board_overlay_signature(game);
		if (solo->board_overlay_plane != NULL
			&& signature == solo->overlay_signature)
			return (0);
		destroy_board_tiles(solo);
		solo_canvas_compose_board(solo, game);
		/* AI-assisted: fallback terminals keep active frames in cells, but
		 * spend one pixel image on the final board once animation stops. */
		use_cells = solo->cell_board && game->phase != SOLO_GAME_OVER;
		if (!update_composite_board(ctx, solo, use_cells))
			return (-1);
		solo->overlay_signature = signature;
		return (1);
	}
	changed = 0;
	if (solo->board_overlay_plane != NULL)
	{
		destroy_plane(&solo->board_overlay_plane);
		solo->overlay_signature = UINT64_MAX;
		changed = 1;
	}
	result = update_settled_rows(ctx, solo, game);
	if (result < 0)
		return (-1);
	changed |= result;
	result = update_piece_planes(ctx, solo, game);
	if (result < 0)
		return (-1);
	changed |= result;
	return (changed);
}

/**
 * @brief Refreshes the board with cells or a final-state pixel image.
 *
 * AI-assisted: true-colour quadrant glyphs encode four samples per terminal
 * cell. Reusing this plane lets Notcurses diff rotations down to changed cells
 * on pixel backends that cannot move image planes reliably. The final board
 * uses one pixel blit because it no longer animates.
 *
 * @param ctx Active render context.
 * @param solo Solo renderer with a freshly composed board pixel buffer.
 * @return true when the board cells were accepted.
 */
static bool	update_composite_board(render_ctx_t *ctx, solo_render_t *solo,
	bool use_cells)
{
	color_t	samples[4];
	int	y;
	int	x;
	int	rows;
	int	cols;
	int	source_x_start;
	int	source_x_end;
	int	source_y_start;
	int	source_y_end;

	y = solo->canvas_row + HUD_BOARD_Y / HUD_TILE_SIZE * solo->tile_rows;
	x = solo->canvas_col + HUD_BOARD_X / HUD_TILE_SIZE * solo->tile_cols;
	rows = SOLO_BOARD_HEIGHT / HUD_TILE_SIZE * solo->tile_rows;
	cols = SOLO_BOARD_WIDTH / HUD_TILE_SIZE * solo->tile_cols;
	if (solo->board_overlay_plane != NULL
		&& solo->board_plane_cells != use_cells)
		destroy_plane(&solo->board_overlay_plane);
	if (solo->board_overlay_plane == NULL)
	{
		solo->board_overlay_plane = create_plane(ctx, y, x, rows, cols);
		if (solo->board_overlay_plane == NULL)
			return (false);
		solo->board_plane_cells = use_cells;
	}
	else
		ncplane_erase(solo->board_overlay_plane);
	if (!use_cells)
	{
		if (!blit_surface(ctx, solo->board_overlay_plane,
				&solo->frame_pixels[(size_t)HUD_BOARD_Y * SOLO_CANVAS_WIDTH
					+ HUD_BOARD_X], SOLO_BOARD_WIDTH, SOLO_BOARD_HEIGHT,
				SOLO_CANVAS_WIDTH, NCBLIT_PIXEL))
			return (false);
	}
	else
	{
		y = 0;
		while (y < rows)
		{
			source_y_start = y * SOLO_BOARD_HEIGHT / rows;
			source_y_end = (y + 1) * SOLO_BOARD_HEIGHT / rows;
			x = 0;
			while (x < cols)
			{
				source_x_start = x * SOLO_BOARD_WIDTH / cols;
				source_x_end = (x + 1) * SOLO_BOARD_WIDTH / cols;
				samples[0] = sample_board_pixel(solo,
					source_x_start, source_y_start);
				samples[1] = sample_board_pixel(solo,
					source_x_end - 1, source_y_start);
				samples[2] = sample_board_pixel(solo,
					source_x_start, source_y_end - 1);
				samples[3] = sample_board_pixel(solo,
					source_x_end - 1, source_y_end - 1);
				if (!put_quadrant_cell(solo->board_overlay_plane,
						y, x, samples))
					return (false);
				x++;
			}
			y++;
		}
	}
	ncplane_move_top(solo->board_overlay_plane);
	return (true);
}

/**
 * @brief Reads one authored board pixel as an RGB colour.
 *
 * Coordinates are clamped because terminal-to-source division can produce an
 * empty final interval at extreme geometries.
 */
static color_t	sample_board_pixel(const solo_render_t *solo, int x, int y)
{
	color_t	color;
	uint32_t	pixel;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= SOLO_BOARD_WIDTH)
		x = SOLO_BOARD_WIDTH - 1;
	if (y >= SOLO_BOARD_HEIGHT)
		y = SOLO_BOARD_HEIGHT - 1;
	pixel = solo->frame_pixels[(size_t)(HUD_BOARD_Y + y)
			* SOLO_CANVAS_WIDTH + HUD_BOARD_X + x];
	color.r = ncpixel_r(pixel);
	color.g = ncpixel_g(pixel);
	color.b = ncpixel_b(pixel);
	return (color);
}

/**
 * @brief Returns squared RGB distance without floating-point work.
 */
static int	color_distance(color_t first, color_t second)
{
	int	red;
	int	green;
	int	blue;

	red = (int)first.r - (int)second.r;
	green = (int)first.g - (int)second.g;
	blue = (int)first.b - (int)second.b;
	return (red * red + green * green + blue * blue);
}

/**
 * @brief Encodes four board samples in one true-colour quadrant cell.
 *
 * AI-assisted: the farthest sample pair seeds a two-colour cluster. The
 * resulting quadrant glyph doubles horizontal board detail without creating
 * a terminal image placement, preserving bounded memory on fallback backends.
 */
static bool	put_quadrant_cell(struct ncplane *plane, int y, int x,
	const color_t samples[4])
{
	static const char	*glyphs[16] = {
		" ", "▗", "▖", "▄", "▝", "▐", "▞", "▟",
		"▘", "▚", "▌", "▙", "▀", "▜", "▛", " "
	};
	color_t			centres[2];
	int				sums[2][3];
	int				counts[2];
	int				farthest;
	int				distance;
	int				first;
	int				second;
	int				cluster;
	int				mask;
	int				i;
	int				j;

	farthest = -1;
	first = 0;
	second = 0;
	i = 0;
	while (i < 4)
	{
		j = i + 1;
		while (j < 4)
		{
			distance = color_distance(samples[i], samples[j]);
			if (distance > farthest)
			{
				farthest = distance;
				first = i;
				second = j;
			}
			j++;
		}
		i++;
	}
	if (farthest == 0)
	{
		(void)ncplane_set_bg_rgb8(plane,
			samples[0].r, samples[0].g, samples[0].b);
		return (ncplane_putegc_yx(plane, y, x, " ", NULL) >= 0);
	}
	centres[0] = samples[first];
	centres[1] = samples[second];
	memset(sums, 0, sizeof(sums));
	memset(counts, 0, sizeof(counts));
	mask = 0;
	i = 0;
	while (i < 4)
	{
		cluster = color_distance(samples[i], centres[1])
			< color_distance(samples[i], centres[0]);
		counts[cluster]++;
		sums[cluster][0] += samples[i].r;
		sums[cluster][1] += samples[i].g;
		sums[cluster][2] += samples[i].b;
		if (cluster == 1)
			mask |= 1 << (3 - i);
		i++;
	}
	(void)ncplane_set_bg_rgb8(plane, sums[0][0] / counts[0],
		sums[0][1] / counts[0], sums[0][2] / counts[0]);
	(void)ncplane_set_fg_rgb8(plane, sums[1][0] / counts[1],
		sums[1][1] / counts[1], sums[1][2] / counts[1]);
	return (ncplane_putegc_yx(plane, y, x, glyphs[mask], NULL) >= 0);
}

/**
 * @brief Hashes every state component used by a composited board.
 *
 * Sixel-class rendering redraws only when settled cells, active piece, pause,
 *   or overlay state changes.
 *
 * @param game Pointer to the current Solo game state.
 * @return Signature for the complete composited board.
 */
static uint64_t	board_overlay_signature(const solo_game_t *game)
{
	uint64_t	hash;
	int			row;

	hash = UINT64_C(1469598103934665603);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		hash = hash_value(hash, settled_row_signature(game, row));
		row++;
	}
	hash = hash_value(hash, (uint64_t)game->phase);
	hash = hash_value(hash, game->paused);
	hash = hash_value(hash, (uint64_t)(game->active.type + 1));
	hash = hash_value(hash, (uint64_t)(game->active.rotation + 1));
	hash = hash_value(hash, (uint64_t)(game->active.col + BOARD_WIDTH));
	hash = hash_value(hash, (uint64_t)(game->active.row + BOARD_HEIGHT));
	return (hash);
}

/**
 * @brief Hashes all visible content in one settled row.
 *
 * Each row can be rebuilt independently when its cells or clear-animation
 *   frame changes.
 *
 * @param game Pointer to the current Solo game state.
 * @param row Board row index.
 * @return Signature for the requested settled row.
 */
static uint64_t	settled_row_signature(const solo_game_t *game, int row)
{
	uint64_t	hash;
	int			col;

	hash = UINT64_C(1469598103934665603);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		hash = hash_value(hash,
			(uint64_t)(board_tile_index(game, col, row) + 1));
		col++;
	}
	return (hash);
}

/**
 * @brief Maps one board cell to its tile-atlas index.
 *
 * Empty, garbage, clearing, and colored settled cells are classified from
 *   authoritative game state.
 *
 * @param game Pointer to the current Solo game state.
 * @param col Board column index.
 * @param row Board row index.
 * @return Tile-atlas index, or -1 for an empty cell.
 */
static int	board_tile_index(const solo_game_t *game, int col, int row)
{
	t_cell	cell;

	if (solo_game_row_is_clearing(game, row))
	{
		if (game->clear_elapsed_ms >= SOLO_CLEAR_ANIMATION_MS / 2)
			return (TILE_CLEAR_SECOND);
		return (TILE_CLEAR_FIRST);
	}
	cell = board_get(&game->board, col, row);
	if (cell.type == CELL_GARBAGE)
		return (TILE_GARBAGE);
	if (cell.type == CELL_FILLED)
		return (solo_canvas_piece_tile((t_piece_type)cell.color));
	return (-1);
}

/**
 * @brief Destroys settled, active, and ghost tile planes.
 *
 * This clears the complete decomposed-board representation while preserving
 *   HUD planes.
 *
 * @param solo Pointer to the Solo render state.
 */
static void	destroy_board_tiles(solo_render_t *solo)
{
	int	row;

	destroy_plane(&solo->active_plane);
	destroy_plane(&solo->ghost_plane);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		destroy_settled_row(solo, row);
		row++;
	}
	solo->active_shape_signature = UINT64_MAX;
	solo->ghost_shape_signature = UINT64_MAX;
	solo->piece_planes_combined = false;
}

/**
 * @brief Destroys every settled-tile run for one board row.
 *
 * Per-row cleanup supports dirty rebuilding without disturbing other settled
 *   rows.
 *
 * @param solo Pointer to the Solo render state.
 * @param row Board row index.
 */
static void	destroy_settled_row(solo_render_t *solo, int row)
{
	int	run;

	run = 0;
	while (run < SOLO_MAX_ROW_RUNS)
	{
		destroy_plane(&solo->settled_runs[row][run]);
		run++;
	}
	solo->settled_run_counts[row] = 0;
	solo->row_signatures[row] = UINT64_MAX;
}

/**
 * @brief Refreshes only settled rows whose signatures changed.
 *
 * Each successful row rebuild updates its signature after all replacement
 *   planes exist.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @return -1 on failure, 0 when unchanged, or 1 when rows changed.
 */
static int	update_settled_rows(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	signature;
	int			changed;
	int			row;

	changed = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		signature = settled_row_signature(game, row);
		if (signature != solo->row_signatures[row])
		{
			if (!rebuild_settled_row(ctx, solo, game, row, signature))
				return (-1);
			changed = 1;
		}
		row++;
	}
	return (changed);
}

/**
 * @brief Rebuilds the contiguous tile runs for one board row.
 *
 * Grouping adjacent occupied cells reduces sprixel count while preserving
 *   crisp atlas pixels.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @param row Board row index.
 * @param signature Pointer to the cached plane signature.
 * @return true on success, otherwise false.
 */
static bool	rebuild_settled_row(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game, int row, uint64_t signature)
{
	uint32_t	pixels[BOARD_WIDTH * TILE_SOURCE_SIZE * TILE_SOURCE_SIZE];
	int			tile_indices[BOARD_WIDTH];
	int			col;
	int			start;
	int			length;
	int			run;

	destroy_settled_row(solo, row);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		tile_indices[col] = board_tile_index(game, col, row);
		col++;
	}
	col = 0;
	run = 0;
	while (col < BOARD_WIDTH)
	{
		while (col < BOARD_WIDTH && tile_indices[col] < 0)
			col++;
		if (col >= BOARD_WIDTH)
			break ;
		start = col;
		while (col < BOARD_WIDTH && tile_indices[col] >= 0)
			col++;
		length = col - start;
		if (run >= SOLO_MAX_ROW_RUNS)
			return (false);
		col = 0;
		while (col < length)
		{
			copy_tile_pixels(solo, tile_indices[start + col],
				&pixels[col * TILE_SOURCE_SIZE],
				length * TILE_SOURCE_SIZE, false);
			col++;
		}
		solo->settled_runs[row][run] = create_pixel_plane_at(ctx, solo,
			pixels, length * TILE_SOURCE_SIZE, TILE_SOURCE_SIZE,
			length * TILE_SOURCE_SIZE,
			HUD_BOARD_X + start * HUD_TILE_SIZE,
			HUD_BOARD_Y + row * HUD_TILE_SIZE);
		if (solo->settled_runs[row][run] == NULL)
		{
			destroy_settled_row(solo, row);
			return (false);
		}
		run++;
		col = start + length;
	}
	solo->settled_run_counts[row] = run;
	solo->row_signatures[row] = signature;
	return (true);
}

/**
 * @brief Copies one authored tile into a temporary RGBA surface.
 *
 * Ghost copies preblend the authored tile against the playfield to avoid
 *   terminal alpha quantization.
 *
 * @param solo Pointer to the Solo render state.
 * @param tile_index Tile-atlas entry to copy.
 * @param destination Destination RGBA buffer.
 * @param destination_stride Number of destination pixels per row.
 * @param ghost Whether to render the translucent landing projection.
 */
static void	copy_tile_pixels(const solo_render_t *solo, int tile_index,
	uint32_t *destination, int destination_stride, bool ghost)
{
	uint32_t	pixel;
	int			y;
	int			x;

	y = 0;
	while (y < TILE_SOURCE_SIZE)
	{
		x = 0;
		while (x < TILE_SOURCE_SIZE)
		{
			pixel = solo->tile_pixels[(size_t)(tile_index * TILE_SOURCE_STRIDE
					+ y) * solo->tile_width + x];
			if (ghost)
				pixel = solo_canvas_ghost_tile_pixel(pixel, x, y);
			destination[(size_t)y * destination_stride + x] = pixel;
			x++;
		}
		y++;
	}
}

/**
 * @brief Creates a high-resolution pixel plane at board-relative geometry.
 *
 * The helper validates terminal and source dimensions before handing pixels to
 *   Notcurses.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param pixels Pointer to the source RGBA pixels.
 * @param width Pixel or terminal width.
 * @param height Pixel or terminal height.
 * @param row_stride Number of source pixels per row.
 * @param source_x Left edge of the source canvas region.
 * @param source_y Top edge of the source canvas region.
 * @return Owned pixel plane, or NULL on validation or render failure.
 */
static struct ncplane	*create_pixel_plane_at(render_ctx_t *ctx,
	solo_render_t *solo, const uint32_t *pixels, int width, int height,
	int row_stride, int source_x, int source_y)
{
	struct ncplane	*plane;
	int				y;
	int				x;
	int				rows;
	int				cols;

	if (width <= 0 || height <= 0 || source_x < 0 || source_y < 0
		|| width % HUD_TILE_SIZE != 0 || height % HUD_TILE_SIZE != 0
		|| source_x % HUD_TILE_SIZE != 0 || source_y % HUD_TILE_SIZE != 0)
		return (NULL);
	y = solo->canvas_row + source_y / HUD_TILE_SIZE * solo->tile_rows;
	x = solo->canvas_col + source_x / HUD_TILE_SIZE * solo->tile_cols;
	rows = height / HUD_TILE_SIZE * solo->tile_rows;
	cols = width / HUD_TILE_SIZE * solo->tile_cols;
	plane = create_plane(ctx, y, x, rows, cols);
	if (plane == NULL)
		return (NULL);
	if (!blit_surface(ctx, plane, pixels, width, height, row_stride,
			NCBLIT_PIXEL))
	{
		destroy_plane(&plane);
		return (NULL);
	}
	ncplane_move_top(plane);
	return (plane);
}

/**
 * @brief Synchronizes active and ghost planes with game state.
 *
 * Overlapping shapes merge atomically, while separated shapes retain
 *   independent crisp planes.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	update_piece_planes(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	t_piece			ghost;
	piece_geometry_t	active_geometry;
	piece_geometry_t	ghost_geometry;
	bool				overlaps;
	int				tile_index;
	int				result;
	int				changed;

	if (game->phase != SOLO_ACTIVE)
	{
		changed = solo->active_plane != NULL || solo->ghost_plane != NULL;
		destroy_plane(&solo->active_plane);
		destroy_plane(&solo->ghost_plane);
		solo->active_shape_signature = UINT64_MAX;
		solo->ghost_shape_signature = UINT64_MAX;
		solo->piece_planes_combined = false;
		return (changed);
	}
	ghost = solo_game_ghost(game);
	if (!piece_geometry(&game->active, &active_geometry)
		|| !piece_geometry(&ghost, &ghost_geometry))
		return (-1);
	tile_index = solo_canvas_piece_tile(game->active.type);
	overlaps = piece_rectangles_overlap(&active_geometry, &ghost_geometry);
	changed = 0;
	if (overlaps)
	{
		if (!solo->piece_planes_combined)
		{
			destroy_plane(&solo->active_plane);
			destroy_plane(&solo->ghost_plane);
			solo->active_shape_signature = UINT64_MAX;
			solo->ghost_shape_signature = UINT64_MAX;
			solo->piece_planes_combined = true;
		}
		result = position_piece_pair(ctx, solo, &solo->active_plane,
			&solo->active_shape_signature, &active_geometry, &ghost_geometry,
			tile_index);
		if (result < 0)
			return (-1);
		changed |= result;
	}
	else
	{
		if (solo->piece_planes_combined)
		{
			destroy_plane(&solo->active_plane);
			solo->active_shape_signature = UINT64_MAX;
			solo->piece_planes_combined = false;
		}
		result = position_atomic_piece(ctx, solo, &solo->active_plane,
			&solo->active_shape_signature, &active_geometry, tile_index, false);
		if (result < 0)
			return (-1);
		changed |= result;
		result = position_atomic_piece(ctx, solo, &solo->ghost_plane,
			&solo->ghost_shape_signature, &ghost_geometry, tile_index, true);
		if (result < 0)
			return (-1);
		changed |= result;
	}
	if (solo->ghost_plane != NULL)
		ncplane_move_top(solo->ghost_plane);
	if (solo->active_plane != NULL)
		ncplane_move_top(solo->active_plane);
	return (changed);
}
/**
 * @brief Extracts occupied cells and bounds from one tetromino.
 *
 * Invalid network- or state-derived pieces fail before geometry indexes are
 *   used.
 *
 * @param piece Pointer to the tetromino.
 * @param geometry Output occupied-cell geometry and bounding box.
 * @return true for a valid piece, otherwise false.
 */
static bool	piece_geometry(const t_piece *piece, piece_geometry_t *geometry)
{
	int	index;

	if (!piece_cells(piece, geometry->cols, geometry->rows))
		return (false);
	geometry->min_col = geometry->cols[0];
	geometry->max_col = geometry->cols[0];
	geometry->min_row = geometry->rows[0];
	geometry->max_row = geometry->rows[0];
	index = 1;
	while (index < 4)
	{
		if (geometry->cols[index] < geometry->min_col)
			geometry->min_col = geometry->cols[index];
		if (geometry->cols[index] > geometry->max_col)
			geometry->max_col = geometry->cols[index];
		if (geometry->rows[index] < geometry->min_row)
			geometry->min_row = geometry->rows[index];
		if (geometry->rows[index] > geometry->max_row)
			geometry->max_row = geometry->rows[index];
		index++;
	}
	return (true);
}

/**
 * @brief Checks whether two piece bounding rectangles intersect.
 *
 * Overlapping active and ghost pieces require one atomic plane to prevent
 *   terminal image shearing.
 *
 * @param first Pointer to the first piece geometry.
 * @param second Pointer to the second piece geometry.
 * @return true when the rectangles overlap, otherwise false.
 */
static bool	piece_rectangles_overlap(const piece_geometry_t *first,
	const piece_geometry_t *second)
{
	return (first->min_col <= second->max_col
		&& first->max_col >= second->min_col
		&& first->min_row <= second->max_row
		&& first->max_row >= second->min_row);
}

/**
 * @brief Moves or rebuilds an overlapping active/ghost plane.
 *
 * Unchanged geometry reuses the bitmap and only updates its terminal position.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param plane Pointer to an owned plane handle.
 * @param cached_signature Pointer to the last rendered pair signature.
 * @param active Pointer to active-piece geometry.
 * @param ghost Pointer to ghost-piece geometry.
 * @param tile_index Tile-atlas entry to copy.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	position_piece_pair(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, uint64_t *cached_signature,
	const piece_geometry_t *active, const piece_geometry_t *ghost,
	int tile_index)
{
	uint64_t		signature;
	piece_bounds_t	bounds;
	int				y;
	int				x;
	int				old_y;
	int				old_x;

	piece_pair_bounds(active, ghost, &bounds);
	if (bounds.min_col < 0 || bounds.max_col >= BOARD_WIDTH
		|| bounds.min_row < 0 || bounds.max_row >= BOARD_HEIGHT)
		return (-1);
	signature = piece_pair_signature(active, ghost, &bounds, tile_index);
	if (*plane == NULL || signature != *cached_signature)
	{
		destroy_plane(plane);
		*plane = create_piece_pair_plane(ctx, solo, active, ghost, &bounds,
			tile_index);
		if (*plane == NULL)
			return (-1);
		*cached_signature = signature;
		return (1);
	}
	y = solo->canvas_row + (HUD_BOARD_Y / HUD_TILE_SIZE + bounds.min_row)
		* solo->tile_rows;
	x = solo->canvas_col + (HUD_BOARD_X / HUD_TILE_SIZE + bounds.min_col)
		* solo->tile_cols;
	ncplane_yx(*plane, &old_y, &old_x);
	if (old_y == y && old_x == x)
		return (0);
	if (ncplane_move_yx(*plane, y, x) != 0)
		return (-1);
	return (1);
}

/**
 * @brief Calculates the union bounds of active and ghost geometry.
 *
 * The merged plane is sized to contain both shapes on the authored board grid.
 *
 * @param active Pointer to active-piece geometry.
 * @param ghost Pointer to ghost-piece geometry.
 * @param bounds Output union bounds for both pieces.
 */
static void	piece_pair_bounds(const piece_geometry_t *active,
	const piece_geometry_t *ghost, piece_bounds_t *bounds)
{
	bounds->min_col = active->min_col;
	if (ghost->min_col < bounds->min_col)
		bounds->min_col = ghost->min_col;
	bounds->max_col = active->max_col;
	if (ghost->max_col > bounds->max_col)
		bounds->max_col = ghost->max_col;
	bounds->min_row = active->min_row;
	if (ghost->min_row < bounds->min_row)
		bounds->min_row = ghost->min_row;
	bounds->max_row = active->max_row;
	if (ghost->max_row > bounds->max_row)
		bounds->max_row = ghost->max_row;
}

/**
 * @brief Hashes active and ghost geometry as one atomic image.
 *
 * The signature changes whenever either shape, position, or tile selection
 *   changes.
 *
 * @param active Pointer to active-piece geometry.
 * @param ghost Pointer to ghost-piece geometry.
 * @param bounds Pointer to the precomputed union bounds.
 * @param tile_index Tile-atlas entry to copy.
 * @return Signature for the combined active and ghost image.
 */
static uint64_t	piece_pair_signature(const piece_geometry_t *active,
	const piece_geometry_t *ghost, const piece_bounds_t *bounds,
	int tile_index)
{
	uint64_t	hash;
	int			index;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, UINT64_C(0x70616972));
	hash = hash_value(hash, (uint64_t)tile_index);
	index = 0;
	while (index < 4)
	{
		hash = hash_value(hash,
			(uint64_t)(active->cols[index] - bounds->min_col));
		hash = hash_value(hash,
			(uint64_t)(active->rows[index] - bounds->min_row));
		index++;
	}
	index = 0;
	while (index < 4)
	{
		hash = hash_value(hash,
			(uint64_t)(ghost->cols[index] - bounds->min_col));
		hash = hash_value(hash,
			(uint64_t)(ghost->rows[index] - bounds->min_row));
		index++;
	}
	return (hash);
}

/**
 * @brief Creates one plane containing overlapping active and ghost pieces.
 *
 * Compositing both pieces into one bitmap keeps all four tiles visually
 *   coherent during movement.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param active Pointer to active-piece geometry.
 * @param ghost Pointer to ghost-piece geometry.
 * @param bounds Pointer to the precomputed union bounds.
 * @param tile_index Tile-atlas entry to copy.
 * @return Owned combined plane, or NULL on allocation or render failure.
 */
static struct ncplane	*create_piece_pair_plane(render_ctx_t *ctx,
	solo_render_t *solo, const piece_geometry_t *active,
	const piece_geometry_t *ghost, const piece_bounds_t *bounds,
	int tile_index)
{
	struct ncplane	*plane;
	uint32_t		*pixels;
	int				width;
	int				height;
	int				index;
	size_t			pixel_bytes;

	width = (bounds->max_col - bounds->min_col + 1) * TILE_SOURCE_SIZE;
	height = (bounds->max_row - bounds->min_row + 1) * TILE_SOURCE_SIZE;
	if (!solo_canvas_buffer_bytes(width, height, &pixel_bytes))
		return (NULL);
	pixels = calloc(1, pixel_bytes);
	if (pixels == NULL)
		return (NULL);
	index = 0;
	while (index < 4)
	{
		copy_tile_pixels(solo, tile_index,
			&pixels[(size_t)(ghost->rows[index] - bounds->min_row)
				* TILE_SOURCE_SIZE * width
				+ (ghost->cols[index] - bounds->min_col) * TILE_SOURCE_SIZE],
			width, true);
		index++;
	}
	index = 0;
	while (index < 4)
	{
		copy_tile_pixels(solo, tile_index,
			&pixels[(size_t)(active->rows[index] - bounds->min_row)
				* TILE_SOURCE_SIZE * width
				+ (active->cols[index] - bounds->min_col) * TILE_SOURCE_SIZE],
			width, false);
		index++;
	}
	plane = create_pixel_plane_at(ctx, solo, pixels, width, height, width,
		HUD_BOARD_X + bounds->min_col * HUD_TILE_SIZE,
		HUD_BOARD_Y + bounds->min_row * HUD_TILE_SIZE);
	free(pixels);
	return (plane);
}

/**
 * @brief Moves or rebuilds a single-piece plane.
 *
 * Shape changes replace pixels; pure translation repositions the existing
 *   plane.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param plane Pointer to an owned plane handle.
 * @param cached_signature Pointer to the last rendered piece signature.
 * @param geometry Pointer to occupied-cell geometry.
 * @param tile_index Tile-atlas entry to copy.
 * @param ghost Whether to render the translucent landing projection.
 * @return -1 on failure, 0 when unchanged, or 1 when updated.
 */
static int	position_atomic_piece(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, uint64_t *cached_signature,
	const piece_geometry_t *geometry, int tile_index, bool ghost)
{
	uint64_t	signature;
	int			y;
	int			x;
	int			old_y;
	int			old_x;
	bool		rebuilt;

	if (geometry->min_col < 0 || geometry->max_col >= BOARD_WIDTH
		|| geometry->min_row < 0 || geometry->max_row >= BOARD_HEIGHT)
	{
		if (*plane == NULL)
			return (0);
		destroy_plane(plane);
		*cached_signature = UINT64_MAX;
		return (1);
	}
	rebuilt = false;
	signature = piece_shape_signature(geometry, tile_index, ghost);
	if (*plane == NULL || signature != *cached_signature)
	{
		destroy_plane(plane);
		*plane = create_atomic_piece_plane(ctx, solo, geometry,
			tile_index, ghost);
		if (*plane == NULL)
			return (-1);
		*cached_signature = signature;
		rebuilt = true;
	}
	y = solo->canvas_row + (HUD_BOARD_Y / HUD_TILE_SIZE
		+ geometry->min_row) * solo->tile_rows;
	x = solo->canvas_col + (HUD_BOARD_X / HUD_TILE_SIZE
		+ geometry->min_col) * solo->tile_cols;
	ncplane_yx(*plane, &old_y, &old_x);
	if (old_y == y && old_x == x)
		return (rebuilt);
	if (ncplane_move_yx(*plane, y, x) != 0)
		return (-1);
	return (1);
}

/**
 * @brief Hashes a piece's tile and normalized occupied cells.
 *
 * Position and shape changes can move or rebuild the minimum necessary plane.
 *
 * @param geometry Pointer to occupied-cell geometry.
 * @param tile_index Tile-atlas entry to copy.
 * @param ghost Whether the signature represents a ghost piece.
 * @return Signature for the piece shape and tile.
 */
static uint64_t	piece_shape_signature(const piece_geometry_t *geometry,
	int tile_index, bool ghost)
{
	uint64_t	hash;
	int			index;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, (uint64_t)tile_index);
	hash = hash_value(hash, ghost);
	index = 0;
	while (index < 4)
	{
		hash = hash_value(hash,
			(uint64_t)(geometry->cols[index] - geometry->min_col));
		hash = hash_value(hash,
			(uint64_t)(geometry->rows[index] - geometry->min_row));
		index++;
	}
	return (hash);
}

/**
 * @brief Creates a plane containing one complete tetromino.
 *
 * All four tiles share one image so horizontal movement and rotation cannot
 *   split the formation.
 *
 * @param ctx Pointer to the active render context.
 * @param solo Pointer to the Solo render state.
 * @param geometry Pointer to occupied-cell geometry.
 * @param tile_index Tile-atlas entry to copy.
 * @param ghost Whether to render the translucent landing projection.
 * @return Owned piece plane, or NULL on allocation or render failure.
 */
static struct ncplane	*create_atomic_piece_plane(render_ctx_t *ctx,
	solo_render_t *solo, const piece_geometry_t *geometry, int tile_index,
	bool ghost)
{
	uint32_t	pixels[4 * 4 * TILE_SOURCE_SIZE * TILE_SOURCE_SIZE];
	int			width;
	int			height;

	width = (geometry->max_col - geometry->min_col + 1) * TILE_SOURCE_SIZE;
	height = (geometry->max_row - geometry->min_row + 1) * TILE_SOURCE_SIZE;
	compose_atomic_piece_pixels(pixels, solo, geometry, tile_index, ghost);
	return (create_pixel_plane_at(ctx, solo, pixels, width, height, width,
			HUD_BOARD_X + geometry->min_col * HUD_TILE_SIZE,
			HUD_BOARD_Y + geometry->min_row * HUD_TILE_SIZE));
}

/**
 * @brief Composes one normalized tetromino into its exact transparent bounds.
 *
 * @param pixels Destination buffer containing 4x4 authored tiles.
 * @param solo Solo renderer and tile atlas.
 * @param geometry Current occupied-cell geometry.
 * @param tile_index Tile-atlas entry.
 * @param ghost Whether to draw landing-projection shading.
 */
static void	compose_atomic_piece_pixels(uint32_t *pixels,
	const solo_render_t *solo, const piece_geometry_t *geometry,
	int tile_index, bool ghost)
{
	int	index;
	int	width;
	int	height;

	width = (geometry->max_col - geometry->min_col + 1) * TILE_SOURCE_SIZE;
	height = (geometry->max_row - geometry->min_row + 1) * TILE_SOURCE_SIZE;
	memset(pixels, 0, (size_t)width * height * sizeof(*pixels));
	index = 0;
	while (index < 4)
	{
		copy_tile_pixels(solo, tile_index,
			&pixels[(size_t)(geometry->rows[index] - geometry->min_row)
				* TILE_SOURCE_SIZE * width
				+ (geometry->cols[index] - geometry->min_col)
				* TILE_SOURCE_SIZE], width, ghost);
		index++;
	}
}

/**
 * @brief Destroys all terminal planes owned by Solo mode.
 *
 * Pixel buffers remain cached so a resize can rebuild planes without decoding
 *   assets again.
 *
 * @param solo Pointer to the Solo render state.
 */
static void	destroy_solo_planes(solo_render_t *solo)
{
	destroy_plane(&solo->status_plane);
	destroy_board_planes(solo);
	destroy_plane(&solo->controls_plane);
	destroy_plane(&solo->score_event_plane);
	destroy_plane(&solo->score_stats_plane);
	destroy_plane(&solo->score_value_plane);
	destroy_plane(&solo->score_header_plane);
	destroy_plane(&solo->mirurun_plane);
	destroy_plane(&solo->meter_plane);
	destroy_plane(&solo->next_plane);
	destroy_plane(&solo->background_plane);
	solo->planes_ready = false;
	reset_render_signatures(solo);
}

/**
 * @brief Destroys both decomposed and composited board planes.
 *
 * Capability changes and resizes use this common board cleanup path.
 *
 * @param solo Pointer to the Solo render state.
 */
static void	destroy_board_planes(solo_render_t *solo)
{
	destroy_plane(&solo->board_overlay_plane);
	solo->board_plane_cells = false;
	destroy_board_tiles(solo);
}
