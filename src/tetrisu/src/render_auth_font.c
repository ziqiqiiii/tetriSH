#include "tetrisu.h"

# define AUTH_ATLAS_COLUMNS		16
# define AUTH_ATLAS_ROWS		6
# define AUTH_ATLAS_GLYPH_WIDTH	20
# define AUTH_ATLAS_GLYPH_HEIGHT	40
# define AUTH_TEXT_CELL_COLS		2
# define AUTH_TEXT_CELL_ROWS		2
# define AUTH_TEXT_ADVANCE_NUM		3
# define AUTH_TEXT_ADVANCE_DEN		2

static const t_color	g_auth_value = {238, 223, 242};
static const t_color	g_auth_focus = {255, 229, 244};
static const t_color	g_auth_gold = {255, 206, 104};
static const t_color	g_auth_lavender = {190, 156, 230};
static const t_color	g_auth_red = {255, 111, 142};
static const t_color	g_auth_green = {112, 224, 174};
static const t_color	g_auth_disabled = {116, 111, 132};

static uint64_t			form_signature(const t_auth_form *form);
static bool				ensure_font_atlas(t_render_ctx *ctx);
static bool				cache_background_visual(t_render_ctx *ctx,
								const char *path);
static bool				add_value_sprite(t_render_ctx *ctx,
							const t_auth_form *form, t_auth_focus focus,
							int row, int x, int width, const char *value,
							bool password);
static bool				add_status_sprite(t_render_ctx *ctx,
							const t_auth_form *form, int row, int center_x,
							int width);
static void				fit_status_text(char *output, size_t capacity,
							const char *text, int plane_width);
static bool				add_empty_slot(t_render_ctx *ctx);
static bool				add_focus_sprites(t_render_ctx *ctx,
							int row, int x, int width, bool focused,
							bool disabled);
static bool				add_text_sprite(t_render_ctx *ctx, const char *text,
							int row, int x, int plane_width, t_color tint,
							bool centered);
static struct ncplane	*create_text_sprite(t_render_ctx *ctx,
							struct ncplane *plane, const char *text,
							int row, int x, int plane_width, t_color tint,
							bool centered);
static bool				prefill_sprite_background(t_render_ctx *ctx,
								uint32_t *pixels, int row, int x,
								int width, int pixel_rows);
static void				blend_sprite_pixel(uint32_t *pixel, t_color tint,
								unsigned alpha);
static void				set_transparent_base(struct ncplane *plane);
static size_t			visible_ascii(char *output, size_t capacity,
							const char *text, int max_width);
static uint64_t			sprite_signature(const char *text, int row, int x,
							int width, t_color tint, bool centered);

/**
 * @brief Loads the authored auth screen for the active form.
 *
 * Static artwork is transferred as one exact-size bitmap. Live values,
 * status, and focus use separate small font sprites so they stay above the
 * background without repainting a full-screen image for every key press.
 */
bool	render_auth_pixel_background_refresh(t_render_ctx *ctx,
	const t_auth_form *form, bool force)
{
	const char	*path;
	uint64_t	signature;

	if (ctx == NULL || form == NULL || !render_pixels_available(ctx)
		|| !notcurses_canpixel(ctx->nc))
		return (false);
	signature = form_signature(form);
	if (!force && ctx->bg_plane != NULL
		&& ctx->auth_background_signature == signature
		&& (ctx->pixels != TETRISU_PIXELS_STATIONARY
			|| ctx->auth_background_visual != NULL))
		return (true);
	/*
	 * The sprites go before the artwork does, not after. A sprite plane holds
	 * a bitmap the terminal is already showing, and the stationary tier cannot
	 * move one: repositioning the status row for the other form left its old
	 * image where the previous form had put it, and the fresh full-screen
	 * artwork below could not cover it because it is emitted first. Dropping
	 * them here means the artwork is the last thing drawn over that region and
	 * every sprite is rebuilt against the form actually on screen.
	 */
	render_auth_pixel_overlay_destroy(ctx);
	if (form->mode == AUTH_FORM_SIGN_UP)
		path = AUTH_SIGNUP_BACKGROUND_PATH;
	else
		path = AUTH_LOGIN_BACKGROUND_PATH;
	if (render_background_replace_exact(ctx, path, false) < 0)
		return (false);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY
		&& !cache_background_visual(ctx, path))
		return (false);
	ctx->auth_background_signature = signature;
	return (true);
}

/**
 * @brief Rebuilds small dynamic text sprites over the static auth bitmap.
 */
bool	render_auth_pixel_overlay_refresh(t_render_ctx *ctx,
	const t_auth_form *form)
{
	int	field_x;
	int	field_width;
	int	value_x;
	int	value_width;
	int	center;
	int	status_width;
	int	button_width;
	int	button_x;

	if (ctx == NULL || form == NULL || !ensure_font_atlas(ctx))
		return (false);
	ctx->auth_overlay_count = 0;
	center = ctx->bg_col + ctx->bg_cols / 2;
	field_width = (ctx->bg_cols * 48) / 100;
	if (field_width < 30)
		field_width = 30;
	field_x = center - field_width / 2;
	value_x = field_x + (field_width * 56) / 100;
	value_width = field_x + field_width - value_x - 2;
	if (value_width < 8)
		value_width = 8;
	if (!add_value_sprite(ctx, form, AUTH_FOCUS_USERNAME,
			ctx->bg_row + (ctx->bg_rows * 21) / 100, value_x, value_width,
			form->username, false)
		|| !add_value_sprite(ctx, form, AUTH_FOCUS_PASSWORD,
			ctx->bg_row + (ctx->bg_rows * 31) / 100, value_x, value_width,
			form->password, true))
		return (false);
	if (form->mode == AUTH_FORM_SIGN_UP)
	{
		if (!add_value_sprite(ctx, form, AUTH_FOCUS_CONFIRM,
				ctx->bg_row + (ctx->bg_rows * 42) / 100,
				value_x, value_width, form->confirm, true)
			|| !add_value_sprite(ctx, form, AUTH_FOCUS_DOMAIN,
				ctx->bg_row + (ctx->bg_rows * 52) / 100,
				value_x, value_width, form->domain, false))
			return (false);
	}
	else
	{
		if (!add_empty_slot(ctx)
			|| !add_value_sprite(ctx, form, AUTH_FOCUS_DOMAIN,
				ctx->bg_row + (ctx->bg_rows * 42) / 100,
				value_x, value_width, form->domain, false))
			return (false);
	}
	/*
	 * The status band is its own full-width box in the artwork, so sizing its
	 * sprite to a fraction of the field only bought silent truncation: the
	 * reader saw "OFFLINE - USE PLAY" and no way to guess the rest.
	 */
	status_width = field_width - 2;
	if (!add_status_sprite(ctx, form,
			ctx->bg_row + (ctx->bg_rows
				* (form->mode == AUTH_FORM_SIGN_UP ? 73 : 52)) / 100,
			center, status_width))
		return (false);
	if (!add_focus_sprites(ctx,
			ctx->bg_row + (ctx->bg_rows * 65) / 100,
			field_x, field_width, form->focus == AUTH_FOCUS_PRIMARY,
			!auth_form_online_enabled(form)))
		return (false);
	/*
	 * Sign Up and Play Offline are painted into the backdrop as two boxes
	 * meeting at the centre line, so their focus marks are placed to match the
	 * art and never resized. Squeezing a third button into that row is what
	 * pushed all three out of their frames.
	 */
	button_width = (ctx->bg_cols * 29) / 100;
	button_x = center - button_width - 1;
	if (!add_focus_sprites(ctx,
		ctx->bg_row + (ctx->bg_rows * 82) / 100,
		button_x, button_width,
		form->focus == AUTH_FOCUS_SECONDARY, false))
		return (false);
	if (!add_focus_sprites(ctx,
		ctx->bg_row + (ctx->bg_rows * 82) / 100,
		center + 1, button_width,
		form->focus == AUTH_FOCUS_OFFLINE, false))
		return (false);
	/*
	 * Preview has no box in the art, so it gets the clear row above the pair
	 * rather than a share of their width. It is a development gate, opt-in
	 * through TETRISU_UI_PREVIEW, and stays out of the authored layout.
	 */
	if (app_ui_preview_enabled() && form->mode == AUTH_FORM_LOGIN
		&& !add_text_sprite(ctx, "P  PREVIEW SIGN-IN",
			ctx->bg_row + (ctx->bg_rows * 76) / 100,
			center - field_width / 2, field_width,
			form->focus == AUTH_FOCUS_PREVIEW
			? g_auth_gold : g_auth_value, true))
		return (false);
	return (true);
}

/**
 * @brief Removes all dynamic auth sprites.
 */
void	render_auth_pixel_overlay_destroy(t_render_ctx *ctx)
{
	int	index;

	if (ctx == NULL)
		return ;
	index = 0;
	while (index < AUTH_OVERLAY_PLANE_MAX)
	{
		if (ctx->auth_overlay_planes[index] != NULL)
			ncplane_destroy(ctx->auth_overlay_planes[index]);
		ctx->auth_overlay_planes[index] = NULL;
		ctx->auth_overlay_signatures[index] = 0;
		index++;
	}
	ctx->auth_overlay_count = 0;
}

/**
 * @brief Invalidates the cached auth artwork signature.
 */
void	render_auth_pixel_background_reset(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->auth_background_visual != NULL)
	{
		ncvisual_destroy(ctx->auth_background_visual);
		ctx->auth_background_visual = NULL;
	}
	ctx->auth_background_signature = 0;
}

static uint64_t	form_signature(const t_auth_form *form)
{
	uint64_t	hash;

	hash = 1469598103934665603ull;
	hash = (hash ^ (uint64_t)form->mode) * 1099511628211ull;
	return (hash);
}

static bool	ensure_font_atlas(t_render_ctx *ctx)
{
	ncvgeom	geom;

	if (ctx->auth_font_visual != NULL)
		return (true);
	ctx->auth_font_visual = ncvisual_from_file(AUTH_FONT_ATLAS_PATH);
	if (ctx->auth_font_visual == NULL)
		return (false);
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, ctx->auth_font_visual, NULL, &geom) != 0
		|| geom.pixx != AUTH_ATLAS_COLUMNS * AUTH_ATLAS_GLYPH_WIDTH
		|| geom.pixy != AUTH_ATLAS_ROWS * AUTH_ATLAS_GLYPH_HEIGHT)
	{
		ncvisual_destroy(ctx->auth_font_visual);
		ctx->auth_font_visual = NULL;
		return (false);
	}
	return (true);
}

static bool	cache_background_visual(t_render_ctx *ctx, const char *path)
{
	struct ncvisual	*ncv;
	int				pixel_rows;
	int				pixel_cols;

	if (ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x)
		return (false);
	pixel_rows = ctx->bg_rows * ctx->cell_px_y;
	pixel_cols = ctx->bg_cols * ctx->cell_px_x;
	ncv = ncvisual_from_file(path);
	if (ncv == NULL || ncvisual_resize(ncv, pixel_rows, pixel_cols) != 0)
	{
		if (ncv != NULL)
			ncvisual_destroy(ncv);
		return (false);
	}
	if (ctx->auth_background_visual != NULL)
		ncvisual_destroy(ctx->auth_background_visual);
	ctx->auth_background_visual = ncv;
	return (true);
}

static bool	add_value_sprite(t_render_ctx *ctx, const t_auth_form *form,
	t_auth_focus focus, int row, int x, int width, const char *value,
	bool password)
{
	char	display[AUTH_FIELD_MAX];
	char	line[AUTH_FIELD_MAX + 4];
	bool	active;

	active = form->focus == focus;
	if (password)
		(void)auth_form_mask_password(value, display, sizeof(display));
	else
		snprintf(display, sizeof(display), "%s", value);
	snprintf(line, sizeof(line), "%s%s", display, active ? "_" : "");
	return (add_text_sprite(ctx, line, row, x + 1, width - 1,
			active ? g_auth_focus : g_auth_value, false));
}

static bool	add_status_sprite(t_render_ctx *ctx, const t_auth_form *form,
	int row, int center_x, int width)
{
	char	fitted[AUTH_STATUS_MAX];
	t_color	tint;
	int		length;
	int		x;

	if (form->feedback == AUTH_FEEDBACK_ERROR)
		tint = g_auth_red;
	else if (form->feedback == AUTH_FEEDBACK_SUCCESS)
		tint = g_auth_green;
	else if (form->feedback == AUTH_FEEDBACK_LOADING)
		tint = g_auth_gold;
	else
		tint = g_auth_lavender;
	length = width;
	x = center_x - length / 2;
	fit_status_text(fitted, sizeof(fitted), form->status, width);
	return (add_text_sprite(ctx, fitted, row, x, width, tint, true));
}

/**
 * @brief Copies status text into the sprite's glyph budget, marking any cut.
 *
 * A status line is a sentence the reader is meant to act on, so a silent cut
 * is worse than a short one: "OFFLINE - USE PLAY" looks like the whole
 * message. The ellipsis says out loud that there is more.
 */
static void	fit_status_text(char *output, size_t capacity, const char *text,
	int plane_width)
{
	int	max_chars;

	max_chars = (plane_width * AUTH_TEXT_ADVANCE_DEN)
		/ AUTH_TEXT_ADVANCE_NUM;
	if (max_chars > (int)capacity - 1)
		max_chars = (int)capacity - 1;
	if (max_chars <= 0)
	{
		output[0] = '\0';
		return ;
	}
	if ((int)strlen(text) <= max_chars)
	{
		snprintf(output, capacity, "%s", text);
		return ;
	}
	if (max_chars <= 3)
	{
		snprintf(output, capacity, "%.*s", max_chars, text);
		return ;
	}
	snprintf(output, capacity, "%.*s...", max_chars - 3, text);
}

static bool	add_empty_slot(t_render_ctx *ctx)
{
	int	slot;

	if (ctx->auth_overlay_count >= AUTH_OVERLAY_PLANE_MAX)
		return (false);
	slot = ctx->auth_overlay_count++;
	if (ctx->auth_overlay_planes[slot] != NULL)
		ncplane_destroy(ctx->auth_overlay_planes[slot]);
	ctx->auth_overlay_planes[slot] = NULL;
	ctx->auth_overlay_signatures[slot] = 0;
	return (true);
}

static bool	add_focus_sprites(t_render_ctx *ctx, int row, int x,
	int width, bool focused, bool disabled)
{
	t_color	tint;

	if (disabled)
		tint = g_auth_disabled;
	else
		tint = g_auth_gold;
	return (add_text_sprite(ctx, focused ? ">" : "", row, x + 2,
			AUTH_TEXT_CELL_COLS, tint, false)
		&& add_text_sprite(ctx, focused ? "<" : "", row, x + width - 4,
			AUTH_TEXT_CELL_COLS, tint, false));
}

static bool	add_text_sprite(t_render_ctx *ctx, const char *text,
	int row, int x, int plane_width, t_color tint, bool centered)
{
	char			visible[AUTH_FIELD_MAX + 4];
	struct ncplane	*plane;
	int				slot;
	int				max_chars;
	uint64_t		signature;

	if (ctx->auth_overlay_count >= AUTH_OVERLAY_PLANE_MAX)
		return (false);
	slot = ctx->auth_overlay_count;
	max_chars = (plane_width * AUTH_TEXT_ADVANCE_DEN)
		/ AUTH_TEXT_ADVANCE_NUM;
	if (visible_ascii(visible, sizeof(visible), text,
			max_chars) == 0)
		visible[0] = '\0';
	signature = sprite_signature(visible, row, x, plane_width, tint,
			centered);
	if (ctx->auth_overlay_planes[slot] != NULL
		&& ctx->auth_overlay_signatures[slot] == signature)
	{
		if (ctx->bg_plane != NULL)
			(void)ncplane_move_above(ctx->auth_overlay_planes[slot],
				ctx->bg_plane);
		ctx->auth_overlay_count++;
		return (true);
	}
	if (ctx->auth_overlay_planes[slot] != NULL)
	{
		unsigned	rows;
		unsigned	cols;

		ncplane_dim_yx(ctx->auth_overlay_planes[slot], &rows, &cols);
		if (rows != AUTH_TEXT_CELL_ROWS
			|| cols != (unsigned)plane_width)
		{
			ncplane_destroy(ctx->auth_overlay_planes[slot]);
			ctx->auth_overlay_planes[slot] = NULL;
			ctx->auth_overlay_signatures[slot] = 0;
		}
	}
	plane = create_text_sprite(ctx, ctx->auth_overlay_planes[slot],
			visible, row, x, plane_width, tint, centered);
	if (plane == NULL)
		return (false);
	ctx->auth_overlay_planes[slot] = plane;
	ctx->auth_overlay_signatures[slot] = signature;
	if (ctx->bg_plane != NULL)
		(void)ncplane_move_above(plane, ctx->bg_plane);
	ctx->auth_overlay_count++;
	return (true);
}

static struct ncplane	*create_text_sprite(t_render_ctx *ctx,
	struct ncplane *plane, const char *text, int row, int x,
	int plane_width, t_color tint, bool centered)
{
	struct ncvisual_options	vopts;
	ncplane_options			opts;
	struct ncvisual			*ncv;
	uint32_t				*pixels;
	uint32_t				source;
	size_t					length;
	size_t					count;
	unsigned				alpha;
	unsigned				codepoint;
	bool					created;
	int						source_x;
	int						source_y;
	int						width;
	int						glyph_width;
	int						glyph_x;
	int						glyph;
	int						advance;
	int						text_offset;
	int						text_width;
	int						pixel_rows;
	int						y;
	int						px;

	length = strlen(text);
	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| ctx->cell_px_y > INT_MAX / AUTH_TEXT_CELL_ROWS
		|| plane_width <= 0 || plane_width > INT_MAX / ctx->cell_px_x)
		return (NULL);
	width = plane_width * ctx->cell_px_x;
	pixel_rows = ctx->cell_px_y * AUTH_TEXT_CELL_ROWS;
	if ((size_t)width > SIZE_MAX / (size_t)pixel_rows)
		return (NULL);
	count = (size_t)width * (size_t)pixel_rows;
	if (count > SIZE_MAX / sizeof(*pixels))
		return (NULL);
	glyph_width = ctx->cell_px_x * AUTH_TEXT_CELL_COLS;
	advance = (ctx->cell_px_x * AUTH_TEXT_ADVANCE_NUM
			+ AUTH_TEXT_ADVANCE_DEN - 1) / AUTH_TEXT_ADVANCE_DEN;
	text_width = 0;
	if (length > 0)
		text_width = ((int)length - 1) * advance + glyph_width;
	text_offset = 0;
	if (centered && text_width < width)
		text_offset = (width - text_width) / 2;
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (NULL);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY
		&& !prefill_sprite_background(ctx, pixels, row, x, width, pixel_rows))
	{
		free(pixels);
		return (NULL);
	}
	y = 0;
	while (y < ctx->cell_px_y * AUTH_TEXT_CELL_ROWS)
	{
		glyph = 0;
		while (glyph < (int)length)
		{
			codepoint = (unsigned char)text[glyph];
			if (codepoint < 32 || codepoint >= 32
				+ AUTH_ATLAS_COLUMNS * AUTH_ATLAS_ROWS)
				codepoint = '?';
			glyph_x = 0;
			while (glyph_x < glyph_width)
			{
				px = text_offset + glyph * advance + glyph_x;
				if (px >= 0 && px < width)
				{
					source_x = ((int)(codepoint - 32)
							% AUTH_ATLAS_COLUMNS)
						* AUTH_ATLAS_GLYPH_WIDTH;
					source_x += glyph_x * AUTH_ATLAS_GLYPH_WIDTH
						/ glyph_width;
					source_y = ((int)(codepoint - 32)
							/ AUTH_ATLAS_COLUMNS)
						* AUTH_ATLAS_GLYPH_HEIGHT;
					source_y += y * AUTH_ATLAS_GLYPH_HEIGHT
						/ (ctx->cell_px_y * AUTH_TEXT_CELL_ROWS);
					if (ncvisual_at_yx(ctx->auth_font_visual,
							(unsigned)source_y, (unsigned)source_x,
							&source) < 0)
					{
						free(pixels);
						return (NULL);
					}
					alpha = ncpixel_a(source);
					if (alpha != 0)
					{
						if (ctx->pixels == TETRISU_PIXELS_STATIONARY)
							blend_sprite_pixel(&pixels[(size_t)y * width + px],
								tint, alpha);
						else
						{
							pixels[(size_t)y * width + px]
								= ncpixel(tint.r, tint.g, tint.b);
							ncpixel_set_a(&pixels[(size_t)y * width + px],
								alpha);
						}
					}
				}
				glyph_x++;
			}
			glyph++;
		}
		y++;
	}
	memset(&opts, 0, sizeof(opts));
	opts.y = row - 1;
	opts.x = x;
	opts.rows = AUTH_TEXT_CELL_ROWS;
	opts.cols = plane_width;
	created = false;
	if (plane != NULL)
	{
		unsigned	plane_rows;
		unsigned	plane_cols;

		ncplane_dim_yx(plane, &plane_rows, &plane_cols);
		if (plane_rows != AUTH_TEXT_CELL_ROWS
			|| plane_cols != (unsigned)plane_width)
		{
			free(pixels);
			return (NULL);
		}
		else
			(void)ncplane_move_yx(plane, opts.y, opts.x);
	}
	if (plane == NULL)
	{
		plane = ncplane_create(ctx->std, &opts);
		created = true;
	}
	if (plane == NULL)
	{
		free(pixels);
		return (NULL);
	}
	/* Reusing a plane without erasing its old sprixel makes Kitty reject the
	 * next bitmap bind, which used to close the auth flow on the first key. */
	set_transparent_base(plane);
	ncv = ncvisual_from_rgba(pixels,
			ctx->cell_px_y * AUTH_TEXT_CELL_ROWS,
			width * (int)sizeof(*pixels), width);
	free(pixels);
	if (ncv == NULL)
	{
		if (created)
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
		if (created)
			ncplane_destroy(plane);
		return (NULL);
	}
	ncvisual_destroy(ncv);
	return (plane);
}

static bool	prefill_sprite_background(t_render_ctx *ctx, uint32_t *pixels,
	int row, int x, int width, int pixel_rows)
{
	uint32_t	pixel;
	int			origin_y;
	int			origin_x;
	int			y;
	int			px;

	if (ctx->auth_background_visual == NULL)
		return (false);
	origin_y = (row - 1 - ctx->bg_row) * ctx->cell_px_y;
	origin_x = (x - ctx->bg_col) * ctx->cell_px_x;
	y = 0;
	while (y < pixel_rows)
	{
		px = 0;
		while (px < width)
		{
			if (origin_y + y < 0 || origin_x + px < 0
				|| ncvisual_at_yx(ctx->auth_background_visual,
					(unsigned)(origin_y + y), (unsigned)(origin_x + px),
					&pixel) < 0)
				pixel = ncpixel(8, 8, 31);
			ncpixel_set_a(&pixel, 255u);
			pixels[(size_t)y * width + px] = pixel;
			px++;
		}
		y++;
	}
	return (true);
}

static void	blend_sprite_pixel(uint32_t *pixel, t_color tint,
	unsigned alpha)
{
	unsigned	red;
	unsigned	green;
	unsigned	blue;

	red = (tint.r * alpha + ncpixel_r(*pixel) * (255u - alpha)) / 255u;
	green = (tint.g * alpha + ncpixel_g(*pixel) * (255u - alpha)) / 255u;
	blue = (tint.b * alpha + ncpixel_b(*pixel) * (255u - alpha)) / 255u;
	*pixel = ncpixel(red, green, blue);
	ncpixel_set_a(pixel, 255u);
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

static size_t	visible_ascii(char *output, size_t capacity,
	const char *text, int max_width)
{
	size_t	length;

	if (capacity == 0 || max_width <= 0)
		return (0);
	length = 0;
	while (text[length] != '\0' && length < (size_t)max_width
		&& length + 1 < capacity)
	{
		if ((unsigned char)text[length] < 32
			|| (unsigned char)text[length] >= 127)
			output[length] = '?';
		else
			output[length] = text[length];
		length++;
	}
	output[length] = '\0';
	return (length);
}

static uint64_t	sprite_signature(const char *text, int row, int x,
	int width, t_color tint, bool centered)
{
	const unsigned char	*cursor;
	uint64_t			hash;

	hash = 1469598103934665603ull;
	cursor = (const unsigned char *)text;
	while (*cursor != '\0')
	{
		hash = (hash ^ *cursor) * 1099511628211ull;
		cursor++;
	}
	hash = (hash ^ (uint64_t)(unsigned)row) * 1099511628211ull;
	hash = (hash ^ (uint64_t)(unsigned)x) * 1099511628211ull;
	hash = (hash ^ (uint64_t)(unsigned)width) * 1099511628211ull;
	hash = (hash ^ tint.r) * 1099511628211ull;
	hash = (hash ^ tint.g) * 1099511628211ull;
	hash = (hash ^ tint.b) * 1099511628211ull;
	hash = (hash ^ (uint64_t)centered) * 1099511628211ull;
	return (hash);
}
