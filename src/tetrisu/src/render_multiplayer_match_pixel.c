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

static bool load_assets(t_render_ctx *ctx);
static bool refresh_background(t_render_ctx *ctx, bool rebuild);
static bool present_canvas(t_render_ctx *ctx, const uint32_t *pixels,
				int width, int height);
static bool create_region_plane(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *region,
				struct ncplane **slot);
static void destroy_region_planes(t_render_ctx *ctx);
static uint64_t match_signature(const t_mp_match_state *state,
				int width, int height);
static uint64_t static_signature(const t_mp_match_state *state,
				int width, int height);
static uint64_t game_signature(const t_solo_game *game);
static uint64_t local_board_signature(const t_solo_game *game,
				bool include_score);
static uint64_t loadout_signature(const t_mp_match_state *state);
static uint64_t hud_signature(const t_mp_match_state *state);
static uint64_t opponents_signature(const t_mp_match_state *state,
				int first, int count);
static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size);
static uint32_t *new_canvas(t_render_ctx *ctx, int width, int height);
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
static void draw_double_caption(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *board,
				const char *name, uint64_t points, int charge, bool local);
static void draw_loadout(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect,
				const t_mp_match_state *state, bool portrait);
static void draw_ability_bar(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_match_pixel_layout *layout,
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
static void draw_piece_cells(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const t_mp_rect *rect, const t_piece *piece,
				bool ghost);
static void draw_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int tile, int x, int y, int size, unsigned opacity,
				bool ghost);
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
 * The font mask and tetromino atlas are the same assets used by Solo. Static
 * chrome stays on the screen plane while board, opponent, loadout, and HUD
 * regions are rewritten independently, so a movement key never retransmits
 * the complete terminal bitmap.
 */
bool render_multiplayer_match_pixel_show(t_render_ctx *ctx,
	const t_mp_match_state *state, bool rebuild_background)
{
	uint32_t *pixels;
	uint64_t signature;
	t_mp_match_pixel_layout layout;
	t_mp_rect region;
	int width;
	int height;
	int opponents;
	int left_count;
	bool changed;
	int piece_changed;

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
	signature = match_signature(state, width, height);
	if (state->phase != MP_MATCH_PLAYING && !rebuild_background
		&& ctx->screen_plane != NULL && ctx->mp_match_signature == signature)
		return (true);
	if (state->phase == MP_MATCH_PLAYING && !rebuild_background
		&& ctx->screen_plane != NULL
		&& ctx->mp_match_static_signature == static_signature(state,
			width, height))
	{
		pixels = NULL;
		changed = false;
		if (ctx->mp_match_local_signature != local_board_signature(
				&state->local_game, state->mode == APP_GAME_MODE_DOUBLE))
		{
			pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			mp_match_piece_planes_destroy(ctx);
			draw_game_board(ctx, pixels, width, height, &layout.local_board,
				&state->local_game, state->mode == APP_GAME_MODE_BATTLE_ROYALE
					? "" : "YOUR BOARD", true);
			if (state->mode == APP_GAME_MODE_DOUBLE)
				draw_double_caption(ctx, pixels, width, height,
					&layout.local_board, state->profile.username,
					state->local_game.scoring.total,
					state->local_game.crystal_charge, true);
			region = (t_mp_rect){layout.local_board.x - 8,
				layout.local_board.y - 8, layout.local_board.width + 16,
				layout.local_board.height + (state->mode
					== APP_GAME_MODE_DOUBLE ? 82 : 16)};
			if (!create_region_plane(ctx, pixels, width, height, &region,
					&ctx->mp_match_local_plane))
				return (free(pixels), false);
			ctx->mp_match_local_signature = local_board_signature(
				&state->local_game, state->mode == APP_GAME_MODE_DOUBLE);
			changed = true;
		}
		if (state->mode == APP_GAME_MODE_DOUBLE
			&& ctx->mp_match_opponent_signature
				!= game_signature(&state->opponent_game))
		{
			if (pixels == NULL)
				pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			draw_game_board(ctx, pixels, width, height, &layout.opponent_board,
				&state->opponent_game, state->opponent_name, false);
			draw_double_caption(ctx, pixels, width, height,
				&layout.opponent_board, state->opponent_name,
				state->opponent_game.scoring.total, state->opponent_charge, false);
			region = (t_mp_rect){layout.opponent_board.x - 8,
				layout.opponent_board.y - 8, layout.opponent_board.width + 16,
				layout.opponent_board.height + 82};
			if (!create_region_plane(ctx, pixels, width, height, &region,
					&ctx->mp_match_opponent_plane))
				return (free(pixels), false);
			ctx->mp_match_opponent_signature = game_signature(&state->opponent_game);
			changed = true;
		}
		opponents = clamp_int(state->players_total - 1, 0,
			APP_ROOM_MAX_PLAYERS - 1);
		left_count = (opponents + 1) / 2;
		if (state->mode == APP_GAME_MODE_BATTLE_ROYALE
			&& ctx->mp_match_left_signature != opponents_signature(state,
				0, left_count))
		{
			if (pixels == NULL)
				pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			draw_opponent_region(ctx, pixels, width, height,
				&layout.left_opponents, state, 0, left_count);
			if (!create_region_plane(ctx, pixels, width, height,
					&layout.left_opponents, &ctx->mp_match_left_plane))
				return (free(pixels), false);
			ctx->mp_match_left_signature = opponents_signature(state, 0, left_count);
			changed = true;
		}
		if (state->mode == APP_GAME_MODE_BATTLE_ROYALE
			&& ctx->mp_match_right_signature != opponents_signature(state,
				left_count, opponents - left_count))
		{
			if (pixels == NULL)
				pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			draw_opponent_region(ctx, pixels, width, height,
				&layout.right_opponents, state, left_count,
				opponents - left_count);
			if (!create_region_plane(ctx, pixels, width, height,
					&layout.right_opponents, &ctx->mp_match_right_plane))
				return (free(pixels), false);
			ctx->mp_match_right_signature = opponents_signature(state,
				left_count, opponents - left_count);
			changed = true;
		}
		if (ctx->mp_match_loadout_signature != loadout_signature(state))
		{
			if (pixels == NULL)
				pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			draw_loadout(ctx, pixels, width, height, &layout.loadout, state, true);
			draw_ability_bar(ctx, pixels, width, height, &layout, state);
			if (!create_region_plane(ctx, pixels, width, height, &layout.loadout,
					&ctx->mp_match_loadout_plane))
				return (free(pixels), false);
			if (!create_region_plane(ctx, pixels, width, height,
					&layout.ability_bar, &ctx->mp_match_ability_plane))
				return (free(pixels), false);
			ctx->mp_match_loadout_signature = loadout_signature(state);
			changed = true;
		}
		if (ctx->mp_match_hud_signature != hud_signature(state))
		{
			if (pixels == NULL)
				pixels = new_canvas(ctx, width, height);
			if (pixels == NULL)
				return (false);
			draw_match_hud(ctx, pixels, width, height, &layout, state);
			if (!create_region_plane(ctx, pixels, width, height, &layout.hud,
					&ctx->mp_match_hud_plane))
				return (free(pixels), false);
			ctx->mp_match_hud_signature = hud_signature(state);
			changed = true;
		}
		piece_changed = mp_match_piece_planes_update(ctx, &layout,
			&state->local_game);
		if (piece_changed < 0)
			return (free(pixels), false);
		changed |= piece_changed != 0;
		free(pixels);
		if (!changed)
			return (true);
		render_notification_raise(ctx);
		return (notcurses_render(ctx->nc) == 0);
	}
	destroy_region_planes(ctx);
	pixels = new_canvas(ctx, width, height);
	if (pixels == NULL)
		return (false);
	if (state->phase == MP_MATCH_CHARACTER_SELECT)
		compose_selection(ctx, pixels, width, height, state);
	else if (state->mode == APP_GAME_MODE_DOUBLE)
		compose_double(ctx, pixels, width, height, state);
	else
		compose_battle(ctx, pixels, width, height, state);
	if (state->phase == MP_MATCH_FINISHED)
		draw_result(ctx, pixels, width, height, state);
	if (!present_canvas(ctx, pixels, width, height))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	if (state->phase == MP_MATCH_PLAYING)
	{
		piece_changed = mp_match_piece_planes_update(ctx, &layout,
			&state->local_game);
		if (piece_changed < 0)
			return (false);
	}
	else
		mp_match_piece_planes_destroy(ctx);
	ctx->mp_match_signature = signature;
	ctx->mp_match_static_signature = state->phase == MP_MATCH_PLAYING
		? static_signature(state, width, height) : 0;
	ctx->mp_match_local_signature = local_board_signature(&state->local_game,
		state->mode == APP_GAME_MODE_DOUBLE);
	ctx->mp_match_opponent_signature = game_signature(&state->opponent_game);
	ctx->mp_match_loadout_signature = loadout_signature(state);
	ctx->mp_match_hud_signature = hud_signature(state);
	opponents = clamp_int(state->players_total - 1, 0,
		APP_ROOM_MAX_PLAYERS - 1);
	left_count = (opponents + 1) / 2;
	ctx->mp_match_left_signature = opponents_signature(state, 0, left_count);
	ctx->mp_match_right_signature = opponents_signature(state, left_count,
		opponents - left_count);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

void render_multiplayer_match_pixel_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->mp_match_tile_visual != NULL)
		ncvisual_destroy(ctx->mp_match_tile_visual);
	if (ctx->mp_match_portrait_visual != NULL)
		ncvisual_destroy(ctx->mp_match_portrait_visual);
	ctx->mp_match_tile_visual = NULL;
	ctx->mp_match_portrait_visual = NULL;
	ctx->mp_match_portrait_source[0] = '\0';
	mp_match_piece_planes_destroy(ctx);
	destroy_region_planes(ctx);
	ctx->mp_match_signature = 0;
	ctx->mp_match_static_signature = 0;
	ctx->mp_match_local_signature = 0;
	ctx->mp_match_opponent_signature = 0;
	ctx->mp_match_left_signature = 0;
	ctx->mp_match_right_signature = 0;
	ctx->mp_match_loadout_signature = 0;
	ctx->mp_match_hud_signature = 0;
}

static bool load_assets(t_render_ctx *ctx)
{
	if (ctx->mp_font_visual == NULL)
		ctx->mp_font_visual = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (ctx->mp_match_tile_visual == NULL)
		ctx->mp_match_tile_visual = ncvisual_from_file(DEFAULT_TILE_PATH);
	return (ctx->mp_font_visual != NULL && ctx->mp_match_tile_visual != NULL);
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

static bool present_canvas(t_render_ctx *ctx, const uint32_t *pixels,
	int width, int height)
{
	ncplane_options options;
	struct ncplane *plane;

	if (render_plane_geometry_matches(ctx->screen_plane, ctx->bg_row,
			ctx->bg_col, (unsigned)ctx->bg_rows, (unsigned)ctx->bg_cols))
		return (render_plane_blit_rgba(ctx, ctx->screen_plane, pixels,
				width, height, width));
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row;
	options.x = ctx->bg_col;
	options.rows = ctx->bg_rows;
	options.cols = ctx->bg_cols;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL || !render_plane_blit_rgba(ctx, plane, pixels,
			width, height, width))
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
	struct ncplane **planes[7];
	int index;

	if (ctx == NULL)
		return ;
	planes[0] = &ctx->mp_match_local_plane;
	planes[1] = &ctx->mp_match_opponent_plane;
	planes[2] = &ctx->mp_match_left_plane;
	planes[3] = &ctx->mp_match_right_plane;
	planes[4] = &ctx->mp_match_loadout_plane;
	planes[5] = &ctx->mp_match_hud_plane;
	planes[6] = &ctx->mp_match_ability_plane;
	index = 0;
	while (index < 7)
	{
		if (*planes[index] != NULL)
			ncplane_destroy(*planes[index]);
		*planes[index] = NULL;
		index++;
	}
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

static uint64_t game_signature(const t_solo_game *game)
{
	uint64_t hash;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &game->board, sizeof(game->board));
	hash = hash_bytes(hash, &game->active, sizeof(game->active));
	hash = hash_bytes(hash, &game->phase, sizeof(game->phase));
	hash = hash_bytes(hash, &game->scoring, sizeof(game->scoring));
	return (hash_bytes(hash, &game->crystal_charge,
			sizeof(game->crystal_charge)));
}

static uint64_t local_board_signature(const t_solo_game *game,
	bool include_score)
{
	uint64_t	hash;
	int			countdown;

	hash = 1469598103934665603ULL;
	hash = hash_bytes(hash, &game->board, sizeof(game->board));
	hash = hash_bytes(hash, &game->phase, sizeof(game->phase));
	hash = hash_bytes(hash, &game->countdown_active,
		sizeof(game->countdown_active));
	countdown = solo_game_countdown_value(game);
	hash = hash_bytes(hash, &countdown, sizeof(countdown));
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

static uint32_t *new_canvas(t_render_ctx *ctx, int width, int height)
{
	const uint32_t *background;
	uint32_t *pixels;
	size_t bytes;
	int source_width;
	int source_height;

	if (!solo_canvas_buffer_bytes(width, height, &bytes))
		return (NULL);
	pixels = malloc(bytes);
	if (pixels == NULL)
		return (NULL);
	background = render_backdrop_pixels(ctx, &source_width, &source_height);
	if (background != NULL && source_width == width && source_height == height)
		memcpy(pixels, background, bytes);
	else
		memset(pixels, 0, bytes);
	return (pixels);
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
	if (character != NULL && mp_match_pixel_load_portrait(ctx, character->portrait_asset))
	{
		t_mp_rect image = {portrait.x + 12, portrait.y + 12,
			portrait.width - 24, portrait.height - 24};
		mp_match_pixel_draw_visual(pixels, width, height,
			ctx->mp_match_portrait_visual, &image);
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
	draw_double_caption(ctx, pixels, width, height, &layout.local_board,
		state->profile.username, state->local_game.scoring.total,
		state->local_game.crystal_charge, true);
	draw_double_caption(ctx, pixels, width, height, &layout.opponent_board,
		state->opponent_name, state->opponent_game.scoring.total,
		state->opponent_charge, false);
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
	uint64_t points, int charge, bool local)
{
	t_mp_rect text;
	char line[APP_ABILITY_TEXT_MAX];

	if (local)
		snprintf(line, sizeof(line), "%s   %010" PRIu64 " PTS   POWER %d",
			name != NULL && name[0] != '\0' ? name : "YOU", points, charge);
	else
		snprintf(line, sizeof(line), "%s   %010" PRIu64 " PTS   POWER %d",
			name != NULL && name[0] != '\0' ? name : "OPPONENT", points, charge);
	text = (t_mp_rect){board->x - 24, board->y + board->height + 20,
		board->width + 48, 42};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, line, &text, 17,
		local ? g_gold : g_lavender, true);
}

static void draw_loadout(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_mp_match_state *state,
	bool portrait)
{
	const t_app_catalogue_item_view_model *character;
	t_mp_match_pixel_layout layout;
	t_mp_rect box;
	int index;

	character = mp_match_selected_character(state);
	mp_match_pixel_layout_build(state->mode, width, height, &layout);
	mp_match_pixel_draw_panel(pixels, width, height, rect, g_gold, 238);
	box = (t_mp_rect){rect->x + 18, rect->y + 16,
		rect->width - 36, 38};
	mp_match_pixel_draw_text_box(ctx, pixels, width, height, "FIGHTER",
		&box, 19, g_gold, true);
	if (portrait && character != NULL
		&& mp_match_pixel_load_portrait(ctx, character->portrait_asset))
		mp_match_pixel_draw_visual(pixels, width, height,
			ctx->mp_match_portrait_visual, &layout.portrait);
	if (character != NULL)
	{
		box = (t_mp_rect){rect->x + 10,
			layout.portrait.y + layout.portrait.height + 16,
			rect->width - 20, 38};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, character->name,
			&box, 19, g_pink, true);
	}
	if (state->hovered_ability == 0)
		draw_hold_next(ctx, pixels, width, height,
			&layout.ability_popover, &state->local_game);
	if (character != NULL && state->hovered_ability >= 1
		&& state->hovered_ability <= APP_CHARACTER_ABILITY_COUNT)
	{
		index = state->hovered_ability - 1;
		box = layout.ability_popover;
		mp_match_pixel_draw_panel(pixels, width, height, &box, g_gold, 248);
		box.x += 14;
		box.y += 12;
		box.width -= 28;
		box.height = 28;
		mp_match_pixel_draw_text_box(ctx, pixels, width, height,
			character->abilities[index].name, &box, 14, g_gold, false);
		box.y += 34;
		box.height = 54;
		mp_match_pixel_draw_text_box(ctx, pixels, width, height,
			character->abilities[index].description, &box, 10, g_cream, false);
	}
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
	tile = min_int((slot->width - 12) / (max_col - min_col + 1),
		(slot->height - 10) / (max_row - min_row + 1));
	tile = min_int(tile, TILE_SOURCE_SIZE);
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

static void draw_game_board(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const t_mp_rect *rect, const t_solo_game *game,
	const char *label, bool local)
{
	t_mp_rect frame;
	t_mp_rect title;
	t_piece ghost;
	char countdown[4];
	int countdown_value;

	frame = (t_mp_rect){rect->x - 7, rect->y - 7,
		rect->width + 14, rect->height + 14};
	mp_match_pixel_draw_panel(pixels, width, height, &frame,
		local ? g_pink : g_lavender, 248);
	mp_match_pixel_fill_rect(pixels, width, height, rect, g_dark, 255);
	ghost = solo_game_ghost(game);
	draw_board_cells(ctx, pixels, width, height, rect, &game->board,
		game->phase == SOLO_ACTIVE && (!local
			|| !render_pixel_planes_reliable(ctx)) && !game->countdown_active
			? &game->active : NULL,
		game->phase == SOLO_ACTIVE && (!local
			|| !render_pixel_planes_reliable(ctx)) && !game->countdown_active
			? &ghost : NULL);
	countdown_value = local ? solo_game_countdown_value(game) : -1;
	if (countdown_value >= 0)
	{
		if (countdown_value == 0)
			snprintf(countdown, sizeof(countdown), "GO");
		else
			snprintf(countdown, sizeof(countdown), "%d", countdown_value);
		title = (t_mp_rect){rect->x + rect->width / 4,
			rect->y + rect->height / 2 - 54, rect->width / 2, 108};
		mp_match_pixel_draw_text_box(ctx, pixels, width, height, countdown, &title,
			76, g_gold, true);
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

static void draw_tile(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int tile, int x, int y, int size, unsigned opacity, bool ghost)
{
	uint32_t source;
	t_color tint;
	unsigned alpha;
	int source_x;
	int source_y;
	int draw_x;
	int draw_y;

	if (size <= 0)
		return ;
	draw_y = 0;
	while (draw_y < size)
	{
		source_y = draw_y * TILE_SOURCE_SIZE / size;
		draw_x = 0;
		while (draw_x < size)
		{
			source_x = draw_x * TILE_SOURCE_SIZE / size;
			if (ncvisual_at_yx(ctx->mp_match_tile_visual,
					(unsigned)(tile * TILE_SOURCE_STRIDE + source_y),
					(unsigned)source_x, &source) >= 0)
			{
				if (ghost)
					source = solo_canvas_ghost_tile_pixel(source,
						source_x, source_y);
				alpha = ncpixel_a(source) * opacity / 255u;
				tint = (t_color){ncpixel_r(source), ncpixel_g(source),
					ncpixel_b(source)};
				put_pixel(pixels, width, height, x + draw_x, y + draw_y,
					tint, alpha);
			}
			draw_x++;
		}
		draw_y++;
	}
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
