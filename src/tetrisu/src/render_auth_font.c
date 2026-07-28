#include "tetrisu.h"

# define AUTH_TITLE_SIZE		24
# define AUTH_LABEL_SIZE		16
# define AUTH_BUTTON_SIZE		18
# define AUTH_FOOTER_SIZE		9
# define AUTH_MIN_GLYPH		7

static const color_t	g_auth_gold = {255, 205, 93};
static const color_t	g_auth_pink = {255, 137, 201};
static const color_t	g_auth_lavender = {196, 158, 244};
static const color_t	g_auth_blue = {105, 202, 255};
static const color_t	g_auth_green = {116, 235, 157};
static const color_t	g_auth_disabled = {113, 109, 130};
static const color_t	g_auth_shadow = {17, 6, 29};

static bool				load_font_mask(pixel_asset_t *font);
static void				font_mask_destroy(pixel_asset_t *font);
static uint64_t			form_signature(const auth_form_t *form);
static bool				compose_auth_visual(const pixel_asset_t *font,
							const auth_form_t *form, struct ncvisual *canvas,
							int canvas_width, int canvas_height);
static void				draw_auth_text(struct ncvisual *canvas,
							int canvas_width,
							int canvas_height, const pixel_asset_t *font,
							const char *text, int x, int y, int glyph_size,
							int spacing, color_t tint, unsigned opacity);
static void				draw_auth_text_centered(struct ncvisual *canvas,
							int canvas_width, int canvas_height,
							const pixel_asset_t *font, const char *text,
							int center_x, int y, int glyph_size, int spacing,
							color_t tint);
static void				draw_auth_text_right(struct ncvisual *canvas,
							int canvas_width, int canvas_height,
							const pixel_asset_t *font, const char *text,
							int right_x, int y, int glyph_size, int spacing,
							color_t tint);
static int				auth_text_width(const char *text, int glyph_size,
							int spacing);
static void				draw_auth_glyph(struct ncvisual *canvas,
							int canvas_width,
							int canvas_height, const pixel_asset_t *font,
							int glyph, int dest_x, int dest_y, int glyph_size,
							color_t tint, unsigned opacity);
static void				put_auth_pixel(struct ncvisual *canvas,
							int canvas_width,
							int canvas_height, int x, int y, color_t tint,
							unsigned alpha);
static int				scaled_size(int base, double scale);
static int				percent_of(int total, int percent);

/**
 * @brief Refreshes one native-resolution background and typography composite.
 */
bool	render_auth_pixel_background_refresh(render_ctx_t *ctx,
	const auth_form_t *form, bool force)
{
	pixel_asset_t		font;
	struct ncvisual		*ncv;
	ncvgeom				geom;
	uint64_t			signature;
	int					result;

	if (ctx == NULL || form == NULL || !render_pixel_planes_reliable(ctx)
		|| !notcurses_canpixel(ctx->nc))
		return (false);
	signature = form_signature(form);
	if (!force && ctx->bg_plane != NULL
		&& ctx->auth_background_signature == signature)
		return (true);
	if (!load_font_mask(&font))
		return (false);
	ncv = ncvisual_from_file(AUTH_BACKGROUND_PATH);
	memset(&geom, 0, sizeof(geom));
	if (ncv == NULL || ncvisual_geom(NULL, ncv, NULL, &geom) != 0
		|| geom.pixx != BACKGROUND_SOURCE_PIXELS_X
		|| geom.pixy != BACKGROUND_SOURCE_PIXELS_Y
		|| !compose_auth_visual(&font, form, ncv,
			(int)geom.pixx, (int)geom.pixy))
	{
		font_mask_destroy(&font);
		if (ncv != NULL)
			ncvisual_destroy(ncv);
		return (false);
	}
	result = render_background_replace_visual(ctx, ncv, false);
	ncvisual_destroy(ncv);
	font_mask_destroy(&font);
	if (result < 0)
		return (false);
	ctx->auth_background_signature = signature;
	return (true);
}

/**
 * @brief Invalidates the cached auth composite signature.
 */
void	render_auth_pixel_background_reset(render_ctx_t *ctx)
{
	if (ctx == NULL)
		return ;
	ctx->auth_background_signature = 0;
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
	uint64_t	hash;

	hash = 1469598103934665603ull;
	hash = (hash ^ (uint64_t)form->mode) * 1099511628211ull;
	hash = (hash ^ (uint64_t)auth_form_online_enabled(form))
		* 1099511628211ull;
	return (hash);
}

static bool	compose_auth_visual(const pixel_asset_t *font,
	const auth_form_t *form, struct ncvisual *canvas,
	int canvas_width, int canvas_height)
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
	int			footer_size;
	int			spacing;
	int			shadow;
	int			index;
	if (canvas == NULL || canvas_width <= 0 || canvas_height <= 0)
		return (false);
	scale_x = (double)canvas_width / BACKGROUND_SOURCE_PIXELS_X;
	scale_y = (double)canvas_height / BACKGROUND_SOURCE_PIXELS_Y;
	scale = scale_x < scale_y ? scale_x : scale_y;
	label_size = scaled_size(AUTH_LABEL_SIZE, scale);
	button_size = scaled_size(AUTH_BUTTON_SIZE, scale);
	footer_size = scaled_size(AUTH_FOOTER_SIZE, scale);
	spacing = label_size / 7;
	if (spacing < 1)
		spacing = 1;
	shadow = label_size / 7;
	if (shadow < 1)
		shadow = 1;
	title = form->mode == AUTH_FORM_SIGN_UP
		? "CREATE ACCOUNT" : "WELCOME TO TETRISU";
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font, title,
		canvas_width / 2 + shadow, percent_of(canvas_height, 10) + shadow,
		scaled_size(AUTH_TITLE_SIZE, scale), spacing, g_auth_shadow);
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font, title,
		canvas_width / 2, percent_of(canvas_height, 10),
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
		draw_auth_text_right(canvas, canvas_width, canvas_height, font,
			labels[index], percent_of(canvas_width, 49),
			percent_of(canvas_height, field_y[index])
			- label_size / 2, label_size, spacing, g_auth_pink);
		index++;
	}
	primary = form->mode == AUTH_FORM_SIGN_UP ? "SIGN UP" : "LOGIN";
	if (!auth_form_online_enabled(form))
		primary_color = g_auth_disabled;
	else
		primary_color = g_auth_green;
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font, primary,
		canvas_width / 2, percent_of(canvas_height, 65) - button_size / 2,
		button_size, spacing, primary_color);
	secondary = form->mode == AUTH_FORM_SIGN_UP
		? "BACK TO LOGIN" : "SIGN UP";
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font, secondary,
		percent_of(canvas_width, 38),
		percent_of(canvas_height, 82) - button_size / 2,
		button_size, 1, g_auth_lavender);
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font,
		"PLAY OFFLINE", percent_of(canvas_width, 62),
		percent_of(canvas_height, 82) - button_size / 2,
		button_size, 1, g_auth_blue);
	draw_auth_text_centered(canvas, canvas_width, canvas_height, font,
		"ENTER ON SERVER ID TO CHECK", canvas_width / 2,
		percent_of(canvas_height, 94) - footer_size / 2,
		footer_size, 1, g_auth_lavender);
	return (true);
}

static void	draw_auth_text(struct ncvisual *canvas, int canvas_width,
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
		draw_auth_glyph(canvas, canvas_width, canvas_height, font, glyph,
			x, y, glyph_size, tint, opacity);
		x += glyph_size + spacing;
		text++;
	}
}

static void	draw_auth_text_centered(struct ncvisual *canvas, int canvas_width,
	int canvas_height, const pixel_asset_t *font, const char *text,
	int center_x, int y, int glyph_size, int spacing, color_t tint)
{
	draw_auth_text(canvas, canvas_width, canvas_height, font, text,
		center_x - auth_text_width(text, glyph_size, spacing) / 2,
		y, glyph_size, spacing, tint, 255u);
}

static void	draw_auth_text_right(struct ncvisual *canvas, int canvas_width,
	int canvas_height, const pixel_asset_t *font, const char *text,
	int right_x, int y, int glyph_size, int spacing, color_t tint)
{
	draw_auth_text(canvas, canvas_width, canvas_height, font, text,
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

static void	draw_auth_glyph(struct ncvisual *canvas, int canvas_width,
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
				put_auth_pixel(canvas, canvas_width, canvas_height,
					dest_x + x, dest_y + y, tint, alpha);
			x++;
		}
		y++;
	}
}

static void	put_auth_pixel(struct ncvisual *canvas, int canvas_width,
	int canvas_height, int x, int y, color_t tint, unsigned alpha)
{
	uint32_t	current;
	uint32_t	pixel;
	unsigned	inverse;
	unsigned	red;
	unsigned	green;
	unsigned	blue;

	if (x < 0 || x >= canvas_width || y < 0 || y >= canvas_height)
		return ;
	if (ncvisual_at_yx(canvas, (unsigned)y, (unsigned)x, &current) < 0)
		return ;
	inverse = 255u - alpha;
	red = (tint.r * alpha + ncpixel_r(current) * inverse + 127u) / 255u;
	green = (tint.g * alpha + ncpixel_g(current) * inverse + 127u) / 255u;
	blue = (tint.b * alpha + ncpixel_b(current) * inverse + 127u) / 255u;
	pixel = ncpixel(red, green, blue);
	ncpixel_set_a(&pixel, 255u);
	(void)ncvisual_set_yx(canvas, y, x, pixel);
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
