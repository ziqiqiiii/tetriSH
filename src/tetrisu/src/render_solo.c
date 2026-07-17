#include "tetrisu.h"
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <time.h>

#define SOLO_CANVAS_WIDTH 512
#define SOLO_CANVAS_HEIGHT 384
#define SOLO_MIN_CANVAS_ROWS 24
#define SOLO_MIN_CANVAS_COLS 64

#define HUD_NEXT_X 80
#define HUD_NEXT_Y 4
#define HUD_NEXT_WIDTH 160
#define HUD_NEXT_HEIGHT 36
#define HUD_BOARD_X 80
#define HUD_BOARD_Y 48
#define HUD_TILE_SIZE 16
#define HUD_METER_X 48
#define HUD_METER_Y 42
#define HUD_METER_WIDTH 16
#define HUD_METER_HEIGHT 320
#define HUD_MIRURUN_X 272
#define HUD_MIRURUN_Y 33
#define HUD_MIRURUN_SIZE 160
#define HUD_SCORE_X 272
#define HUD_SCORE_Y 203
#define HUD_SCORE_WIDTH 160
#define HUD_SCORE_STAT_RIGHT_INSET 20
#define HUD_SCORE_STAT_FIRST_Y 258
#define HUD_SCORE_STAT_ROW_STEP 22
#define HUD_CONTROLS_X 80
#define HUD_CONTROLS_Y 376
#define HUD_CONTROLS_WIDTH 352

#define HUD_ART_BOARD_LEFT 77
#define HUD_ART_BOARD_TOP 40
#define HUD_ART_BOARD_CONTENT_TOP 43
#define HUD_ART_BOARD_RIGHT 242
#define HUD_ART_BOARD_BOTTOM 365
#define HUD_ART_BOARD_SHIFT_Y (HUD_BOARD_Y - HUD_ART_BOARD_CONTENT_TOP)
#define HUD_ART_CONTROLS_LEFT 94
#define HUD_ART_CONTROLS_TOP 367
#define HUD_ART_CONTROLS_RIGHT 417
#define HUD_ART_CONTROLS_BOTTOM 383

#define TILE_SOURCE_SIZE 16
#define TILE_SOURCE_STRIDE 18
#define TILE_ATLAS_COUNT 10
#define TILE_GARBAGE 7
#define TILE_CLEAR_FIRST 8
#define TILE_CLEAR_SECOND 9
#define PREVIEW_TILE_SIZE 12

#define FONT_COLUMNS 16
#define FONT_ROWS 6
#define FONT_GLYPH_WIDTH 8
#define FONT_GLYPH_HEIGHT 16
#define FONT_INK_Y 4
#define FONT_INK_HEIGHT 8
#define NUMBER_SLOT_WIDTH 10
#define NUMBER_GLYPH_WIDTH 8
#define NUMBER_GLYPH_HEIGHT 16
#define NUMBER_GLYPH_COUNT 14
#define GHOST_OUTLINE_BLEND 96u
#define GHOST_INTERIOR_BLEND 24u

#define SOLO_MAX_CATCHUP_MS 1000
#define SOLO_RESIZE_POLL_MS 100

typedef struct s_pixel_asset
{
	uint32_t	*pixels;
	int			width;
	int			height;
}	pixel_asset_t;

typedef struct s_color
{
	unsigned	r;
	unsigned	g;
	unsigned	b;
}	color_t;

typedef struct s_piece_geometry
{
	int	cols[4];
	int	rows[4];
	int	min_col;
	int	max_col;
	int	min_row;
	int	max_row;
}	piece_geometry_t;

typedef struct s_piece_bounds
{
	int	min_col;
	int	max_col;
	int	min_row;
	int	max_row;
}	piece_bounds_t;

static const color_t	g_white = {250, 245, 250};
static const color_t	g_pink = {255, 112, 190};
static const color_t	g_purple = {112, 62, 145};
static const color_t	g_dark = {20, 8, 28};
static const color_t	g_ghost = {255, 255, 255};
static const color_t	g_playfield = {7, 13, 23};
static const color_t	g_panel = {11, 23, 34};

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

static uint32_t	make_pixel(color_t color, unsigned alpha)
{
	uint32_t	pixel;

	pixel = ncpixel(color.r, color.g, color.b);
	ncpixel_set_a(&pixel, alpha);
	return (pixel);
}

/* AI-assisted: preblend ghost ink with the opaque playfield instead of
 * relying on terminal image alpha, which Notcurses 3.0.17 quantises heavily. */
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

static uint32_t	ghost_tile_pixel(uint32_t pixel, int x, int y)
{
	if (x >= 3 && x < 13 && y >= 3 && y < 13)
		return (make_ghost_pixel(pixel, GHOST_INTERIOR_BLEND));
	return (make_ghost_pixel(pixel, GHOST_OUTLINE_BLEND));
}

static uint32_t	with_opacity(uint32_t pixel, unsigned opacity)
{
	unsigned	alpha;

	alpha = (ncpixel_a(pixel) * opacity + 127u) / 255u;
	ncpixel_set_a(&pixel, alpha);
	return (pixel);
}

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

static void	put_pixel_sized(uint32_t *canvas, int canvas_width,
	int canvas_height, int x, int y, uint32_t pixel)
{
	size_t	index;

	if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
		return ;
	index = (size_t)y * (size_t)canvas_width + (size_t)x;
	canvas[index] = blend_pixel(canvas[index], pixel);
}

static void	put_pixel(uint32_t *canvas, int x, int y, uint32_t pixel)
{
	put_pixel_sized(canvas, SOLO_CANVAS_WIDTH, SOLO_CANVAS_HEIGHT,
		x, y, pixel);
}

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

static void	draw_outline(uint32_t *canvas, int x, int y, int width,
	int height, int thickness, uint32_t pixel)
{
	draw_rect(canvas, x, y, width, thickness, pixel);
	draw_rect(canvas, x, y + height - thickness, width, thickness, pixel);
	draw_rect(canvas, x, y, thickness, height, pixel);
	draw_rect(canvas, x + width - thickness, y, thickness, height, pixel);
}

static void	pixel_asset_destroy(pixel_asset_t *asset)
{
	free(asset->pixels);
	memset(asset, 0, sizeof(*asset));
}

static bool	pixel_buffer_bytes(int width, int height, size_t *bytes)
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
	if (!pixel_buffer_bytes(asset->width, asset->height, &pixel_bytes))
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

static bool	pixel_asset_load(const char *path, pixel_asset_t *asset)
{
	return (pixel_asset_load_sized(path, 0, 0, asset));
}

/* The supplied Solo background has one transparent vertical export seam.
 * A screen backdrop is contractually opaque, so copy a neighboring source
 * pixel instead of allowing the seam to scale into a black line. */
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

static bool	asset_dimensions_are(const pixel_asset_t *asset, int width,
	int height)
{
	return (asset->width == width && asset->height == height);
}

/* AI-assisted: move the supplied board frame down by five source pixels so
 * its 160x320 interior starts on the 16px gameplay grid. The exact authored
 * RGBA pixels are copied; no mask tint or replacement border is generated. */
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

static void	set_asset_error(solo_render_t *solo, const char *message,
	const char *path)
{
	if (path == NULL)
		snprintf(solo->asset_error, sizeof(solo->asset_error), "%s", message);
	else
		snprintf(solo->asset_error, sizeof(solo->asset_error), "%s: %s",
			message, path);
}

static bool	load_solo_assets(solo_render_t *solo)
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
		set_asset_error(solo, "Could not load Solo background",
			SOLO_BACKGROUND_PATH);
	if (loaded)
		repair_background_alpha(&background);
	if (loaded && !pixel_asset_load(DEFAULT_HUD_PATH, &hud))
	{
		loaded = false;
		set_asset_error(solo, "Could not load HUD", DEFAULT_HUD_PATH);
	}
	else if (loaded && !asset_dimensions_are(&hud, SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT))
	{
		loaded = false;
		set_asset_error(solo, "HUD must be 512x384", DEFAULT_HUD_PATH);
	}
	if (loaded && !pixel_asset_load(DEFAULT_MIRURUN_PATH, &mirurun))
	{
		loaded = false;
		set_asset_error(solo, "Could not load Mirurun", DEFAULT_MIRURUN_PATH);
	}
	if (loaded && !pixel_asset_load(DEFAULT_TILE_PATH, &tiles))
	{
		loaded = false;
		set_asset_error(solo, "Could not load tile atlas", DEFAULT_TILE_PATH);
	}
	if (loaded && !asset_dimensions_are(&tiles, TILE_SOURCE_SIZE,
			TILE_SOURCE_STRIDE * TILE_ATLAS_COUNT))
	{
		loaded = false;
		set_asset_error(solo, "Tile atlas must be 16x180", DEFAULT_TILE_PATH);
	}
	if (loaded && !pixel_asset_load(SHARED_FONT_MASK_PATH, &font))
	{
		loaded = false;
		set_asset_error(solo, "Could not load font mask", SHARED_FONT_MASK_PATH);
	}
	if (loaded && !asset_dimensions_are(&font,
			FONT_COLUMNS * FONT_GLYPH_WIDTH, FONT_ROWS * FONT_GLYPH_HEIGHT))
	{
		loaded = false;
		set_asset_error(solo, "Font mask must be 128x96", SHARED_FONT_MASK_PATH);
	}
	if (loaded && !pixel_asset_load(SHARED_NUMBERS_MASK_PATH, &numbers))
	{
		loaded = false;
		set_asset_error(solo, "Could not load number mask",
			SHARED_NUMBERS_MASK_PATH);
	}
	if (loaded && !asset_dimensions_are(&numbers,
			NUMBER_GLYPH_COUNT * NUMBER_SLOT_WIDTH, NUMBER_GLYPH_HEIGHT))
	{
		loaded = false;
		set_asset_error(solo, "Number mask must be 140x16",
			SHARED_NUMBERS_MASK_PATH);
	}
	if (loaded && !pixel_buffer_bytes(SOLO_CANVAS_WIDTH,
			SOLO_CANVAS_HEIGHT, &canvas_bytes))
	{
		loaded = false;
		set_asset_error(solo, "Solo canvas dimensions overflow", NULL);
	}
	if (loaded)
	{
		solo->static_pixels = malloc(canvas_bytes);
		solo->frame_pixels = malloc(canvas_bytes);
		if (solo->static_pixels == NULL || solo->frame_pixels == NULL)
		{
			loaded = false;
			set_asset_error(solo, "Could not allocate Solo canvas", NULL);
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

static int	text_width(const char *text, int glyph_width, int spacing)
{
	int	length;

	length = (int)strlen(text);
	if (length == 0)
		return (0);
	return (length * glyph_width + (length - 1) * spacing);
}

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

static void	draw_text_shadowed(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint)
{
	draw_text(canvas, solo, text, x + 1, y + 1, glyph_width, glyph_height,
		spacing, g_dark, 190);
	draw_text(canvas, solo, text, x, y, glyph_width, glyph_height,
		spacing, tint, 255);
}

static void	draw_text_centered(uint32_t *canvas, const solo_render_t *solo,
	const char *text, int center_x, int y, int glyph_width, int glyph_height,
	int spacing, color_t tint)
{
	int	width;

	width = text_width(text, glyph_width, spacing);
	draw_text_shadowed(canvas, solo, text, center_x - width / 2, y,
		glyph_width, glyph_height, spacing, tint);
}

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

static int	tile_index_from_piece(t_piece_type type)
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
				pixel = ghost_tile_pixel(pixel, source_x, source_y);
			else
				pixel = with_opacity(pixel, opacity);
			put_pixel(canvas, x + draw_x, y + draw_y, pixel);
			draw_x++;
		}
		draw_y++;
	}
}

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
			draw_tile(canvas, solo, tile_index_from_piece(piece->type),
				HUD_BOARD_X + cols[index] * HUD_TILE_SIZE,
				HUD_BOARD_Y + rows[index] * HUD_TILE_SIZE,
				HUD_TILE_SIZE, opacity, ghost);
		index++;
	}
}

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
					tile_index_from_piece((t_piece_type)cell.color),
					HUD_BOARD_X + col * HUD_TILE_SIZE,
					HUD_BOARD_Y + row * HUD_TILE_SIZE,
					HUD_TILE_SIZE, 255, false);
			col++;
		}
		row++;
	}
}

static void	draw_preview_piece(uint32_t *canvas,
	const solo_render_t *solo, t_piece_type type, int slot_x, int slot_width)
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
	origin_x = slot_x + (slot_width
		- (max_col - min_col + 1) * PREVIEW_TILE_SIZE) / 2;
	origin_y = HUD_NEXT_Y + (HUD_NEXT_HEIGHT
		- (max_row - min_row + 1) * PREVIEW_TILE_SIZE) / 2;
	index = 0;
	while (index < 4)
	{
		draw_tile(canvas, solo, tile_index_from_piece(type),
			origin_x + (cols[index] - min_col) * PREVIEW_TILE_SIZE,
			origin_y + (rows[index] - min_row) * PREVIEW_TILE_SIZE,
			PREVIEW_TILE_SIZE, 255, false);
		index++;
	}
}

static void	draw_next_queue(uint32_t *canvas, const solo_render_t *solo,
	const solo_game_t *game)
{
	int	index;
	int	slot_x;
	int	next_slot_x;

	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		slot_x = HUD_NEXT_X + index * HUD_NEXT_WIDTH / SOLO_NEXT_COUNT;
		next_slot_x = HUD_NEXT_X
			+ (index + 1) * HUD_NEXT_WIDTH / SOLO_NEXT_COUNT;
		draw_preview_piece(canvas, solo, game->next[index], slot_x,
			next_slot_x - slot_x);
		index++;
	}
}

static void	draw_crystal_meter(uint32_t *canvas, const solo_game_t *game)
{
	int	segment;
	int	segment_height;
	int	y;

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
}

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

/* AI-assisted: scale long score/award strings before centering so their
 * shadows remain inside the independently refreshed 160px score plane. */
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

static void	draw_score_number(uint32_t *canvas, const solo_render_t *solo,
	uint64_t score)
{
	char	score_text[32];

	snprintf(score_text, sizeof(score_text), "%010" PRIu64, score);
	draw_numbers_centered_fit(canvas, solo, score_text, HUD_SCORE_Y + 23,
		g_pink);
}

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


#define SOLO_NEXT_X 80
#define SOLO_NEXT_Y 0
#define SOLO_NEXT_WIDTH 160
#define SOLO_NEXT_HEIGHT 48
#define SOLO_METER_X 48
#define SOLO_METER_Y 32
#define SOLO_METER_WIDTH 16
#define SOLO_METER_HEIGHT 336
#define SOLO_MIRURUN_X 272
#define SOLO_MIRURUN_Y 32
#define SOLO_MIRURUN_WIDTH 160
#define SOLO_MIRURUN_HEIGHT 160
#define SOLO_SCORE_HEADER_Y 192
#define SOLO_SCORE_HEADER_HEIGHT 32
#define SOLO_SCORE_VALUE_Y 224
#define SOLO_SCORE_VALUE_HEIGHT 32
#define SOLO_SCORE_STATS_Y 256
#define SOLO_SCORE_STATS_HEIGHT 64
#define SOLO_SCORE_EVENT_Y 320
#define SOLO_SCORE_EVENT_HEIGHT 48
#define SOLO_CONTROLS_X 80
#define SOLO_CONTROLS_WIDTH 352
#define SOLO_BOARD_WIDTH 160
#define SOLO_BOARD_HEIGHT 320
#define SOLO_MAX_ROW_RUNS ((BOARD_WIDTH + 1) / 2)

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

static void	destroy_board_planes(solo_render_t *solo)
{
	destroy_plane(&solo->board_overlay_plane);
	destroy_board_tiles(solo);
}

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

/* A 16px HUD tile always maps to an integral cell rectangle. Its physical
 * width and height are kept nearly equal so the board remains square. */
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
	text = "ARROWS MOVE | UP/X CW | Z CCW | SPACE DROP | P PAUSE | ESC HOME";
	if (cols < strlen(text))
		text = "ARROWS MOVE | X CW | SPACE DROP | P PAUSE | ESC HOME";
	if (cols < strlen(text))
		text = "ARROWS MOVE | SPACE DROP | ESC HOME";
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

static void	set_standard_backdrop(render_ctx_t *ctx)
{
	uint64_t	channels;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 7, 13, 23);
	(void)ncchannels_set_bg_rgb8(&channels, 7, 13, 23);
	(void)ncplane_set_base(ctx->std, " ", 0, channels);
	ncplane_erase(ctx->std);
}

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
		return (tile_index_from_piece((t_piece_type)cell.color));
	return (-1);
}

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

static uint64_t	score_stats_signature(const solo_game_t *game)
{
	uint64_t	hash;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, (uint64_t)(game->scoring.combo + 1));
	hash = hash_value(hash, (uint64_t)game->level);
	hash = hash_value(hash, (uint64_t)game->total_lines);
	return (hash);
}

static uint64_t	score_event_signature(const solo_game_t *game)
{
	uint64_t	hash;

	hash = UINT64_C(1469598103934665603);
	hash = hash_value(hash, game->scoring.back_to_back);
	hash = hash_value(hash, (uint64_t)game->last_lines);
	hash = hash_value(hash, (uint64_t)game->last_spin);
	hash = hash_value(hash, game->last_perfect_clear);
	hash = hash_value(hash, game->last_score.total_awarded);
	return (hash);
}

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
				pixel = ghost_tile_pixel(pixel, x, y);
			destination[(size_t)y * destination_stride + x] = pixel;
			x++;
		}
		y++;
	}
}

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

static bool	piece_rectangles_overlap(const piece_geometry_t *first,
	const piece_geometry_t *second)
{
	return (first->min_col <= second->max_col
		&& first->max_col >= second->min_col
		&& first->min_row <= second->max_row
		&& first->max_row >= second->min_row);
}

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

/* AI-assisted: when the active and ghost rectangles overlap, compose both into
 * one bitmap so terminal image elision cannot make the ghost flicker away. */
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
	if (!pixel_buffer_bytes(width, height, &pixel_bytes))
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

static struct ncplane	*create_atomic_piece_plane(render_ctx_t *ctx,
	solo_render_t *solo, const piece_geometry_t *geometry, int tile_index,
	bool ghost)
{
	uint32_t	pixels[4 * 4 * TILE_SOURCE_SIZE * TILE_SOURCE_SIZE];
	int			width;
	int			height;
	int			index;

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
	return (create_pixel_plane_at(ctx, solo, pixels, width, height, width,
			HUD_BOARD_X + geometry->min_col * HUD_TILE_SIZE,
			HUD_BOARD_Y + geometry->min_row * HUD_TILE_SIZE));
}

static int	position_atomic_piece(render_ctx_t *ctx, solo_render_t *solo,
	struct ncplane **plane, uint64_t *cached_signature,
	const piece_geometry_t *geometry, int tile_index, bool ghost)
{
	uint64_t	signature;
	int			y;
	int			x;
	int			old_y;
	int			old_x;

	if (geometry->min_col < 0 || geometry->max_col >= BOARD_WIDTH
		|| geometry->min_row < 0 || geometry->max_row >= BOARD_HEIGHT)
	{
		if (*plane == NULL)
			return (0);
		destroy_plane(plane);
		*cached_signature = UINT64_MAX;
		return (1);
	}
	signature = piece_shape_signature(geometry, tile_index, ghost);
	if (*plane == NULL || signature != *cached_signature)
	{
		destroy_plane(plane);
		*plane = create_atomic_piece_plane(ctx, solo, geometry,
			tile_index, ghost);
		if (*plane == NULL)
			return (-1);
		*cached_signature = signature;
		return (1);
	}
	y = solo->canvas_row + (HUD_BOARD_Y / HUD_TILE_SIZE
		+ geometry->min_row) * solo->tile_rows;
	x = solo->canvas_col + (HUD_BOARD_X / HUD_TILE_SIZE
		+ geometry->min_col) * solo->tile_cols;
	ncplane_yx(*plane, &old_y, &old_x);
	if (old_y == y && old_x == x)
		return (0);
	if (ncplane_move_yx(*plane, y, x) != 0)
		return (-1);
	return (1);
}

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
	tile_index = tile_index_from_piece(game->active.type);
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

static void	compose_hud_frame(solo_render_t *solo,
	const solo_game_t *game)
{
	memcpy(solo->frame_pixels, solo->static_pixels,
		(size_t)SOLO_CANVAS_WIDTH * SOLO_CANVAS_HEIGHT
		* sizeof(*solo->frame_pixels));
	draw_next_queue(solo->frame_pixels, solo, game);
	draw_crystal_meter(solo->frame_pixels, game);
	draw_score_panel(solo->frame_pixels, solo, game);
}

static int	update_hud_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	uint64_t	signature;
	int			changed;

	compose_hud_frame(solo, game);
	changed = 0;
	signature = next_frame_signature(game);
	if (signature != solo->next_signature)
	{
		if (!update_pixel_region(ctx, solo, &solo->next_plane,
				SOLO_NEXT_X, SOLO_NEXT_Y, SOLO_NEXT_WIDTH, SOLO_NEXT_HEIGHT))
			return (-1);
		solo->next_signature = signature;
		changed = 1;
	}
	if ((uint64_t)game->crystal_charge != solo->meter_signature)
	{
		if (!update_pixel_region(ctx, solo, &solo->meter_plane,
				SOLO_METER_X, SOLO_METER_Y,
				SOLO_METER_WIDTH, SOLO_METER_HEIGHT))
			return (-1);
		solo->meter_signature = (uint64_t)game->crystal_charge;
		changed = 1;
	}
	if (solo->mirurun_plane == NULL)
	{
		if (!update_pixel_region(ctx, solo, &solo->mirurun_plane,
				SOLO_MIRURUN_X, SOLO_MIRURUN_Y,
				SOLO_MIRURUN_WIDTH, SOLO_MIRURUN_HEIGHT))
			return (-1);
		changed = 1;
	}
	if (solo->score_header_plane == NULL)
	{
		if (!update_pixel_region(ctx, solo, &solo->score_header_plane,
				HUD_SCORE_X, SOLO_SCORE_HEADER_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_HEADER_HEIGHT))
			return (-1);
		changed = 1;
	}
	if (game->scoring.total != solo->score_value_signature)
	{
		if (!update_pixel_region(ctx, solo, &solo->score_value_plane,
				HUD_SCORE_X, SOLO_SCORE_VALUE_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_VALUE_HEIGHT))
			return (-1);
		solo->score_value_signature = game->scoring.total;
		changed = 1;
	}
	signature = score_stats_signature(game);
	if (signature != solo->score_stats_signature)
	{
		if (!update_pixel_region(ctx, solo, &solo->score_stats_plane,
				HUD_SCORE_X, SOLO_SCORE_STATS_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_STATS_HEIGHT))
			return (-1);
		solo->score_stats_signature = signature;
		changed = 1;
	}
	signature = score_event_signature(game);
	if (signature != solo->score_event_signature)
	{
		if (!update_pixel_region(ctx, solo, &solo->score_event_plane,
				HUD_SCORE_X, SOLO_SCORE_EVENT_Y,
				HUD_SCORE_WIDTH, SOLO_SCORE_EVENT_HEIGHT))
			return (-1);
		solo->score_event_signature = signature;
		changed = 1;
	}
	if (solo->controls_plane == NULL)
	{
		if (!create_controls_plane(ctx, solo))
			return (-1);
		changed = 1;
	}
	return (changed);
}

static int	update_board_region(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	t_piece		ghost;
	uint64_t	signature;
	int			result;
	int			changed;

	if (game->paused || game->phase == SOLO_GAME_OVER)
	{
		signature = board_overlay_signature(game);
		if (solo->board_overlay_plane != NULL
			&& signature == solo->overlay_signature)
			return (0);
		destroy_board_tiles(solo);
		draw_settled_board(solo->frame_pixels, solo, game);
		if (game->phase == SOLO_ACTIVE)
		{
			ghost = solo_game_ghost(game);
			draw_piece(solo->frame_pixels, solo, &ghost, 255u, true);
			draw_piece(solo->frame_pixels, solo, &game->active, 255u, false);
		}
		draw_overlays(solo->frame_pixels, solo, game);
		if (!update_pixel_region(ctx, solo, &solo->board_overlay_plane,
				HUD_BOARD_X, HUD_BOARD_Y,
				SOLO_BOARD_WIDTH, SOLO_BOARD_HEIGHT))
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

/* AI-assisted: static art is cell-rendered once. Moving tetrominoes use
 * atomic planes, merged when active/ghost bounds overlap, so tiles cannot
 * shear. */
static int	update_foreground_regions(render_ctx_t *ctx, solo_render_t *solo,
	const solo_game_t *game)
{
	int	result;
	int	changed;

	changed = update_hud_regions(ctx, solo, game);
	if (changed < 0)
		return (-1);
	result = update_board_region(ctx, solo, game);
	if (result < 0)
		return (-1);
	return (changed | result);
}

static bool	create_solo_planes(render_ctx_t *ctx, solo_render_t *solo)
{
	set_standard_backdrop(ctx);
	if (!create_background_plane(ctx, solo))
		return (false);
	solo->planes_ready = true;
	return (true);
}

void	render_solo_create(render_ctx_t *ctx, solo_render_t *solo)
{
	memset(solo, 0, sizeof(*solo));
	reset_render_signatures(solo);
	calculate_solo_layout(ctx, solo);
	if (!notcurses_canpixel(ctx->nc))
	{
		set_asset_error(solo,
			"Solo requires Kitty, Sixel, or another pixel-graphics terminal",
			NULL);
		return ;
	}
	solo->assets_ready = load_solo_assets(solo);
	if (solo->layout_valid && solo->assets_ready
		&& !create_solo_planes(ctx, solo))
		set_asset_error(solo, "Terminal rejected the Solo foreground bitmap",
			NULL);
}

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
		goto render_failure;
	changed |= result;
	if (changed > 0 && notcurses_render(ctx->nc) != 0)
		goto render_failure;
	return ;
render_failure:
	set_asset_error(solo, "Notcurses could not present the Solo frame", NULL);
	destroy_solo_planes(solo);
	set_standard_backdrop(ctx);
	if (draw_status_message(ctx, solo, solo->asset_error))
		(void)notcurses_render(ctx->nc);
}

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
		set_asset_error(solo,
			"Solo requires Kitty, Sixel, or another pixel-graphics terminal",
			NULL);
		return ;
	}
	if (solo->layout_valid && solo->assets_ready
		&& !create_solo_planes(ctx, solo))
		set_asset_error(solo, "Terminal rejected the Solo foreground bitmap",
			NULL);
}

static uint64_t	monotonic_ms(void)
{
	struct timespec	now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
}

static bool	dispatch_game_key(solo_game_t *game, uint32_t key)
{
	if (key == NCKEY_LEFT)
		return (solo_game_apply_action(game, SOLO_MOVE_LEFT));
	else if (key == NCKEY_RIGHT)
		return (solo_game_apply_action(game, SOLO_MOVE_RIGHT));
	else if (key == NCKEY_UP || key == 'x' || key == 'X')
		return (solo_game_apply_action(game, SOLO_ROTATE_CW));
	else if (key == 'z' || key == 'Z')
		return (solo_game_apply_action(game, SOLO_ROTATE_CCW));
	else if (key == NCKEY_DOWN)
		return (solo_game_apply_action(game, SOLO_SOFT_DROP));
	else if (key == ' ')
		return (solo_game_apply_action(game, SOLO_HARD_DROP));
	return (false);
}

/* notcurses_get() supports absolute deadlines, but on macOS 3.0.17 its
 * timed condition-wait path can consume a core. The documented input-ready
 * fd integrates cleanly with poll(), which sleeps in the kernel and keeps
 * gameplay deadlines relative to the state timer. */
static uint32_t	wait_solo_input(render_ctx_t *ctx, int timeout_ms,
	ncinput *input, int *input_errno)
{
	struct pollfd	poll_fd;
	uint32_t		key;
	int				result;

	poll_fd.fd = notcurses_inputready_fd(ctx->nc);
	poll_fd.events = POLLIN;
	poll_fd.revents = 0;
	if (poll_fd.fd < 0)
	{
		*input_errno = EIO;
		return ((uint32_t)-1);
	}
	errno = 0;
	result = poll(&poll_fd, 1, timeout_ms);
	*input_errno = errno;
	if (result < 0)
		return ((uint32_t)-1);
	if (result == 0)
		return (0);
	if ((poll_fd.revents & POLLIN) == 0)
	{
		*input_errno = EIO;
		return ((uint32_t)-1);
	}
	errno = 0;
	key = notcurses_get_nblock(ctx->nc, input);
	*input_errno = errno;
	return (key);
}

static bool	terminal_geometry_changed(const render_ctx_t *ctx)
{
	struct winsize	terminal;
	unsigned		plane_rows;
	unsigned		plane_cols;

	memset(&terminal, 0, sizeof(terminal));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal) != 0
		|| terminal.ws_row == 0 || terminal.ws_col == 0)
		return (false);
	ncplane_dim_yx(ctx->std, &plane_rows, &plane_cols);
	return (terminal.ws_row != plane_rows || terminal.ws_col != plane_cols);
}

static bool	solo_display_ready(const solo_render_t *solo)
{
	return (solo->layout_valid && solo->assets_ready && solo->planes_ready);
}

static uint32_t	new_game_seed(void)
{
	return ((uint32_t)(monotonic_ms() ^ (uint64_t)getpid()));
}

static bool	handle_solo_key(solo_game_t *game, uint32_t key,
	const ncinput *input, bool display_ready, bool *resize_pending,
	bool *state_changed)
{
	if (input->evtype == NCTYPE_RELEASE)
		return (false);
	if (key == NCKEY_ESC || key == NCKEY_EOF || key == 'q' || key == 'Q')
		return (true);
	if (key == NCKEY_RESIZE || key == 12u)
	{
		*resize_pending = true;
		return (false);
	}
	if ((key == 'r' || key == 'R') && game->phase == SOLO_GAME_OVER)
	{
		solo_game_init(game, new_game_seed());
		*state_changed = true;
		return (false);
	}
	if (key == 'p' || key == 'P')
	{
		solo_game_toggle_pause(game);
		*state_changed = true;
		return (false);
	}
	if (display_ready && !*resize_pending)
		*state_changed = dispatch_game_key(game, key) || *state_changed;
	return (false);
}

static int	restore_home(render_ctx_t *ctx)
{
	ncplane_erase(ctx->std);
	if (render_background_replace(ctx, SPLASH_ASSET_PATH, false) < 0)
		return (-1);
	render_menu_create(ctx);
	return (0);
}

/* AI-assisted: temporary local authority loop. It never performs network or
 * IPC calls; migration replaces apply/update with HTTTP actions and STATE. */
int	solo_mode_run(render_ctx_t *ctx)
{
	solo_game_t	game;
	solo_render_t	solo;
	ncinput			input;
	uint32_t		key;
	uint64_t		previous_ms;
	uint64_t		now_ms;
	int				elapsed_ms;
	int				input_errno;
	int				wake_ms;
	int				wait_ms;
	bool			leave;
	bool			resize_pending;
	bool			display_ready;
	bool			needs_draw;

	render_menu_destroy(ctx);
	render_background_destroy(ctx);
	ncplane_erase(ctx->std);
	if (render_geometry_refresh(ctx, false) < 0)
	{
		(void)restore_home(ctx);
		return (-1);
	}
	solo_game_init(&game, new_game_seed());
	render_solo_create(ctx, &solo);
	render_solo_draw(ctx, &solo, &game);
	previous_ms = monotonic_ms();
	leave = false;
	while (!leave)
	{
		display_ready = solo_display_ready(&solo);
		wake_ms = display_ready ? solo_game_next_wake_ms(&game) : -1;
		wait_ms = wake_ms;
		if (wait_ms < 0 || wait_ms > SOLO_RESIZE_POLL_MS)
			wait_ms = SOLO_RESIZE_POLL_MS;
		key = wait_solo_input(ctx, wait_ms, &input, &input_errno);
		now_ms = monotonic_ms();
		if (now_ms < previous_ms)
			elapsed_ms = 0;
		else if (now_ms - previous_ms > SOLO_MAX_CATCHUP_MS)
			elapsed_ms = SOLO_MAX_CATCHUP_MS;
		else
			elapsed_ms = (int)(now_ms - previous_ms);
		previous_ms = now_ms;
		needs_draw = false;
		if (display_ready)
			needs_draw = solo_game_update(&game, elapsed_ms);
		resize_pending = terminal_geometry_changed(ctx);
		if (key == (uint32_t)-1)
		{
			if (input_errno == EINTR)
				key = 0;
			else
				break ;
		}
		while (key != 0)
		{
			if (handle_solo_key(&game, key, &input, display_ready,
					&resize_pending, &needs_draw))
			{
				leave = true;
				break ;
			}
			errno = 0;
			key = notcurses_get_nblock(ctx->nc, &input);
			if (key == (uint32_t)-1)
			{
				if (errno != EINTR)
					leave = true;
				break ;
			}
		}
		if (leave)
			break ;
		if (resize_pending)
		{
			render_solo_resize(ctx, &solo);
			needs_draw = true;
			/* Resizing is an explicit gameplay pause. Do not charge the
			 * terminal's protocol negotiation/reflow time to gravity. */
			previous_ms = monotonic_ms();
		}
		if (needs_draw)
			render_solo_draw(ctx, &solo, &game);
	}
	render_solo_destroy(&solo);
	return (restore_home(ctx));
}
