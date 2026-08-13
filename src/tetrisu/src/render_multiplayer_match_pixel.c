#include "tetrisu.h"
#include "render_multiplayer_match_pixel_draw.h"

# define MATCH_FONT_COLUMNS 16
# define MATCH_FONT_ROWS 6
# define MATCH_FONT_WIDTH 8
# define MATCH_FONT_HEIGHT 16
# define MATCH_FONT_INK_Y 4

static const t_color g_cream = {250, 242, 221};
static const t_color g_gold = {255, 203, 102};
static const t_color g_pink = {255, 112, 190};
static const t_color g_lavender = {190, 155, 218};
static const t_color g_green = {112, 214, 174};
static const t_color g_red = {255, 92, 118};
static const t_color g_dark = {7, 13, 23};
static const t_color g_panel = {20, 9, 34};
static const t_color g_panel_light = {39, 20, 58};

/*
 * One pass over the surfaces that can change without the layout changing.
 * The canvas is composed lazily: a frame in which nothing moved never
 * allocates one. force says the canvas already holds the finished artwork, so
 * a region only has to be cut out of it, not drawn again.
 */
typedef struct s_match_regions
{
	t_render_ctx					*ctx;
	uint32_t						*pixels;
	int								width;
	int								height;
	const t_mp_match_pixel_layout	*layout;
	const t_mp_match_state			*state;
	bool							force;
	bool							changed;
}	t_match_regions;

static bool load_assets(t_render_ctx *ctx);
static bool load_tile_atlas(t_render_ctx *ctx);
static bool refresh_background(t_render_ctx *ctx, bool rebuild);
static bool present_canvas(t_render_ctx *ctx, const uint32_t *pixels,
				int width, int height, bool cells);
static bool create_region_plane(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *region,
				struct ncplane **slot);
static void destroy_region_planes(t_render_ctx *ctx);
static bool regions_prepare(t_match_regions *pass);
static int regions_local(t_match_regions *pass);
static int regions_opponent(t_match_regions *pass);
static int regions_battle(t_match_regions *pass);
static bool regions_side(t_match_regions *pass, const t_mp_rect *rect,
				int first, int count, struct ncplane **slot);
static int regions_loadout(t_match_regions *pass);
static int regions_opponent_loadout(t_match_regions *pass);
static int regions_hud(t_match_regions *pass);
static void popover_place(const t_mp_match_pixel_layout *layout, int ability,
				t_mp_rect *out);
static void popover_draw(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *card,
				const t_app_catalogue_item_view_model *character, int ability);
static int popover_wrap(const char *text, int width, int line, char *out,
				size_t cap);
static void snap_layout_to_cells(const t_render_ctx *ctx,
				t_mp_match_pixel_layout *layout);
static void snap_rect_to_cells(t_mp_rect *rect, int cell_x, int cell_y);
static void shift_ability_centres(t_mp_match_pixel_layout *layout,
				const t_mp_rect *before, const t_render_ctx *ctx);
static int refresh_regions(t_match_regions *pass);
static void compose_match(t_match_regions *pass);
static void store_signatures(t_match_regions *pass, uint64_t signature);
static bool match_incremental(t_match_regions *pass);
static bool match_rebuild(t_match_regions *pass, uint64_t signature);
static uint64_t match_signature(const t_mp_match_state *state,
				int width, int height);
static uint64_t static_signature(const t_mp_match_state *state,
				int width, int height);
static uint64_t game_signature(const t_solo_game *game);
static int clear_flash_step(const t_solo_game *game);
static uint64_t local_board_signature(const t_solo_game *game,
				bool include_score);
static uint64_t loadout_signature(const t_mp_match_state *state);
static uint64_t opponent_loadout_signature(const t_mp_match_state *state);
static uint64_t hud_signature(const t_mp_match_state *state);
static uint64_t opponents_signature(const t_mp_match_state *state,
				int first, int count);
static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size);
static uint32_t *new_canvas(t_render_ctx *ctx, int width, int height);
static uint32_t *canvas_keep(t_render_ctx *ctx, int width, int height);
static void clear_rect(uint32_t *pixels, int width, int height,
				const t_mp_rect *rect);
static void board_cell_grid(const t_solo_game *game,
				int grid[BOARD_HEIGHT][BOARD_WIDTH], bool with_piece);
static void repaint_cell(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *board, int col, int row, int slot);
static int repaint_changed_cells(t_match_regions *pass, int slot,
				const t_mp_rect *board, const t_solo_game *game,
				bool with_piece);
static bool board_region_sync(t_match_regions *pass, int slot,
				const t_mp_rect *board, const t_solo_game *game);
static bool blit_board_bands(t_match_regions *pass, const t_mp_rect *region,
				int slot, struct ncplane **planes, uint64_t *signatures);
static bool regions_opponent_caption(t_match_regions *pass,
				const t_mp_rect *board);
static uint64_t band_signature(const t_render_ctx *ctx, int slot, int first,
				int last);
static void band_bounds(const t_render_ctx *ctx, const t_mp_rect *region,
				int band, t_mp_rect *out);
static bool caption_stale(t_match_regions *pass);
static int opponent_pending(const t_mp_match_state *state);
static bool regions_caption(t_match_regions *pass, const t_mp_rect *board);
static void stamp_piece_cells(int grid[BOARD_HEIGHT][BOARD_WIDTH],
				const t_piece *piece, int layer);
static void blackout_grid(const t_solo_game *game,
				int grid[BOARD_HEIGHT][BOARD_WIDTH]);
static void blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha);
static void put_pixel(uint32_t *pixels, int width, int height, int x, int y,
				 t_color tint, unsigned alpha);
static void compose_selection(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_match_state *state);
static void compose_double(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_match_state *state);
static void compose_battle(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_match_state *state);
static void draw_match_hud(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_match_pixel_layout *layout,
				const t_mp_match_state *state);
static void effect_line(const t_solo_effects *effects, char *out,
				size_t size);
static void draw_double_caption(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *board,
				const char *name, uint64_t points, int charge, int pending,
				bool local);
static void draw_loadout(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect,
				const t_mp_match_state *state, bool portrait);
static void draw_ability_bar(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_match_pixel_layout *layout,
				const t_mp_match_state *state);
static void draw_opponent_column(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height,
				const t_mp_match_pixel_layout *layout,
				const t_mp_match_state *state);
static void draw_hold_next(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect, const t_solo_game *game);
static void draw_piece_preview(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *slot, t_piece_type type,
				unsigned opacity);
static void draw_game_board(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect, const t_solo_game *game,
				const char *label, bool local);
static void draw_snapshot_board(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *rect,
				const struct s_mp_opponent_snapshot *opponent, int player,
				bool compact);
static void draw_board_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect, const t_board *board,
				const t_piece *active, const t_piece *ghost);
static void draw_grid_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect,
				const int grid[BOARD_HEIGHT][BOARD_WIDTH], t_color backing);
static void draw_piece_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect, const t_piece *piece,
				bool ghost);
static void draw_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int tile, int x, int y, int size, unsigned opacity,
				bool ghost);
static bool tile_cache_ensure(t_render_ctx *ctx, int size, t_color backing);
static void tile_cache_bake(t_render_ctx *ctx, int tile, int size, bool ghost,
				t_color backing);
static void draw_board_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int tile, int x, int y, int size, bool ghost,
				t_color backing);
static int danger_step(const t_solo_game *game);
static t_color danger_backing(int step);
static t_color danger_frame(int step, bool local);
static void draw_opponent_region(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *rect,
				const t_mp_match_state *state, int first, int count);
static void opponent_grid(int count, const t_mp_rect *rect,
				int *columns, int *rows);
static void draw_targeting(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect,
				const t_mp_match_state *state);
static void draw_result(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_match_state *state);
static int min_int(int left, int right);
static int max_int(int left, int right);
static int clamp_int(int value, int minimum, int maximum);
static void draw_squircle(uint32_t *pixels, int width, int height,
				const t_mp_rect *rect, t_color edge, unsigned alpha);
static void fill_squircle(uint32_t *pixels, int width, int height,
				const t_mp_rect *rect, t_color tint, unsigned alpha);
static void draw_hud_backdrop(uint32_t *pixels, int width, int height,
				const t_mp_rect *rect);

/**
 * @brief Draws the authored-pixel multiplayer match surface.
 *
 * The font mask and tetromino atlas are the same assets used by Solo, and so
 * is the layering rule that makes Solo smooth: the stationary chrome is a
 * quadrant-cell plane, and only the surfaces that animate - the boards, the
 * loadout, the HUD, the falling piece - are bitmaps above it. Terminal bitmaps
 * do not compose. Anything drawn over one invalidates it, so a full-screen
 * bitmap underneath the moving parts makes every keypress cost a full-screen
 * transfer; measured here, that was the whole of the input latency. Cells cost
 * nothing to cover, so the chrome now pays once and the moving regions pay for
 * their own area only.
 */
bool render_multiplayer_match_pixel_show(t_render_ctx *ctx,
	const t_mp_match_state *state, bool rebuild_background)
{
	t_match_regions pass;
	uint64_t signature;
	t_mp_match_pixel_layout layout;
	int width;
	int height;

	if (ctx == NULL || state == NULL || render_compatibility_mode(ctx)
		|| !render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc))
		return (false);
	if (!refresh_background(ctx, rebuild_background) || !load_assets(ctx))
		return (false);
	width = ctx->bg_cols * ctx->cell_px_x;
	height = ctx->bg_rows * ctx->cell_px_y;
	if (width <= 0 || height <= 0)
		return (false);
	mp_match_pixel_layout_build(state->mode, width, height, &layout);
	if (!layout.valid)
		return (false);
	snap_layout_to_cells(ctx, &layout);
	signature = match_signature(state, width, height);
	if (state->phase != MP_MATCH_PLAYING && !rebuild_background
		&& ctx->screen_plane != NULL && ctx->mp_match_signature == signature)
		return (true);
	memset(&pass, 0, sizeof(pass));
	pass.ctx = ctx;
	pass.width = width;
	pass.height = height;
	pass.layout = &layout;
	pass.state = state;
	if (state->phase == MP_MATCH_PLAYING && !rebuild_background
		&& ctx->screen_plane != NULL
		&& ctx->mp_match_static_signature == static_signature(state,
			width, height))
		return (match_incremental(&pass));
	return (match_rebuild(&pass, signature));
}

/**
 * @brief Redraws only the regions whose contents changed since the last frame.
 *
 * Nothing under these planes is a bitmap any more, so a region costs its own
 * area and no more. A frame in which nothing moved returns without rendering.
 */
static bool match_incremental(t_match_regions *pass)
{
	if (refresh_regions(pass) < 0)
		return (false);
	pass->pixels = NULL;
	if (!pass->changed)
		return (true);
	render_notification_raise(pass->ctx);
	return (notcurses_render(pass->ctx->nc) == 0);
}

/**
 * @brief Rebuilds every plane from a freshly composed canvas.
 *
 * During a match the composed canvas goes down as cells and the moving regions
 * are cut from the same canvas as bitmaps on top, so the two layers agree
 * pixel for pixel and only the sharper one is visible. The other phases are
 * still one bitmap: they hold still, so nothing above them ever invalidates
 * them, and they keep the authored resolution across the whole screen.
 */
static bool match_rebuild(t_match_regions *pass, uint64_t signature)
{
	t_render_ctx *ctx;
	bool playing;

	ctx = pass->ctx;
	playing = pass->state->phase == MP_MATCH_PLAYING;
	destroy_region_planes(ctx);
	pass->pixels = new_canvas(ctx, pass->width, pass->height);
	if (pass->pixels == NULL)
		return (false);
	compose_match(pass);
	if (!present_canvas(ctx, pass->pixels, pass->width, pass->height, playing))
		return (false);
	pass->force = true;
	if (playing && refresh_regions(pass) < 0)
		return (false);
	pass->pixels = NULL;
	store_signatures(pass, signature);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}


/**
 * @brief Records what each board cell shows, piece and ghost included.
 *
 * One number per cell, so a frame can find the handful that differ from the
 * frame before it instead of composing the board again.
 */
static void board_cell_grid(const t_solo_game *game,
	int grid[BOARD_HEIGHT][BOARD_WIDTH], bool with_piece)
{
	t_piece ghost;
	t_cell cell;
	int row;
	int col;

	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			cell = board_get(&game->board, col, row);
			grid[row][col] = 0;
			if (solo_game_row_is_clearing(game, row))
				grid[row][col] = 1000 + (clear_flash_step(game) == 2
						? TILE_CLEAR_SECOND : TILE_CLEAR_FIRST);
			else if (cell.type == CELL_GARBAGE)
				grid[row][col] = 1000 + TILE_GARBAGE;
			else if (cell.type == CELL_FILLED)
				grid[row][col] = 1000
					+ solo_canvas_piece_tile((t_piece_type)cell.color);
			col++;
		}
		row++;
	}
	if (!with_piece || game->phase != SOLO_ACTIVE || game->countdown_active)
		return ;
	ghost = solo_game_ghost(game);
	stamp_piece_cells(grid, &ghost, 2000);
	stamp_piece_cells(grid, &game->active, 3000);
	blackout_grid(game, grid);
}

/**
 * @brief Halloween L2 (Dark): hides everything but a window under the piece.
 *
 * The only effect in the catalogue a server cannot carry out. Every other one
 * is a rule about what a player may do, which tetrisd enforces by refusing the
 * input; this one is a rule about what they may *see*, and only the thing
 * drawing the board can enforce that.
 *
 * It is applied to the cell grid rather than to the board, so the hidden rows
 * are still there and still collide - the player is blinded, not helped. The
 * window travels with the falling piece and covers the rows just below it,
 * which is what makes Dark survivable: you can place the piece in your hand
 * and nothing else.
 *
 * @param game Game being drawn.
 * @param grid Cell grid to blank outside the window.
 */
static void	blackout_grid(const t_solo_game *game,
	int grid[BOARD_HEIGHT][BOARD_WIDTH])
{
	int	first;
	int	last;
	int	row;
	int	col;

	if (game->effects.dark <= 0)
		return ;
	first = game->active.row;
	last = first + MP_MATCH_DARK_WINDOW_ROWS;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if ((row < first || row > last) && grid[row][col] < 3000)
				grid[row][col] = 0;
			col++;
		}
		row++;
	}
}

static void stamp_piece_cells(int grid[BOARD_HEIGHT][BOARD_WIDTH],
	const t_piece *piece, int layer)
{
	int cols[4];
	int rows[4];
	int index;

	if (!piece_cells(piece, cols, rows))
		return ;
	index = 0;
	while (index < 4)
	{
		if (board_in_bounds(cols[index], rows[index]))
			grid[rows[index]][cols[index]] = layer
				+ solo_canvas_piece_tile(piece->type);
		index++;
	}
}

/**
 * @brief Repaints one cell over the board's own background.
 *
 * The board is filled opaque before its cells are drawn, so repainting a cell
 * is that same fill over one cell followed by whatever now occupies it.
 */
static void repaint_cell(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *board, int col, int row, int slot)
{
	t_mp_rect cell;
	int tile_size;
	int id;

	id = ctx->mp_match_boards[slot].cells[row][col];
	tile_size = min_int(board->width / BOARD_WIDTH,
			board->height / BOARD_HEIGHT);
	cell = (t_mp_rect){board->x + col * tile_size, board->y + row * tile_size,
		tile_size, tile_size};
	/*
	 * A baked tile already carries the board's backing colour, so the fill only
	 * has to run for a cell that has become empty.
	 */
	if (id < 1000)
	{
		mp_match_pixel_fill_rect(pixels, width, height, &cell,
			danger_backing(ctx->mp_match_boards[slot].danger), 255);
		return ;
	}
	draw_board_tile(ctx, pixels, width, height, id % 1000, cell.x, cell.y,
		tile_size, id >= 2000 && id < 3000,
		danger_backing(ctx->mp_match_boards[slot].danger));
}

/**
 * @brief Repaints only the cells that differ from the retained composition.
 *
 * Returns 0 when the cache cannot be trusted - a different board rectangle, a
 * countdown, a finished game - and the caller composes the region in full.
 */
static int repaint_changed_cells(t_match_regions *pass, int slot,
	const t_mp_rect *board, const t_solo_game *game, bool with_piece)
{
	int grid[BOARD_HEIGHT][BOARD_WIDTH];
	t_mp_board_cache *cache;
	int row;
	int col;

	cache = &pass->ctx->mp_match_boards[slot];
	if (!cache->valid || memcmp(&cache->rect, board, sizeof(*board)) != 0
		|| game->countdown_active || cache->danger != danger_step(game))
		return (0);
	board_cell_grid(game, grid, with_piece);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (grid[row][col] != cache->cells[row][col])
			{
				cache->cells[row][col] = grid[row][col];
				repaint_cell(pass->ctx, pass->pixels, pass->width,
					pass->height, board, col, row, slot);
			}
			col++;
		}
		row++;
	}
	return (1);
}

/**
 * @brief Brings one board region up to date, by cell diff where it can.
 *
 * Both boards go through here, and both draw their falling piece. The
 * opponent's used to be left off, which was right while the opponent was a
 * fixture nobody was steering: there was no piece worth showing and the cache
 * survived every tick. Now the server sends it, and a board that only changed
 * when something locked read as a rival sitting motionless and then jumping.
 *
 * It costs nothing to keep: the cell diff below is built from the same grid,
 * so a moving piece repaints the cells it moved through rather than the board.
 */
static bool board_region_sync(t_match_regions *pass, int slot,
	const t_mp_rect *board, const t_solo_game *game)
{
	t_mp_board_cache *cache;
	t_mp_rect region;
	bool with_piece;

	cache = &pass->ctx->mp_match_boards[slot];
	with_piece = true;
	if (pass->force)
		return (true);
	if (repaint_changed_cells(pass, slot, board, game, with_piece))
		return (true);
	region = (t_mp_rect){board->x - 8, board->y - 8, board->width + 16,
		board->height + 16};
	clear_rect(pass->pixels, pass->width, pass->height, &region);
	draw_game_board(pass->ctx, pass->pixels, pass->width, pass->height, board,
		game, slot == 0 ? (pass->state->mode == APP_GAME_MODE_BATTLE_ROYALE
			? "" : "YOUR BOARD") : pass->state->opponent_name, with_piece);
	board_cell_grid(game, cache->cells, with_piece);
	cache->rect = *board;
	cache->danger = danger_step(game);
	cache->valid = !game->countdown_active;
	return (true);
}

/**
 * @brief Gives one band's rectangle, snapped to whole terminal cells.
 *
 * The snap is the point: create_region_plane rounds a region out to the cells
 * that contain it, so two bands whose unsnapped edges fall inside one cell row
 * would both claim that row and blank each other. Cutting on cell boundaries
 * makes the bands exactly tile the region.
 */
static void band_bounds(const t_render_ctx *ctx, const t_mp_rect *region,
	int band, t_mp_rect *out)
{
	int	rows;
	int	first;
	int	last;

	rows = (region->height + ctx->cell_px_y - 1) / ctx->cell_px_y;
	first = rows * band / MP_MATCH_BOARD_BANDS;
	last = rows * (band + 1) / MP_MATCH_BOARD_BANDS;
	out->x = region->x;
	out->width = region->width;
	out->y = region->y + first * ctx->cell_px_y;
	out->height = (last - first) * ctx->cell_px_y;
}

/**
 * @brief Hashes the board rows a band covers, plus the band's own geometry.
 *
 * The cell cache is hashed rather than the game, so this reads whichever board
 * the caller is banding; the rows outside any band's board area contribute
 * nothing, so a band holding only margin settles to a constant and stops being
 * re-blitted.
 */
static uint64_t band_signature(const t_render_ctx *ctx, int slot, int first,
	int last)
{
	uint64_t	hash;
	int			row;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &first, sizeof(first));
	hash = hash_bytes(hash, &last, sizeof(last));
	row = max_int(0, first);
	while (row < min_int(last, BOARD_HEIGHT))
	{
		hash = hash_bytes(hash, ctx->mp_match_boards[slot].cells[row],
				sizeof(ctx->mp_match_boards[slot].cells[row]));
		row++;
	}
	return (hash);
}

/**
 * @brief Re-blits only the bands of one board whose rows changed.
 *
 * Both boards come through here. A forced pass re-blits every band, because
 * the canvas underneath it is new.
 *
 * @param pass The frame in progress.
 * @param region The board's rectangle including its margin.
 * @param slot Which board: 0 local, 1 opponent.
 * @param planes The band planes to reuse, and their stored signatures.
 * @param signatures What each band last held.
 * @return true unless a plane could not be created.
 */
static bool blit_board_bands(t_match_regions *pass, const t_mp_rect *region,
	int slot, struct ncplane **planes, uint64_t *signatures)
{
	t_render_ctx	*ctx;
	t_mp_rect		strip;
	uint64_t		signature;
	int				tile_size;
	int				band;

	ctx = pass->ctx;
	tile_size = max_int(1, min_int((region->width - 16) / BOARD_WIDTH,
				(region->height - 16) / BOARD_HEIGHT));
	band = 0;
	while (band < MP_MATCH_BOARD_BANDS)
	{
		band_bounds(ctx, region, band, &strip);
		signature = band_signature(ctx, slot,
				(strip.y - region->y - 8) / tile_size,
				(strip.y + strip.height - region->y - 8 + tile_size - 1)
				/ tile_size);
		if (strip.height > 0 && (pass->force
				|| signatures[band] != signature))
		{
			if (!create_region_plane(ctx, pass->pixels, pass->width,
					pass->height, &strip, &planes[band]))
				return (false);
			signatures[band] = signature;
		}
		band++;
	}
	return (true);
}

/**
 * @brief Reports whether the name-and-score strip under the board has moved on.
 *
 * The strip is part of the board's region but is not made of cells, so a cell
 * diff cannot carry it. It changes only on a clear, which makes a full redraw
 * of the region the cheap answer rather than a special case.
 */
static bool caption_stale(t_match_regions *pass)
{
	uint64_t signature;

	if (pass->state->mode != APP_GAME_MODE_DOUBLE)
		return (false);
	signature = 1469598103934665603ULL;
	signature = hash_bytes(signature, pass->state->profile.username,
			strlen(pass->state->profile.username));
	signature = hash_bytes(signature, &pass->state->local_game.scoring.total,
			sizeof(pass->state->local_game.scoring.total));
	signature = hash_bytes(signature, &pass->state->local_game.crystal_charge,
			sizeof(pass->state->local_game.crystal_charge));
	signature = hash_bytes(signature, &pass->state->incoming_garbage,
			sizeof(pass->state->incoming_garbage));
	if (signature == pass->ctx->mp_match_caption_signature)
		return (false);
	pass->ctx->mp_match_caption_signature = signature;
	return (true);
}

/**
 * @brief Garbage queued against the opponent, as their caption reports it.
 *
 * Double reads it from the one opponent slot the snapshot fills. It is a
 * different number from state->incoming_garbage and drawing one where the
 * other belongs would warn the wrong player, which is the mistake this exists
 * to make hard.
 *
 * @param state Match model to read.
 * @return Rows owed to the opponent, or 0 when there is no opponent.
 */
static int opponent_pending(const t_mp_match_state *state)
{
	if (state == NULL || !state->opponents[0].present)
		return (0);
	return (state->opponents[0].garbage_pending);
}

/**
 * @brief Keeps the name-and-score strip on a plane of its own.
 *
 * The strip changes on a clear and the board changes on every input, so
 * sharing one plane made each lock pay for a whole board. Two planes let each
 * pay for itself.
 */
static bool regions_caption(t_match_regions *pass, const t_mp_rect *board)
{
	t_mp_rect strip;

	strip = (t_mp_rect){board->x - 8, board->y + board->height + 8,
		board->width + 16, 66};
	if (pass->force || caption_stale(pass))
	{
		if (!pass->force)
		{
			clear_rect(pass->pixels, pass->width, pass->height, &strip);
			draw_double_caption(pass->ctx, pass->pixels, pass->width,
				pass->height, board, pass->state->profile.username,
				pass->state->local_game.scoring.total,
				pass->state->local_game.crystal_charge,
				pass->state->incoming_garbage, true);
		}
		return (create_region_plane(pass->ctx, pass->pixels, pass->width,
				pass->height, &strip,
				&pass->ctx->mp_match_caption_plane));
	}
	return (true);
}


static void compose_match(t_match_regions *pass)
{
	if (pass->state->phase == MP_MATCH_CHARACTER_SELECT)
		compose_selection(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->state);
	else if (pass->state->mode == APP_GAME_MODE_DOUBLE)
		compose_double(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->state);
	else
		compose_battle(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->state);
	if (pass->state->phase == MP_MATCH_FINISHED)
		draw_result(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->state);
}

/*
 * A playing rebuild has already written every region signature through
 * refresh_regions. Any other phase leaves no region planes behind, so their
 * signatures are cleared and the first playing frame rebuilds from scratch.
 */
static void store_signatures(t_match_regions *pass, uint64_t signature)
{
	t_render_ctx *ctx;

	ctx = pass->ctx;
	ctx->mp_match_signature = signature;
	if (pass->state->phase == MP_MATCH_PLAYING)
	{
		ctx->mp_match_static_signature = static_signature(pass->state,
			pass->width, pass->height);
		return ;
	}
	ctx->mp_match_static_signature = 0;
	ctx->mp_match_local_signature = 0;
	ctx->mp_match_opponent_signature = 0;
	ctx->mp_match_left_signature = 0;
	ctx->mp_match_right_signature = 0;
	ctx->mp_match_loadout_signature = 0;
	ctx->mp_match_opponent_loadout_signature = 0;
	ctx->mp_match_hud_signature = 0;
}

/**
 * @brief Pulls every region inside its own whole cells.
 *
 * A bitmap plane can only start and end on a cell boundary, so a region whose
 * edge falls mid-cell is given the whole cell - and the region on the other
 * side of that edge is given it too. The two then share a cell while sharing
 * no pixels, and on a protocol whose bitmaps do not compose that shared cell
 * is expensive: the terminal has no way to redraw half of it, so touching
 * either region retransmits both.
 *
 * The Double arena has two such seams and they were the whole of the cost. The
 * HUD ends exactly where the boards begin, on a pixel row four fifths of the
 * way down a cell, so every falling piece retransmitted the full-width HUD -
 * 78 KB of Sixel per frame for a panel that had not changed. The rival's meter
 * begins exactly where their board ends, so it went out again with every frame
 * of theirs. On foot, which is Sixel, that is what the flicker was.
 *
 * Snapping inwards rather than outwards is what makes neighbours disjoint
 * without having to know about each other: a region that is never given a cell
 * it does not fully cover cannot be given one its neighbour covers either. The
 * cost is at most one cell off each edge - a few pixels of a board tile - and
 * the artwork is composed into the snapped rectangle, so nothing is clipped.
 *
 * @param ctx Render context carrying the terminal's cell size.
 * @param layout Layout to align in place.
 */
static void snap_layout_to_cells(const t_render_ctx *ctx,
	t_mp_match_pixel_layout *layout)
{
	t_mp_rect	*rects[5];
	t_mp_rect	before[2];
	int			index;

	if (ctx == NULL || layout == NULL || ctx->cell_px_x <= 0
		|| ctx->cell_px_y <= 0)
		return ;
	before[0] = layout->ability_bar;
	before[1] = layout->opponent_ability_bar;
	rects[0] = &layout->loadout;
	rects[1] = &layout->ability_bar;
	rects[2] = &layout->opponent_ability_bar;
	rects[3] = &layout->opponent_loadout;
	rects[4] = &layout->hud;
	index = 0;
	while (index < 5)
	{
		snap_rect_to_cells(rects[index], ctx->cell_px_x, ctx->cell_px_y);
		index++;
	}
	/*
	 * The meters carry the circles' own coordinates, which the hit test reads
	 * and the drawing reads, so they travel with the rectangle they belong to
	 * rather than being left where an unsnapped bar used to be.
	 */
	shift_ability_centres(layout, before, ctx);
}

/**
 * @brief Moves the ability circles by however far their meter moved.
 *
 * @param layout Layout whose meters have already been snapped.
 * @param before The two meters as they were before snapping.
 * @param ctx Render context, for the cell size the snap used.
 */
static void shift_ability_centres(t_mp_match_pixel_layout *layout,
	const t_mp_rect *before, const t_render_ctx *ctx)
{
	int	index;

	(void)ctx;
	layout->ability_center_x += layout->ability_bar.x - before[0].x;
	layout->opponent_ability_center_x += layout->opponent_ability_bar.x
		- before[1].x;
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		layout->ability_center_y[index] += layout->ability_bar.y
			- before[0].y;
		layout->opponent_ability_center_y[index]
			+= layout->opponent_ability_bar.y - before[1].y;
		index++;
	}
}

/**
 * @brief Shrinks one rectangle to the whole cells it entirely covers.
 *
 * A rectangle too small to contain a whole cell is left alone: it has no
 * plane of its own to be charged for, and rounding it to nothing would only
 * make the artwork drawn into it disappear.
 *
 * @param rect Rectangle to align in place.
 * @param cell_x Cell width in pixels.
 * @param cell_y Cell height in pixels.
 */
static void snap_rect_to_cells(t_mp_rect *rect, int cell_x, int cell_y)
{
	int	left;
	int	top;
	int	right;
	int	bottom;

	if (rect->width <= 0 || rect->height <= 0)
		return ;
	left = (rect->x + cell_x - 1) / cell_x * cell_x;
	top = (rect->y + cell_y - 1) / cell_y * cell_y;
	right = (rect->x + rect->width) / cell_x * cell_x;
	bottom = (rect->y + rect->height) / cell_y * cell_y;
	if (right - left <= 0 || bottom - top <= 0)
		return ;
	rect->x = left;
	rect->y = top;
	rect->width = right - left;
	rect->height = bottom - top;
}

static int refresh_regions(t_match_regions *pass)
{
	static int (*const steps[6])(t_match_regions *) = {regions_local,
		regions_opponent, regions_battle, regions_loadout,
		regions_opponent_loadout, regions_hud};
	int index;
	int result;

	index = 0;
	while (index < 6)
	{
		result = steps[index](pass);
		if (result < 0)
			return (-1);
		if (result != 0)
			pass->changed = true;
		index++;
	}
	return (pass->changed);
}

/*
 * On a rebuild the canvas is the caller's finished artwork and must not be
 * overwritten; on an incremental frame the first region that needs one gets a
 * blank canvas and every later region shares it.
 */
static bool regions_prepare(t_match_regions *pass)
{
	if (pass->pixels != NULL)
		return (true);
	pass->pixels = canvas_keep(pass->ctx, pass->width, pass->height);
	return (pass->pixels != NULL);
}

static int regions_local(t_match_regions *pass)
{
	const t_mp_match_state *state;
	const t_mp_rect *board;
	t_mp_rect region;
	uint64_t signature;
	bool doubles;

	state = pass->state;
	board = &pass->layout->local_board;
	doubles = state->mode == APP_GAME_MODE_DOUBLE;
	signature = local_board_signature(&state->local_game, doubles);
	if (!pass->force && pass->ctx->mp_match_local_signature == signature)
		return (0);
	if (!regions_prepare(pass))
		return (-1);
	region = (t_mp_rect){board->x - 8, board->y - 8, board->width + 16,
		board->height + 16};
	if (!board_region_sync(pass, 0, board, &state->local_game))
		return (-1);
	if (!blit_board_bands(pass, &region, 0, pass->ctx->mp_match_local_bands,
			pass->ctx->mp_match_band_signatures))
		return (-1);
	if (doubles && !regions_caption(pass, board))
		return (-1);
	pass->ctx->mp_match_local_signature = signature;
	return (1);
}

/**
 * @brief Brings the opponent's board and caption up to date, at their own rate.
 *
 * Two things separate this from regions_local. The board is banded the same way
 * but under its own signatures, and the caption strip beneath it keeps a plane
 * of its own so a score changing does not re-blit board rows.
 *
 * The other is the presentation floor. A rebuild that is merely due is deferred
 * to MP_MATCH_OPPONENT_PRESENT_MS rather than skipped: the stored signature is
 * left alone, so the next frame past the floor draws whatever the board has
 * become by then, and nothing arrives late except the picture of it. A forced
 * pass ignores the floor, because the canvas underneath it is new and a region
 * left undrawn on a fresh canvas is a hole.
 */
static int regions_opponent(t_match_regions *pass)
{
	const t_mp_match_state *state;
	const t_mp_rect *board;
	t_mp_rect region;
	uint64_t signature;

	state = pass->state;
	if (state->mode != APP_GAME_MODE_DOUBLE)
		return (0);
	board = &pass->layout->opponent_board;
	signature = game_signature(&state->opponent_game);
	if (!pass->force && pass->ctx->mp_match_opponent_signature == signature)
		return (0);
	if (!pass->force
		&& ui_notification_now_ms() < pass->ctx->mp_match_opponent_due_ms)
	{
		pass->ctx->mp_match_opponent_deferred = true;
		return (0);
	}
	if (!regions_prepare(pass))
		return (-1);
	if (!board_region_sync(pass, 1, board, &state->opponent_game))
		return (-1);
	region = (t_mp_rect){board->x - 8, board->y - 8, board->width + 16,
		board->height + 16};
	if (!blit_board_bands(pass, &region, 1,
			pass->ctx->mp_match_opponent_bands,
			pass->ctx->mp_match_opponent_band_signatures))
		return (-1);
	if (!regions_opponent_caption(pass, board))
		return (-1);
	pass->ctx->mp_match_opponent_signature = signature;
	pass->ctx->mp_match_opponent_due_ms = ui_notification_now_ms()
		+ MP_MATCH_OPPONENT_PRESENT_MS;
	pass->ctx->mp_match_opponent_deferred = false;
	return (1);
}

/**
 * @brief How long until a held-back opponent frame may be drawn.
 *
 * The loop only renders when something changed, so a frame the floor deferred
 * would sit undrawn until the next thing that did change - which during a lull
 * is a gravity step away, and at the end of a match is forever. Reporting the
 * wait puts it back on the loop's timetable: it sleeps until the floor lifts
 * and draws then.
 *
 * @param ctx Render context to ask.
 * @return Milliseconds until the deferred frame is due, 0 when it is due now,
 *         or -1 when nothing is being held back.
 */
int	render_multiplayer_match_deferred_ms(const t_render_ctx *ctx)
{
	uint64_t	now;

	if (ctx == NULL || !ctx->mp_match_opponent_deferred)
		return (-1);
	now = ui_notification_now_ms();
	if (now >= ctx->mp_match_opponent_due_ms)
		return (0);
	return ((int)(ctx->mp_match_opponent_due_ms - now));
}

/**
 * @brief Redraws the name-and-score strip under the opponent's board.
 *
 * A forced pass has already had it composed onto the canvas by compose_match
 * and only needs the plane, which is why the drawing is conditional and the
 * plane is not.
 *
 * @param pass The frame in progress.
 * @param board The opponent's board rectangle.
 * @return true unless the plane could not be created.
 */
static bool regions_opponent_caption(t_match_regions *pass,
	const t_mp_rect *board)
{
	const t_mp_match_state	*state;
	t_mp_rect				strip;

	state = pass->state;
	strip = (t_mp_rect){board->x - 8, board->y + board->height + 8,
		board->width + 16, 66};
	if (!pass->force)
	{
		clear_rect(pass->pixels, pass->width, pass->height, &strip);
		draw_double_caption(pass->ctx, pass->pixels, pass->width, pass->height,
			board, state->opponent_name, state->opponent_game.scoring.total,
			state->opponent_charge, opponent_pending(state), false);
	}
	return (create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &strip, &pass->ctx->mp_match_opponent_plane));
}

static int regions_battle(t_match_regions *pass)
{
	uint64_t left_signature;
	uint64_t right_signature;
	int opponents;
	int left;
	int changed;

	if (pass->state->mode != APP_GAME_MODE_BATTLE_ROYALE)
		return (0);
	opponents = clamp_int(pass->state->players_total - 1, 0,
			APP_ROOM_MAX_PLAYERS - 1);
	left = (opponents + 1) / 2;
	left_signature = opponents_signature(pass->state, 0, left);
	right_signature = opponents_signature(pass->state, left, opponents - left);
	changed = 0;
	if (pass->force || pass->ctx->mp_match_left_signature != left_signature)
	{
		if (!regions_side(pass, &pass->layout->left_opponents, 0, left,
				&pass->ctx->mp_match_left_plane))
			return (-1);
		pass->ctx->mp_match_left_signature = left_signature;
		changed = 1;
	}
	if (pass->force || pass->ctx->mp_match_right_signature != right_signature)
	{
		if (!regions_side(pass, &pass->layout->right_opponents, left,
				opponents - left, &pass->ctx->mp_match_right_plane))
			return (-1);
		pass->ctx->mp_match_right_signature = right_signature;
		changed = 1;
	}
	return (changed);
}

static bool regions_side(t_match_regions *pass, const t_mp_rect *rect,
	int first, int count, struct ncplane **slot)
{
	if (!regions_prepare(pass))
		return (false);
	if (!pass->force)
	{
		clear_rect(pass->pixels, pass->width, pass->height, rect);
		draw_opponent_region(pass->ctx, pass->pixels, pass->width,
			pass->height, rect, pass->state, first, count);
	}
	return (create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, rect, slot));
}

static int regions_loadout(t_match_regions *pass)
{
	const t_app_catalogue_item_view_model	*character;
	t_mp_rect								card;
	uint64_t								signature;
	int										hovered;

	signature = loadout_signature(pass->state);
	if (!pass->force && pass->ctx->mp_match_loadout_signature == signature)
		return (0);
	if (!regions_prepare(pass))
		return (-1);
	if (!pass->force)
	{
		clear_rect(pass->pixels, pass->width, pass->height,
			&pass->layout->loadout);
		clear_rect(pass->pixels, pass->width, pass->height,
			&pass->layout->ability_bar);
		draw_loadout(pass->ctx, pass->pixels, pass->width, pass->height,
			&pass->layout->loadout, pass->state, true);
		draw_ability_bar(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->layout, pass->state);
		/*
		 * The card is drawn into this region rather than given one of its
		 * own, and that is the whole reason it is visible. Two bitmap planes
		 * do not compose - anything drawn over one invalidates it - so a card
		 * on its own plane over the board was covered by the next frame of
		 * the board. Inside the loadout it is the same bitmap as the column
		 * it sits on, and nothing can arrive on top of it.
		 */
		character = mp_match_selected_character(pass->state);
		hovered = pass->state->hovered_ability;
		if (character != NULL && hovered >= 1
			&& hovered <= APP_CHARACTER_ABILITY_COUNT)
		{
			popover_place(pass->layout, hovered, &card);
			popover_draw(pass->ctx, pass->pixels, pass->width, pass->height,
				&card, character, hovered);
		}
	}
	if (!create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &pass->layout->loadout,
			&pass->ctx->mp_match_loadout_plane))
		return (-1);
	if (!create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &pass->layout->ability_bar,
			&pass->ctx->mp_match_ability_plane))
		return (-1);
	pass->ctx->mp_match_loadout_signature = signature;
	return (1);
}

static int regions_opponent_loadout(t_match_regions *pass)
{
	uint64_t signature;

	if (pass->state->mode != APP_GAME_MODE_DOUBLE
		|| pass->layout->opponent_loadout.width <= 0)
		return (0);
	signature = opponent_loadout_signature(pass->state);
	if (!pass->force
		&& pass->ctx->mp_match_opponent_loadout_signature == signature)
		return (0);
	if (!regions_prepare(pass))
		return (-1);
	if (!pass->force)
	{
		clear_rect(pass->pixels, pass->width, pass->height,
			&pass->layout->opponent_loadout);
		clear_rect(pass->pixels, pass->width, pass->height,
			&pass->layout->opponent_ability_bar);
		draw_opponent_column(pass->ctx, pass->pixels, pass->width,
			pass->height, pass->layout, pass->state);
	}
	if (!create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &pass->layout->opponent_loadout,
			&pass->ctx->mp_match_opponent_loadout_plane))
		return (-1);
	if (!create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &pass->layout->opponent_ability_bar,
			&pass->ctx->mp_match_opponent_ability_plane))
		return (-1);
	pass->ctx->mp_match_opponent_loadout_signature = signature;
	return (1);
}

/**
 * @brief Puts the hovered ability's card beside the meter, or takes it away.
 *
 * The card is the one surface here that is not part of the composed bitmap.
 * It used to be, and it was unreadable: the description is a sentence from the
 * catalogue, the slot it borrowed was the narrow HOLD/NEXT column, and the
 * bitmap text drawer shrinks a line until it fits - so eighty characters in a
 * hundred-pixel column bottomed out at a four-pixel glyph and overflowed the
 * panel anyway. A terminal plane wraps instead of shrinking and is legible at
 * whatever size the terminal's font is.
 *
 * It also sits where the thing it describes is. The bitmap card lived one
 * column left of the meter; this one tracks the circle being hovered.
 *
 * @param pass The frame being refreshed.
 * @return 1 when the card appeared, moved or changed, 0 when nothing did,
 *         -1 when a plane could not be made.
 */
/**
 * @brief Places the card beside the meter, vertically on the circle it names.
 *
 * Over the board rather than beside it, because there is no beside: the
 * chrome column is the width of a portrait and a card that fitted in it would
 * be the unreadable one this replaced. Solo overlays its card for the same
 * reason. A player reading an ability is working the meter with the mouse,
 * not placing a piece.
 *
 * @param layout Geometry carrying the meter, its circles and the board.
 * @param ability 1-based ability being hovered.
 * @param out Receives the card's rectangle.
 */
static void	popover_place(const t_mp_match_pixel_layout *layout, int ability,
	t_mp_rect *out)
{
	int	height;

	/*
	 * Tall enough for a title, a cost line and the wrapped description, and
	 * no taller: a card sized to the column left a panel of empty space
	 * under two lines of text.
	 */
	height = clamp_int(layout->loadout.height / 2, 190, 330);
	out->width = layout->loadout.width - 12;
	out->height = height;
	out->x = layout->loadout.x + 6;
	out->y = layout->ability_center_y[ability - 1] - height / 2;
	out->y = clamp_int(out->y, layout->loadout.y + 6,
			layout->loadout.y + layout->loadout.height - height - 6);
}

/**
 * @brief Draws the card: a frame, the name and slot, the cost, the effect.
 *
 * The same four things Solo's card says, in the same order, so an ability
 * learnt in one mode reads identically in the other. The description is
 * word-wrapped over three lines at a fixed size rather than shrunk to fit on
 * one - shrinking is what made the old card bottom out at a four-pixel glyph
 * and overflow its panel anyway.
 *
 * @param ctx Render context, for the glyph sheet.
 * @param pixels Canvas being composed.
 * @param width Canvas width.
 * @param height Canvas height.
 * @param card The card's rectangle.
 * @param character The fighter whose ability is hovered.
 * @param ability 1-based ability being hovered.
 */
static void	popover_draw(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *card,
	const t_app_catalogue_item_view_model *character, int ability)
{
	char		line[APP_TEXT_MAX];
	t_mp_rect	box;
	int			body;
	int			row;

	mp_match_pixel_fill_rect(pixels, width, height, card, g_dark, 244);
	mp_match_pixel_outline_rect(pixels, width, height, card, 3, g_pink, 255);
	body = clamp_int(card->height / 18, 10, 14);
	box = (t_mp_rect){card->x + 14, card->y + 12, card->width - 28,
		body * 3 / 2};
	snprintf(line, sizeof(line), "%.*s",
		(int)sizeof(line) - 2, character->abilities[ability - 1].name);
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &box,
		body * 3 / 2, g_cream, true);
	box.y += body * 3 / 2 + 8;
	box.height = body;
	snprintf(line, sizeof(line), "[%d]  COST %d", ability,
		solo_ability_cost((t_solo_ability)ability));
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &box,
		body, g_gold, true);
	row = 0;
	while (row < MP_MATCH_POPOVER_TEXT_ROWS)
	{
		box.y += body + 6;
		/*
		 * The wrap width is in characters, so it is the glyph's advance -
		 * its size plus the fifth of it the drawer uses for spacing - that
		 * decides how many fit. Guessing low here is what made a long line
		 * shrink to fit while a short one beside it did not, and the card
		 * read as two different sizes of the same sentence.
		 */
		if (popover_wrap(character->abilities[ability - 1].description,
				(card->width - 28) / max_int(1, body * 6 / 5), row, line,
				sizeof(line)) > 0)
			mp_match_pixel_draw_text_box(ctx, pixels, width, height, line,
				&box, body, g_lavender, false);
		row++;
	}
}

/**
 * @brief Copies out one word-wrapped line of a description.
 *
 * Wrapping rather than shrinking is the whole point of this card. Words are
 * kept whole unless one is longer than the line, in which case it is cut -
 * a word that cannot fit anywhere has to break somewhere.
 *
 * @param text The full description.
 * @param width Usable columns per line.
 * @param line Which wrapped line is wanted, 0-based.
 * @param out Receives the line, NUL-terminated.
 * @param cap Size of out.
 * @return The number of characters written.
 */
static int	popover_wrap(const char *text, int width, int line, char *out,
	size_t cap)
{
	int	start;
	int	end;
	int	cut;

	out[0] = '\0';
	if (text == NULL || width <= 0 || (size_t)width >= cap)
		return (0);
	start = 0;
	while (line >= 0 && text[start] != '\0')
	{
		while (text[start] == ' ')
			start++;
		end = start;
		cut = 0;
		while (text[end] != '\0' && end - start < width)
		{
			if (text[end] == ' ')
				cut = end;
			end++;
		}
		if (text[end] != '\0' && cut > start)
			end = cut;
		if (line == 0)
		{
			snprintf(out, cap, "%.*s", end - start, text + start);
			return (end - start);
		}
		start = end;
		line--;
	}
	return (0);
}

static int regions_hud(t_match_regions *pass)
{
	uint64_t signature;

	signature = hud_signature(pass->state);
	if (!pass->force && pass->ctx->mp_match_hud_signature == signature)
		return (0);
	if (!regions_prepare(pass))
		return (-1);
	if (!pass->force)
	{
		clear_rect(pass->pixels, pass->width, pass->height,
			&pass->layout->hud);
		draw_match_hud(pass->ctx, pass->pixels, pass->width, pass->height,
			pass->layout, pass->state);
	}
	if (!create_region_plane(pass->ctx, pass->pixels, pass->width,
			pass->height, &pass->layout->hud,
			&pass->ctx->mp_match_hud_plane))
		return (-1);
	pass->ctx->mp_match_hud_signature = signature;
	return (1);
}

void render_multiplayer_match_pixel_destroy(t_render_ctx *ctx)
{
	int	slot;

	if (ctx == NULL)
		return ;
	if (ctx->mp_match_tile_visual != NULL)
		ncvisual_destroy(ctx->mp_match_tile_visual);
	free(ctx->mp_match_tile_atlas);
	ctx->mp_match_tile_atlas = NULL;
	free(ctx->mp_match_tile_scaled);
	ctx->mp_match_tile_scaled = NULL;
	ctx->mp_match_tile_scaled_size = 0;
	slot = 0;
	while (slot < MP_MATCH_PORTRAITS)
	{
		if (ctx->mp_match_portrait_visual[slot] != NULL)
			ncvisual_destroy(ctx->mp_match_portrait_visual[slot]);
		/*
		 * Cleared, not merely freed. This runs again on the next rebuild,
		 * and a pointer left behind is one ncvisual_destroy takes for a live
		 * decoder the second time - which is a bus error inside libavcodec,
		 * a long way from the line that caused it.
		 */
		ctx->mp_match_portrait_visual[slot] = NULL;
		free(ctx->mp_match_portrait_pixels[slot]);
		ctx->mp_match_portrait_pixels[slot] = NULL;
		ctx->mp_match_portrait_px_width[slot] = 0;
		ctx->mp_match_portrait_px_height[slot] = 0;
		ctx->mp_match_portrait_source[slot][0] = '\0';
		slot++;
	}
	free(ctx->mp_match_canvas);
	ctx->mp_match_canvas = NULL;
	ctx->mp_match_canvas_width = 0;
	ctx->mp_match_canvas_height = 0;
	memset(ctx->mp_match_boards, 0, sizeof(ctx->mp_match_boards));
	ctx->mp_match_tile_visual = NULL;
	destroy_region_planes(ctx);
	ctx->mp_match_signature = 0;
	ctx->mp_match_static_signature = 0;
	ctx->mp_match_local_signature = 0;
	ctx->mp_match_opponent_signature = 0;
	ctx->mp_match_left_signature = 0;
	ctx->mp_match_right_signature = 0;
	ctx->mp_match_loadout_signature = 0;
	ctx->mp_match_opponent_loadout_signature = 0;
	ctx->mp_match_hud_signature = 0;
}

static bool load_assets(t_render_ctx *ctx)
{
	if (ctx->mp_font_visual == NULL)
		ctx->mp_font_visual = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (ctx->mp_match_tile_visual == NULL)
		ctx->mp_match_tile_visual = ncvisual_from_file(DEFAULT_TILE_PATH);
	if (ctx->mp_font_visual == NULL || ctx->mp_match_tile_visual == NULL)
		return (false);
	return (load_tile_atlas(ctx));
}

static bool refresh_background(t_render_ctx *ctx, bool rebuild)
{
	if (rebuild)
	{
		render_multiplayer_match_pixel_destroy(ctx);
		render_multiplayer_destroy(ctx);
		if (render_background_replace(ctx,
				ctx->theme_assets.solo_background, false) < 0)
			return (false);
	}
	return (ctx->bg_cols > 0 && ctx->bg_rows > 0
		&& ctx->cell_px_x > 0 && ctx->cell_px_y > 0);
}

/**
 * @brief Puts the composed canvas on the full-screen plane.
 *
 * cells selects the quadrant blitter over the bitmap one. It is set for the
 * playing phase, where region bitmaps sit on top and a bitmap here would be
 * invalidated by every one of them; the still phases keep the bitmap, since
 * nothing covers it and the extra resolution is free.
 */
static bool present_canvas(t_render_ctx *ctx, const uint32_t *pixels,
	int width, int height, bool cells)
{
	ncplane_options options;
	struct ncplane *plane;

	if (render_plane_geometry_matches(ctx->screen_plane, ctx->bg_row,
			ctx->bg_col, (unsigned)ctx->bg_rows, (unsigned)ctx->bg_cols))
	{
		ncplane_erase(ctx->screen_plane);
		if (cells)
			return (render_plane_blit_rgba_cells(ctx, ctx->screen_plane,
					pixels, width, height, width));
		return (render_plane_blit_rgba(ctx, ctx->screen_plane, pixels,
				width, height, width));
	}
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row;
	options.x = ctx->bg_col;
	options.rows = ctx->bg_rows;
	options.cols = ctx->bg_cols;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL || !(cells ? render_plane_blit_rgba_cells(ctx, plane,
				pixels, width, height, width)
			: render_plane_blit_rgba(ctx, plane, pixels, width, height,
				width)))
	{
		if (plane != NULL)
			ncplane_destroy(plane);
		return (false);
	}
	if (ctx->screen_plane != NULL)
		ncplane_destroy(ctx->screen_plane);
	ctx->screen_plane = plane;
	return (true);
}

static bool create_region_plane(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *region, struct ncplane **slot)
{
	ncplane_options options;
	struct ncplane *plane;
	const uint32_t *origin;
	int crop_x;
	int crop_y;
	int crop_width;
	int crop_height;

	if (region == NULL || slot == NULL || ctx->cell_px_x <= 0
		|| ctx->cell_px_y <= 0 || region->width <= 0 || region->height <= 0)
		return (false);
	crop_x = max_int(0, region->x / ctx->cell_px_x * ctx->cell_px_x);
	crop_y = max_int(0, region->y / ctx->cell_px_y * ctx->cell_px_y);
	crop_width = min_int(((region->x + region->width + ctx->cell_px_x - 1)
			/ ctx->cell_px_x) * ctx->cell_px_x, width) - crop_x;
	crop_height = min_int(((region->y + region->height + ctx->cell_px_y - 1)
			/ ctx->cell_px_y) * ctx->cell_px_y, height) - crop_y;
	if (crop_width <= 0 || crop_height <= 0)
		return (false);
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row + crop_y / ctx->cell_px_y;
	options.x = ctx->bg_col + crop_x / ctx->cell_px_x;
	options.rows = (unsigned)(crop_height + ctx->cell_px_y - 1)
		/ (unsigned)ctx->cell_px_y;
	options.cols = (unsigned)(crop_width + ctx->cell_px_x - 1)
		/ (unsigned)ctx->cell_px_x;
	origin = pixels + (size_t)crop_y * width + crop_x;
	if (render_plane_geometry_matches(*slot, options.y, options.x,
			options.rows, options.cols))
		return (render_plane_blit_rgba(ctx, *slot, origin, crop_width,
				crop_height, width));
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL || !render_plane_blit_rgba(ctx, plane, origin,
			crop_width, crop_height, width))
	{
		if (plane != NULL)
			ncplane_destroy(plane);
		return (false);
	}
	if (*slot != NULL)
		ncplane_destroy(*slot);
	*slot = plane;
	return (true);
}

static void destroy_region_planes(t_render_ctx *ctx)
{
	struct ncplane **planes[9];
	int index;

	if (ctx == NULL)
		return ;
	planes[0] = &ctx->mp_match_opponent_plane;
	planes[1] = &ctx->mp_match_left_plane;
	planes[2] = &ctx->mp_match_right_plane;
	planes[3] = &ctx->mp_match_loadout_plane;
	planes[4] = &ctx->mp_match_hud_plane;
	planes[5] = &ctx->mp_match_ability_plane;
	planes[6] = &ctx->mp_match_caption_plane;
	planes[7] = &ctx->mp_match_opponent_loadout_plane;
	planes[8] = &ctx->mp_match_opponent_ability_plane;
	index = 0;
	while (index < 9)
	{
		if (*planes[index] != NULL)
			ncplane_destroy(*planes[index]);
		*planes[index] = NULL;
		index++;
	}
	index = 0;
	while (index < MP_MATCH_BOARD_BANDS)
	{
		if (ctx->mp_match_local_bands[index] != NULL)
			ncplane_destroy(ctx->mp_match_local_bands[index]);
		ctx->mp_match_local_bands[index] = NULL;
		ctx->mp_match_band_signatures[index] = 0;
		if (ctx->mp_match_opponent_bands[index] != NULL)
			ncplane_destroy(ctx->mp_match_opponent_bands[index]);
		ctx->mp_match_opponent_bands[index] = NULL;
		ctx->mp_match_opponent_band_signatures[index] = 0;
		index++;
	}
	ctx->mp_match_opponent_due_ms = 0;
}

static uint64_t match_signature(const t_mp_match_state *state,
	int width, int height)
{
	uint64_t hash;
	int opponents;
	int seconds;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &width, sizeof(width));
	hash = hash_bytes(hash, &height, sizeof(height));
	hash = hash_bytes(hash, &state->mode, sizeof(state->mode));
	hash = hash_bytes(hash, &state->phase, sizeof(state->phase));
	seconds = mp_match_character_seconds(state);
	hash = hash_bytes(hash, &state->selection.selected,
		sizeof(state->selection.selected));
	hash = hash_bytes(hash, &state->selection.locked,
		sizeof(state->selection.locked));
	hash = hash_bytes(hash, &seconds, sizeof(seconds));
	hash = hash_bytes(hash, &state->target_mode, sizeof(state->target_mode));
	hash = hash_bytes(hash, &state->players_total,
		sizeof(state->players_total) * 5);
	hash = hash_bytes(hash, &state->local_game.board,
		sizeof(state->local_game.board));
	hash = hash_bytes(hash, &state->local_game.active,
		sizeof(state->local_game.active));
	hash = hash_bytes(hash, &state->local_game.phase,
		sizeof(state->local_game.phase));
	hash = hash_bytes(hash, &state->local_game.scoring,
		sizeof(state->local_game.scoring));
	hash = hash_bytes(hash, &state->local_game.crystal_charge,
		sizeof(state->local_game.crystal_charge));
	hash = hash_bytes(hash, &state->opponent_game.board,
		sizeof(state->opponent_game.board));
	hash = hash_bytes(hash, &state->opponent_game.active,
		sizeof(state->opponent_game.active));
	hash = hash_bytes(hash, &state->opponent_game.phase,
		sizeof(state->opponent_game.phase));
	hash = hash_bytes(hash, &state->opponent_game.scoring,
		sizeof(state->opponent_game.scoring));
	opponents = clamp_int(state->players_total - 1, 0,
		APP_ROOM_MAX_PLAYERS - 1);
	hash = hash_bytes(hash, state->opponents,
		(size_t)opponents * sizeof(state->opponents[0]));
	return (hash_bytes(hash, state->status, strlen(state->status)));
}

static uint64_t static_signature(const t_mp_match_state *state,
	int width, int height)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &width, sizeof(width));
	hash = hash_bytes(hash, &height, sizeof(height));
	hash = hash_bytes(hash, &state->mode, sizeof(state->mode));
	hash = hash_bytes(hash, &state->phase, sizeof(state->phase));
	hash = hash_bytes(hash, &state->players_total, sizeof(state->players_total));
	hash = hash_bytes(hash, &state->selection.selected,
		sizeof(state->selection.selected));
	return (hash);
}

/**
 * @brief Reports which frame of the clear flash a board is showing.
 *
 * The completed rows stay on the board for the whole hold, so nothing else a
 * board signature hashes moves when the flash flips from its first frame to its
 * second. Without this the animation would only ever appear on a frame some
 * other change had already paid for.
 *
 * @return 0 when no row is clearing, otherwise 1 or 2 for the flash frame.
 */
static int clear_flash_step(const t_solo_game *game)
{
	if (game->phase != SOLO_CLEARING || game->clear_count <= 0)
		return (0);
	if (game->clear_elapsed_ms >= solo_clear_duration_ms(game->level) / 2)
		return (2);
	return (1);
}

/**
 * @brief Hashes everything drawn for a rival's board, the active piece
 *        included.
 *
 * The piece has to be in here because board_region_sync draws it for both
 * boards. Gravity moves nothing else - not the board, which only changes at
 * lock, and not the score, which only changes on a drop or a clear - so a
 * signature over the settled cells alone reported "unchanged" for the whole
 * fall. The rival's piece hung motionless and then teleported at lock, and it
 * appeared to work only when they soft or hard dropped, because those two add
 * drop score and the score was hashed.
 */
static uint64_t game_signature(const t_solo_game *game)
{
	uint64_t hash;
	int flash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &game->board, sizeof(game->board));
	hash = hash_bytes(hash, &game->active, sizeof(game->active));
	hash = hash_bytes(hash, &game->phase, sizeof(game->phase));
	hash = hash_bytes(hash, &game->scoring, sizeof(game->scoring));
	flash = clear_flash_step(game);
	hash = hash_bytes(hash, &flash, sizeof(flash));
	return (hash_bytes(hash, &game->crystal_charge,
			sizeof(game->crystal_charge)));
}

static uint64_t local_board_signature(const t_solo_game *game,
	bool include_score)
{
	uint64_t	hash;
	int			countdown;
	int			flash;
	int			danger;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &game->board, sizeof(game->board));
	hash = hash_bytes(hash, &game->active, sizeof(game->active));
	hash = hash_bytes(hash, &game->phase, sizeof(game->phase));
	hash = hash_bytes(hash, &game->countdown_active,
		sizeof(game->countdown_active));
	countdown = solo_game_countdown_value(game);
	hash = hash_bytes(hash, &countdown, sizeof(countdown));
	flash = clear_flash_step(game);
	hash = hash_bytes(hash, &flash, sizeof(flash));
	danger = danger_step(game);
	hash = hash_bytes(hash, &danger, sizeof(danger));
	/*
	 * Dark changes what is drawn without changing the board, so a signature
	 * that ignored it would keep showing a blacked-out field for as long as
	 * the piece happened to sit still - and would keep showing it after the
	 * effect had expired.
	 */
	hash = hash_bytes(hash, &game->effects, sizeof(game->effects));
	if (include_score)
		hash = hash_bytes(hash, &game->scoring, sizeof(game->scoring));
	return (hash);
}

static uint64_t loadout_signature(const t_mp_match_state *state)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &state->selection.selected,
		sizeof(state->selection.selected));
	hash = hash_bytes(hash, &state->local_game.crystal_charge,
		sizeof(state->local_game.crystal_charge));
	hash = hash_bytes(hash, &state->local_game.has_hold,
		sizeof(state->local_game.has_hold));
	hash = hash_bytes(hash, &state->local_game.hold,
		sizeof(state->local_game.hold));
	hash = hash_bytes(hash, &state->local_game.hold_used,
		sizeof(state->local_game.hold_used));
	hash = hash_bytes(hash, state->local_game.next,
		sizeof(state->local_game.next));
	return (hash_bytes(hash, &state->hovered_ability,
			sizeof(state->hovered_ability)));
}

/**
 * @brief Hashes the rival's column: their meter and who they are playing as.
 *
 * Deliberately not their board. The column redraws when the charge moves or
 * the fighter is settled, which is a handful of times a match, rather than on
 * every gravity tick the board next to it takes.
 */
static uint64_t opponent_loadout_signature(const t_mp_match_state *state)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &state->opponent_charge,
			sizeof(state->opponent_charge));
	hash = hash_bytes(hash, &state->opponent_character,
			sizeof(state->opponent_character));
	return (hash_bytes(hash, state->opponent_name,
			strlen(state->opponent_name)));
}

static uint64_t hud_signature(const t_mp_match_state *state)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &state->target_mode, sizeof(state->target_mode));
	hash = hash_bytes(hash, &state->players_alive,
		sizeof(state->players_alive));
	hash = hash_bytes(hash, &state->ko_count, sizeof(state->ko_count));
	hash = hash_bytes(hash, &state->incoming_attackers,
		sizeof(state->incoming_attackers));
	/* The standing effect line lives in this region, so it dirties it. */
	hash = hash_bytes(hash, &state->local_game.effects,
		sizeof(state->local_game.effects));
	return (hash_bytes(hash, &state->local_game.scoring.total,
			sizeof(state->local_game.scoring.total)));
}

static uint64_t opponents_signature(const t_mp_match_state *state,
	int first, int count)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	if (count <= 0 || first < 0 || first >= APP_ROOM_MAX_PLAYERS - 1)
		return (hash);
	if (first + count > APP_ROOM_MAX_PLAYERS - 1)
		count = APP_ROOM_MAX_PLAYERS - 1 - first;
	return (hash_bytes(hash, &state->opponents[first],
			(size_t)count * sizeof(state->opponents[0])));
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
	const unsigned char *bytes;
	size_t index;

	bytes = data;
	index = 0;
	while (index < size)
	{
		hash ^= bytes[index];
		hash *= 1099511628211ULL;
		index++;
	}
	return (hash);
}

/**
 * @brief Returns the retained canvas, resetting it to the backdrop.
 *
 * Used where a whole composition is about to be written. Incremental frames
 * take the same buffer through canvas_keep() and leave the previous frame's
 * pixels in place.
 */
static uint32_t *new_canvas(t_render_ctx *ctx, int width, int height)
{
	const uint32_t *background;
	size_t bytes;
	int source_width;
	int source_height;

	if (canvas_keep(ctx, width, height) == NULL
		|| !solo_canvas_buffer_bytes(width, height, &bytes))
		return (NULL);
	background = render_backdrop_pixels(ctx, &source_width, &source_height);
	if (background != NULL && source_width == width && source_height == height)
		memcpy(ctx->mp_match_canvas, background, bytes);
	else
		memset(ctx->mp_match_canvas, 0, bytes);
	memset(ctx->mp_match_boards, 0, sizeof(ctx->mp_match_boards));
	return (ctx->mp_match_canvas);
}

/**
 * @brief Returns the retained canvas at this geometry, keeping its contents.
 *
 * A geometry change throws the old buffer away; the caller then has to
 * recompose, which the invalidated cell cache already forces.
 */
static uint32_t *canvas_keep(t_render_ctx *ctx, int width, int height)
{
	size_t bytes;

	if (!solo_canvas_buffer_bytes(width, height, &bytes))
		return (NULL);
	if (ctx->mp_match_canvas != NULL
		&& ctx->mp_match_canvas_width == width
		&& ctx->mp_match_canvas_height == height)
		return (ctx->mp_match_canvas);
	free(ctx->mp_match_canvas);
	ctx->mp_match_canvas = malloc(bytes);
	ctx->mp_match_canvas_width = width;
	ctx->mp_match_canvas_height = height;
	memset(ctx->mp_match_boards, 0, sizeof(ctx->mp_match_boards));
	if (ctx->mp_match_canvas == NULL)
		return (NULL);
	memset(ctx->mp_match_canvas, 0, bytes);
	return (ctx->mp_match_canvas);
}

/**
 * @brief Clears a rectangle back to transparent.
 *
 * Regions that redraw wholesale used to get a blank canvas every frame. The
 * canvas is retained now, so each of them clears its own rectangle instead and
 * keeps composing exactly as it did.
 */
static void clear_rect(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect)
{
	int y;
	int x;

	y = max_int(0, rect->y);
	while (y < min_int(rect->y + rect->height, height))
	{
		x = max_int(0, rect->x);
		while (x < min_int(rect->x + rect->width, width))
		{
			pixels[(size_t)y * width + x] = 0;
			x++;
		}
		y++;
	}
}

static void compose_selection(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_match_state *state)
{
	const t_app_catalogue_item_view_model *character;
	t_mp_rect stage;
	t_mp_rect portrait;
	t_mp_rect text;
	t_mp_rect bar;
	char line[APP_ABILITY_TEXT_MAX];
	int index;
	int y;
	int seconds;

	seconds = mp_match_character_seconds(state);
	stage.width = min_int(width * 78 / 100, 1320);
	stage.height = min_int(height * 64 / 100, 760);
	stage.x = (width - stage.width) / 2;
	stage.y = (height - stage.height) / 2 + 28;
	mp_match_pixel_fill_rect(pixels, width, height, &stage, g_panel, 220);
	mp_match_pixel_outline_rect(pixels, width, height, &stage, 4, g_pink, 220);
	text = (t_mp_rect){stage.x, max_int(28, stage.y - 104),
		stage.width, 58};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "CHOOSE YOUR CHARACTER",
		&text, 31, g_pink, true);
	bar = (t_mp_rect){stage.x, stage.y - 25,
		stage.width * state->selection.remaining_ms / MP_CHARACTER_SELECT_MS, 9};
	mp_match_pixel_fill_rect(pixels, width, height, &bar,
		seconds <= 5 ? g_red : g_gold, 255);
	portrait.width = stage.width * 43 / 100;
	portrait.height = stage.height - 70;
	portrait.x = stage.x + 35;
	portrait.y = stage.y + 35;
	mp_match_pixel_draw_panel(pixels, width, height, &portrait, g_lavender, 205);
	character = mp_match_selected_character(state);
	if (character != NULL && mp_match_pixel_load_portrait(ctx, 0, character->portrait_asset))
	{
		t_mp_rect image = {portrait.x + 12, portrait.y + 12,
			portrait.width - 24, portrait.height - 24};
		mp_match_pixel_draw_portrait(ctx, 0, pixels, width, height, &image);
	}
	text = (t_mp_rect){stage.x + stage.width * 50 / 100, stage.y + 34,
		stage.width * 45 / 100, 72};
	snprintf(line, sizeof(line), state->selection.locked ? "LOCKED   %02d" : "%02d",
		seconds);
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &text, 42,
		seconds <= 5 ? g_red : g_gold, true);
	if (character != NULL)
	{
		text = (t_mp_rect){stage.x + stage.width * 50 / 100,
			stage.y + 125, stage.width * 45 / 100, 58};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, character->name,
			&text, 30, g_gold, true);
		y = text.y + 92;
		index = 0;
		while (index < APP_CHARACTER_ABILITY_COUNT)
		{
			snprintf(line, sizeof(line), "0%d   %s", index + 1,
				character->abilities[index].name);
			text = (t_mp_rect){stage.x + stage.width * 51 / 100, y,
				stage.width * 42 / 100, 27};
			mp_match_pixel_draw_text_box(ctx, pixels, width, height, line,
				&text, 15, index == 0 ? g_pink : g_cream, false);
			text.y = y + 29;
			text.height = 34;
			mp_match_pixel_draw_text_box(ctx, pixels, width, height,
				character->abilities[index].description, &text, 10,
				g_lavender, false);
			bar = (t_mp_rect){text.x, y + 68, text.width, 2};
			mp_match_pixel_fill_rect(pixels, width, height, &bar, g_lavender, 150);
			y += 82;
			index++;
		}
	}
	text = (t_mp_rect){stage.x, stage.y + stage.height + 28,
		stage.width, 36};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height,
		"A D OR ARROWS TO BROWSE      ENTER TO LOCK IN", &text, 16,
		g_lavender, true);
}

static void compose_double(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_match_state *state)
{
	t_mp_match_pixel_layout layout;

	mp_match_pixel_layout_build(state->mode, width, height, &layout);
	draw_match_hud(ctx, pixels, width, height, &layout, state);
	draw_loadout(ctx, pixels, width, height, &layout.loadout, state, true);
	draw_ability_bar(ctx, pixels, width, height, &layout, state);
	draw_game_board(ctx, pixels, width, height, &layout.local_board,
		&state->local_game, "YOUR BOARD", true);
	draw_game_board(ctx, pixels, width, height, &layout.opponent_board,
		&state->opponent_game, state->opponent_name, false);
	draw_opponent_column(ctx, pixels, width, height, &layout, state);
	draw_double_caption(ctx, pixels, width, height, &layout.local_board,
		state->profile.username, state->local_game.scoring.total,
		state->local_game.crystal_charge, state->incoming_garbage, true);
	draw_double_caption(ctx, pixels, width, height, &layout.opponent_board,
		state->opponent_name, state->opponent_game.scoring.total,
		state->opponent_charge, opponent_pending(state), false);
}

static void compose_battle(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_match_state *state)
{
	t_mp_match_pixel_layout layout;
	int opponents;
	int left_count;

	mp_match_pixel_layout_build(state->mode, width, height, &layout);
	draw_match_hud(ctx, pixels, width, height, &layout, state);
	draw_loadout(ctx, pixels, width, height, &layout.loadout, state, true);
	draw_ability_bar(ctx, pixels, width, height, &layout, state);
	draw_game_board(ctx, pixels, width, height, &layout.local_board,
		&state->local_game, "", true);
	opponents = clamp_int(state->players_total - 1, 0,
		APP_ROOM_MAX_PLAYERS - 1);
	left_count = (opponents + 1) / 2;
	draw_opponent_region(ctx, pixels, width, height, &layout.left_opponents,
		state, 0, left_count);
	draw_opponent_region(ctx, pixels, width, height, &layout.right_opponents,
		state, left_count, opponents - left_count);
}

static void draw_match_hud(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_match_pixel_layout *layout,
	const t_mp_match_state *state)
{
	t_mp_rect text;
	t_mp_rect score_backdrop;
	char line[APP_ABILITY_TEXT_MAX];
	int margin;

	margin = layout->hud.x;
	text = (t_mp_rect){margin, 24, width - margin * 2, 46};
	if (state->mode == APP_GAME_MODE_DOUBLE)
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, "DOUBLE PLAYER MODE",
			&text, 30, g_pink, true);
	else
	{
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, "BATTLE ROYALE",
			&text, 29, g_pink, true);
		snprintf(line, sizeof(line), "SCORE %010" PRIu64
			"     ALIVE %d/%d     K.O. %02d", state->local_game.scoring.total,
			state->players_alive, state->players_total, state->ko_count);
		text.y += 48;
		score_backdrop.width = min_int(text.width, max_int(620, width * 42 / 100));
		score_backdrop.x = (width - score_backdrop.width) / 2;
		score_backdrop.y = text.y + 3;
		score_backdrop.height = 36;
		mp_match_pixel_fill_rect(pixels, width, height, &score_backdrop,
			g_panel_light, 216);
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &text, 18,
			g_gold, true);
		draw_targeting(ctx, pixels, width, height, &layout->targeting, state);
	}
	/*
	 * A standing line, because the card that announced the effect fades and
	 * the effect does not: Paralysis outlives its own notification by two
	 * pieces, and a player who looked away in between is back to guessing why
	 * their piece will not turn.
	 *
	 * It goes *inside* layout->hud rather than beside the controls at the foot
	 * of the screen. The controls are drawn here but live outside that
	 * rectangle, which makes them part of the composition that is painted once
	 * on a rebuild - a line put there would be right on the frame it appeared
	 * and then frozen, because only the hud region is re-blitted when the hud
	 * signature moves.
	 */
	effect_line(&state->local_game.effects, line, sizeof(line));
	if (line[0] != '\0')
	{
		text = (t_mp_rect){margin, layout->hud.y
			+ (state->mode == APP_GAME_MODE_DOUBLE ? 50 : 84),
			width - margin * 2, 28};
		if (text.y + text.height <= layout->hud.y + layout->hud.height)
			mp_match_pixel_draw_text_box(ctx, pixels, width, height, line,
				&text, 16, g_red, true);
	}
	text = layout->controls;
	if (state->mode == APP_GAME_MODE_BATTLE_ROYALE)
		draw_hud_backdrop(pixels, width, height, &layout->controls);
	mp_match_pixel_draw_text_box(ctx, pixels, width, height,
		state->mode == APP_GAME_MODE_DOUBLE
		? "ARROWS MOVE   Z X ROTATE   SPACE DROP   C HOLD   1-4 POWERS"
		: "W KOs   A RANDOMS   S ATTACKERS   D BADGES     ARROWS Z X SPACE C PLAY",
		&text, state->mode == APP_GAME_MODE_DOUBLE ? 15 : 14,
		g_lavender, true);
}

static void draw_double_caption(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *board, const char *name,
	uint64_t points, int charge, int pending, bool local)
{
	t_mp_rect text;
	char line[APP_ABILITY_TEXT_MAX];
	char warning[32];

	warning[0] = '\0';
	/*
	 * Garbage is announced when it is queued and lands at the next lock, so
	 * this is the window in which saying so is worth anything: there is still
	 * a piece to place before the rows arrive.
	 */
	if (pending > 0)
		snprintf(warning, sizeof(warning), "   +%d INCOMING", pending);
	/*
	 * Exactly the board's width, and two lines inside it.
	 *
	 * It used to be the board's width plus 24 either side, which is wider
	 * than the gap between the two boards - so the two captions overlapped in
	 * the middle and each painted over the other's end. What reached the
	 * screen was one player's score running into the other's name.
	 *
	 * Everything then had to fit on one line of a shared box, which put a
	 * name, a ten-digit score, a charge and a garbage warning into 360 pixels
	 * and shrank the lot to six. Splitting the line is what buys the size
	 * back: half as many characters per row at twice the height.
	 */
	snprintf(line, sizeof(line), "%s   %010" PRIu64 " PTS",
		name != NULL && name[0] != '\0' ? name : (local ? "YOU" : "OPPONENT"),
		points);
	text = (t_mp_rect){board->x, board->y + board->height + 14,
		board->width, 22};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &text, 17,
		local ? g_gold : g_lavender, true);
	snprintf(line, sizeof(line), "POWER %d%s", charge, warning);
	text.y += 24;
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &text, 17,
		pending > 0 ? g_red : g_lavender, true);
}

static void draw_loadout(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_mp_match_state *state,
	bool portrait)
{
	const t_app_catalogue_item_view_model *character;
	t_mp_match_pixel_layout layout;
	t_mp_rect box;

	character = mp_match_selected_character(state);
	mp_match_pixel_layout_build(state->mode, width, height, &layout);
	mp_match_pixel_draw_panel(pixels, width, height, rect, g_gold, 238);
	box = (t_mp_rect){rect->x + 18, rect->y + 16,
		rect->width - 36, 38};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "FIGHTER",
		&box, 19, g_gold, true);
	if (portrait && character != NULL
		&& mp_match_pixel_load_portrait(ctx, 0, character->portrait_asset))
		mp_match_pixel_draw_portrait(ctx, 0, pixels, width, height,
			&layout.portrait);
	if (character != NULL)
	{
		box = (t_mp_rect){rect->x + 10,
			layout.portrait.y + layout.portrait.height + 16,
			rect->width - 20, 38};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, character->name,
			&box, 19, g_pink, true);
	}
	/*
	 * HOLD and NEXT keep this slot for the whole match now. The hovered
	 * ability used to evict them for a card drawn here in bitmap text, which
	 * was both unreadable and a column away from the meter it described;
	 * regions_popover draws it as a terminal plane beside the circle instead.
	 */
	draw_hold_next(ctx, pixels, width, height,
		&layout.ability_popover, &state->local_game);
}

static void draw_hold_next(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_solo_game *game)
{
	t_mp_rect	label;
	t_mp_rect	slot;
	int			index;

	label = (t_mp_rect){rect->x, rect->y, rect->width, 24};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "HOLD",
		&label, 13, g_gold, true);
	slot = (t_mp_rect){rect->x + 6, rect->y + 27, rect->width - 12,
		max_int(44, rect->height / 4)};
	mp_match_pixel_draw_panel(pixels, width, height, &slot, g_lavender, 210);
	if (game->has_hold)
		draw_piece_preview(ctx, pixels, width, height, &slot, game->hold,
			game->hold_used ? 140u : 255u);
	label.y = slot.y + slot.height + 7;
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "NEXT",
		&label, 13, g_gold, true);
	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		slot.y = label.y + 27 + index * max_int(38,
			(rect->y + rect->height - label.y - 28) / SOLO_NEXT_COUNT);
		slot.height = max_int(34,
			(rect->y + rect->height - label.y - 32) / SOLO_NEXT_COUNT - 5);
		mp_match_pixel_draw_panel(pixels, width, height, &slot,
			g_panel_light, 190);
		draw_piece_preview(ctx, pixels, width, height, &slot,
			game->next[index], 255u);
		index++;
	}
}

static void draw_piece_preview(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *slot, t_piece_type type, unsigned opacity)
{
	t_piece	piece;
	int		cols[4];
	int		rows[4];
	int		min_col;
	int		max_col;
	int		min_row;
	int		max_row;
	int		tile;
	int		origin_x;
	int		origin_y;
	int		index;

	piece = piece_spawn(type);
	if (!piece_cells(&piece, cols, rows))
		return ;
	min_col = cols[0];
	max_col = cols[0];
	min_row = rows[0];
	max_row = rows[0];
	index = 1;
	while (index < 4)
	{
		min_col = min_int(min_col, cols[index]);
		max_col = max_int(max_col, cols[index]);
		min_row = min_int(min_row, rows[index]);
		max_row = max_int(max_row, rows[index]);
		index++;
	}
	/*
	 * The preview fills its slot. draw_tile() samples the 16px source at
	 * whatever size it is asked for - the same upscale the board itself runs
	 * at - so clamping a preview to the source size only ever made Hold and
	 * Next smaller than the space reserved for them.
	 */
	tile = min_int((slot->width - 12) / (max_col - min_col + 1),
		(slot->height - 10) / (max_row - min_row + 1));
	tile = max_int(tile, 1);
	origin_x = slot->x + (slot->width
		- (max_col - min_col + 1) * tile) / 2;
	origin_y = slot->y + (slot->height
		- (max_row - min_row + 1) * tile) / 2;
	index = 0;
	while (index < 4)
	{
		draw_tile(ctx, pixels, width, height, solo_canvas_piece_tile(type),
			origin_x + (cols[index] - min_col) * tile,
			origin_y + (rows[index] - min_row) * tile,
			tile, opacity, false);
		index++;
	}
}

static void draw_ability_bar(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_match_pixel_layout *layout,
	const t_mp_match_state *state)
{
	const t_app_catalogue_item_view_model	*character;
	t_mp_rect	box;
	t_color	marker;
	char		line[24];
	int		segment;
	int		segment_height;
	int		marker_radius;
	int		index;

	character = mp_match_selected_character(state);
	mp_match_pixel_draw_panel(pixels, width, height, &layout->ability_bar, g_gold, 242);
	segment_height = max_int(4,
		(layout->ability_bar.height - 24) / 10 - 3);
	segment = 0;
	while (segment < 10)
	{
		box = (t_mp_rect){layout->ability_bar.x + 12,
			layout->ability_bar.y + layout->ability_bar.height - 12
				- (segment + 1) * (layout->ability_bar.height - 24) / 10 + 2,
			layout->ability_bar.width - 24, segment_height};
		mp_match_pixel_fill_rect(pixels, width, height, &box,
			segment < state->local_game.crystal_charge ? g_pink : g_panel_light,
			segment < state->local_game.crystal_charge ? 255 : 210);
		segment++;
	}
	marker_radius = min_int(layout->ability_hit_radius - 3,
		layout->ability_bar.width / 2 - 5);
	index = 0;
	while (character != NULL && index < APP_CHARACTER_ABILITY_COUNT)
	{
		marker = state->local_game.crystal_charge >= (index + 1) * 2
			? g_pink : g_lavender;
		if (state->hovered_ability == index + 1)
			marker = g_gold;
		mp_match_pixel_draw_circle(pixels, width, height, layout->ability_center_x,
			layout->ability_center_y[index], marker_radius, marker, 255);
		snprintf(line, sizeof(line), "%d", index + 1);
		box = (t_mp_rect){layout->ability_center_x - marker_radius,
			layout->ability_center_y[index] - marker_radius,
			marker_radius * 2, marker_radius * 2};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &box, 15, g_dark, true);
		index++;
	}
}

/**
 * @brief Draws the rival's fighter and meter, mirroring the local pair.
 *
 * The same two panels the player has, on the far side of their opponent's
 * board: who they are fighting, and how close that fighter is to being able to
 * do something about it. A meter filling opposite you is the only warning an
 * ability gives, and before this there was none - the rival's side of the
 * screen was a board with nobody behind it.
 *
 * It is deliberately not interactive. There are no hover states and no
 * popover, because these are not buttons: nothing the player can press lives
 * on this side of the screen.
 *
 * @param ctx Render context holding the fonts and the portrait cache.
 * @param pixels Destination canvas.
 * @param width Canvas width in pixels.
 * @param height Canvas height in pixels.
 * @param layout Geometry carrying the two opponent rectangles.
 * @param state Match model holding the rival's charge and fighter.
 */
static void draw_opponent_column(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_match_pixel_layout *layout,
	const t_mp_match_state *state)
{
	const t_app_catalogue_item_view_model	*character;
	t_mp_rect	box;
	char		line[24];
	int			segment;
	int			segment_height;
	int			marker_radius;
	int			index;

	if (layout->opponent_loadout.width <= 0)
		return ;
	character = mp_match_opponent_character(state);
	mp_match_pixel_draw_panel(pixels, width, height,
		&layout->opponent_loadout, g_lavender, 238);
	box = (t_mp_rect){layout->opponent_loadout.x + 18,
		layout->opponent_loadout.y + 16,
		layout->opponent_loadout.width - 36, 38};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "RIVAL",
		&box, 19, g_lavender, true);
	/*
	 * Slot 1 is the rival's half of the portrait cache. Sharing slot 0 with
	 * the local fighter would decode both PNGs on every frame that drew them
	 * both, which is every frame of the match.
	 */
	if (character != NULL
		&& mp_match_pixel_load_portrait(ctx, 1, character->portrait_asset))
		mp_match_pixel_draw_portrait(ctx, 1, pixels, width, height,
			&layout->opponent_portrait);
	if (character != NULL)
	{
		box = (t_mp_rect){layout->opponent_loadout.x + 10,
			layout->opponent_portrait.y + layout->opponent_portrait.height
			+ 16, layout->opponent_loadout.width - 20, 38};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height,
			character->name, &box, 19, g_pink, true);
	}
	box = (t_mp_rect){layout->opponent_loadout.x + 10,
		layout->opponent_portrait.y + layout->opponent_portrait.height + 58,
		layout->opponent_loadout.width - 20, 30};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height,
		state->opponent_name, &box, 15, g_cream, true);
	mp_match_pixel_draw_panel(pixels, width, height,
		&layout->opponent_ability_bar, g_lavender, 242);
	segment_height = max_int(4,
			(layout->opponent_ability_bar.height - 24) / 10 - 3);
	segment = 0;
	while (segment < 10)
	{
		box = (t_mp_rect){layout->opponent_ability_bar.x + 12,
			layout->opponent_ability_bar.y
			+ layout->opponent_ability_bar.height - 12
			- (segment + 1) * (layout->opponent_ability_bar.height - 24) / 10
			+ 2, layout->opponent_ability_bar.width - 24, segment_height};
		mp_match_pixel_fill_rect(pixels, width, height, &box,
			segment < state->opponent_charge ? g_gold : g_panel_light,
			segment < state->opponent_charge ? 255 : 210);
		segment++;
	}
	marker_radius = min_int(layout->ability_hit_radius - 3,
			layout->opponent_ability_bar.width / 2 - 5);
	/*
	 * The circles are the rival's charge, not their roster entry: which of
	 * the four they can afford is answered by the meter beside them and by
	 * nothing else. Gating them on knowing the character left the whole
	 * meter blank for a rival who had not named one - which is every offline
	 * preview, and any frame that arrives before their pick does.
	 */
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		mp_match_pixel_draw_circle(pixels, width, height,
			layout->opponent_ability_center_x,
			layout->opponent_ability_center_y[index], marker_radius,
			state->opponent_charge >= (index + 1) * 2 ? g_gold : g_lavender,
			255);
		snprintf(line, sizeof(line), "%d", index + 1);
		box = (t_mp_rect){layout->opponent_ability_center_x - marker_radius,
			layout->opponent_ability_center_y[index] - marker_radius,
			marker_radius * 2, marker_radius * 2};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &box,
			15, g_dark, true);
		index++;
	}
}

static void draw_game_board(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_solo_game *game,
	const char *label, bool local)
{
	int grid[BOARD_HEIGHT][BOARD_WIDTH];
	t_mp_rect frame;
	t_mp_rect title;
	/* "3", "2", "1", "GO" - sized for any int so the format is provably
	** bounded rather than bounded by what the countdown happens to count */
	char countdown[12];
	int countdown_value;
	int step;

	step = danger_step(game);
	frame = (t_mp_rect){rect->x - 7, rect->y - 7,
		rect->width + 14, rect->height + 14};
	mp_match_pixel_draw_panel(pixels, width, height, &frame,
		danger_frame(step, local), 248);
	mp_match_pixel_fill_rect(pixels, width, height, rect,
		danger_backing(step), 255);
	/*
	 * The whole-board redraw and the per-cell diff read the same grid, so the
	 * two paths cannot disagree about what a cell shows - which is what makes
	 * the clear flash survive a fallback redraw. Both boards carry their
	 * falling piece, the rival's included: the incremental path draws theirs,
	 * so a rebuild that left it out made the rival's piece vanish until the
	 * next cell that happened to differ brought it back.
	 */
	board_cell_grid(game, grid, true);
	/*
	 * board_cell_grid fills the grid and draw_grid_cells only reads it, so
	 * the cast is the qualifier the call adds, not one it drops. Before C23
	 * int (*)[N] does not convert to const int (*)[N] on its own.
	 */
	draw_grid_cells(ctx, pixels, width, height, rect,
		(const int (*)[BOARD_WIDTH])grid, danger_backing(step));
	countdown_value = local ? solo_game_countdown_value(game) : -1;
	if (countdown_value >= 0)
	{
		if (countdown_value == 0)
			snprintf(countdown, sizeof(countdown), "GO");
		else
			snprintf(countdown, sizeof(countdown), "%d", countdown_value);
		/*
		 * Across the whole board and behind a scrim, rather than a small
		 * number floating over the stack. This is the one moment in a match
		 * where nothing else on the board matters, and it is the moment the
		 * player is deciding whether the game has started - a digit they had
		 * to look for read as the screen being stuck.
		 */
		title = (t_mp_rect){rect->x, rect->y + rect->height / 2
			- rect->height / 6, rect->width, rect->height / 3};
		mp_match_pixel_fill_rect(pixels, width, height, &title, g_dark, 190);
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, countdown,
			&title, rect->height / 4, g_gold, true);
	}
	if (label != NULL && label[0] != '\0')
	{
		title = (t_mp_rect){rect->x - 30, rect->y - 46,
			rect->width + 60, 32};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, label, &title,
			18, local ? g_gold : g_lavender, true);
	}
}

static void draw_snapshot_board(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *rect,
	const struct s_mp_opponent_snapshot *opponent, int player, bool compact)
{
	t_mp_rect frame;
	t_mp_rect title;
	char label[APP_TEXT_MAX];

	frame = (t_mp_rect){rect->x - 3, rect->y - 3,
		rect->width + 6, rect->height + 6};
	mp_match_pixel_draw_panel(pixels, width, height, &frame,
		opponent->targeting_local ? g_red : g_lavender, 235);
	mp_match_pixel_fill_rect(pixels, width, height, rect, g_dark, 255);
	if (opponent->alive)
		draw_board_cells(ctx, pixels, width, height, rect,
			&opponent->board, NULL, NULL);
	if (!compact || rect->width >= 55)
	{
		snprintf(label, sizeof(label), opponent->alive ? "#%02d" : "#%02d KO",
			player);
		title = (t_mp_rect){rect->x - 4, rect->y - 22,
			rect->width + 8, 18};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, label, &title,
			10, opponent->alive ? g_green : g_red, true);
	}
	if (!opponent->alive)
	{
		title = (t_mp_rect){rect->x, rect->y + rect->height / 2 - 12,
			rect->width, 24};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, "KO", &title,
			14, g_red, true);
	}
}

static void draw_board_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_board *board,
	const t_piece *active, const t_piece *ghost)
{
	t_cell cell;
	int tile_size;
	int row;
	int col;

	tile_size = min_int(rect->width / BOARD_WIDTH,
		rect->height / BOARD_HEIGHT);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			cell = board_get(board, col, row);
			if (cell.type == CELL_GARBAGE)
				draw_tile(ctx, pixels, width, height, TILE_GARBAGE,
					rect->x + col * tile_size, rect->y + row * tile_size,
					tile_size, 255, false);
			else if (cell.type == CELL_FILLED)
				draw_tile(ctx, pixels, width, height,
					solo_canvas_piece_tile((t_piece_type)cell.color),
					rect->x + col * tile_size, rect->y + row * tile_size,
					tile_size, 255, false);
			col++;
		}
		row++;
	}
	draw_piece_cells(ctx, pixels, width, height, rect, ghost, true);
	draw_piece_cells(ctx, pixels, width, height, rect, active, false);
}

/**
 * @brief Paints a whole board from the cell grid the diff cache also holds.
 *
 * The layer encoding is the one repaint_cell decodes: 3000 the active piece,
 * 2000 its ghost, 1000 a settled or clearing cell, 0 empty. Keeping the two
 * readers of that encoding side by side is what stops a fallback redraw from
 * disagreeing with an incremental one.
 */
static void draw_grid_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect,
	const int grid[BOARD_HEIGHT][BOARD_WIDTH], t_color backing)
{
	int tile_size;
	int row;
	int col;
	int id;

	tile_size = min_int(rect->width / BOARD_WIDTH,
			rect->height / BOARD_HEIGHT);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			id = grid[row][col];
			if (id >= 1000)
				draw_board_tile(ctx, pixels, width, height, id % 1000,
					rect->x + col * tile_size, rect->y + row * tile_size,
					tile_size, id >= 2000 && id < 3000, backing);
			col++;
		}
		row++;
	}
}

static void draw_piece_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_piece *piece, bool ghost)
{
	int cols[4];
	int rows[4];
	int tile_size;
	int index;

	if (piece == NULL || !piece_cells(piece, cols, rows))
		return ;
	tile_size = min_int(rect->width / BOARD_WIDTH,
		rect->height / BOARD_HEIGHT);
	index = 0;
	while (index < 4)
	{
		if (board_in_bounds(cols[index], rows[index]))
			draw_tile(ctx, pixels, width, height,
				solo_canvas_piece_tile(piece->type),
				rect->x + cols[index] * tile_size,
				rect->y + rows[index] * tile_size, tile_size,
				255u, ghost);
		index++;
	}
}

/*
 * The atlas is read out of the visual once and kept as plain memory. Sampling
 * it through ncvisual_at_yx() cost one library call per output pixel, and a
 * board at this scale is over a million of them - that single call was most of
 * the frame time, and all of the input latency it produced.
 */
static void draw_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int tile, int x, int y, int size, unsigned opacity, bool ghost)
{
	const uint32_t *row;
	uint32_t source;
	unsigned alpha;
	int source_y;
	int draw_x;
	int draw_y;

	if (size <= 0 || ctx->mp_match_tile_atlas == NULL || tile < 0
		|| tile >= TILE_ATLAS_COUNT)
		return ;
	draw_y = 0;
	while (draw_y < size)
	{
		source_y = draw_y * TILE_SOURCE_SIZE / size;
		row = ctx->mp_match_tile_atlas + (size_t)(tile * TILE_SOURCE_STRIDE
				+ source_y) * TILE_SOURCE_SIZE;
		draw_x = 0;
		while (draw_x < size)
		{
			source = row[draw_x * TILE_SOURCE_SIZE / size];
			if (ghost)
				source = solo_canvas_ghost_tile_pixel(source,
					draw_x * TILE_SOURCE_SIZE / size, source_y);
			alpha = ncpixel_a(source) * opacity / 255u;
			put_pixel(pixels, width, height, x + draw_x, y + draw_y,
				(t_color){ncpixel_r(source), ncpixel_g(source),
				ncpixel_b(source)}, alpha);
			draw_x++;
		}
		draw_y++;
	}
}

/**
 * @brief Quantises Solo's danger fade into the steps the board is painted at.
 *
 * solo_game_danger_dim() is the shared signal: settled blocks in the top rows
 * enter danger at once and leaving it needs the stack to stay clear, so the
 * match and Solo agree about when a player is in trouble without either one
 * owning the rule.
 *
 * @return 0 when clear, up to MP_MATCH_DANGER_STEPS at full danger.
 */
static int danger_step(const t_solo_game *game)
{
	unsigned	dim;

	dim = solo_game_danger_dim(game);
	if (dim == 0 || SOLO_DANGER_DIM_MAX == 0)
		return (0);
	return ((int)((dim * MP_MATCH_DANGER_STEPS + SOLO_DANGER_DIM_MAX - 1)
			/ SOLO_DANGER_DIM_MAX));
}

/**
 * @brief The board's backing colour at a danger step.
 *
 * Solo darkens the scenery and leaves the playfield lit. The match cannot do
 * that - its scenery is a cell plane a full recompose owns - so it reddens the
 * playfield instead, which carries the same warning without touching any
 * surface outside the region that already redraws on every input.
 */
static t_color danger_backing(int step)
{
	static const t_color	peak = {74, 10, 20};
	t_color					out;

	if (step <= 0)
		return (g_dark);
	out.r = (unsigned char)(g_dark.r + (peak.r - g_dark.r) * step
			/ MP_MATCH_DANGER_STEPS);
	out.g = (unsigned char)(g_dark.g + (peak.g - g_dark.g) * step
			/ MP_MATCH_DANGER_STEPS);
	out.b = (unsigned char)(g_dark.b + (peak.b - g_dark.b) * step
			/ MP_MATCH_DANGER_STEPS);
	return (out);
}

/* The frame moves with the backing so the warning reads from the border too. */
static t_color danger_frame(int step, bool local)
{
	t_color	base;
	t_color	out;

	base = g_pink;
	if (!local)
		base = g_lavender;
	if (step <= 0)
		return (base);
	out.r = (unsigned char)(base.r + (g_red.r - base.r) * step
			/ MP_MATCH_DANGER_STEPS);
	out.g = (unsigned char)(base.g + (g_red.g - base.g) * step
			/ MP_MATCH_DANGER_STEPS);
	out.b = (unsigned char)(base.b + (g_red.b - base.b) * step
			/ MP_MATCH_DANGER_STEPS);
	return (out);
}

/**
 * @brief Bakes one tile at the live cell size over the board's own backing.
 *
 * The board is filled opaque before its cells are drawn, so a tile composited
 * over that same colour is what the blended result would have been - which is
 * what lets the copy replace the blend. Alpha comes out 255 for every pixel,
 * including the fully transparent ones, which land on the backing colour
 * exactly as put_pixel's early return left them.
 */
static void tile_cache_bake(t_render_ctx *ctx, int tile, int size, bool ghost,
	t_color backing)
{
	uint32_t	*out;
	uint32_t	source;
	unsigned	alpha;
	int			source_x;
	int			source_y;
	int			x;
	int			y;

	out = ctx->mp_match_tile_scaled + (size_t)((ghost ? TILE_ATLAS_COUNT : 0)
			+ tile) * (size_t)size * (size_t)size;
	y = 0;
	while (y < size)
	{
		source_y = y * TILE_SOURCE_SIZE / size;
		x = 0;
		while (x < size)
		{
			source_x = x * TILE_SOURCE_SIZE / size;
			source = ctx->mp_match_tile_atlas[(size_t)(tile
					* TILE_SOURCE_STRIDE + source_y) * TILE_SOURCE_SIZE
				+ source_x];
			if (ghost)
				source = solo_canvas_ghost_tile_pixel(source, source_x,
						source_y);
			alpha = ncpixel_a(source);
			out[(size_t)y * size + x] = ncpixel(
					(ncpixel_r(source) * alpha + backing.r * (255u - alpha))
					/ 255u,
					(ncpixel_g(source) * alpha + backing.g * (255u - alpha))
					/ 255u,
					(ncpixel_b(source) * alpha + backing.b * (255u - alpha))
					/ 255u);
			ncpixel_set_a(&out[(size_t)y * size + x], 255u);
			x++;
		}
		y++;
	}
}

/**
 * @brief Makes sure every board tile is baked at this cell size.
 *
 * The cell size only changes when the terminal is resized, so this is a
 * once-per-layout cost that replaces a per-pixel rescale on every cell of
 * every frame.
 */
static bool tile_cache_ensure(t_render_ctx *ctx, int size, t_color backing)
{
	uint32_t	key;
	size_t		count;
	int			tile;

	if (size <= 0 || ctx->mp_match_tile_atlas == NULL)
		return (false);
	key = ncpixel(backing.r, backing.g, backing.b);
	if (ctx->mp_match_tile_scaled != NULL
		&& ctx->mp_match_tile_scaled_size == size
		&& ctx->mp_match_tile_scaled_backing == key)
		return (true);
	count = (size_t)TILE_ATLAS_COUNT * 2u * (size_t)size * (size_t)size;
	if (ctx->mp_match_tile_scaled == NULL
		|| ctx->mp_match_tile_scaled_size != size)
	{
		free(ctx->mp_match_tile_scaled);
		ctx->mp_match_tile_scaled_size = 0;
		ctx->mp_match_tile_scaled = malloc(count
				* sizeof(*ctx->mp_match_tile_scaled));
		if (ctx->mp_match_tile_scaled == NULL)
			return (false);
	}
	tile = 0;
	while (tile < TILE_ATLAS_COUNT)
	{
		tile_cache_bake(ctx, tile, size, false, backing);
		tile_cache_bake(ctx, tile, size, true, backing);
		tile++;
	}
	ctx->mp_match_tile_scaled_size = size;
	ctx->mp_match_tile_scaled_backing = key;
	return (true);
}

/**
 * @brief Paints one baked board tile by copying its rows.
 *
 * Falls back to the blending path when the cache is not available, so a failed
 * allocation costs frame time rather than the picture.
 */
static void draw_board_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int tile, int x, int y, int size, bool ghost, t_color backing)
{
	const uint32_t	*source;
	int				span;
	int				rows;
	int				row;

	if (tile < 0 || tile >= TILE_ATLAS_COUNT
		|| !tile_cache_ensure(ctx, size, backing))
	{
		draw_tile(ctx, pixels, width, height, tile, x, y, size, 255, ghost);
		return ;
	}
	source = ctx->mp_match_tile_scaled + (size_t)((ghost ? TILE_ATLAS_COUNT : 0)
			+ tile) * (size_t)size * (size_t)size;
	span = min_int(size, width - x);
	rows = min_int(size, height - y);
	if (x < 0 || y < 0 || span <= 0 || rows <= 0)
		return ;
	row = 0;
	while (row < rows)
	{
		memcpy(&pixels[(size_t)(y + row) * width + x],
			source + (size_t)row * size, (size_t)span * sizeof(*pixels));
		row++;
	}
}

/**
 * @brief Reads the tetromino atlas out of its visual into plain memory.
 *
 * One pass over 16 by 180 pixels, done when the visual is loaded, so the
 * per-pixel path in draw_tile() is an array index.
 */
static bool load_tile_atlas(t_render_ctx *ctx)
{
	uint32_t pixel;
	int rows;
	int y;
	int x;

	if (ctx->mp_match_tile_atlas != NULL)
		return (true);
	rows = TILE_SOURCE_STRIDE * TILE_ATLAS_COUNT;
	ctx->mp_match_tile_atlas = malloc((size_t)rows * TILE_SOURCE_SIZE
			* sizeof(*ctx->mp_match_tile_atlas));
	if (ctx->mp_match_tile_atlas == NULL)
		return (false);
	y = 0;
	while (y < rows)
	{
		x = 0;
		while (x < TILE_SOURCE_SIZE)
		{
			pixel = 0;
			(void)ncvisual_at_yx(ctx->mp_match_tile_visual, (unsigned)y,
				(unsigned)x, &pixel);
			ctx->mp_match_tile_atlas[(size_t)y * TILE_SOURCE_SIZE + x] = pixel;
			x++;
		}
		y++;
	}
	return (true);
}

static void draw_opponent_region(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *rect,
	const t_mp_match_state *state, int first, int count)
{
	t_mp_rect board;
	int columns;
	int rows;
	int slot_width;
	int slot_height;
	int tile;
	int index;

	if (count <= 0 || rect->width <= 0 || rect->height <= 0)
		return ;
	opponent_grid(count, rect, &columns, &rows);
	slot_width = rect->width / columns;
	slot_height = rect->height / rows;
	tile = min_int((slot_width - 12) / BOARD_WIDTH,
		(slot_height - 28) / BOARD_HEIGHT);
	tile = max_int(1, tile);
	index = 0;
	while (index < count && first + index < APP_ROOM_MAX_PLAYERS - 1)
	{
		board.width = tile * BOARD_WIDTH;
		board.height = tile * BOARD_HEIGHT;
		board.x = rect->x + (index % columns) * slot_width
			+ (slot_width - board.width) / 2;
		board.y = rect->y + (index / columns) * slot_height
			+ max_int(22, (slot_height - board.height) / 2);
		draw_snapshot_board(ctx, pixels, width, height, &board,
			&state->opponents[first + index], first + index + 2,
			tile <= 2);
		index++;
	}
}

static void opponent_grid(int count, const t_mp_rect *rect,
	int *columns, int *rows)
{
	int candidate;
	int candidate_rows;
	int tile;
	int best;

	*columns = 1;
	*rows = count;
	best = 0;
	candidate = 1;
	while (candidate <= count)
	{
		candidate_rows = (count + candidate - 1) / candidate;
		tile = min_int((rect->width / candidate - 12) / BOARD_WIDTH,
			(rect->height / candidate_rows - 28) / BOARD_HEIGHT);
		if (tile > best)
		{
			best = tile;
			*columns = candidate;
			*rows = candidate_rows;
		}
		candidate++;
	}
}

static void draw_targeting(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_mp_match_state *state)
{
	const char *labels[4] = {"W KOs", "A RANDOMS", "S ATTACKERS", "D BADGES"};
	t_target_mode modes[4] = {TARGET_KO, TARGET_RANDOM,
		TARGET_ATTACKERS, TARGET_TOP_SCORE};
	t_mp_rect box;
	int box_width;
	int index;

	box_width = rect->width / 3;
	index = 0;
	while (index < 4)
	{
		if (index == 0)
			box = (t_mp_rect){rect->x + box_width, rect->y,
				box_width - 8, rect->height / 2 - 3};
		else
			box = (t_mp_rect){rect->x + (index - 1) * box_width,
				rect->y + rect->height / 2 + 3,
				box_width - 8, rect->height / 2 - 3};
		draw_squircle(pixels, width, height, &box,
			state->target_mode == modes[index] ? g_gold : g_lavender,
			state->target_mode == modes[index] ? 225 : 150);
		box.x += 6;
		box.width -= 12;
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, labels[index], &box,
			13, state->target_mode == modes[index] ? g_gold : g_lavender,
			true);
		index++;
	}
}

static void draw_squircle(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect, t_color edge, unsigned alpha)
{
	t_mp_rect inner;

	fill_squircle(pixels, width, height, rect, edge, alpha);
	inner = (t_mp_rect){rect->x + 3, rect->y + 3,
		rect->width - 6, rect->height - 6};
	fill_squircle(pixels, width, height, &inner, g_panel, 238);
}

static void fill_squircle(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect, t_color tint, unsigned alpha)
{
	t_mp_rect	middle;
	int			radius;

	radius = min_int(rect->height / 3, rect->width / 8);
	middle = (t_mp_rect){rect->x + radius, rect->y,
		rect->width - radius * 2, rect->height};
	mp_match_pixel_fill_rect(pixels, width, height, &middle, tint, alpha);
	middle = (t_mp_rect){rect->x, rect->y + radius,
		rect->width, rect->height - radius * 2};
	mp_match_pixel_fill_rect(pixels, width, height, &middle, tint, alpha);
	mp_match_pixel_draw_circle(pixels, width, height,
		rect->x + radius, rect->y + radius, radius, tint, alpha);
	mp_match_pixel_draw_circle(pixels, width, height,
		rect->x + rect->width - radius - 1, rect->y + radius,
		radius, tint, alpha);
	mp_match_pixel_draw_circle(pixels, width, height,
		rect->x + radius, rect->y + rect->height - radius - 1,
		radius, tint, alpha);
	mp_match_pixel_draw_circle(pixels, width, height,
		rect->x + rect->width - radius - 1,
		rect->y + rect->height - radius - 1, radius, tint, alpha);
}

static void draw_hud_backdrop(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect)
{
	t_mp_rect	inner;

	if (rect == NULL || rect->width < 8 || rect->height < 8)
		return ;
	fill_squircle(pixels, width, height, rect, g_lavender, 92);
	inner = (t_mp_rect){rect->x + 2, rect->y + 2,
		rect->width - 4, rect->height - 4};
	fill_squircle(pixels, width, height, &inner, g_dark, 188);
}

static void draw_result(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_match_state *state)
{
	t_mp_rect panel;
	t_mp_rect text;
	char line[MP_MATCH_STATUS_MAX];

	panel = (t_mp_rect){width / 2 - min_int(width / 3, 440),
		height / 2 - 130, min_int(width * 2 / 3, 880), 260};
	mp_match_pixel_draw_panel(pixels, width, height, &panel,
		state->result == MP_MATCH_RESULT_WON ? g_gold : g_red, 250);
	text = (t_mp_rect){panel.x + 30, panel.y + 62,
		panel.width - 60, 58};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height,
		mp_match_result_text(state, line, sizeof(line)), &text, 30,
		state->result == MP_MATCH_RESULT_WON ? g_gold : g_red, true);
	text.y += 88;
	text.height = 32;
	mp_match_pixel_draw_text_box(ctx, pixels, width, height,
		"ENTER OR ESC TO RETURN TO THE ROOM", &text, 15, g_cream, true);
}

static int min_int(int left, int right)
{
	return (left < right ? left : right);
}

static int max_int(int left, int right)
{
	return (left > right ? left : right);
}

static int clamp_int(int value, int minimum, int maximum)
{
	return (max_int(minimum, min_int(maximum, value)));
}

static void put_pixel(uint32_t *pixels, int width, int height, int x, int y,
	t_color tint, unsigned alpha)
{
	if (x < 0 || x >= width || y < 0 || y >= height || alpha == 0)
		return ;
	blend_pixel(&pixels[(size_t)y * width + x], tint, alpha);
}

static void blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha)
{
	unsigned old_alpha;
	unsigned out_alpha;
	unsigned red;
	unsigned green;
	unsigned blue;

	if (alpha >= 255u)
	{
		*pixel = ncpixel(tint.r, tint.g, tint.b);
		ncpixel_set_a(pixel, 255u);
		return ;
	}
	old_alpha = ncpixel_a(*pixel);
	out_alpha = alpha + old_alpha * (255u - alpha) / 255u;
	if (out_alpha == 0)
		return ;
	red = (tint.r * alpha + ncpixel_r(*pixel) * old_alpha
		* (255u - alpha) / 255u) / out_alpha;
	green = (tint.g * alpha + ncpixel_g(*pixel) * old_alpha
		* (255u - alpha) / 255u) / out_alpha;
	blue = (tint.b * alpha + ncpixel_b(*pixel) * old_alpha
		* (255u - alpha) / 255u) / out_alpha;
	*pixel = ncpixel(red, green, blue);
	ncpixel_set_a(pixel, out_alpha);
}

/**
 * @brief Names the effect currently riding on the player, or nothing.
 *
 * One at a time, worst first. Two effects at once is possible and rare, and a
 * line long enough to hold both would be a line nobody reads mid-piece.
 *
 * @param effects The counts the server sent.
 * @param out Destination buffer, emptied when nothing is active.
 * @param size Capacity of out.
 */
static void effect_line(const t_solo_effects *effects, char *out, size_t size)
{
	out[0] = '\0';
	if (effects->paralysis > 0)
		snprintf(out, size, "NO ROTATION  -  %d PIECES", effects->paralysis);
	else if (effects->inversion > 0)
		snprintf(out, size, "CONTROLS INVERTED  -  %d PIECES",
			effects->inversion);
	else if (effects->nue > 0)
		snprintf(out, size, "NO FAST DROP  -  %d PIECES", effects->nue);
	else if (effects->dark > 0)
		snprintf(out, size, "BLACKOUT  -  %d PIECES", effects->dark);
	else if (effects->fry > 0)
		snprintf(out, size, "%d ROWS BURN AT THE NEXT LOCK", effects->fry);
	else if (effects->thwack > 0)
		snprintf(out, size, "THWACK  -  %d PIECES", effects->thwack);
	else if (effects->pals > 0)
		snprintf(out, size, "PALS  -  GARBAGE LOWERS YOUR STACK");
	else if (effects->mirror > 0)
		snprintf(out, size, "MIRROR  -  THE NEXT ABILITY REBOUNDS");
}
