#include "render_multiplayer_match_pixel_draw.h"

# define MATCH_FONT_COLUMNS 16
# define MATCH_FONT_ROWS 6
# define MATCH_FONT_WIDTH 8
# define MATCH_FONT_HEIGHT 16
# define MATCH_FONT_INK_Y 4

static const t_color g_dark = {7, 13, 23};
static const t_color g_panel = {20, 9, 34};
static const t_color g_panel_light = {39, 20, 58};

static void	cache_portrait_pixels(t_render_ctx *ctx);
static void	draw_text(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, const char *text, int x, int y, int glyph_size,
				int spacing, t_color tint);
static void	draw_glyph(t_render_ctx *ctx, uint32_t *pixels, int width,
				int height, int glyph, int x, int y, int glyph_size, t_color tint);
static void	blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha);
static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
				t_color tint, unsigned alpha);
static int	text_width(const char *text, int glyph, int spacing);
static int	min_int(int left, int right);
static int	max_int(int left, int right);
static int	clamp_int(int value, int minimum, int maximum);

bool mp_match_pixel_load_portrait(t_render_ctx *ctx, const char *path)
{
	if (path == NULL || path[0] == '\0')
		return (false);
	if (ctx->mp_match_portrait_visual != NULL
		&& strcmp(ctx->mp_match_portrait_source, path) == 0)
		return (true);
	if (ctx->mp_match_portrait_visual != NULL)
		ncvisual_destroy(ctx->mp_match_portrait_visual);
	ctx->mp_match_portrait_visual = ncvisual_from_file(path);
	if (ctx->mp_match_portrait_visual == NULL)
	{
		ctx->mp_match_portrait_source[0] = '\0';
		return (false);
	}
	snprintf(ctx->mp_match_portrait_source,
		sizeof(ctx->mp_match_portrait_source), "%s", path);
	cache_portrait_pixels(ctx);
	return (true);
}

/**
 * @brief Reads the loaded portrait out of its visual into plain memory.
 *
 * One pass when the portrait changes, so drawing it is an array index rather
 * than a library call per output pixel. A failed read leaves the cache empty
 * and the drawing path falls back to sampling the visual.
 */
static void	cache_portrait_pixels(t_render_ctx *ctx)
{
	ncvgeom	geometry;
	int		x;
	int		y;

	free(ctx->mp_match_portrait_pixels);
	ctx->mp_match_portrait_pixels = NULL;
	ctx->mp_match_portrait_px_width = 0;
	ctx->mp_match_portrait_px_height = 0;
	memset(&geometry, 0, sizeof(geometry));
	if (ncvisual_geom(NULL, ctx->mp_match_portrait_visual, NULL,
			&geometry) != 0 || geometry.pixx == 0 || geometry.pixy == 0)
		return ;
	ctx->mp_match_portrait_pixels = malloc((size_t)geometry.pixx
			* geometry.pixy * sizeof(*ctx->mp_match_portrait_pixels));
	if (ctx->mp_match_portrait_pixels == NULL)
		return ;
	y = 0;
	while (y < (int)geometry.pixy)
	{
		x = 0;
		while (x < (int)geometry.pixx)
		{
			if (ncvisual_at_yx(ctx->mp_match_portrait_visual, (unsigned)y,
					(unsigned)x, &ctx->mp_match_portrait_pixels[(size_t)y
					* geometry.pixx + x]) < 0)
				ctx->mp_match_portrait_pixels[(size_t)y * geometry.pixx + x]
					= 0;
			x++;
		}
		y++;
	}
	ctx->mp_match_portrait_px_width = (int)geometry.pixx;
	ctx->mp_match_portrait_px_height = (int)geometry.pixy;
}

/**
 * @brief Loads the authored ability-popover frame once and reads it out.
 */
bool	mp_match_pixel_load_popover(t_render_ctx *ctx, const char *path)
{
	ncvgeom	geometry;
	int		x;
	int		y;

	if (path == NULL || path[0] == '\0')
		return (false);
	if (ctx->mp_match_popover_pixels != NULL)
		return (true);
	if (ctx->mp_match_popover_visual == NULL)
		ctx->mp_match_popover_visual = ncvisual_from_file(path);
	if (ctx->mp_match_popover_visual == NULL)
		return (false);
	memset(&geometry, 0, sizeof(geometry));
	if (ncvisual_geom(NULL, ctx->mp_match_popover_visual, NULL, &geometry) != 0
		|| geometry.pixx == 0 || geometry.pixy == 0)
		return (false);
	ctx->mp_match_popover_pixels = malloc((size_t)geometry.pixx * geometry.pixy
			* sizeof(*ctx->mp_match_popover_pixels));
	if (ctx->mp_match_popover_pixels == NULL)
		return (false);
	y = 0;
	while (y < (int)geometry.pixy)
	{
		x = 0;
		while (x < (int)geometry.pixx)
		{
			if (ncvisual_at_yx(ctx->mp_match_popover_visual, (unsigned)y,
					(unsigned)x, &ctx->mp_match_popover_pixels[(size_t)y
					* geometry.pixx + x]) < 0)
				ctx->mp_match_popover_pixels[(size_t)y * geometry.pixx + x] = 0;
			x++;
		}
		y++;
	}
	ctx->mp_match_popover_px_width = (int)geometry.pixx;
	ctx->mp_match_popover_px_height = (int)geometry.pixy;
	return (true);
}

/**
 * @brief Stretches the popover frame over the rectangle it decorates.
 *
 * Stretched rather than fitted: the frame is a border, so it has to reach the
 * edges of the box whose contents it is framing.
 */
void	mp_match_pixel_draw_popover_art(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *rect)
{
	uint32_t	source;
	int			x;
	int			y;

	if (ctx->mp_match_popover_pixels == NULL
		|| ctx->mp_match_popover_px_width <= 0
		|| ctx->mp_match_popover_px_height <= 0
		|| rect->width <= 0 || rect->height <= 0)
		return ;
	y = 0;
	while (y < rect->height)
	{
		x = 0;
		while (x < rect->width)
		{
			source = ctx->mp_match_popover_pixels[(size_t)((int64_t)y
					* ctx->mp_match_popover_px_height / rect->height)
				* ctx->mp_match_popover_px_width + (int64_t)x
				* ctx->mp_match_popover_px_width / rect->width];
			put_pixel(pixels, width, height, rect->x + x, rect->y + y,
				(t_color){ncpixel_r(source), ncpixel_g(source),
				ncpixel_b(source)}, ncpixel_a(source));
			x++;
		}
		y++;
	}
}

/**
 * @brief Draws the equipped portrait, scaled to fit and keeping its ratio.
 */
void	mp_match_pixel_draw_portrait(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *rect)
{
	t_mp_rect	destination;
	uint32_t	source;
	int			source_width;
	int			source_height;
	int			x;
	int			y;

	source_width = ctx->mp_match_portrait_px_width;
	source_height = ctx->mp_match_portrait_px_height;
	if (ctx->mp_match_portrait_pixels == NULL || source_width <= 0
		|| source_height <= 0)
	{
		mp_match_pixel_draw_visual(pixels, width, height,
			ctx->mp_match_portrait_visual, rect);
		return ;
	}
	destination = *rect;
	if ((int64_t)rect->width * source_height
		> (int64_t)rect->height * source_width)
	{
		destination.width = (int)((int64_t)rect->height * source_width
				/ source_height);
		destination.x += (rect->width - destination.width) / 2;
	}
	else
	{
		destination.height = (int)((int64_t)rect->width * source_height
				/ source_width);
		destination.y += (rect->height - destination.height) / 2;
	}
	y = 0;
	while (y < destination.height)
	{
		x = 0;
		while (x < destination.width)
		{
			source = ctx->mp_match_portrait_pixels[(size_t)((int64_t)y
					* source_height / destination.height) * source_width
				+ (int64_t)x * source_width / destination.width];
			put_pixel(pixels, width, height, destination.x + x,
				destination.y + y, (t_color){ncpixel_r(source),
				ncpixel_g(source), ncpixel_b(source)}, ncpixel_a(source));
			x++;
		}
		y++;
	}
}

void mp_match_pixel_draw_visual(uint32_t *pixels, int width, int height,
	struct ncvisual *visual, const t_mp_rect *rect)
{
	ncvgeom geometry;
	t_mp_rect destination;
	uint32_t source;
	t_color tint;
	int x;
	int y;

	memset(&geometry, 0, sizeof(geometry));
	if (visual == NULL || ncvisual_geom(NULL, visual, NULL, &geometry) != 0
		|| geometry.pixx == 0 || geometry.pixy == 0)
		return ;
	destination = *rect;
	if ((uint64_t)rect->width * geometry.pixy
		> (uint64_t)rect->height * geometry.pixx)
	{
		destination.width = (int)((uint64_t)rect->height
			* geometry.pixx / geometry.pixy);
		destination.x += (rect->width - destination.width) / 2;
	}
	else
	{
		destination.height = (int)((uint64_t)rect->width
			* geometry.pixy / geometry.pixx);
		destination.y += (rect->height - destination.height) / 2;
	}
	y = 0;
	while (y < destination.height)
	{
		x = 0;
		while (x < destination.width)
		{
			if (ncvisual_at_yx(visual,
					(unsigned)((uint64_t)y * geometry.pixy / destination.height),
					(unsigned)((uint64_t)x * geometry.pixx / destination.width),
					&source) >= 0)
			{
				tint = (t_color){ncpixel_r(source), ncpixel_g(source),
					ncpixel_b(source)};
				put_pixel(pixels, width, height, destination.x + x,
					destination.y + y,
					tint, ncpixel_a(source));
			}
			x++;
		}
		y++;
	}
}

void mp_match_pixel_draw_panel(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect, t_color edge, unsigned alpha)
{
	t_mp_rect inner;
	int thickness;

	mp_match_pixel_fill_rect(pixels, width, height, rect, g_panel, alpha);
	thickness = clamp_int(min_int(rect->width, rect->height) / 40, 2, 6);
	mp_match_pixel_outline_rect(pixels, width, height, rect, thickness, edge, 255);
	inner = (t_mp_rect){rect->x + thickness * 2, rect->y + thickness * 2,
		rect->width - thickness * 4, rect->height - thickness * 4};
	if (inner.width > 0 && inner.height > 0)
		mp_match_pixel_outline_rect(pixels, width, height, &inner, 1, g_panel_light, 210);
}

void mp_match_pixel_draw_circle(uint32_t *pixels, int width, int height,
	int center_x, int center_y, int radius, t_color tint, unsigned alpha)
{
	int x;
	int y;

	y = -radius;
	while (y <= radius)
	{
		x = -radius;
		while (x <= radius)
		{
			if (x * x + y * y <= radius * radius)
				put_pixel(pixels, width, height, center_x + x, center_y + y,
					tint, alpha);
			x++;
		}
		y++;
	}
}

/*
 * An opaque fill is the same value in every pixel, so it does not need the
 * blend at all. That matters because the largest fill in the match is the
 * board's own backing: over a million pixels, each of which used to be a call
 * through put_pixel into blend_pixel.
 */
void mp_match_pixel_fill_rect(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect, t_color tint, unsigned alpha)
{
	uint32_t	opaque;
	int			first;
	int			last;
	int			x;
	int			y;

	first = max_int(0, rect->x);
	last = min_int(rect->x + rect->width, width);
	opaque = ncpixel(tint.r, tint.g, tint.b);
	ncpixel_set_a(&opaque, 255u);
	y = max_int(0, rect->y);
	while (y < rect->y + rect->height && y < height)
	{
		x = first;
		while (x < last)
		{
			if (alpha >= 255u)
				pixels[(size_t)y * width + x] = opaque;
			else
				put_pixel(pixels, width, height, x, y, tint, alpha);
			x++;
		}
		y++;
	}
}

void mp_match_pixel_outline_rect(uint32_t *pixels, int width, int height,
	const t_mp_rect *rect, int thickness, t_color tint, unsigned alpha)
{
	t_mp_rect line;

	line = (t_mp_rect){rect->x, rect->y, rect->width, thickness};
	mp_match_pixel_fill_rect(pixels, width, height, &line, tint, alpha);
	line.y = rect->y + rect->height - thickness;
	mp_match_pixel_fill_rect(pixels, width, height, &line, tint, alpha);
	line = (t_mp_rect){rect->x, rect->y, thickness, rect->height};
	mp_match_pixel_fill_rect(pixels, width, height, &line, tint, alpha);
	line.x = rect->x + rect->width - thickness;
	mp_match_pixel_fill_rect(pixels, width, height, &line, tint, alpha);
}

void mp_match_pixel_draw_text_box(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const char *text, const t_mp_rect *rect, int preferred,
	t_color tint, bool centered)
{
	int glyph;
	int spacing;
	int rendered;
	int x;
	int y;

	if (text == NULL || rect->width <= 0 || rect->height <= 0)
		return ;
	glyph = preferred;
	spacing = max_int(1, glyph / 5);
	while (glyph > 4 && text_width(text, glyph, spacing) > rect->width)
	{
		glyph--;
		spacing = max_int(1, glyph / 5);
	}
	rendered = text_width(text, glyph, spacing);
	x = rect->x;
	if (centered)
		x += (rect->width - rendered) / 2;
	y = rect->y + (rect->height - glyph) / 2;
	draw_text(ctx, pixels, width, height, text, x + 2, y + 2,
		glyph, spacing, g_dark);
	draw_text(ctx, pixels, width, height, text, x, y,
		glyph, spacing, tint);
}

static void draw_text(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, const char *text, int x, int y, int glyph_size,
	int spacing, t_color tint)
{
	int codepoint;
	int index;

	index = 0;
	while (text[index] != '\0')
	{
		codepoint = (unsigned char)text[index];
		if (codepoint < 32
			|| codepoint >= 32 + MATCH_FONT_COLUMNS * MATCH_FONT_ROWS)
			codepoint = '?';
		draw_glyph(ctx, pixels, width, height, codepoint - 32,
			x + index * (glyph_size + spacing), y, glyph_size, tint);
		index++;
	}
}

static void draw_glyph(t_render_ctx *ctx, uint32_t *pixels, int width,
	int height, int glyph, int x, int y, int glyph_size, t_color tint)
{
	uint32_t source;
	unsigned alpha;
	int source_x;
	int source_y;
	int draw_x;
	int draw_y;
	int glyph_height;

	glyph_height = glyph_size * (FONT_INK_BOTTOM - FONT_INK_TOP)
		/ FONT_INK_HEIGHT;
	draw_y = 0;
	while (draw_y < glyph_height)
	{
		source_y = FONT_INK_TOP + draw_y
			* (FONT_INK_BOTTOM - FONT_INK_TOP) / glyph_height;
		draw_x = 0;
		while (draw_x < glyph_size)
		{
			source_x = draw_x * MATCH_FONT_WIDTH / glyph_size;
			if (ncvisual_at_yx(ctx->mp_font_visual,
					(unsigned)((glyph / MATCH_FONT_COLUMNS)
						* MATCH_FONT_HEIGHT + source_y),
					(unsigned)((glyph % MATCH_FONT_COLUMNS)
						* MATCH_FONT_WIDTH + source_x), &source) >= 0)
			{
				alpha = ncpixel_a(source);
				if (alpha > 0)
					put_pixel(pixels, width, height, x + draw_x,
						y + draw_y - (MATCH_FONT_INK_Y - FONT_INK_TOP)
						* glyph_size / FONT_INK_HEIGHT, tint, alpha);
			}
			draw_x++;
		}
		draw_y++;
	}
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

static int text_width(const char *text, int glyph, int spacing)
{
	int length;

	length = (int)strlen(text);
	if (length == 0)
		return (0);
	return (length * glyph + (length - 1) * spacing);
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
