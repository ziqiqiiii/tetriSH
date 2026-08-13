#include "tetrisu.h"

# define MENU_FONT_BASE_GLYPH	28
# define MENU_FONT_BASE_SPACING	4
# define MENU_FONT_BASE_SHADOW	3
# define MENU_FONT_MIN_GLYPH	8

static const t_color	g_menu_colors[MENU_ITEM_COUNT] =
{
	{116, 235, 92},
	{244, 142, 219},
	{88, 193, 255},
	{255, 199, 82},
	{190, 151, 255}
};
static const t_color	g_menu_shadow = {18, 5, 24};

static bool	compose_menu_pixels(const t_pixel_asset *font,
					int width, int height,
					uint32_t **pixels);
static void	draw_menu_text(uint32_t *pixels, int canvas_width,
					int canvas_height, const t_pixel_asset *font,
					const char *text, int x, int y, int glyph_size, int spacing,
					t_color tint, unsigned opacity);
static void	draw_menu_glyph(uint32_t *pixels, int canvas_width,
					int canvas_height, const t_pixel_asset *font, int glyph,
					int dest_x, int dest_y, int glyph_size,
					t_color tint, unsigned opacity);
static void	draw_menu_texel(uint32_t *pixels, int canvas_width,
					int canvas_height, const t_pixel_asset *font,
					int source_y, int source_x, int dest_x, int dest_y,
					int cell_width, int cell_height, t_color tint,
					unsigned opacity);
static int	menu_ink_span(int units, int glyph_size);
static void	put_menu_pixel(uint32_t *pixels, int canvas_width,
					int canvas_height, int x, int y,
					t_color tint, unsigned alpha);
static struct ncplane	*blit_menu_pixels(t_render_ctx *ctx,
					const t_pixel_asset *font);
static int	selector_crop_cols(const t_render_ctx *ctx, int panel_x,
					int panel_cols);
static struct ncplane	*create_text_fallback(t_render_ctx *ctx);
static void	set_transparent_base(struct ncplane *plane);
static int	max_int(int left, int right);
static int	clamp_int(int value, int min, int max);

/**
 * @brief Creates one stationary high-resolution surface for all menu labels.
 *
 * Pixel-capable terminals receive a canvas composed at their exact physical
 * pixel geometry. The surface is created once and never touched by arrow-key
 * navigation, so the bunny can move independently without disturbing either
 * labels or the cell-rendered Gaiden background.
 *
 * @param ctx Active render context with fitted background geometry.
 * @return Owned label plane, or a terminal-text emergency fallback.
 */
struct ncplane	*render_menu_labels_create(t_render_ctx *ctx)
{
	t_pixel_asset	font;
	struct ncplane	*plane;

	if (!render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc)
		|| !render_font_mask_load(&font))
		return (create_text_fallback(ctx));
	plane = blit_menu_pixels(ctx, &font);
	render_font_mask_free(&font);
	if (plane == NULL)
		plane = create_text_fallback(ctx);
	if (plane != NULL && ctx->bg_plane != NULL)
		(void)ncplane_move_above(plane, ctx->bg_plane);
	return (plane);
}

/**
 * @brief Returns the exact terminal row used by a fallback menu label.
 *
 * Sharing this calculation with the compatibility selector prevents separate
 * rounding steps from placing the marker one row above or below its label.
 *
 * @param ctx Active render context with fitted background geometry.
 * @param index Zero-based menu item index.
 * @return Absolute terminal row for the label baseline.
 */
int	render_menu_label_y(const t_render_ctx *ctx, int index)
{
	double	ratio;
	int		panel_y;
	int		panel_rows;
	int		row;

	panel_y = ctx->bg_row
		+ (int)(ctx->bg_rows * MENU_PANEL_Y_RATIO + 0.5);
	panel_rows = (int)(ctx->bg_rows * MENU_PANEL_HEIGHT_RATIO + 0.5);
	if (panel_rows < 1)
		panel_rows = 1;
	ratio = (MENU_FIRST_Y_RATIO + index * MENU_STEP_Y_RATIO
			- MENU_PANEL_Y_RATIO) / MENU_PANEL_HEIGHT_RATIO;
	row = clamp_int((int)(ratio * panel_rows + 0.5), 0, panel_rows - 1);
	return (panel_y + row);
}

/**
 * @brief Decodes and validates the shared 16-by-6 ASCII glyph sheet.
 *
 * Public so every surface drawing atlas lettering — menus, notifications —
 * shares one loader and one validation of the sheet's geometry.
 *
 * @param font Receives the decoded RGBA mask; zeroed on failure.
 * @return true when the sheet decoded at its expected size.
 */
bool	render_font_mask_load(t_pixel_asset *font)
{
	struct ncvisual	*ncv;
	ncvgeom			geom;
	size_t			count;
	int				y;
	int				x;

	memset(font, 0, sizeof(*font));
	ncv = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (ncv == NULL)
		return (false);
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, ncv, NULL, &geom) != 0
		|| geom.pixx != FONT_COLUMNS * FONT_GLYPH_WIDTH
		|| geom.pixy != FONT_ROWS * FONT_GLYPH_HEIGHT)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	font->width = (int)geom.pixx;
	font->height = (int)geom.pixy;
	count = (size_t)font->width * font->height;
	font->pixels = malloc(count * sizeof(*font->pixels));
	if (font->pixels == NULL)
	{
		ncvisual_destroy(ncv);
		return (false);
	}
	y = 0;
	while (y < font->height)
	{
		x = 0;
		while (x < font->width)
		{
			if (ncvisual_at_yx(ncv, (unsigned)y, (unsigned)x,
					&font->pixels[(size_t)y * font->width + x]) < 0)
			{
				render_font_mask_free(font);
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
 * @brief Releases a glyph sheet decoded by render_font_mask_load().
 *
 * @param font Mask to release; safe to call on an already-empty mask.
 */
void	render_font_mask_free(t_pixel_asset *font)
{
	if (font == NULL)
		return ;
	free(font->pixels);
	memset(font, 0, sizeof(*font));
}

/**
 * @brief Composes left-aligned labels at exact destination pixel resolution.
 */
static bool	compose_menu_pixels(const t_pixel_asset *font,
	int width, int height, uint32_t **pixels)
{
	const char	*label;
	double		scale_x;
	double		scale_y;
	double		ratio;
	int			glyph_size;
	int			spacing;
	int			shadow;
	int			label_x;
	int			index;
	int			y;
	size_t		count;

	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height)
		return (false);
	count = (size_t)width * height;
	if (count > SIZE_MAX / sizeof(**pixels))
		return (false);
	*pixels = calloc(count, sizeof(**pixels));
	if (*pixels == NULL)
		return (false);
	scale_x = (double)width / (BACKGROUND_SOURCE_PIXELS_X
			* MENU_PANEL_WIDTH_RATIO);
	scale_y = (double)height / (BACKGROUND_SOURCE_PIXELS_Y
			* MENU_PANEL_HEIGHT_RATIO);
	glyph_size = max_int(MENU_FONT_MIN_GLYPH,
		(int)(MENU_FONT_BASE_GLYPH * (scale_x < scale_y
				? scale_x : scale_y) + 0.5));
	spacing = max_int(1, glyph_size * MENU_FONT_BASE_SPACING
		/ MENU_FONT_BASE_GLYPH);
	shadow = max_int(1, glyph_size * MENU_FONT_BASE_SHADOW
		/ MENU_FONT_BASE_GLYPH);
	ratio = (MENU_LABEL_LEFT_X_RATIO - MENU_PANEL_X_RATIO)
		/ MENU_PANEL_WIDTH_RATIO;
	label_x = clamp_int((int)(ratio * width + 0.5), shadow + 1,
		width - shadow - 1);
	index = 0;
	while (index < MENU_ITEM_COUNT)
	{
		label = menu_item_label(index);
		ratio = (MENU_FIRST_Y_RATIO + index * MENU_STEP_Y_RATIO
				- MENU_PANEL_Y_RATIO) / MENU_PANEL_HEIGHT_RATIO;
		y = (int)(ratio * height + 0.5) - glyph_size / 2;
		y = clamp_int(y, 1, height - glyph_size - shadow - 1);
		draw_menu_text(*pixels, width, height, font, label,
			label_x + shadow, y + shadow, glyph_size, spacing,
			g_menu_shadow, 245u);
		draw_menu_text(*pixels, width, height, font, label,
			label_x, y, glyph_size, spacing, g_menu_colors[index], 255u);
		index++;
	}
	return (true);
}

static void	draw_menu_text(uint32_t *pixels, int canvas_width,
	int canvas_height, const t_pixel_asset *font, const char *text,
	int x, int y, int glyph_size, int spacing,
	t_color tint, unsigned opacity)
{
	unsigned	codepoint;
	int			glyph;

	while (*text != '\0')
	{
		codepoint = (unsigned char)*text;
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		glyph = (int)codepoint - 32;
		draw_menu_glyph(pixels, canvas_width, canvas_height, font, glyph,
			x, y, glyph_size, tint, opacity);
		x += glyph_size + spacing;
		text++;
	}
}

/**
 * @brief Scales a signed offset in atlas rows into destination pixels.
 *
 * Rows above the cap band are negative, and C division truncates towards zero,
 * which would sample the wrong source row for them.
 */
static int	menu_ink_span(int units, int glyph_size)
{
	if (units >= 0)
		return (units * glyph_size / FONT_INK_HEIGHT);
	return (-((-units * glyph_size + FONT_INK_HEIGHT - 1) / FONT_INK_HEIGHT));
}

/**
 * @brief Draws one atlas glyph with its ascender dots and descender tails.
 *
 * Walking source rows keeps the whole ink band on the cap band's scale while
 * leaving the cap row itself at dest_y, so no existing label shifts.
 */
static void	draw_menu_glyph(uint32_t *pixels, int canvas_width,
	int canvas_height, const t_pixel_asset *font, int glyph,
	int dest_x, int dest_y, int glyph_size,
	t_color tint, unsigned opacity)
{
	int	source_y;
	int	source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < FONT_GLYPH_WIDTH)
		{
			draw_menu_texel(pixels, canvas_width, canvas_height, font,
				(glyph / FONT_COLUMNS) * FONT_GLYPH_HEIGHT + source_y,
				(glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH + source_x,
				dest_x + source_x * glyph_size / FONT_GLYPH_WIDTH,
				dest_y + menu_ink_span(source_y - FONT_INK_Y, glyph_size),
				(source_x + 1) * glyph_size / FONT_GLYPH_WIDTH
				- source_x * glyph_size / FONT_GLYPH_WIDTH,
				menu_ink_span(source_y + 1 - FONT_INK_Y, glyph_size)
				- menu_ink_span(source_y - FONT_INK_Y, glyph_size),
				tint, opacity);
			source_x++;
		}
		source_y++;
	}
}

/**
 * @brief Expands one atlas texel into its destination rectangle.
 */
static void	draw_menu_texel(uint32_t *pixels, int canvas_width,
	int canvas_height, const t_pixel_asset *font, int source_y, int source_x,
	int dest_x, int dest_y, int cell_width, int cell_height,
	t_color tint, unsigned opacity)
{
	unsigned	alpha;
	int			y;
	int			x;

	if (cell_width <= 0 || cell_height <= 0)
		return ;
	alpha = ncpixel_a(font->pixels[(size_t)source_y * font->width + source_x]);
	alpha = (alpha * opacity + 127u) / 255u;
	if (alpha == 0)
		return ;
	y = 0;
	while (y < cell_height)
	{
		x = 0;
		while (x < cell_width)
		{
			put_menu_pixel(pixels, canvas_width, canvas_height,
				dest_x + x, dest_y + y, tint, alpha);
			x++;
		}
		y++;
	}
}

static void	put_menu_pixel(uint32_t *pixels, int canvas_width,
	int canvas_height, int x, int y, t_color tint, unsigned alpha)
{
	uint32_t	pixel;

	if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
		return ;
	pixel = ncpixel(tint.r, tint.g, tint.b);
	ncpixel_set_a(&pixel, alpha);
	pixels[(size_t)y * canvas_width + x] = pixel;
}

/**
 * @brief Blits one exact-resolution pixel plane above the cell background.
 *
 * The composition grid always spans the whole authored panel so glyphs land on
 * their designed coordinates, but the plane itself starts past the selector
 * column on terminals that cannot restack a bitmap. Those terminals would
 * otherwise have to wipe and rebuild the bitmap every time the marker is
 * recreated over it, which is exactly the operation Sixel does not survive.
 */
static struct ncplane	*blit_menu_pixels(t_render_ctx *ctx,
	const t_pixel_asset *font)
{
	ncplane_options			opts;
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	struct ncplane			*plane;
	uint32_t				*pixels;
	int						pixel_width;
	int						pixel_height;
	int						crop_cols;

	memset(&opts, 0, sizeof(opts));
	opts.y = ctx->bg_row
		+ (int)(ctx->bg_rows * MENU_PANEL_Y_RATIO + 0.5);
	opts.x = ctx->bg_col
		+ (int)(ctx->bg_cols * MENU_PANEL_X_RATIO + 0.5);
	opts.rows = (unsigned)(ctx->bg_rows * MENU_PANEL_HEIGHT_RATIO + 0.5);
	opts.cols = (unsigned)(ctx->bg_cols * MENU_PANEL_WIDTH_RATIO + 0.5);
	if (opts.rows == 0 || opts.cols == 0)
		return (NULL);
	crop_cols = selector_crop_cols(ctx, opts.x, (int)opts.cols);
	pixel_width = (int)opts.cols * ctx->cell_px_x;
	pixel_height = (int)opts.rows * ctx->cell_px_y;
	pixels = NULL;
	if (!compose_menu_pixels(font, pixel_width, pixel_height, &pixels))
		return (NULL);
	opts.x += crop_cols;
	opts.cols -= (unsigned)crop_cols;
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
	{
		free(pixels);
		return (NULL);
	}
	set_transparent_base(plane);
	ncv = ncvisual_from_rgba(pixels + crop_cols * ctx->cell_px_x,
		pixel_height, pixel_width * (int)sizeof(*pixels),
		pixel_width - crop_cols * ctx->cell_px_x);
	if (ncv == NULL)
	{
		free(pixels);
		ncplane_destroy(plane);
		return (NULL);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		free(pixels);
		ncplane_destroy(plane);
		return (NULL);
	}
	ncvisual_destroy(ncv);
	free(pixels);
	return (plane);
}

/**
 * @brief Counts the leading panel columns the selector marker needs to itself.
 *
 * Zero on the movable tier, where the sprite may overlap the labels freely.
 * The result never consumes the whole panel, so a cramped terminal still gets
 * a label surface rather than none at all.
 *
 * @param ctx Active render context with selector geometry already resolved.
 * @param panel_x Absolute terminal column of the authored panel's left edge.
 * @param panel_cols Panel width in terminal columns.
 * @return Columns to drop from the left of the label plane.
 */
static int	selector_crop_cols(const t_render_ctx *ctx, int panel_x,
	int panel_cols)
{
	int	crop;

	if (render_pixel_planes_reliable(ctx))
		return (0);
	crop = ctx->menu_col + ctx->bunny_cols - panel_x;
	return (clamp_int(crop, 0, panel_cols - 1));
}

/**
 * @brief Keeps navigation usable when bitmap text cannot be rendered.
 */
static struct ncplane	*create_text_fallback(t_render_ctx *ctx)
{
	ncplane_options	opts;
	struct ncplane	*plane;
	double			ratio;
	int				left;
	int				index;
	int				row;
	char			label[48];

	memset(&opts, 0, sizeof(opts));
	opts.y = ctx->bg_row
		+ (int)(ctx->bg_rows * MENU_PANEL_Y_RATIO + 0.5);
	opts.x = ctx->bg_col
		+ (int)(ctx->bg_cols * MENU_PANEL_X_RATIO + 0.5);
	opts.rows = (unsigned)(ctx->bg_rows * MENU_PANEL_HEIGHT_RATIO + 0.5);
	opts.cols = (unsigned)(ctx->bg_cols * MENU_PANEL_WIDTH_RATIO + 0.5);
	if (opts.rows < MENU_ITEM_COUNT || opts.cols < 12)
		return (NULL);
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
		return (NULL);
	set_transparent_base(plane);
	ratio = (MENU_LABEL_LEFT_X_RATIO - MENU_PANEL_X_RATIO)
		/ MENU_PANEL_WIDTH_RATIO;
	left = clamp_int((int)(ratio * opts.cols + 0.5), 0,
		(int)opts.cols - 1);
	index = 0;
	while (index < MENU_ITEM_COUNT)
	{
		row = render_menu_label_y(ctx, index) - opts.y;
		ncplane_set_fg_rgb8(plane, g_menu_colors[index].r,
			g_menu_colors[index].g, g_menu_colors[index].b);
		(void)ncplane_set_bg_rgb8(plane, 18, 5, 24);
		(void)ncplane_set_bg_alpha(plane, NCALPHA_BLEND);
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
		snprintf(label, sizeof(label), "  %s  ", menu_item_label(index));
		(void)ncplane_putstr_yx(plane, row, left, label);
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
		(void)ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
		index++;
	}
	return (plane);
}

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

static int	max_int(int left, int right)
{
	if (left > right)
		return (left);
	return (right);
}

static int	clamp_int(int value, int min, int max)
{
	if (value < min)
		return (min);
	if (value > max)
		return (max);
	return (value);
}
