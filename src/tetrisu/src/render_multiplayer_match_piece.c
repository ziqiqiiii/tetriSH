#include "tetrisu.h"

typedef struct s_match_piece_surface
{
	t_piece_geometry	active;
	t_piece_geometry	ghost;
	t_piece_bounds		bounds;
	bool				combined;
	int					tile;
	int					tile_size;
} t_match_piece_surface;

static bool	piece_geometry(const t_piece *piece, t_piece_geometry *geometry);
static bool	pieces_overlap(const t_piece_geometry *first,
				const t_piece_geometry *second);
static void	piece_bounds(const t_piece_geometry *first,
				const t_piece_geometry *second, t_piece_bounds *bounds);
static int	update_combined(t_render_ctx *ctx,
				const t_mp_match_pixel_layout *layout,
				const t_match_piece_surface *surface);
static int	update_separate(t_render_ctx *ctx,
				const t_mp_match_pixel_layout *layout,
				const t_match_piece_surface *surface);
static int	update_piece_plane(t_render_ctx *ctx, struct ncplane **plane,
				uint64_t *cached_signature, const t_mp_rect *board,
				const t_match_piece_surface *surface, bool ghost_only);
static bool	compose_piece_surface(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, int offset_x, int offset_y,
				const t_match_piece_surface *surface, bool ghost_only);
static void	copy_piece(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int offset_x, int offset_y,
				const t_piece_geometry *geometry, const t_piece_bounds *bounds,
				int tile, int tile_size, bool ghost);
static void	copy_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int x, int y, int size, int tile, bool ghost);
static uint64_t	piece_signature(const t_match_piece_surface *surface,
				bool ghost_only, int offset_x, int offset_y, int rows, int cols);
static uint64_t	hash_value(uint64_t hash, uint64_t value);
static void	destroy_plane(struct ncplane **plane);
static int	min_int(int left, int right);
static int	max_int(int left, int right);

/**
 * @brief Updates only the active tetromino and Solo-style landing ghost.
 *
 * Settled cells live on a different retained plane. Pure translations move
 * these small planes; rotations or sub-cell offsets rewrite only their tiny
 * bitmaps instead of the entire playfield.
 */
int	mp_match_piece_planes_update(t_render_ctx *ctx,
	const t_mp_match_pixel_layout *layout, const t_solo_game *game)
{
	t_match_piece_surface	surface;
	t_piece				ghost;
	int					result;

	if (ctx == NULL || layout == NULL || game == NULL
		|| !render_pixel_planes_reliable(ctx) || game->phase != SOLO_ACTIVE
		|| game->countdown_active)
	{
		result = ctx != NULL && (ctx->mp_match_active_plane != NULL
				|| ctx->mp_match_ghost_plane != NULL);
		mp_match_piece_planes_destroy(ctx);
		return (result);
	}
	ghost = solo_game_ghost(game);
	if (!piece_geometry(&game->active, &surface.active)
		|| !piece_geometry(&ghost, &surface.ghost))
		return (-1);
	surface.tile = solo_canvas_piece_tile(game->active.type);
	surface.tile_size = min_int(layout->local_board.width / BOARD_WIDTH,
		layout->local_board.height / BOARD_HEIGHT);
	surface.combined = pieces_overlap(&surface.active, &surface.ghost);
	if (surface.combined)
		piece_bounds(&surface.active, &surface.ghost, &surface.bounds);
	else
		surface.bounds = (t_piece_bounds){0, 0, 0, 0};
	if (surface.combined)
		return (update_combined(ctx, layout, &surface));
	return (update_separate(ctx, layout, &surface));
}

/** @brief Releases all transient multiplayer piece planes. */
void	mp_match_piece_planes_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	destroy_plane(&ctx->mp_match_active_plane);
	destroy_plane(&ctx->mp_match_ghost_plane);
	ctx->mp_match_active_signature = UINT64_MAX;
	ctx->mp_match_ghost_signature = UINT64_MAX;
	ctx->mp_match_piece_planes_combined = false;
}

static int	update_combined(t_render_ctx *ctx,
	const t_mp_match_pixel_layout *layout,
	const t_match_piece_surface *surface)
{
	int	changed;
	int	result;

	changed = 0;
	if (!ctx->mp_match_piece_planes_combined)
	{
		destroy_plane(&ctx->mp_match_active_plane);
		destroy_plane(&ctx->mp_match_ghost_plane);
		ctx->mp_match_active_signature = UINT64_MAX;
		ctx->mp_match_ghost_signature = UINT64_MAX;
		ctx->mp_match_piece_planes_combined = true;
		changed = 1;
	}
	result = update_piece_plane(ctx, &ctx->mp_match_active_plane,
		&ctx->mp_match_active_signature, &layout->local_board, surface, false);
	if (result < 0)
		return (-1);
	return (changed | result);
}

static int	update_separate(t_render_ctx *ctx,
	const t_mp_match_pixel_layout *layout,
	const t_match_piece_surface *surface)
{
	int	changed;
	int	result;

	changed = 0;
	if (ctx->mp_match_piece_planes_combined)
	{
		destroy_plane(&ctx->mp_match_active_plane);
		ctx->mp_match_active_signature = UINT64_MAX;
		ctx->mp_match_piece_planes_combined = false;
		changed = 1;
	}
	result = update_piece_plane(ctx, &ctx->mp_match_ghost_plane,
		&ctx->mp_match_ghost_signature, &layout->local_board, surface, true);
	if (result < 0)
		return (-1);
	changed |= result;
	result = update_piece_plane(ctx, &ctx->mp_match_active_plane,
		&ctx->mp_match_active_signature, &layout->local_board, surface, false);
	if (result < 0)
		return (-1);
	return (changed | result);
}

static int	update_piece_plane(t_render_ctx *ctx, struct ncplane **plane,
	uint64_t *cached_signature, const t_mp_rect *board,
	const t_match_piece_surface *surface, bool ghost_only)
{
	const t_piece_geometry	*geometry;
	t_piece_bounds			bounds;
	ncplane_options			options;
	uint32_t				*pixels;
	uint64_t				signature;
	size_t					bytes;
	int						tile_size;
	int						pixel_x;
	int						pixel_y;
	int						offset_x;
	int						offset_y;
	int						width;
	int						height;
	bool					created;

	geometry = ghost_only ? &surface->ghost : &surface->active;
	if (surface->combined)
		bounds = surface->bounds;
	else
		bounds = (t_piece_bounds){geometry->min_col, geometry->max_col,
			geometry->min_row, geometry->max_row};
	if (bounds.min_col < 0 || bounds.max_col >= BOARD_WIDTH
		|| bounds.min_row < 0 || bounds.max_row >= BOARD_HEIGHT)
		return (destroy_plane(plane), *cached_signature = UINT64_MAX, 1);
	tile_size = min_int(board->width / BOARD_WIDTH,
		board->height / BOARD_HEIGHT);
	pixel_x = board->x + bounds.min_col * tile_size;
	pixel_y = board->y + bounds.min_row * tile_size;
	offset_x = pixel_x % ctx->cell_px_x;
	offset_y = pixel_y % ctx->cell_px_y;
	memset(&options, 0, sizeof(options));
	options.x = ctx->bg_col + pixel_x / ctx->cell_px_x;
	options.y = ctx->bg_row + pixel_y / ctx->cell_px_y;
	options.cols = (unsigned)(offset_x
		+ (bounds.max_col - bounds.min_col + 1) * tile_size
		+ ctx->cell_px_x - 1) / (unsigned)ctx->cell_px_x;
	options.rows = (unsigned)(offset_y
		+ (bounds.max_row - bounds.min_row + 1) * tile_size
		+ ctx->cell_px_y - 1) / (unsigned)ctx->cell_px_y;
	width = (int)options.cols * ctx->cell_px_x;
	height = (int)options.rows * ctx->cell_px_y;
	signature = piece_signature(surface, ghost_only, offset_x, offset_y,
		(int)options.rows, (int)options.cols);
	created = false;
	if (!render_plane_geometry_matches(*plane, options.y, options.x,
		options.rows, options.cols))
	{
		if (*plane != NULL && signature == *cached_signature)
		{
			unsigned old_rows;
			unsigned old_cols;

			ncplane_dim_yx(*plane, &old_rows, &old_cols);
			if (old_rows == options.rows && old_cols == options.cols
				&& ncplane_move_yx(*plane, options.y, options.x) == 0)
				return (1);
		}
		destroy_plane(plane);
		*plane = ncplane_create(ctx->std, &options);
		if (*plane == NULL)
			return (-1);
		created = true;
	}
	if (!created && signature == *cached_signature)
		return (0);
	if (!solo_canvas_buffer_bytes(width, height, &bytes))
		return (-1);
	pixels = calloc(1, bytes);
	if (pixels == NULL)
		return (-1);
	if (!compose_piece_surface(ctx, pixels, width, height, offset_x,
			offset_y, surface, ghost_only)
		|| !render_plane_blit_rgba(ctx, *plane, pixels, width, height, width))
	{
		free(pixels);
		if (created)
			destroy_plane(plane);
		return (-1);
	}
	free(pixels);
	*cached_signature = signature;
	return (1);
}

static bool	compose_piece_surface(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, int offset_x, int offset_y,
	const t_match_piece_surface *surface, bool ghost_only)
{
	t_piece_bounds	bounds;
	int				tile_size;

	if (ctx->mp_match_tile_visual == NULL)
		return (false);
	if (surface->combined)
		bounds = surface->bounds;
	else if (ghost_only)
		bounds = (t_piece_bounds){surface->ghost.min_col,
			surface->ghost.max_col, surface->ghost.min_row,
			surface->ghost.max_row};
	else
		bounds = (t_piece_bounds){surface->active.min_col,
			surface->active.max_col, surface->active.min_row,
			surface->active.max_row};
	tile_size = surface->tile_size;
	if (surface->combined || ghost_only)
		copy_piece(ctx, pixels, width, height, offset_x, offset_y,
			&surface->ghost, &bounds, surface->tile, tile_size, true);
	if (surface->combined || !ghost_only)
		copy_piece(ctx, pixels, width, height, offset_x, offset_y,
			&surface->active, &bounds, surface->tile, tile_size, false);
	return (true);
}

static void	copy_piece(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int offset_x, int offset_y, const t_piece_geometry *geometry,
	const t_piece_bounds *bounds, int tile, int tile_size, bool ghost)
{
	int	index;

	index = 0;
	while (index < 4)
	{
		copy_tile(ctx, pixels, width, height,
			offset_x + (geometry->cols[index] - bounds->min_col) * tile_size,
			offset_y + (geometry->rows[index] - bounds->min_row) * tile_size,
			tile_size, tile, ghost);
		index++;
	}
}

static void	copy_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int x, int y, int size, int tile, bool ghost)
{
	uint32_t	pixel;
	int			draw_y;
	int			draw_x;
	int			source_y;
	int			source_x;

	draw_y = 0;
	while (draw_y < size && y + draw_y < height)
	{
		source_y = draw_y * TILE_SOURCE_SIZE / size;
		draw_x = 0;
		while (draw_x < size && x + draw_x < width)
		{
			source_x = draw_x * TILE_SOURCE_SIZE / size;
			if (ncvisual_at_yx(ctx->mp_match_tile_visual,
					(unsigned)(tile * TILE_SOURCE_STRIDE + source_y),
					(unsigned)source_x, &pixel) >= 0)
			{
				if (ghost)
					pixel = solo_canvas_ghost_tile_pixel(pixel,
						source_x, source_y);
				pixels[(size_t)(y + draw_y) * width + x + draw_x] = pixel;
			}
			draw_x++;
		}
		draw_y++;
	}
}

static bool	piece_geometry(const t_piece *piece, t_piece_geometry *geometry)
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
		geometry->min_col = min_int(geometry->min_col, geometry->cols[index]);
		geometry->max_col = max_int(geometry->max_col, geometry->cols[index]);
		geometry->min_row = min_int(geometry->min_row, geometry->rows[index]);
		geometry->max_row = max_int(geometry->max_row, geometry->rows[index]);
		index++;
	}
	return (true);
}

static bool	pieces_overlap(const t_piece_geometry *first,
	const t_piece_geometry *second)
{
	return (first->min_col <= second->max_col
		&& first->max_col >= second->min_col
		&& first->min_row <= second->max_row
		&& first->max_row >= second->min_row);
}

static void	piece_bounds(const t_piece_geometry *first,
	const t_piece_geometry *second, t_piece_bounds *bounds)
{
	bounds->min_col = min_int(first->min_col, second->min_col);
	bounds->max_col = max_int(first->max_col, second->max_col);
	bounds->min_row = min_int(first->min_row, second->min_row);
	bounds->max_row = max_int(first->max_row, second->max_row);
}

static uint64_t	piece_signature(const t_match_piece_surface *surface,
	bool ghost_only, int offset_x, int offset_y, int rows, int cols)
{
	const t_piece_geometry	*geometry;
	const t_piece_bounds		*bounds;
	uint64_t					hash;
	int						index;

	geometry = ghost_only ? &surface->ghost : &surface->active;
	bounds = &surface->bounds;
	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, (uint64_t)surface->tile);
	hash = hash_value(hash, (uint64_t)surface->tile_size);
	hash = hash_value(hash, (uint64_t)surface->combined);
	hash = hash_value(hash, (uint64_t)ghost_only);
	hash = hash_value(hash, (uint64_t)offset_x);
	hash = hash_value(hash, (uint64_t)offset_y);
	hash = hash_value(hash, (uint64_t)rows);
	hash = hash_value(hash, (uint64_t)cols);
	index = 0;
	while (index < 4)
	{
		hash = hash_value(hash, (uint64_t)(geometry->cols[index]
			- (surface->combined ? bounds->min_col : geometry->min_col)));
		hash = hash_value(hash, (uint64_t)(geometry->rows[index]
			- (surface->combined ? bounds->min_row : geometry->min_row)));
		index++;
	}
	if (surface->combined)
	{
		index = 0;
		while (index < 4)
		{
			hash = hash_value(hash, (uint64_t)(surface->ghost.cols[index]
				- bounds->min_col));
			hash = hash_value(hash, (uint64_t)(surface->ghost.rows[index]
				- bounds->min_row));
			index++;
		}
	}
	return (hash);
}

static uint64_t	hash_value(uint64_t hash, uint64_t value)
{
	hash ^= value;
	return (hash * UINT64_C(1099511628211));
}

static void	destroy_plane(struct ncplane **plane)
{
	if (plane != NULL && *plane != NULL)
		ncplane_destroy(*plane);
	if (plane != NULL)
		*plane = NULL;
}

static int	min_int(int left, int right)
{
	return (left < right ? left : right);
}

static int	max_int(int left, int right)
{
	return (left > right ? left : right);
}
