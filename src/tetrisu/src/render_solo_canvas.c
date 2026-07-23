#include "tetrisu.h"

// Static Variables
static const color_t	g_white = {250, 245, 250};
static const color_t	g_pink = {255, 112, 190};
static const color_t	g_purple = {112, 62, 145};
static const color_t	g_dark = {20, 8, 28};
static const color_t	g_ghost = {255, 255, 255};
static const color_t	g_playfield = {7, 13, 23};
static const color_t	g_panel = {11, 23, 34};

// Static Functions
static bool	pixel_asset_load(const char *path, pixel_asset_t *asset);
static bool	pixel_asset_load_sized(const char *path, int width, int height,
	pixel_asset_t *asset);
static void	pixel_asset_destroy(pixel_asset_t *asset);
static void	repair_background_alpha(pixel_asset_t *asset);
static bool	asset_dimensions_are(const pixel_asset_t *asset, int width,
	int height);
static void	align_authored_hud(pixel_asset_t *hud);
static void	blit_asset_scaled(uint32_t *canvas, int canvas_width,
	int canvas_height, const pixel_asset_t *asset, int dest_x, int dest_y,
	int dest_width, int dest_height, unsigned opacity);
static void	put_pixel_sized(uint32_t *canvas, int canvas_width,
	int canvas_height, int x, int y, uint32_t pixel);
static uint32_t	blend_pixel(uint32_t below, uint32_t above);
static uint32_t	with_opacity(uint32_t pixel, unsigned opacity);
static void	draw_rect(uint32_t *canvas, int x, int y, int width, int height,
	uint32_t pixel);
static void	put_pixel(uint32_t *canvas, int x, int y, uint32_t pixel);
static uint32_t	make_pixel(color_t color, unsigned alpha);
static uint32_t	make_ghost_pixel(uint32_t source, unsigned blend);
static void	draw_next_queue(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game);
static void	draw_preview_piece(uint32_t *canvas,
	const solo_render_t *solo, t_piece_type type, int slot_x, int slot_y,
	int slot_width, int slot_height, unsigned opacity, int forced_tile);
static void	draw_tile(uint32_t *canvas, const solo_render_t *solo,
	int tile_index, int x, int y, int size, unsigned opacity, bool outline_only);
static void	draw_crystal_meter(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game);
static void	draw_ability_marker(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game, solo_ability_t ability);
static void	draw_filled_circle(uint32_t *canvas, int center_x, int center_y,
	int radius, uint32_t pixel);
static void	draw_circle_ring(uint32_t *canvas, int center_x, int center_y,
	int radius, int thickness, uint32_t pixel);
static void	draw_score_panel(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game);
static bool	draw_ability_message(uint32_t *canvas,
	const solo_render_t *solo, const solo_game_t *game);
static void	draw_text_centered(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int center_x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint);
static int	text_width(const char *text, int glyph_width, int spacing);
static void	draw_text_shadowed(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint);
static void	draw_text(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint, unsigned opacity);
static void	draw_mask(uint32_t *canvas, const uint32_t *mask,
	int mask_width, int source_x, int source_y, int source_width,
	int source_height, int dest_x, int dest_y, int dest_width, int dest_height,
	color_t tint, unsigned opacity);
static void	draw_score_number(uint32_t *canvas, const solo_render_t *solo,
	uint64_t score);
static void	draw_numbers_centered_fit(uint32_t *canvas,
	const solo_render_t *solo, const char *text, int y, color_t tint);
static void	draw_numbers_shadowed(uint32_t *canvas,
	const solo_render_t *solo, const char *text, int x, int y,
	int glyph_width, int glyph_height, int spacing, color_t tint);
static void	draw_numbers(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint, unsigned opacity);
static int	number_glyph_index(char c);
static void	draw_stat_line(uint32_t *canvas, const solo_render_t *solo,
	const char *label, int value, int y);
static const char	*clear_name(const solo_game_t *game);
static void	reset_board_frame(solo_render_t *solo);
static void	draw_settled_board(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game);
static void	draw_piece(uint32_t *canvas, const solo_render_t *solo,
	const t_piece *piece, unsigned opacity, bool ghost);
static void	draw_overlays(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game);
static void	draw_outline(uint32_t *canvas, int x, int y, int width,
	int height, int thickness, uint32_t pixel);

/**
 * @brief Loads and composes all authored Solo canvas assets.
 *
 * Images are decoded into owned RGBA buffers, validated against their asset
 *   contracts, and composed once into static and mutable canvases.
 *
 * @param solo Pointer to the Solo render state.
 * @return true when every asset and allocation succeeds, otherwise false.
 */
bool	solo_canvas_load(solo_render_t *solo)
{
	pixel_asset_t	background;
	pixel_asset_t	hud;
	pixel_asset_t	mirurun;
	pixel_asset_t	tiles;
	pixel_asset_t	font;
	pixel_asset_t	numbers;
	size_t			canvas_bytes;
	bool			loaded;

	memset(&background, 0, sizeof(background));
	memset(&hud, 0, sizeof(hud));
	memset(&mirurun, 0, sizeof(mirurun));
	memset(&tiles, 0, sizeof(tiles));
	memset(&font, 0, sizeof(font));
	memset(&numbers, 0, sizeof(numbers));
	loaded = pixel_asset_load(SOLO_BACKGROUND_PATH, &background);
	if (!loaded)
		solo_canvas_set_error(solo, "Could not load Solo background",
			SOLO_BACKGROUND_PATH);
	if (loaded)
		repair_background_alpha(&background);
	if (loaded && !pixel_asset_load(DEFAULT_HUD_PATH, &hud))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Could not load HUD", DEFAULT_HUD_PATH);
	}
	else if (loaded && !asset_dimensions_are(&hud, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT))
	{
		loaded = false;
		solo_canvas_set_error(solo, "HUD must be 512x384", DEFAULT_HUD_PATH);
	}
	if (loaded && !pixel_asset_load(DEFAULT_MIRURUN_PATH, &mirurun))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Could not load Mirurun",
			DEFAULT_MIRURUN_PATH);
	}
	if (loaded && !pixel_asset_load(DEFAULT_TILE_PATH, &tiles))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Could not load tile atlas",
			DEFAULT_TILE_PATH);
	}
	if (loaded && !asset_dimensions_are(&tiles, TILE_SOURCE_SIZE,
			TILE_SOURCE_STRIDE * TILE_ATLAS_COUNT))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Tile atlas must be 16x180",
			DEFAULT_TILE_PATH);
	}
	if (loaded && !pixel_asset_load(SHARED_FONT_MASK_PATH, &font))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Could not load font mask",
			SHARED_FONT_MASK_PATH);
	}
	if (loaded && !asset_dimensions_are(&font,
			FONT_COLUMNS * FONT_GLYPH_WIDTH, FONT_ROWS * FONT_GLYPH_HEIGHT))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Font mask must be 128x96",
			SHARED_FONT_MASK_PATH);
	}
	if (loaded && !pixel_asset_load(SHARED_NUMBERS_MASK_PATH, &numbers))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Could not load number mask",
			SHARED_NUMBERS_MASK_PATH);
	}
	if (loaded && !asset_dimensions_are(&numbers,
			NUMBER_GLYPH_COUNT * NUMBER_SLOT_WIDTH, NUMBER_GLYPH_HEIGHT))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Number mask must be 140x16",
			SHARED_NUMBERS_MASK_PATH);
	}
	if (loaded && !solo_canvas_buffer_bytes(SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, &canvas_bytes))
	{
		loaded = false;
		solo_canvas_set_error(solo, "Solo canvas dimensions overflow", NULL);
	}
	if (loaded)
	{
		solo->static_pixels = malloc(canvas_bytes);
		solo->frame_pixels = malloc(canvas_bytes);
		if (solo->static_pixels == NULL || solo->frame_pixels == NULL)
		{
			loaded = false;
			solo_canvas_set_error(solo, "Could not allocate Solo canvas", NULL);
		}
	}
	if (loaded)
	{
		align_authored_hud(&hud);
		memset(solo->static_pixels, 0, canvas_bytes);
		blit_asset_scaled(solo->static_pixels, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, &background, 0, 0, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, 255);
		/* AI-assisted: opaque play surfaces prevent detailed scenery from
		 * competing with falling pieces and tiny HUD glyphs. */
		draw_rect(solo->static_pixels, HUD_NEXT_X, HUD_NEXT_Y,
			HUD_NEXT_WIDTH, HUD_NEXT_HEIGHT, make_pixel(g_panel, 248));
		draw_rect(solo->static_pixels, HUD_HOLD_X, HUD_HOLD_Y,
			HUD_HOLD_WIDTH, HUD_HOLD_HEIGHT, make_pixel(g_panel, 248));
		draw_rect(solo->static_pixels, HUD_METER_X, HUD_METER_Y,
			HUD_METER_WIDTH, HUD_METER_HEIGHT, make_pixel(g_panel, 255));
		draw_rect(solo->static_pixels, HUD_MIRURUN_X, HUD_MIRURUN_Y,
			HUD_MIRURUN_SIZE, HUD_MIRURUN_SIZE, make_pixel(g_panel, 220));
		draw_rect(solo->static_pixels, HUD_SCORE_X, HUD_SCORE_Y,
			HUD_SCORE_WIDTH, HUD_MIRURUN_SIZE, make_pixel(g_panel, 250));
		/* The PNG owns every frame pixel; code supplies only the dark interior. */
		draw_rect(solo->static_pixels, HUD_BOARD_X, HUD_BOARD_Y,
			HUD_NEXT_WIDTH, HUD_METER_HEIGHT, make_pixel(g_playfield, 255));
		draw_rect(solo->static_pixels, HUD_CONTROLS_X, HUD_CONTROLS_Y,
			HUD_CONTROLS_WIDTH, SOLO_CANVAS_HEIGHT - HUD_CONTROLS_Y,
			make_pixel(g_playfield, 248));
		blit_asset_scaled(solo->static_pixels, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, &hud, 0, 0, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, 255);
		blit_asset_scaled(solo->static_pixels, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, &mirurun, HUD_MIRURUN_X, HUD_MIRURUN_Y,
			HUD_MIRURUN_SIZE, HUD_MIRURUN_SIZE, 255);
		memcpy(solo->frame_pixels, solo->static_pixels, canvas_bytes);
		solo->tile_pixels = tiles.pixels;
		solo->tile_width = tiles.width;
		tiles.pixels = NULL;
		solo->font_pixels = font.pixels;
		solo->font_width = font.width;
		font.pixels = NULL;
		solo->number_pixels = numbers.pixels;
		solo->number_width = numbers.width;
		numbers.pixels = NULL;
	}
	pixel_asset_destroy(&background);
	pixel_asset_destroy(&hud);
	pixel_asset_destroy(&mirurun);
	pixel_asset_destroy(&tiles);
	pixel_asset_destroy(&font);
	pixel_asset_destroy(&numbers);
	return (loaded);
}

/**
 * @brief Maps a tetromino type to its Guideline-ordered atlas entry.
 *
 * The atlas order is I, J, L, O, S, Z, T followed by utility animation tiles.
 *
 * @param type Tetromino type to map.
 * @return Tile-atlas index for the piece type.
 */
int	solo_canvas_piece_tile(t_piece_type type)
{
	if (type == PIECE_I)
		return (0);
	if (type == PIECE_J)
		return (1);
	if (type == PIECE_L)
		return (2);
	if (type == PIECE_O)
		return (3);
	if (type == PIECE_S)
		return (4);
	if (type == PIECE_Z)
		return (5);
	if (type == PIECE_T)
		return (6);
	return (TILE_GARBAGE);
}

/**
 * @brief Converts one tile pixel to authored ghost shading.
 *
 * Outline and interior pixels use different preblended strengths so terminal
 *   alpha quantization cannot hide the landing projection.
 *
 * @param pixel Source RGBA pixel.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @return Opaque preblended ghost pixel, or transparent zero for empty source
 *   pixels.
 */
uint32_t	solo_canvas_ghost_tile_pixel(uint32_t pixel, int x, int y)
{
	if (x >= 3 && x < 13 && y >= 3 && y < 13)
		return (make_ghost_pixel(pixel, GHOST_INTERIOR_BLEND));
	return (make_ghost_pixel(pixel, GHOST_OUTLINE_BLEND));
}

/**
 * @brief Calculates a checked RGBA allocation size.
 *
 * Both pixel-count multiplication and byte-size multiplication are guarded
 *   against size overflow.
 *
 * @param width Pixel width.
 * @param height Pixel height.
 * @param bytes Output checked allocation size.
 * @return true when dimensions are valid and fit in `size_t`, otherwise false.
 */
bool	solo_canvas_buffer_bytes(int width, int height, size_t *bytes)
{
	size_t	pixel_count;

	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height)
		return (false);
	pixel_count = (size_t)width * (size_t)height;
	if (pixel_count > SIZE_MAX / sizeof(uint32_t))
		return (false);
	*bytes = pixel_count * sizeof(uint32_t);
	return (true);
}

/**
 * @brief Stores a bounded Solo renderer error message.
 *
 * An optional asset path is appended without exposing allocation or ownership
 *   changes.
 *
 * @param solo Pointer to the Solo render state.
 * @param message Human-readable error message.
 * @param path Optional asset path.
 */
void	solo_canvas_set_error(solo_render_t *solo, const char *message,
	const char *path)
{
	if (path == NULL)
		snprintf(solo->asset_error, sizeof(solo->asset_error), "%s", message);
	else
		snprintf(solo->asset_error, sizeof(solo->asset_error), "%s: %s",
			message, path);
}

/**
 * @brief Recomposes every dynamic HUD element into the frame canvas.
 *
 * The static canvas is restored before drawing previews, crystal charge, and
 *   score text.
 *
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
void	solo_canvas_compose_hud(solo_render_t *solo,
	const solo_game_t *game)
{
	memcpy(solo->frame_pixels, solo->static_pixels,
		(size_t)SOLO_CANVAS_WIDTH * SOLO_CANVAS_HEIGHT
		* sizeof(*solo->frame_pixels));
	draw_next_queue(solo->frame_pixels, solo, game);
	draw_crystal_meter(solo->frame_pixels, solo, game);
	draw_score_panel(solo->frame_pixels, solo, game);
}

/**
 * @brief Recomposes the opaque board region for fallback terminals.
 *
 * The board background is restored before settled cells, ghost, active piece,
 *   and pause or top-out overlays are drawn.
 *
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
void	solo_canvas_compose_board(solo_render_t *solo,
	const solo_game_t *game)
{
	t_piece	ghost;

	reset_board_frame(solo);
	draw_settled_board(solo->frame_pixels, solo, game);
	if (game->phase == SOLO_ACTIVE)
	{
		ghost = solo_game_ghost(game);
		draw_piece(solo->frame_pixels, solo, &ghost, 255u, true);
		draw_piece(solo->frame_pixels, solo, &game->active, 255u, false);
	}
	draw_overlays(solo->frame_pixels, solo, game);
}

/**
 * @brief Decodes an image at its authored dimensions.
 *
 * The sized loader is reused with no requested resize.
 *
 * @param path Optional asset path.
 * @param asset Pointer to the decoded pixel asset.
 * @return true on complete decode, otherwise false.
 */
static bool	pixel_asset_load(const char *path, pixel_asset_t *asset)
{
	return (pixel_asset_load_sized(path, 0, 0, asset));
}

/**
 * @brief Decodes an image and optionally resizes it without interpolation.
 *
 * Notcurses visual pixels are copied into caller-owned memory after checked
 *   geometry and allocation validation.
 *
 * @param path Optional asset path.
 * @param width Pixel width.
 * @param height Pixel height.
 * @param asset Pointer to the decoded pixel asset.
 * @return true on complete decode, otherwise false with no owned pixels left
 *   behind.
 */
static bool	pixel_asset_load_sized(const char *path, int width, int height,
	pixel_asset_t *asset)
{
	struct ncvisual	*ncv;
	ncvgeom			geom;
	size_t			pixel_bytes;
	int				y;
	int				x;

	memset(asset, 0, sizeof(*asset));
	ncv = ncvisual_from_file(path);
	if (ncv == NULL)
		return (false);
	if (width > 0 && height > 0
		&& ncvisual_resize_noninterpolative(ncv, height, width) != 0)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, ncv, NULL, &geom) != 0
		|| geom.pixy == 0 || geom.pixx == 0
		|| geom.pixy > INT32_MAX || geom.pixx > INT32_MAX)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	asset->height = (int)geom.pixy;
	asset->width = (int)geom.pixx;
	if (!solo_canvas_buffer_bytes(asset->width, asset->height, &pixel_bytes))
	{
		ncvisual_destroy(ncv);
		memset(asset, 0, sizeof(*asset));
		return (false);
	}
	asset->pixels = malloc(pixel_bytes);
	if (asset->pixels == NULL)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	y = 0;
	while (y < asset->height)
	{
		x = 0;
		while (x < asset->width)
		{
			if (ncvisual_at_yx(ncv, (unsigned)y, (unsigned)x,
					&asset->pixels[(size_t)y * asset->width + x]) < 0)
			{
				pixel_asset_destroy(asset);
				ncvisual_destroy(ncv);
				return (false);
			}
			x++;
		}
		y++;
	}
	ncvisual_destroy(ncv);
	return (true);
}

/**
 * @brief Releases one temporary decoded pixel asset.
 *
 * The structure is zeroed after free so cleanup paths can safely run more than
 *   once.
 *
 * @param asset Pointer to the decoded pixel asset.
 */
static void	pixel_asset_destroy(pixel_asset_t *asset)
{
	free(asset->pixels);
	memset(asset, 0, sizeof(*asset));
}

/**
 * @brief Repairs transparent export seams in the backdrop.
 *
 * Every non-opaque pixel copies a neighboring opaque color because the
 *   fullscreen background contract forbids transparency.
 *
 * @param asset Pointer to the decoded pixel asset.
 */
static void	repair_background_alpha(pixel_asset_t *asset)
{
	uint32_t	replacement;
	uint32_t	*pixel;
	int			y;
	int			x;

	y = 0;
	while (y < asset->height)
	{
		x = 0;
		while (x < asset->width)
		{
			pixel = &asset->pixels[(size_t)y * asset->width + x];
			if (ncpixel_a(*pixel) != 255u)
			{
				replacement = ncpixel(0, 0, 0);
				if (x > 0)
					replacement = pixel[-1];
				else if (x + 1 < asset->width)
					replacement = pixel[1];
				ncpixel_set_a(&replacement, 255u);
				*pixel = replacement;
			}
			x++;
		}
		y++;
	}
}

/**
 * @brief Checks an asset against an exact pixel contract.
 *
 * Masks and atlases must match their documented slicing geometry before any
 *   indexing occurs.
 *
 * @param asset Pointer to the decoded pixel asset.
 * @param width Pixel width.
 * @param height Pixel height.
 * @return true when both dimensions match, otherwise false.
 */
static bool	asset_dimensions_are(const pixel_asset_t *asset, int width,
	int height)
{
	return (asset->width == width && asset->height == height);
}

/**
 * @brief Aligns authored HUD borders with the 16-pixel gameplay grid.
 *
 * Existing PNG pixels are moved without generating or recoloring borders; the
 *   obsolete baked controls strip is cleared.
 *
 * @param hud Pointer to the decoded authored HUD asset.
 */
static void	align_authored_hud(pixel_asset_t *hud)
{
	int	y;
	int	x;

	y = HUD_ART_CONTROLS_TOP;
	while (y <= HUD_ART_CONTROLS_BOTTOM)
	{
		x = HUD_ART_CONTROLS_LEFT;
		while (x <= HUD_ART_CONTROLS_RIGHT)
		{
			hud->pixels[(size_t)y * hud->width + x] = 0;
			x++;
		}
		y++;
	}
	y = HUD_ART_BOARD_BOTTOM;
	while (y >= HUD_ART_BOARD_CONTENT_TOP)
	{
		x = HUD_ART_BOARD_LEFT;
		while (x <= HUD_ART_BOARD_RIGHT)
		{
			hud->pixels[(size_t)(y + HUD_ART_BOARD_SHIFT_Y)
				* hud->width + x]
				= hud->pixels[(size_t)y * hud->width + x];
			hud->pixels[(size_t)y * hud->width + x] = 0;
			x++;
		}
		y--;
	}
	y = HUD_ART_BOARD_TOP;
	while (y < HUD_ART_BOARD_CONTENT_TOP)
	{
		x = HUD_ART_BOARD_LEFT;
		while (x <= HUD_ART_BOARD_RIGHT)
		{
			hud->pixels[(size_t)(y + HUD_ART_BOARD_SHIFT_Y)
				* hud->width + x]
				= hud->pixels[(size_t)y * hud->width + x];
			x++;
		}
		y++;
	}
}

/**
 * @brief Nearest-neighbor blits an asset into an RGBA canvas.
 *
 * Explicit source sampling preserves hard pixel-art edges while caller opacity
 *   controls layering.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param canvas_width Destination width in pixels.
 * @param canvas_height Destination height in pixels.
 * @param asset Pointer to the decoded pixel asset.
 * @param dest_x Destination x coordinate.
 * @param dest_y Destination y coordinate.
 * @param dest_width Destination width in pixels.
 * @param dest_height Destination height in pixels.
 * @param opacity Alpha multiplier from 0 through 255.
 */
static void	blit_asset_scaled(uint32_t *canvas, int canvas_width,
	int canvas_height, const pixel_asset_t *asset, int dest_x, int dest_y,
	int dest_width, int dest_height, unsigned opacity)
{
	int			y;
	int			x;
	int			source_y;
	int			source_x;
	uint32_t	pixel;

	y = 0;
	while (y < dest_height)
	{
		source_y = y * asset->height / dest_height;
		x = 0;
		while (x < dest_width)
		{
			source_x = x * asset->width / dest_width;
			pixel = asset->pixels[(size_t)source_y * asset->width + source_x];
			put_pixel_sized(canvas, canvas_width, canvas_height,
				dest_x + x, dest_y + y, with_opacity(pixel, opacity));
			x++;
		}
		y++;
	}
}

/**
 * @brief Composites one pixel into a bounds-checked canvas.
 *
 * Coordinates outside the supplied canvas are ignored instead of indexing
 *   memory.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param canvas_width Destination width in pixels.
 * @param canvas_height Destination height in pixels.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param pixel Source RGBA pixel.
 */
static void	put_pixel_sized(uint32_t *canvas, int canvas_width,
	int canvas_height, int x, int y, uint32_t pixel)
{
	size_t	index;

	if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
		return ;
	index = (size_t)y * (size_t)canvas_width + (size_t)x;
	canvas[index] = blend_pixel(canvas[index], pixel);
}

/**
 * @brief Alpha-composites one RGBA pixel over another.
 *
 * Straight-alpha source-over arithmetic retains both authored masks and
 *   background pixels.
 *
 * @param below Destination pixel below the new source.
 * @param above Source pixel composited above the destination.
 * @return Source-over composited RGBA pixel.
 */
static uint32_t	blend_pixel(uint32_t below, uint32_t above)
{
	unsigned	sa;
	unsigned	da;
	unsigned	inverse;
	unsigned	out_a;
	unsigned	r;
	unsigned	g;
	unsigned	b;
	uint32_t	out;

	sa = ncpixel_a(above);
	if (sa == 0)
		return (below);
	if (sa == 255)
		return (above);
	da = ncpixel_a(below);
	inverse = 255u - sa;
	out_a = sa + (da * inverse + 127u) / 255u;
	if (out_a == 0)
		return (0);
	r = (ncpixel_r(above) * sa
		+ (ncpixel_r(below) * da * inverse + 127u) / 255u
		+ out_a / 2u) / out_a;
	g = (ncpixel_g(above) * sa
		+ (ncpixel_g(below) * da * inverse + 127u) / 255u
		+ out_a / 2u) / out_a;
	b = (ncpixel_b(above) * sa
		+ (ncpixel_b(below) * da * inverse + 127u) / 255u
		+ out_a / 2u) / out_a;
	out = ncpixel(r, g, b);
	ncpixel_set_a(&out, out_a);
	return (out);
}

/**
 * @brief Multiplies an RGBA pixel's existing alpha.
 *
 * Color channels are preserved while authored transparency and caller opacity
 *   combine.
 *
 * @param pixel Source RGBA pixel.
 * @param opacity Alpha multiplier from 0 through 255.
 * @return Pixel with multiplied alpha.
 */
static uint32_t	with_opacity(uint32_t pixel, unsigned opacity)
{
	unsigned	alpha;

	alpha = (ncpixel_a(pixel) * opacity + 127u) / 255u;
	ncpixel_set_a(&pixel, alpha);
	return (pixel);
}

/**
 * @brief Fills an axis-aligned rectangle on the Solo canvas.
 *
 * Each covered coordinate uses source-over composition so translucent panels
 *   preserve intended layering.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param width Pixel width.
 * @param height Pixel height.
 * @param pixel Source RGBA pixel.
 */
static void	draw_rect(uint32_t *canvas, int x, int y, int width, int height,
	uint32_t pixel)
{
	int	draw_y;
	int	draw_x;

	draw_y = 0;
	while (draw_y < height)
	{
		draw_x = 0;
		while (draw_x < width)
		{
			put_pixel(canvas, x + draw_x, y + draw_y, pixel);
			draw_x++;
		}
		draw_y++;
	}
}

/**
 * @brief Composites one pixel into the fixed Solo canvas.
 *
 * The fixed canvas dimensions are forwarded to the checked pixel writer.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param pixel Source RGBA pixel.
 */
static void	put_pixel(uint32_t *canvas, int x, int y, uint32_t pixel)
{
	put_pixel_sized(canvas, SOLO_CANVAS_WIDTH, SOLO_CANVAS_HEIGHT,
		x, y, pixel);
}

/**
 * @brief Creates one Notcurses RGBA pixel from a color and alpha.
 *
 * RGB channels come from the small renderer color value and alpha remains
 *   explicit.
 *
 * @param color RGB color to encode.
 * @param alpha Alpha channel from 0 through 255.
 * @return Encoded Notcurses RGBA pixel.
 */
static uint32_t	make_pixel(color_t color, unsigned alpha)
{
	uint32_t	pixel;

	pixel = ncpixel(color.r, color.g, color.b);
	ncpixel_set_a(&pixel, alpha);
	return (pixel);
}

/**
 * @brief Preblends one visible source pixel into ghost ink.
 *
 * Blending against the opaque playfield avoids the coarse alpha levels used by
 *   some terminal image protocols.
 *
 * @param source Source RGBA pixel.
 * @param blend Ghost blend strength from 0 through 255.
 * @return Preblended ghost pixel, or transparent zero for an empty source
 *   pixel.
 */
static uint32_t	make_ghost_pixel(uint32_t source, unsigned blend)
{
	color_t	color;

	if (ncpixel_a(source) == 0)
		return (0);
	color.r = (g_playfield.r * (255u - blend) + g_ghost.r * blend + 127u)
		/ 255u;
	color.g = (g_playfield.g * (255u - blend) + g_ghost.g * blend + 127u)
		/ 255u;
	color.b = (g_playfield.b * (255u - blend) + g_ghost.b * blend + 127u)
		/ 255u;
	return (make_pixel(color, 255u));
}

/**
 * @brief Draws the HOLD preview and the three queued NEXT previews.
 *
 * HOLD sits in the authored top-left frame with fixed-size tiles and dims to
 * 55% once consumed for the current piece. NEXT keeps its three equal slots in
 * the authored panel to its right. The authored frame is never redrawn here;
 * only the piece interiors are painted.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
static void	draw_next_queue(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	int	index;
	int	slot_x;
	int	slot_width;

	if (game->has_hold)
		draw_preview_piece(canvas, solo, game->hold, HUD_HOLD_X, HUD_HOLD_Y,
			HUD_HOLD_WIDTH, HUD_HOLD_HEIGHT,
			game->hold_used ? HOLD_USED_OPACITY : 255u, HOLD_PREVIEW_TILE_SIZE);
	slot_width = HUD_NEXT_WIDTH / SOLO_NEXT_COUNT;
	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		slot_x = HUD_NEXT_X + index * slot_width;
		draw_preview_piece(canvas, solo, game->next[index], slot_x,
			HUD_NEXT_Y, slot_width, HUD_NEXT_HEIGHT, 255u, 0);
		index++;
	}
}

/**
 * @brief Centers one spawned tetromino in a HOLD or NEXT slot.
 *
 * With forced_tile set (HOLD), every shape uses that exact tile size for a
 * consistent preview. Otherwise (NEXT) full-size atlas tiles are preferred and
 * only shapes that exceed the slot drop to the smaller preview scale. Either
 * way the result is clamped to fit the slot.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param type Tetromino type to map.
 * @param slot_x Left edge of the preview slot.
 * @param slot_y Top edge of the preview slot.
 * @param slot_width Preview-slot width in pixels.
 * @param slot_height Preview-slot height in pixels.
 * @param opacity Alpha multiplier for available or consumed HOLD state.
 * @param forced_tile Fixed tile size in pixels, or 0 to auto-fit.
 */
static void	draw_preview_piece(uint32_t *canvas,
	const solo_render_t *solo, t_piece_type type, int slot_x, int slot_y,
	int slot_width, int slot_height, unsigned opacity, int forced_tile)
{
	t_piece	piece;
	int		cols[4];
	int		rows[4];
	int		min_col;
	int		max_col;
	int		min_row;
	int		max_row;
	int		origin_x;
	int		origin_y;
	int		index;
	int		tile_size;
	int		piece_width;
	int		piece_height;

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
		if (cols[index] < min_col)
			min_col = cols[index];
		if (cols[index] > max_col)
			max_col = cols[index];
		if (rows[index] < min_row)
			min_row = rows[index];
		if (rows[index] > max_row)
			max_row = rows[index];
		index++;
	}
	piece_width = max_col - min_col + 1;
	piece_height = max_row - min_row + 1;
	/* HOLD forces one size; NEXT prefers crisp atlas pixels, then shrinks. */
	if (forced_tile > 0)
		tile_size = forced_tile;
	else
	{
		tile_size = TILE_SOURCE_SIZE;
		if (piece_width * tile_size > slot_width
			|| piece_height * tile_size > slot_height)
			tile_size = PREVIEW_TILE_SIZE;
	}
	if (piece_width * tile_size > slot_width)
		tile_size = slot_width / piece_width;
	if (piece_height * tile_size > slot_height)
		tile_size = slot_height / piece_height;
	if (tile_size < 1)
		return ;
	origin_x = slot_x + (slot_width
		- piece_width * tile_size) / 2;
	origin_y = slot_y + (slot_height - piece_height * tile_size) / 2;
	index = 0;
	while (index < 4)
	{
		draw_tile(canvas, solo, solo_canvas_piece_tile(type),
			origin_x + (cols[index] - min_col) * tile_size,
			origin_y + (rows[index] - min_row) * tile_size,
			tile_size, opacity, false);
		index++;
	}
}

/**
 * @brief Draws one scaled atlas tile on the canvas.
 *
 * Invalid tile indexes fall back to garbage; ghost tiles use preblended
 *   outline and interior shading.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param tile_index Tile-atlas index.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param size Rendered tile size in pixels.
 * @param opacity Alpha multiplier from 0 through 255.
 * @param outline_only Whether to draw ghost-style shading.
 */
static void	draw_tile(uint32_t *canvas, const solo_render_t *solo,
	int tile_index, int x, int y, int size, unsigned opacity, bool outline_only)
{
	int			draw_y;
	int			draw_x;
	int			source_y;
	int			source_x;
	uint32_t	pixel;

	if (tile_index < 0 || tile_index >= TILE_ATLAS_COUNT)
		tile_index = TILE_GARBAGE;
	draw_y = 0;
	while (draw_y < size)
	{
		source_y = draw_y * TILE_SOURCE_SIZE / size;
		draw_x = 0;
		while (draw_x < size)
		{
			source_x = draw_x * TILE_SOURCE_SIZE / size;
			pixel = solo->tile_pixels[(size_t)(tile_index
					* TILE_SOURCE_STRIDE + source_y) * solo->tile_width
				+ source_x];
			if (outline_only)
				pixel = solo_canvas_ghost_tile_pixel(pixel, source_x, source_y);
			else
				pixel = with_opacity(pixel, opacity);
			put_pixel(canvas, x + draw_x, y + draw_y, pixel);
			draw_x++;
		}
		draw_y++;
	}
}

/**
 * @brief Draws the Mirurun charge meter and four interactive thresholds.
 *
 * Ten segments show stored charge. Costs 2, 4, 6, and 8 position the numbered
 * ability circles at equal intervals from bottom to top.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the renderer containing hover state.
 * @param game Pointer to the current Solo game state.
 */
static void	draw_crystal_meter(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	solo_ability_t	ability;
	int				segment;
	int				segment_height;
	int				y;

	draw_rect(canvas, HUD_METER_X, HUD_METER_Y, HUD_METER_WIDTH,
		HUD_METER_HEIGHT, make_pixel(g_dark, 115));
	segment_height = HUD_METER_HEIGHT / SOLO_CRYSTAL_CAPACITY;
	segment = 0;
	while (segment < SOLO_CRYSTAL_CAPACITY)
	{
		y = HUD_METER_Y + HUD_METER_HEIGHT
			- (segment + 1) * segment_height + 2;
		if (segment < game->crystal_charge)
			draw_rect(canvas, HUD_METER_X, y, HUD_METER_WIDTH,
				segment_height - 2, make_pixel(g_pink, 225));
		else
			draw_rect(canvas, HUD_METER_X, y, HUD_METER_WIDTH,
				segment_height - 2, make_pixel(g_purple, 65));
		segment++;
	}
	ability = SOLO_ABILITY_MIRURUN;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		draw_ability_marker(canvas, solo, game, ability);
		ability++;
	}
}

/**
 * @brief Draws one numbered ability circle with affordability and hover state.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the renderer containing hover state.
 * @param game Pointer to the current Solo state.
 * @param ability Ability represented by this marker.
 */
static void	draw_ability_marker(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game, solo_ability_t ability)
{
	char		number[2];
	color_t	ring;
	color_t	digit;
	int			center_x;
	int			center_y;
	bool		affordable;
	bool		feedback;

	center_x = HUD_METER_X + HUD_METER_WIDTH / 2;
	center_y = solo_ability_center_y(ability);
	affordable = game->crystal_charge >= solo_ability_cost(ability);
	feedback = game->last_ability == ability
		&& game->ability_result != SOLO_ABILITY_RESULT_NONE;
	ring = g_purple;
	digit = g_purple;
	if (affordable)
	{
		ring = g_pink;
		digit = g_white;
	}
	if (solo->hovered_ability == ability || feedback)
	{
		ring = g_white;
		digit = g_white;
	}
	draw_filled_circle(canvas, center_x, center_y,
		SOLO_ABILITY_CIRCLE_RADIUS - 2, make_pixel(g_dark, 245));
	draw_circle_ring(canvas, center_x, center_y,
		SOLO_ABILITY_CIRCLE_RADIUS, 2, make_pixel(ring, 255));
	number[0] = (char)('0' + ability);
	number[1] = '\0';
	draw_numbers(canvas, solo, number, center_x - 3, center_y - 6,
		6, 12, 0, digit, 255);
}

/**
 * @brief Fills a pixel circle without interpolation.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param center_x Circle centre x coordinate.
 * @param center_y Circle centre y coordinate.
 * @param radius Circle radius in pixels.
 * @param pixel Fill pixel.
 */
static void	draw_filled_circle(uint32_t *canvas, int center_x, int center_y,
	int radius, uint32_t pixel)
{
	int	x;
	int	y;

	y = -radius;
	while (y <= radius)
	{
		x = -radius;
		while (x <= radius)
		{
			if (x * x + y * y <= radius * radius)
				put_pixel(canvas, center_x + x, center_y + y, pixel);
			x++;
		}
		y++;
	}
}

/**
 * @brief Draws a fixed-thickness pixel circle outline.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param center_x Circle centre x coordinate.
 * @param center_y Circle centre y coordinate.
 * @param radius Outer circle radius.
 * @param thickness Ring thickness in pixels.
 * @param pixel Outline pixel.
 */
static void	draw_circle_ring(uint32_t *canvas, int center_x, int center_y,
	int radius, int thickness, uint32_t pixel)
{
	int	inner;
	int	distance;
	int	x;
	int	y;

	inner = radius - thickness;
	y = -radius;
	while (y <= radius)
	{
		x = -radius;
		while (x <= radius)
		{
			distance = x * x + y * y;
			if (distance <= radius * radius && distance >= inner * inner)
				put_pixel(canvas, center_x + x, center_y + y, pixel);
			x++;
		}
		y++;
	}
}

/**
 * @brief Draws total score, level, lines, combo, and award state.
 *
 * Guideline score state is converted into independently refreshable authored
 *   HUD regions.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
static void	draw_score_panel(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	char	points[32];
	int		combo;

	draw_text_centered(canvas, solo, "SCORE",
		HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 6,
		8, 8, 1, g_white);
	draw_score_number(canvas, solo, game->scoring.total);
	combo = game->scoring.combo;
	if (combo < 0)
		combo = 0;
	draw_stat_line(canvas, solo, "LEVEL", game->level,
		HUD_SCORE_STAT_FIRST_Y);
	draw_stat_line(canvas, solo, "LINES", game->total_lines,
		HUD_SCORE_STAT_FIRST_Y + HUD_SCORE_STAT_ROW_STEP);
	draw_stat_line(canvas, solo, "COMBO", combo,
		HUD_SCORE_STAT_FIRST_Y + 2 * HUD_SCORE_STAT_ROW_STEP);
	if (draw_ability_message(canvas, solo, game))
		return ;
	if (game->scoring.back_to_back)
		draw_text_centered(canvas, solo, "BACK-TO-BACK",
			HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 118,
			8, 8, 1, g_pink);
	if (game->last_score.total_awarded > 0)
	{
		draw_text_centered(canvas, solo, clear_name(game),
			HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 130,
			8, 8, 1, g_white);
		snprintf(points, sizeof(points), "+%" PRIu64,
			game->last_score.total_awarded);
		draw_numbers_centered_fit(canvas, solo, points, HUD_SCORE_Y + 143,
			g_pink);
	}
}

/**
 * @brief Draws hover help or bounded activation feedback in the event panel.
 *
 * Activation feedback takes priority for one second; afterward the current
 * hovered marker supplies its name, cost, description, and keyboard hint.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the renderer containing hover state.
 * @param game Pointer to the current Solo state.
 * @return true when ability content replaced ordinary score-event content.
 */
static bool	draw_ability_message(uint32_t *canvas,
	const solo_render_t *solo, const solo_game_t *game)
{
	solo_ability_t	ability;
	char				title[48];
	char				detail[48];
	char				hint[48];
	int				cost;

	ability = solo->hovered_ability;
	if (game->ability_result != SOLO_ABILITY_RESULT_NONE)
		ability = game->last_ability;
	if (ability == SOLO_ABILITY_NONE)
		return (false);
	cost = solo_ability_cost(ability);
	if (game->ability_result == SOLO_ABILITY_RESULT_NONE)
	{
		snprintf(title, sizeof(title), "%s [%d] COST %d",
			solo_ability_name(ability), ability, cost);
		snprintf(detail, sizeof(detail), "%s",
			solo_ability_description(ability));
		snprintf(hint, sizeof(hint), "CLICK OR PRESS %d", ability);
	}
	else if (game->ability_result == SOLO_ABILITY_RESULT_ACTIVATED)
	{
		snprintf(title, sizeof(title), "%s ACTIVATED",
			solo_ability_name(ability));
		if (ability == SOLO_ABILITY_MIRURUN)
			snprintf(detail, sizeof(detail), "BOTTOM 4 ROWS REMOVED");
		else
			snprintf(detail, sizeof(detail), "SOLO TEST - NO TARGET");
		snprintf(hint, sizeof(hint), "-%d CRYSTALS", cost);
	}
	else if (game->ability_result == SOLO_ABILITY_RESULT_NO_CHARGE)
	{
		snprintf(title, sizeof(title), "%s NOT READY",
			solo_ability_name(ability));
		snprintf(detail, sizeof(detail), "NEED %d CRYSTALS", cost);
		snprintf(hint, sizeof(hint), "CLEAR 2 LINES = 1");
	}
	else if (game->ability_result == SOLO_ABILITY_RESULT_BLOCKED)
	{
		snprintf(title, sizeof(title), "MIRURUN BLOCKED");
		snprintf(detail, sizeof(detail), "ACTIVE PIECE COLLISION");
		snprintf(hint, sizeof(hint), "TRY AGAIN AFTER LOCK");
	}
	else
	{
		snprintf(title, sizeof(title), "%s UNAVAILABLE",
			solo_ability_name(ability));
		snprintf(detail, sizeof(detail), "WAIT FOR ACTIVE PLAY");
		snprintf(hint, sizeof(hint), "CHARGE NOT SPENT");
	}
	draw_text_centered(canvas, solo, title,
		HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 119,
		5, 6, 1, g_pink);
	draw_text_centered(canvas, solo, detail,
		HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 133,
		5, 6, 1, g_white);
	draw_text_centered(canvas, solo, hint,
		HUD_SCORE_X + HUD_SCORE_WIDTH / 2, HUD_SCORE_Y + 147,
		5, 6, 1, g_purple);
	return (true);
}

/**
 * @brief Centers one masked string around a canvas coordinate.
 *
 * Measured glyph width determines the left edge before shadowed drawing.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param center_x Horizontal center coordinate.
 * @param y Canvas y coordinate.
 * @param glyph_width Rendered glyph width in pixels.
 * @param glyph_height Rendered glyph height in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @param tint Color applied to visible mask pixels.
 */
static void	draw_text_centered(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int center_x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint)
{
	int	width;

	width = text_width(text, glyph_width, spacing);
	draw_text_shadowed(canvas, solo, text, center_x - width / 2, y,
		glyph_width, glyph_height, spacing, tint);
}

/**
 * @brief Calculates the rendered width of a fixed-size glyph string.
 *
 * Spacing is included between glyphs but never after the final character.
 *
 * @param text Null-terminated text to draw.
 * @param glyph_width Rendered glyph width in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @return Rendered width in pixels.
 */
static int	text_width(const char *text, int glyph_width, int spacing)
{
	int	length;

	length = (int)strlen(text);
	if (length == 0)
		return (0);
	return (length * glyph_width + (length - 1) * spacing);
}

/**
 * @brief Draws legible masked text with a one-pixel shadow.
 *
 * A dark translucent pass precedes the opaque tinted foreground pass.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param glyph_width Rendered glyph width in pixels.
 * @param glyph_height Rendered glyph height in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @param tint Color applied to visible mask pixels.
 */
static void	draw_text_shadowed(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint)
{
	draw_text(canvas, solo, text, x + 1, y + 1, glyph_width, glyph_height,
		spacing, g_dark, 190);
	draw_text(canvas, solo, text, x, y, glyph_width, glyph_height,
		spacing, tint, 255);
}

/**
 * @brief Draws a string from the shared font mask.
 *
 * Printable ASCII maps into the 16-column sheet and unsupported bytes fall
 *   back to the question-mark glyph.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param glyph_width Rendered glyph width in pixels.
 * @param glyph_height Rendered glyph height in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @param tint Color applied to visible mask pixels.
 * @param opacity Alpha multiplier from 0 through 255.
 */
static void	draw_text(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint, unsigned opacity)
{
	unsigned	codepoint;
	int			glyph;
	int			draw_x;

	draw_x = x;
	while (*text != '\0')
	{
		codepoint = (unsigned char)*text;
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		glyph = (int)codepoint - 32;
		draw_mask(canvas, solo->font_pixels, solo->font_width,
			(glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH,
			(glyph / FONT_COLUMNS) * FONT_GLYPH_HEIGHT + FONT_INK_Y,
			FONT_GLYPH_WIDTH, FONT_INK_HEIGHT, draw_x, y,
			glyph_width, glyph_height, tint, opacity);
		draw_x += glyph_width + spacing;
		text++;
	}
}

/**
 * @brief Tints and scales one alpha mask onto the canvas.
 *
 * Only mask alpha is used, keeping shared glyph shapes theme-neutral.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param mask Pointer to alpha-mask pixels.
 * @param mask_width Mask row stride in pixels.
 * @param source_x Source-region x coordinate.
 * @param source_y Source-region y coordinate.
 * @param source_width Source-region width in pixels.
 * @param source_height Source-region height in pixels.
 * @param dest_x Destination x coordinate.
 * @param dest_y Destination y coordinate.
 * @param dest_width Destination width in pixels.
 * @param dest_height Destination height in pixels.
 * @param tint Color applied to visible mask pixels.
 * @param opacity Alpha multiplier from 0 through 255.
 */
static void	draw_mask(uint32_t *canvas, const uint32_t *mask,
	int mask_width, int source_x, int source_y, int source_width,
	int source_height, int dest_x, int dest_y, int dest_width, int dest_height,
	color_t tint, unsigned opacity)
{
	int			y;
	int			x;
	int			sample_y;
	int			sample_x;
	unsigned	alpha;

	y = 0;
	while (y < dest_height)
	{
		sample_y = source_y + y * source_height / dest_height;
		x = 0;
		while (x < dest_width)
		{
			sample_x = source_x + x * source_width / dest_width;
			alpha = ncpixel_a(mask[(size_t)sample_y * mask_width + sample_x]);
			alpha = (alpha * opacity + 127u) / 255u;
			if (alpha != 0)
				put_pixel(canvas, dest_x + x, dest_y + y,
					make_pixel(tint, alpha));
			x++;
		}
		y++;
	}
}

/**
 * @brief Formats and draws the zero-padded total score.
 *
 * A ten-digit decimal field uses the shared tinted number mask.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param score Total score value.
 */
static void	draw_score_number(uint32_t *canvas, const solo_render_t *solo,
	uint64_t score)
{
	char	score_text[32];

	snprintf(score_text, sizeof(score_text), "%010" PRIu64, score);
	draw_numbers_centered_fit(canvas, solo, score_text, HUD_SCORE_Y + 23,
		g_pink);
}

/**
 * @brief Centers a numeric award string inside the score panel.
 *
 * Long strings shrink uniformly before centering so their shadow remains
 *   inside the independently refreshed plane.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param y Canvas y coordinate.
 * @param tint Color applied to visible mask pixels.
 */
static void	draw_numbers_centered_fit(uint32_t *canvas,
	const solo_render_t *solo, const char *text, int y, color_t tint)
{
	int		length;
	int		glyph_width;
	int		glyph_height;
	int		spacing;
	int		width;

	length = (int)strlen(text);
	if (length == 0)
		return ;
	glyph_width = NUMBER_GLYPH_WIDTH;
	glyph_height = NUMBER_GLYPH_HEIGHT;
	spacing = 1;
	width = text_width(text, glyph_width, spacing) + 1;
	if (width > HUD_SCORE_WIDTH - 16)
	{
		spacing = 0;
		glyph_width = (HUD_SCORE_WIDTH - 16) / length;
		glyph_height = glyph_width * NUMBER_GLYPH_HEIGHT
			/ NUMBER_GLYPH_WIDTH;
		width = text_width(text, glyph_width, spacing) + 1;
	}
	draw_numbers_shadowed(canvas, solo, text,
		HUD_SCORE_X + (HUD_SCORE_WIDTH - width) / 2, y,
		glyph_width, glyph_height, spacing, tint);
}

/**
 * @brief Draws crisp score digits with a one-pixel shadow.
 *
 * The foreground mask is not expanded or postprocessed, preserving its
 *   authored pixels.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param glyph_width Rendered glyph width in pixels.
 * @param glyph_height Rendered glyph height in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @param tint Color applied to visible mask pixels.
 */
static void	draw_numbers_shadowed(uint32_t *canvas,
	const solo_render_t *solo, const char *text, int x, int y,
	int glyph_width, int glyph_height, int spacing, color_t tint)
{
	draw_numbers(canvas, solo, text, x + 1, y + 1, glyph_width, glyph_height,
		spacing, g_dark, 190);
	/* Keep the authored mask exact: expanding it also expands the intentional
	 * slot-edge pixels and produces punctuation-like artifacts after tinting. */
	draw_numbers(canvas, solo, text, x, y, glyph_width, glyph_height,
		spacing, tint, 255);
}

/**
 * @brief Draws score characters from the shared numeric mask.
 *
 * Authored inter-slot offsets are preserved so tinting cannot create
 *   punctuation-like edge artifacts.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param text Null-terminated text to draw.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param glyph_width Rendered glyph width in pixels.
 * @param glyph_height Rendered glyph height in pixels.
 * @param spacing Pixels between adjacent glyphs.
 * @param tint Color applied to visible mask pixels.
 * @param opacity Alpha multiplier from 0 through 255.
 */
static void	draw_numbers(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint, unsigned opacity)
{
	int	glyph;
	int	draw_x;
	int	source_x;

	draw_x = x;
	while (*text != '\0')
	{
		glyph = number_glyph_index(*text);
		if (glyph >= 0)
		{
			source_x = glyph * NUMBER_SLOT_WIDTH;
			/* The authored 8px ink starts one pixel before each nominal slot
			 * after zero; the remaining columns are inter-glyph padding. */
			if (glyph > 0)
				source_x--;
			draw_mask(canvas, solo->number_pixels, solo->number_width,
				source_x, 0, NUMBER_GLYPH_WIDTH,
				NUMBER_GLYPH_HEIGHT, draw_x, y, glyph_width, glyph_height,
				tint, opacity);
		}
		draw_x += glyph_width + spacing;
		text++;
	}
}

/**
 * @brief Maps a score character to the shared number-mask slot.
 *
 * Digits and signs have authored slots; unsupported characters are
 *   intentionally blank.
 *
 * @param c Character to map.
 * @return Mask slot index, or -1 for an unsupported character.
 */
static int	number_glyph_index(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c == '+')
		return (10);
	if (c == '-')
		return (11);
	return (-1);
}

/**
 * @brief Draws one score-panel label and right-aligned numeric value.
 *
 * Custom font labels and number-mask values share the same baseline.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param label Stat label text.
 * @param value Numeric stat value.
 * @param y Canvas y coordinate.
 */
static void	draw_stat_line(uint32_t *canvas, const solo_render_t *solo,
	const char *label, int value, int y)
{
	char	value_text[32];
	int		width;

	snprintf(value_text, sizeof(value_text), "%d", value);
	draw_text_shadowed(canvas, solo, label, HUD_SCORE_X + 10,
		y + (NUMBER_GLYPH_HEIGHT - 8) / 2,
		8, 8, 1, g_white);
	width = text_width(value_text, NUMBER_GLYPH_WIDTH, 1);
	draw_numbers(canvas, solo, value_text,
		HUD_SCORE_X + HUD_SCORE_WIDTH - HUD_SCORE_STAT_RIGHT_INSET - width, y,
		NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, 1, g_white, 255u);
}

/**
 * @brief Selects the label for the last scoring event.
 *
 * Perfect clears and spins take precedence over ordinary line-count names.
 *
 * @param game Pointer to the current Solo game state.
 * @return Static event-label string, which may be empty.
 */
static const char	*clear_name(const solo_game_t *game)
{
	if (game->last_perfect_clear)
		return ("PERFECT CLEAR");
	if (game->last_spin == T_SPIN_FULL)
		return ("T-SPIN");
	if (game->last_spin == T_SPIN_MINI)
		return ("MINI T-SPIN");
	if (game->last_lines == 4)
		return ("TETRIS");
	if (game->last_lines == 3)
		return ("TRIPLE");
	if (game->last_lines == 2)
		return ("DOUBLE");
	if (game->last_lines == 1)
		return ("SINGLE");
	return ("");
}

/**
 * @brief Restores only the board rectangle from the static canvas.
 *
 * Fallback composition erases old moving-piece pixels without rebuilding the
 *   full HUD.
 *
 * @param solo Pointer to the Solo render state.
 */
static void	reset_board_frame(solo_render_t *solo)
{
	size_t	bytes;
	int		row;

	bytes = (size_t)SOLO_BOARD_WIDTH * sizeof(*solo->frame_pixels);
	row = 0;
	while (row < SOLO_BOARD_HEIGHT)
	{
		memcpy(&solo->frame_pixels[(size_t)(HUD_BOARD_Y + row)
				* SOLO_CANVAS_WIDTH + HUD_BOARD_X],
			&solo->static_pixels[(size_t)(HUD_BOARD_Y + row)
				* SOLO_CANVAS_WIDTH + HUD_BOARD_X], bytes);
		row++;
	}
}

/**
 * @brief Draws settled and clearing cells from authoritative board state.
 *
 * Clear-animation rows swap atlas frames at the animation midpoint.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
static void	draw_settled_board(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	t_cell	cell;
	int		clear_tile;
	int		row;
	int		col;

	clear_tile = TILE_CLEAR_FIRST;
	if (game->clear_elapsed_ms >= SOLO_CLEAR_ANIMATION_MS / 2)
		clear_tile = TILE_CLEAR_SECOND;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			cell = board_get(&game->board, col, row);
			if (solo_game_row_is_clearing(game, row))
				draw_tile(canvas, solo, clear_tile,
					HUD_BOARD_X + col * HUD_TILE_SIZE,
					HUD_BOARD_Y + row * HUD_TILE_SIZE,
					HUD_TILE_SIZE, 255, false);
			else if (cell.type == CELL_GARBAGE)
				draw_tile(canvas, solo, TILE_GARBAGE,
					HUD_BOARD_X + col * HUD_TILE_SIZE,
					HUD_BOARD_Y + row * HUD_TILE_SIZE,
					HUD_TILE_SIZE, 255, false);
			else if (cell.type == CELL_FILLED)
				draw_tile(canvas, solo,
					solo_canvas_piece_tile((t_piece_type)cell.color),
					HUD_BOARD_X + col * HUD_TILE_SIZE,
					HUD_BOARD_Y + row * HUD_TILE_SIZE,
					HUD_TILE_SIZE, 255, false);
			col++;
		}
		row++;
	}
}

/**
 * @brief Draws all in-bounds cells of one tetromino.
 *
 * Piece coordinates are converted to the authored board grid before tile
 *   composition.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param piece Pointer to the tetromino.
 * @param opacity Alpha multiplier from 0 through 255.
 * @param ghost Whether to draw the landing-projection shading.
 */
static void	draw_piece(uint32_t *canvas, const solo_render_t *solo,
	const t_piece *piece, unsigned opacity, bool ghost)
{
	int	cols[4];
	int	rows[4];
	int	index;

	if (!piece_cells(piece, cols, rows))
		return ;
	index = 0;
	while (index < 4)
	{
		if (board_in_bounds(cols[index], rows[index]))
			draw_tile(canvas, solo, solo_canvas_piece_tile(piece->type),
				HUD_BOARD_X + cols[index] * HUD_TILE_SIZE,
				HUD_BOARD_Y + rows[index] * HUD_TILE_SIZE,
				HUD_TILE_SIZE, opacity, ghost);
		index++;
	}
}

/**
 * @brief Draws pause or top-out messaging over the board.
 *
 * The dark bounded panel preserves board visibility while keeping action text
 *   readable.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param solo Pointer to the Solo render state.
 * @param game Pointer to the current Solo game state.
 */
static void	draw_overlays(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	const char	*title;
	const char	*help;
	int			x;
	int			y;

	if (!game->paused && game->phase != SOLO_GAME_OVER)
		return ;
	x = HUD_BOARD_X + 16;
	y = HUD_BOARD_Y + 128;
	draw_rect(canvas, x, y, 128, 64, make_pixel(g_dark, 225));
	draw_outline(canvas, x, y, 128, 64, 2, make_pixel(g_pink, 255));
	if (game->phase == SOLO_GAME_OVER)
	{
		title = "TOP OUT";
		help = "R RESTART";
	}
	else
	{
		title = "PAUSED";
		help = "P RESUME";
	}
	draw_text_centered(canvas, solo, title, x + 64, y + 12,
		16, 16, 1, g_pink);
	draw_text_centered(canvas, solo, help, x + 64, y + 39,
		8, 8, 1, g_white);
}

/**
 * @brief Draws a rectangular outline of fixed thickness.
 *
 * Four filled edges reuse the canvas rectangle primitive.
 *
 * @param canvas Pointer to the destination RGBA canvas.
 * @param x Canvas x coordinate.
 * @param y Canvas y coordinate.
 * @param width Pixel width.
 * @param height Pixel height.
 * @param thickness Outline thickness in pixels.
 * @param pixel Source RGBA pixel.
 */
static void	draw_outline(uint32_t *canvas, int x, int y, int width,
	int height, int thickness, uint32_t pixel)
{
	draw_rect(canvas, x, y, width, thickness, pixel);
	draw_rect(canvas, x, y + height - thickness, width, thickness, pixel);
	draw_rect(canvas, x, y, thickness, height, pixel);
	draw_rect(canvas, x + width - thickness, y, thickness, height, pixel);
}
