#include "tetrisu.h"

# define AUTH_TITLE_SIZE		24
# define AUTH_LABEL_SIZE		16
# define AUTH_BUTTON_SIZE		18
# define AUTH_STATUS_SIZE		13
# define AUTH_FOOTER_SIZE		9
# define AUTH_MIN_GLYPH		7

static const color_t	g_auth_gold = {255, 205, 93};
static const color_t	g_auth_pink = {255, 137, 201};
static const color_t	g_auth_lavender = {196, 158, 244};
static const color_t	g_auth_blue = {105, 202, 255};
static const color_t	g_auth_green = {116, 235, 157};
static const color_t	g_auth_red = {255, 112, 142};
static const color_t	g_auth_disabled = {113, 109, 130};
static const color_t	g_auth_shadow = {17, 6, 29};

static bool				load_font_mask(pixel_asset_t *font);
static void				font_mask_destroy(pixel_asset_t *font);
static uint64_t			form_signature(const auth_form_t *form);
static bool				compose_auth_pixels(const pixel_asset_t *font,
							const auth_form_t *form, int width, int height,
							uint32_t **pixels);
static void				draw_auth_text(uint32_t *pixels, int canvas_width,
							int canvas_height, const pixel_asset_t *font,
							const char *text, int x, int y, int glyph_size,
							int spacing, color_t tint, unsigned opacity);
static void				draw_auth_text_centered(uint32_t *pixels,
							int canvas_width, int canvas_height,
							const pixel_asset_t *font, const char *text,
							int center_x, int y, int glyph_size, int spacing,
							color_t tint);
static void				draw_auth_text_right(uint32_t *pixels,
							int canvas_width, int canvas_height,
							const pixel_asset_t *font, const char *text,
							int right_x, int y, int glyph_size, int spacing,
							color_t tint);
static int				auth_text_width(const char *text, int glyph_size,
							int spacing);
static void				draw_auth_glyph(uint32_t *pixels, int canvas_width,
							int canvas_height, const pixel_asset_t *font,
							int glyph, int dest_x, int dest_y, int glyph_size,
							color_t tint, unsigned opacity);
static void				put_auth_pixel(uint32_t *pixels, int canvas_width,
							int canvas_height, int x, int y, color_t tint,
							unsigned alpha);
static struct ncplane	*blit_auth_pixels(render_ctx_t *ctx,
							const auth_form_t *form,
							const pixel_asset_t *font);
static void				set_transparent_base(struct ncplane *plane);
static int				scaled_size(int base, double scale);
static int				percent_of(int total, int percent);

/**
 * @brief Refreshes the high-resolution auth typography when its state changes.
 */
bool	render_auth_pixel_labels_refresh(render_ctx_t *ctx,
	const auth_form_t *form, bool force)
{
	pixel_asset_t	font;
	uint64_t		signature;

	if (ctx == NULL || form == NULL || !render_pixel_planes_reliable(ctx)
		|| !notcurses_canpixel(ctx->nc))
		return (false);
	signature = form_signature(form);
	if (!force && ctx->auth_labels_plane != NULL
		&& ctx->auth_labels_signature == signature)
		return (true);
	render_auth_pixel_labels_destroy(ctx);
	if (!load_font_mask(&font))
		return (false);
	ctx->auth_labels_plane = blit_auth_pixels(ctx, form, &font);
	font_mask_destroy(&font);
	if (ctx->auth_labels_plane == NULL)
		return (false);
	ctx->auth_labels_signature = signature;
	if (ctx->bg_plane != NULL)
		(void)ncplane_move_above(ctx->auth_labels_plane, ctx->bg_plane);
	return (true);
}

/**
 * @brief Removes the supported-terminal pixel typography surface.
 */
void	render_auth_pixel_labels_destroy(render_ctx_t *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->auth_labels_plane != NULL)
	{
		ncplane_destroy(ctx->auth_labels_plane);
		ctx->auth_labels_plane = NULL;
	}
	ctx->auth_labels_signature = 0;
}

static bool	load_font_mask(pixel_asset_t *font)
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
				font_mask_destroy(font);
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

static void	font_mask_destroy(pixel_asset_t *font)
{
	free(font->pixels);
	memset(font, 0, sizeof(*font));
}

static uint64_t	form_signature(const auth_form_t *form)
{
	const unsigned char	*cursor;
	uint64_t			hash;

	hash = 1469598103934665603ull;
	hash = (hash ^ (uint64_t)form->mode) * 1099511628211ull;
	hash = (hash ^ (uint64_t)form->feedback) * 1099511628211ull;
	hash = (hash ^ (uint64_t)form->server_state) * 1099511628211ull;
	cursor = (const unsigned char *)form->status;
	while (*cursor != '\0')
	{
		hash = (hash ^ *cursor) * 1099511628211ull;
		cursor++;
	}
	return (hash);
}

static bool	compose_auth_pixels(const pixel_asset_t *font,
	const auth_form_t *form, int width, int height, uint32_t **pixels)
{
	const char	*labels[4];
	const char	*title;
	const char	*primary;
	const char	*secondary;
	color_t		primary_color;
	double		scale;
	double		scale_x;
	double		scale_y;
	int			field_count;
	int			field_y[4];
	int			label_size;
	int			button_size;
	int			status_size;
	int			footer_size;
	int			spacing;
	int			shadow;
	int			index;
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
	scale_x = (double)width / BACKGROUND_SOURCE_PIXELS_X;
	scale_y = (double)height / BACKGROUND_SOURCE_PIXELS_Y;
	scale = scale_x < scale_y ? scale_x : scale_y;
	label_size = scaled_size(AUTH_LABEL_SIZE, scale);
	button_size = scaled_size(AUTH_BUTTON_SIZE, scale);
	status_size = scaled_size(AUTH_STATUS_SIZE, scale);
	footer_size = scaled_size(AUTH_FOOTER_SIZE, scale);
	spacing = label_size / 7;
	if (spacing < 1)
		spacing = 1;
	shadow = label_size / 7;
	if (shadow < 1)
		shadow = 1;
	title = form->mode == AUTH_FORM_SIGN_UP
		? "CREATE ACCOUNT" : "WELCOME TO TETRISU";
	draw_auth_text_centered(*pixels, width, height, font, title,
		width / 2 + shadow, percent_of(height, 10) + shadow,
		scaled_size(AUTH_TITLE_SIZE, scale), spacing, g_auth_shadow);
	draw_auth_text_centered(*pixels, width, height, font, title,
		width / 2, percent_of(height, 10),
		scaled_size(AUTH_TITLE_SIZE, scale), spacing, g_auth_gold);
	labels[0] = "USERNAME";
	labels[1] = "PASSWORD";
	labels[2] = form->mode == AUTH_FORM_SIGN_UP
		? "RE-ENTER PASSWORD" : "SERVER ID";
	labels[3] = "SERVER ID";
	field_y[0] = 21;
	field_y[1] = 31;
	field_y[2] = 42;
	field_y[3] = 52;
	field_count = form->mode == AUTH_FORM_SIGN_UP ? 4 : 3;
	index = 0;
	while (index < field_count)
	{
		draw_auth_text_right(*pixels, width, height, font, labels[index],
			percent_of(width, 49), percent_of(height, field_y[index])
			- label_size / 2, label_size, spacing, g_auth_pink);
		index++;
	}
	primary = form->mode == AUTH_FORM_SIGN_UP ? "SIGN UP" : "LOGIN";
	if (!auth_form_online_enabled(form))
		primary_color = g_auth_disabled;
	else
		primary_color = g_auth_green;
	draw_auth_text_centered(*pixels, width, height, font, primary,
		width / 2, percent_of(height, 65) - button_size / 2,
		button_size, spacing, primary_color);
	draw_auth_text_centered(*pixels, width, height, font, form->status,
		width / 2, percent_of(height,
			form->mode == AUTH_FORM_SIGN_UP ? 73 : 52) - status_size / 2,
		status_size, 1, form->feedback == AUTH_FEEDBACK_ERROR
		? g_auth_red : (form->feedback == AUTH_FEEDBACK_SUCCESS
			? g_auth_green : g_auth_lavender));
	secondary = form->mode == AUTH_FORM_SIGN_UP
		? "BACK TO LOGIN" : "SIGN UP";
	draw_auth_text_centered(*pixels, width, height, font, secondary,
		percent_of(width, 38), percent_of(height, 82) - button_size / 2,
		button_size, 1, g_auth_lavender);
	draw_auth_text_centered(*pixels, width, height, font, "PLAY OFFLINE",
		percent_of(width, 62), percent_of(height, 82) - button_size / 2,
		button_size, 1, g_auth_blue);
	draw_auth_text_centered(*pixels, width, height, font,
		"ENTER ON SERVER ID TO CHECK", width / 2,
		percent_of(height, 94) - footer_size / 2,
		footer_size, 1, g_auth_lavender);
	return (true);
}

static void	draw_auth_text(uint32_t *pixels, int canvas_width,
	int canvas_height, const pixel_asset_t *font, const char *text,
	int x, int y, int glyph_size, int spacing, color_t tint, unsigned opacity)
{
	unsigned	codepoint;
	int			glyph;

	while (*text != '\0')
	{
		codepoint = (unsigned char)*text;
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		glyph = (int)codepoint - 32;
		draw_auth_glyph(pixels, canvas_width, canvas_height, font, glyph,
			x, y, glyph_size, tint, opacity);
		x += glyph_size + spacing;
		text++;
	}
}

static void	draw_auth_text_centered(uint32_t *pixels, int canvas_width,
	int canvas_height, const pixel_asset_t *font, const char *text,
	int center_x, int y, int glyph_size, int spacing, color_t tint)
{
	draw_auth_text(pixels, canvas_width, canvas_height, font, text,
		center_x - auth_text_width(text, glyph_size, spacing) / 2,
		y, glyph_size, spacing, tint, 255u);
}

static void	draw_auth_text_right(uint32_t *pixels, int canvas_width,
	int canvas_height, const pixel_asset_t *font, const char *text,
	int right_x, int y, int glyph_size, int spacing, color_t tint)
{
	draw_auth_text(pixels, canvas_width, canvas_height, font, text,
		right_x - auth_text_width(text, glyph_size, spacing),
		y, glyph_size, spacing, tint, 255u);
}

static int	auth_text_width(const char *text, int glyph_size, int spacing)
{
	size_t	length;

	length = strlen(text);
	if (length == 0)
		return (0);
	return ((int)length * glyph_size + ((int)length - 1) * spacing);
}

static void	draw_auth_glyph(uint32_t *pixels, int canvas_width,
	int canvas_height, const pixel_asset_t *font, int glyph,
	int dest_x, int dest_y, int glyph_size, color_t tint, unsigned opacity)
{
	int			source_x;
	int			source_y;
	unsigned	alpha;
	int			y;
	int			x;

	y = 0;
	while (y < glyph_size)
	{
		source_y = (glyph / FONT_COLUMNS) * FONT_GLYPH_HEIGHT + FONT_INK_Y
			+ y * FONT_INK_HEIGHT / glyph_size;
		x = 0;
		while (x < glyph_size)
		{
			source_x = (glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH
				+ x * FONT_GLYPH_WIDTH / glyph_size;
			alpha = ncpixel_a(font->pixels[(size_t)source_y
					* font->width + source_x]);
			alpha = (alpha * opacity + 127u) / 255u;
			if (alpha != 0)
				put_auth_pixel(pixels, canvas_width, canvas_height,
					dest_x + x, dest_y + y, tint, alpha);
			x++;
		}
		y++;
	}
}

static void	put_auth_pixel(uint32_t *pixels, int canvas_width,
	int canvas_height, int x, int y, color_t tint, unsigned alpha)
{
	uint32_t	pixel;

	if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
		return ;
	pixel = ncpixel(tint.r, tint.g, tint.b);
	ncpixel_set_a(&pixel, alpha);
	pixels[(size_t)y * canvas_width + x] = pixel;
}

static struct ncplane	*blit_auth_pixels(render_ctx_t *ctx,
	const auth_form_t *form, const pixel_asset_t *font)
{
	ncplane_options			opts;
	struct ncvisual			*ncv;
	struct ncvisual_options	vopts;
	struct ncplane			*plane;
	uint32_t				*pixels;
	int						pixel_width;
	int						pixel_height;

	memset(&opts, 0, sizeof(opts));
	opts.y = ctx->bg_row;
	opts.x = ctx->bg_col;
	opts.rows = (unsigned)ctx->bg_rows;
	opts.cols = (unsigned)ctx->bg_cols;
	if (opts.rows == 0 || opts.cols == 0)
		return (NULL);
	pixel_width = (int)opts.cols * ctx->cell_px_x;
	pixel_height = (int)opts.rows * ctx->cell_px_y;
	pixels = NULL;
	if (!compose_auth_pixels(font, form, pixel_width, pixel_height, &pixels))
		return (NULL);
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
	{
		free(pixels);
		return (NULL);
	}
	set_transparent_base(plane);
	ncv = ncvisual_from_rgba(pixels, pixel_height,
		pixel_width * (int)sizeof(*pixels), pixel_width);
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

static int	scaled_size(int base, double scale)
{
	int	result;

	result = (int)(base * scale + 0.5);
	if (result < AUTH_MIN_GLYPH)
		result = AUTH_MIN_GLYPH;
	return (result);
}

static int	percent_of(int total, int percent)
{
	return ((total * percent) / 100);
}
