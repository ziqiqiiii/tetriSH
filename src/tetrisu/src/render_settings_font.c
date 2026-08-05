#include "tetrisu.h"

# define SETTINGS_FONT_COLUMNS	16
# define SETTINGS_FONT_ROWS	6
# define SETTINGS_FONT_WIDTH	8
# define SETTINGS_FONT_HEIGHT	16
# define SETTINGS_FONT_INK_Y	4
# define SETTINGS_FONT_INK_HEIGHT	8
# define SETTINGS_FONT_SPACING_REF	4
# define SETTINGS_FONT_SHADOW_REF	3

static const color_t	g_settings_cream = {250, 242, 221};
static const color_t	g_settings_gold = {255, 203, 102};
static const color_t	g_settings_pink = {255, 112, 190};
static const color_t	g_settings_lavender = {190, 155, 218};
static const color_t	g_settings_green = {112, 214, 174};
static const color_t	g_settings_disabled = {105, 99, 120};
static const color_t	g_settings_shadow = {22, 8, 31};

static bool	load_font(render_ctx_t *ctx, struct ncvisual **font);
static bool	refresh_background(render_ctx_t *ctx, bool force);
static bool	cache_background(render_ctx_t *ctx);
static bool	compose_settings(render_ctx_t *ctx,
		const app_screen_view_model_t *view, const settings_state_t *state,
		const settings_layout_t *layout, struct ncvisual *font);
static bool	compose_controls(render_ctx_t *ctx,
		const app_settings_view_model_t *settings,
		const settings_state_t *state, const settings_layout_t *layout,
		struct ncvisual *font);
static bool	compose_character_controls(render_ctx_t *ctx,
		const settings_state_t *state, const settings_layout_t *layout,
		struct ncvisual *font);
static bool	compose_volume(render_ctx_t *ctx,
		const app_settings_view_model_t *settings,
		const settings_layout_t *layout, struct ncvisual *font);
static bool	compose_ability(render_ctx_t *ctx,
		const app_settings_view_model_t *settings,
		const settings_state_t *state, const settings_layout_t *layout,
		struct ncvisual *font);
static bool	prefill_background(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height);
static bool	create_settings_plane(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height);
static bool	create_region_plane(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height, const settings_rect_t *region,
		struct ncplane **slot);
static void	draw_profile(uint32_t *pixels, int width, int height,
		const app_screen_view_model_t *view, const settings_layout_t *layout,
		struct ncvisual *font);
static void	draw_inventory(uint32_t *pixels, int width, int height,
		const app_catalogue_view_model_t *catalogue,
		const settings_rect_t *panel, const settings_layout_t *layout,
		struct ncvisual *font, bool characters);
static void	draw_stats(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_volume_value(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_buttons(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings, const settings_state_t *state,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_portrait(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, const char *path);
static void	draw_character_arrows(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const settings_state_t *state);
static void	draw_ability_card(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const app_settings_view_model_t *settings);
static void	draw_wrapped_text_ref(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x, int ref_y, int ref_width,
		int ref_glyph, color_t tint, int max_lines);
static const app_catalogue_item_view_model_t	*equipped_character(
		const app_settings_view_model_t *settings);
static void	draw_text_ref(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x, int ref_y, int ref_width,
		int ref_glyph, color_t tint, bool centered);
static void	draw_text_run(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const char *text, int x, int y, int glyph_size, int spacing,
		color_t tint);
static void	draw_glyph(uint32_t *pixels, int width, int height,
		struct ncvisual *font, int glyph, int x, int y, int glyph_size,
		color_t tint, const settings_layout_t *layout);
static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
		color_t tint, unsigned alpha, bool opaque);
static void	blend_pixel(uint32_t *pixel, color_t tint, unsigned alpha);
static int	ref_x(const settings_layout_t *layout, int value);
static int	ref_y(const settings_layout_t *layout, int value);
static int	ref_size(const settings_layout_t *layout, int value);
static int	text_width(const char *text, int glyph_size, int spacing);
static int	min_int(int left, int right);
static int	max_int(int left, int right);
static const char	*nonempty(const char *text);
static const char	*status_title(const app_screen_view_model_t *view);
static const char	*status_detail(const app_screen_view_model_t *view);
static const char	*renderer_name(tetrisu_renderer_mode_t mode);
static uint64_t	settings_hash(const void *data, size_t size, uint64_t hash);
static uint64_t	static_signature(const app_screen_view_model_t *view,
		const settings_layout_t *layout);
static bool	settings_render_failed(const char *stage);

/**
 * @brief Renders the approved Settings/Profile art at exact fitted pixels.
 */
bool	render_settings_pixel_show(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state,
	bool rebuild_background)
{
	settings_layout_t	layout;
	struct ncvisual	*font;
	uint64_t		signature;

	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL
		|| !render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc))
		return (false);
	if (!refresh_background(ctx, rebuild_background))
		return (settings_render_failed("background"));
	if (rebuild_background)
	{
		render_screen_destroy(ctx);
		ctx->settings_static_signature = 0;
		ctx->settings_controls_signature = 0;
		ctx->settings_character_signature = 0;
		ctx->settings_volume_signature = 0;
		ctx->settings_ability_signature = 0;
	}
	settings_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	layout.opaque_background = ctx->pixels == TETRISU_PIXELS_STATIONARY;
	if (!load_font(ctx, &font))
		return (settings_render_failed("font"));
	if (layout.opaque_background)
	{
		signature = settings_hash(&view->data.settings,
			sizeof(view->data.settings),
			(uint64_t)layout.pixel_width << 32
				| (unsigned)layout.pixel_height);
		signature = settings_hash(state, sizeof(*state), signature);
		if ((ctx->screen_plane == NULL
				|| signature != ctx->settings_static_signature)
			&& !compose_settings(ctx, view, state, &layout, font))
			return (settings_render_failed("stationary frame"));
		ctx->settings_static_signature = signature;
		ncplane_move_top(ctx->screen_plane);
		render_compatibility_badge_hide(ctx);
		render_notification_raise(ctx);
		return (notcurses_render(ctx->nc) == 0);
	}
	signature = static_signature(view, &layout);
	if ((ctx->screen_plane == NULL || signature != ctx->settings_static_signature)
		&& !compose_settings(ctx, view, state, &layout, font))
		return (settings_render_failed("static frame"));
	ctx->settings_static_signature = signature;
	signature = settings_hash(&state->focus, sizeof(state->focus),
		(uint64_t)layout.pixel_width << 32 | (unsigned)layout.pixel_height);
	signature = settings_hash(&view->data.settings.signed_in,
		sizeof(view->data.settings.signed_in), signature);
	if (ctx->settings_controls_plane == NULL
		|| signature != ctx->settings_controls_signature)
	{
		if (!compose_controls(ctx, &view->data.settings, state, &layout, font)
			|| !compose_character_controls(ctx, state, &layout, font))
			return (settings_render_failed("controls"));
		ctx->settings_controls_signature = signature;
		ctx->settings_character_signature = signature;
	}
	signature = settings_hash(&view->data.settings.music_volume,
		sizeof(view->data.settings.music_volume),
		(uint64_t)layout.pixel_width << 32 | (unsigned)layout.pixel_height);
	if (ctx->settings_volume_plane == NULL
		|| signature != ctx->settings_volume_signature)
	{
		if (!compose_volume(ctx, &view->data.settings, &layout, font))
			return (settings_render_failed("volume"));
		ctx->settings_volume_signature = signature;
	}
	signature = settings_hash(&state->ability_info_visible,
		sizeof(state->ability_info_visible), ctx->settings_static_signature);
	if (signature != ctx->settings_ability_signature)
	{
		if (!compose_ability(ctx, &view->data.settings, state, &layout, font))
			return (settings_render_failed("ability"));
		ctx->settings_ability_signature = signature;
	}
	ncplane_move_top(ctx->screen_plane);
	if (ctx->settings_controls_plane != NULL)
		ncplane_move_top(ctx->settings_controls_plane);
	if (ctx->settings_character_plane != NULL)
		ncplane_move_top(ctx->settings_character_plane);
	if (ctx->settings_volume_plane != NULL)
		ncplane_move_top(ctx->settings_volume_plane);
	if (ctx->settings_ability_plane != NULL)
		ncplane_move_top(ctx->settings_ability_plane);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	if (notcurses_render(ctx->nc) != 0)
		return (settings_render_failed("render"));
	return (true);
}

/**
 * @brief Releases Settings-only cached artwork while retaining the backdrop.
 */
void	render_settings_pixel_destroy(render_ctx_t *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->settings_background_visual != NULL)
	{
		ncvisual_destroy(ctx->settings_background_visual);
		ctx->settings_background_visual = NULL;
	}
	if (ctx->settings_font_visual != NULL)
	{
		ncvisual_destroy(ctx->settings_font_visual);
		ctx->settings_font_visual = NULL;
	}
	if (ctx->settings_controls_plane != NULL)
	{
		ncplane_destroy(ctx->settings_controls_plane);
		ctx->settings_controls_plane = NULL;
	}
	if (ctx->settings_character_plane != NULL)
	{
		ncplane_destroy(ctx->settings_character_plane);
		ctx->settings_character_plane = NULL;
	}
	if (ctx->settings_volume_plane != NULL)
	{
		ncplane_destroy(ctx->settings_volume_plane);
		ctx->settings_volume_plane = NULL;
	}
	if (ctx->settings_ability_plane != NULL)
	{
		ncplane_destroy(ctx->settings_ability_plane);
		ctx->settings_ability_plane = NULL;
	}
	ctx->settings_background_ready = false;
	ctx->settings_background_rows = 0;
	ctx->settings_background_cols = 0;
	ctx->settings_static_signature = 0;
	ctx->settings_controls_signature = 0;
	ctx->settings_character_signature = 0;
	ctx->settings_volume_signature = 0;
	ctx->settings_ability_signature = 0;
}

static bool	refresh_background(render_ctx_t *ctx, bool force)
{
	bool	geometry_changed;

	geometry_changed = !ctx->settings_background_ready
		|| ctx->settings_background_rows != ctx->bg_rows
		|| ctx->settings_background_cols != ctx->bg_cols;
	if (!force && !geometry_changed && ctx->bg_plane != NULL)
		return (true);
	if (render_background_replace_exact(ctx, SETTINGS_BACKGROUND_PATH, false)
		< 0)
		return (false);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY
		&& !cache_background(ctx))
		return (false);
	if (ctx->pixels != TETRISU_PIXELS_STATIONARY
		&& ctx->settings_background_visual != NULL)
	{
		ncvisual_destroy(ctx->settings_background_visual);
		ctx->settings_background_visual = NULL;
	}
	ctx->settings_background_ready = true;
	ctx->settings_background_rows = ctx->bg_rows;
	ctx->settings_background_cols = ctx->bg_cols;
	return (true);
}

static bool	cache_background(render_ctx_t *ctx)
{
	struct ncvisual	*visual;
	int			width;
	int			height;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y)
		return (false);
	width = ctx->bg_cols * ctx->cell_px_x;
	height = ctx->bg_rows * ctx->cell_px_y;
	visual = ncvisual_from_file(SETTINGS_BACKGROUND_PATH);
	if (visual == NULL || ncvisual_resize(visual, height, width) != 0)
	{
		if (visual != NULL)
			ncvisual_destroy(visual);
		return (false);
	}
	if (ctx->settings_background_visual != NULL)
		ncvisual_destroy(ctx->settings_background_visual);
	ctx->settings_background_visual = visual;
	return (true);
}

static bool	load_font(render_ctx_t *ctx, struct ncvisual **font)
{
	ncvgeom	geom;

	if (ctx->settings_font_visual != NULL)
	{
		*font = ctx->settings_font_visual;
		return (true);
	}
	*font = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (*font == NULL)
		return (false);
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, *font, NULL, &geom) != 0
		|| geom.pixx != SETTINGS_FONT_COLUMNS * SETTINGS_FONT_WIDTH
		|| geom.pixy != SETTINGS_FONT_ROWS * SETTINGS_FONT_HEIGHT)
	{
		ncvisual_destroy(*font);
		*font = NULL;
		return (false);
	}
	ctx->settings_font_visual = *font;
	return (true);
}

static bool	compose_settings(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	uint32_t	*pixels;
	size_t		count;
	int			width;
	int			height;

	width = layout->pixel_width;
	height = layout->pixel_height;
	if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / (size_t)height)
		return (false);
	count = (size_t)width * (size_t)height;
	if (count > SIZE_MAX / sizeof(*pixels))
		return (false);
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (false);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY
		&& !prefill_background(ctx, pixels, width, height))
	{
		free(pixels);
		return (false);
	}
	draw_profile(pixels, width, height, view, layout, font);
	if (view->status == APP_DATA_READY && view->data.settings.signed_in
		&& !view->data.settings.offline)
		draw_portrait(pixels, width, height, layout,
			view->data.settings.profile.portrait_asset);
	draw_stats(pixels, width, height, &view->data.settings, layout, font);
	if (layout->opaque_background)
	{
		draw_volume_value(pixels, width, height, &view->data.settings,
			layout, font);
		draw_character_arrows(pixels, width, height, layout, font, state);
		draw_buttons(pixels, width, height, &view->data.settings, state,
			layout, font);
		if (state->ability_info_visible)
			draw_ability_card(pixels, width, height, layout, font,
				&view->data.settings);
	}
	if (ctx->screen_plane != NULL)
		ncplane_destroy(ctx->screen_plane);
	ctx->screen_plane = NULL;
	if (!create_settings_plane(ctx, pixels, width, height))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

static bool	compose_controls(render_ctx_t *ctx,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	settings_rect_t	region;
	uint32_t		*pixels;
	size_t			count;

	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (false);
	draw_buttons(pixels, layout->pixel_width, layout->pixel_height, settings,
		state, layout, font);
	region.x = ref_x(layout, 200);
	region.y = ref_y(layout, 850);
	region.width = ref_x(layout, 1080);
	region.height = ref_y(layout, 205);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->settings_controls_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

static bool	compose_character_controls(render_ctx_t *ctx,
	const settings_state_t *state, const settings_layout_t *layout,
	struct ncvisual *font)
{
	settings_rect_t	region;
	uint32_t		*pixels;
	size_t			count;

	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (false);
	draw_character_arrows(pixels, layout->pixel_width, layout->pixel_height,
		layout, font, state);
	region.x = ref_x(layout, 180);
	region.y = ref_y(layout, 210);
	region.width = ref_x(layout, 430);
	region.height = ref_y(layout, 150);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->settings_character_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

static bool	compose_volume(render_ctx_t *ctx,
	const app_settings_view_model_t *settings,
	const settings_layout_t *layout, struct ncvisual *font)
{
	settings_rect_t	region;
	uint32_t		*pixels;
	size_t			count;

	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (false);
	draw_volume_value(pixels, layout->pixel_width, layout->pixel_height,
		settings, layout, font);
	if (!settings->offline && settings->signed_in)
	{
		region.x = ref_x(layout, SETTINGS_REF_PROFILE_X + 168);
		region.y = ref_y(layout, SETTINGS_REF_PROFILE_Y + 251);
		region.width = ref_x(layout, 112);
		region.height = ref_y(layout, 42);
	}
	else
	{
		region.x = ref_x(layout, SETTINGS_REF_WALLET_X + 60);
		region.y = ref_y(layout, SETTINGS_REF_WALLET_Y + 30);
		region.width = ref_x(layout, 230);
		region.height = ref_y(layout, 42);
	}
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->settings_volume_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

static bool	compose_ability(render_ctx_t *ctx,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	settings_rect_t	region;
	uint32_t		*pixels;
	size_t			count;

	if (!state->ability_info_visible)
	{
		if (ctx->settings_ability_plane != NULL)
		{
			ncplane_destroy(ctx->settings_ability_plane);
			ctx->settings_ability_plane = NULL;
		}
		return (true);
	}
	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	pixels = calloc(count, sizeof(*pixels));
	if (pixels == NULL)
		return (false);
	draw_ability_card(pixels, layout->pixel_width, layout->pixel_height,
		layout, font, settings);
	region.x = ref_x(layout, 570);
	region.y = ref_y(layout, 118);
	region.width = ref_x(layout, 652);
	region.height = ref_y(layout, 370);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->settings_ability_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

static bool	prefill_background(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height)
{
	uint32_t	pixel;
	int		y;
	int		x;

	if (ctx->settings_background_visual == NULL)
		return (false);
	y = 0;
	while (y < height)
	{
		x = 0;
		while (x < width)
		{
			if (ncvisual_at_yx(ctx->settings_background_visual,
					(unsigned)y, (unsigned)x, &pixel) < 0)
				pixel = ncpixel(8, 8, 31);
			ncpixel_set_a(&pixel, 255u);
			pixels[(size_t)y * width + x] = pixel;
			x++;
		}
		y++;
	}
	return (true);
}

static bool	create_settings_plane(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height)
{
	ncplane_options			options;
	struct ncvisual		*ncv;
	struct ncvisual_options	vopts;
	struct ncplane		*plane;

	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row;
	options.x = ctx->bg_col;
	options.rows = ctx->bg_rows;
	options.cols = ctx->bg_cols;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (false);
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
	ncv = ncvisual_from_rgba(pixels, height, width * (int)sizeof(*pixels),
		width);
	if (ncv == NULL)
	{
		ncplane_destroy(plane);
		return (false);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		ncplane_destroy(plane);
		return (false);
	}
	ncvisual_destroy(ncv);
	ctx->screen_plane = plane;
	return (true);
}

static bool	create_region_plane(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height, const settings_rect_t *region,
	struct ncplane **slot)
{
	ncplane_options		options;
	struct ncvisual		*ncv;
	struct ncvisual_options	vopts;
	struct ncplane			*plane;
	uint32_t				*cropped;
	int					crop_x;
	int					crop_y;
	int					crop_right;
	int					crop_bottom;
	int					crop_width;
	int					crop_height;
	int					y;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| region->x < 0 || region->y < 0 || region->width <= 0
		|| region->height <= 0 || region->x + region->width > width
		|| region->y + region->height > height)
		return (false);
	/*
	 * Pixel offsets combined with source cropping are unreliable on some
	 * Kitty/Notcurses combinations.  Expand the dirty rectangle to cell
	 * boundaries and blit a compact, zero-offset visual instead.
	 */
	crop_x = region->x / ctx->cell_px_x * ctx->cell_px_x;
	crop_y = region->y / ctx->cell_px_y * ctx->cell_px_y;
	crop_right = ((region->x + region->width + ctx->cell_px_x - 1)
		/ ctx->cell_px_x) * ctx->cell_px_x;
	crop_bottom = ((region->y + region->height + ctx->cell_px_y - 1)
		/ ctx->cell_px_y) * ctx->cell_px_y;
	crop_right = min_int(crop_right, width);
	crop_bottom = min_int(crop_bottom, height);
	crop_width = crop_right - crop_x;
	crop_height = crop_bottom - crop_y;
	cropped = malloc((size_t)crop_width * (size_t)crop_height
		* sizeof(*cropped));
	if (cropped == NULL)
		return (false);
	y = 0;
	while (y < crop_height)
	{
		memcpy(cropped + (size_t)y * crop_width,
			pixels + (size_t)(crop_y + y) * width + crop_x,
			(size_t)crop_width * sizeof(*cropped));
		y++;
	}
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row + crop_y / ctx->cell_px_y;
	options.x = ctx->bg_col + crop_x / ctx->cell_px_x;
	options.rows = (unsigned)(crop_height + ctx->cell_px_y - 1)
		/ (unsigned)ctx->cell_px_y;
	options.cols = (unsigned)(crop_width + ctx->cell_px_x - 1)
		/ (unsigned)ctx->cell_px_x;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
	{
		free(cropped);
		return (false);
	}
	ncv = ncvisual_from_rgba(cropped, crop_height,
		crop_width * (int)sizeof(*cropped), crop_width);
	if (ncv == NULL)
	{
		ncplane_destroy(plane);
		free(cropped);
		return (false);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		ncplane_destroy(plane);
		free(cropped);
		return (false);
	}
	ncvisual_destroy(ncv);
	free(cropped);
	if (*slot != NULL)
		ncplane_destroy(*slot);
	*slot = plane;
	return (true);
}

static void	draw_profile(uint32_t *pixels, int width, int height,
	const app_screen_view_model_t *view, const settings_layout_t *layout,
	struct ncvisual *font)
{
	const app_settings_view_model_t	*settings;

	settings = &view->data.settings;
	draw_text_ref(pixels, width, height, layout, font, "SETTINGS / PROFILE",
		SETTINGS_REF_PROFILE_X + 12, SETTINGS_REF_PROFILE_Y + 12,
		SETTINGS_REF_PROFILE_WIDTH - 24, 28, g_settings_pink, true);
	if (view->local_preview)
		draw_text_ref(pixels, width, height, layout, font, "LOCAL UI PREVIEW",
			SETTINGS_REF_PROFILE_X + 12, SETTINGS_REF_PROFILE_Y + 52,
			SETTINGS_REF_PROFILE_WIDTH - 24, 17, g_settings_gold, true);
	if (view->status != APP_DATA_READY)
	{
		draw_text_ref(pixels, width, height, layout, font,
			status_title(view), SETTINGS_REF_PROFILE_X + 20,
			SETTINGS_REF_PROFILE_Y + 108, SETTINGS_REF_PROFILE_WIDTH - 40,
			24, g_settings_gold, true);
		draw_text_ref(pixels, width, height, layout, font,
			status_detail(view), SETTINGS_REF_PROFILE_X + 20,
			SETTINGS_REF_PROFILE_Y + 150, SETTINGS_REF_PROFILE_WIDTH - 40,
			16, g_settings_lavender, true);
	}
	else if (settings->offline || !settings->signed_in)
	{
		draw_text_ref(pixels, width, height, layout, font,
			"OFFLINE LOCAL SETTINGS", SETTINGS_REF_PROFILE_X + 18,
			SETTINGS_REF_PROFILE_Y + 106, SETTINGS_REF_PROFILE_WIDTH - 36,
			23, g_settings_cream, true);
		draw_text_ref(pixels, width, height, layout, font,
			"ACCOUNT DATA UNAVAILABLE", SETTINGS_REF_PROFILE_X + 18,
			SETTINGS_REF_PROFILE_Y + 150, SETTINGS_REF_PROFILE_WIDTH - 36,
			16, g_settings_lavender, true);
		draw_text_ref(pixels, width, height, layout, font,
			"LOCAL CONTROLS ONLY", SETTINGS_REF_PROFILE_X + 18,
			SETTINGS_REF_PROFILE_Y + 190, SETTINGS_REF_PROFILE_WIDTH - 36,
			16, g_settings_green, true);
		draw_text_ref(pixels, width, height, layout, font,
			nonempty(settings->local_status), SETTINGS_REF_PROFILE_X + 18,
			SETTINGS_REF_PROFILE_Y + 230, SETTINGS_REF_PROFILE_WIDTH - 36,
			13, g_settings_lavender, true);
	}
	else
	{
		draw_text_ref(pixels, width, height, layout, font, "USERNAME:",
			SETTINGS_REF_PROFILE_X + 26, SETTINGS_REF_PROFILE_Y + 104,
			160, 13, g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font,
			nonempty(settings->profile.username), SETTINGS_REF_PROFILE_X + 194,
			SETTINGS_REF_PROFILE_Y + 104, 370, 18, g_settings_cream, false);
		draw_text_ref(pixels, width, height, layout, font,
			"CHARACTER:", SETTINGS_REF_PROFILE_X + 26,
			SETTINGS_REF_PROFILE_Y + 158, 175, 13, g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font,
			nonempty(settings->profile.character), SETTINGS_REF_PROFILE_X + 209,
			SETTINGS_REF_PROFILE_Y + 158, 345, 18, g_settings_cream, false);
		draw_text_ref(pixels, width, height, layout, font,
			"THEME:", SETTINGS_REF_PROFILE_X + 26,
			SETTINGS_REF_PROFILE_Y + 210, 100, 13, g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font,
			nonempty(settings->profile.theme), SETTINGS_REF_PROFILE_X + 134,
			SETTINGS_REF_PROFILE_Y + 210, 420, 18, g_settings_cream, false);
		draw_text_ref(pixels, width, height, layout, font, "MUSIC VOLUME:",
			SETTINGS_REF_PROFILE_X + 26, SETTINGS_REF_PROFILE_Y + 263, 155, 13,
			g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font, "RENDERER:",
			SETTINGS_REF_PROFILE_X + 280, SETTINGS_REF_PROFILE_Y + 263, 112, 13,
			g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font,
			renderer_name(settings->renderer_mode), SETTINGS_REF_PROFILE_X + 400,
			SETTINGS_REF_PROFILE_Y + 263, 150, 14, g_settings_green, false);
	}
	if (view->status == APP_DATA_READY && settings->signed_in
		&& !settings->offline)
	{
		draw_inventory(pixels, width, height, &settings->characters,
			&layout->characters, layout, font, true);
		draw_inventory(pixels, width, height, &settings->themes,
			&layout->themes, layout, font, false);
	}
}

static void	draw_inventory(uint32_t *pixels, int width, int height,
	const app_catalogue_view_model_t *catalogue, const settings_rect_t *panel,
	const settings_layout_t *layout, struct ncvisual *font, bool characters)
{
	int		owned;
	int		index;
	int		slot;
	int		ref_panel_x;
	int		ref_panel_y;
	int		ref_panel_width;
	int		ref_slot_width;
	int		row;
	int		column;
	int		limit;
	color_t	color;
	char	label[APP_TEXT_MAX + 4];

	owned = 0;
	index = 0;
	while (index < catalogue->count)
	{
		if (catalogue->items[index].owned)
			owned++;
		index++;
	}
	ref_panel_x = characters ? SETTINGS_REF_CHARACTERS_X
		: SETTINGS_REF_THEMES_X;
	ref_panel_y = characters ? SETTINGS_REF_CHARACTERS_Y : SETTINGS_REF_THEMES_Y;
	ref_panel_width = characters ? SETTINGS_REF_CHARACTERS_WIDTH
		: SETTINGS_REF_THEMES_WIDTH;
	(void)panel;
	draw_text_ref(pixels, width, height, layout, font,
		characters ? "OWNED CHARACTERS" : "OWNED THEMES", ref_panel_x + 10,
		ref_panel_y, ref_panel_width - 20, 17, g_settings_pink, true);
	ref_slot_width = (ref_panel_width - 20) / 2;
	limit = characters ? 4 : 6;
	slot = 0;
	index = 0;
	while (index < catalogue->count && slot < limit)
	{
		if (catalogue->items[index].owned)
		{
			color = catalogue->items[index].equipped
				? g_settings_gold : g_settings_cream;
			row = slot / 2;
			column = slot % 2;
			snprintf(label, sizeof(label), "%s%s",
				catalogue->items[index].equipped ? "* " : "",
				catalogue->items[index].name);
			draw_text_ref(pixels, width, height, layout, font,
				label, ref_panel_x + 10 + column * ref_slot_width,
				ref_panel_y + 51 + row * 38, ref_slot_width - 4, 12,
				color, true);
			slot++;
		}
		index++;
	}
	if (owned == 0)
		draw_text_ref(pixels, width, height, layout, font, "NONE OWNED",
			ref_panel_x + 10, ref_panel_y + 82, ref_panel_width - 20, 13,
			g_settings_lavender, true);
	else if (owned > limit)
	{
		char	more[24];

		snprintf(more, sizeof(more), "+%d MORE", owned - limit);
		draw_text_ref(pixels, width, height, layout, font, more,
			ref_panel_x + ref_panel_width - 120, ref_panel_y + 4, 110, 11,
			g_settings_lavender, true);
	}
}

static void	draw_stats(uint32_t *pixels, int width, int height,
	const app_settings_view_model_t *settings, const settings_layout_t *layout,
	struct ncvisual *font)
{
	char		value[64];
	const char	*labels[3];

	if (settings->offline || !settings->signed_in)
	{
		labels[0] = "MUSIC VOLUME";
		labels[1] = "RENDERER";
		labels[2] = "PROFILE";
		draw_text_ref(pixels, width, height, layout, font, labels[0],
			SETTINGS_REF_WALLET_X + 72, SETTINGS_REF_WALLET_Y + 13, 210,
			14, g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font, labels[1],
			SETTINGS_REF_SCORE_X + 72, SETTINGS_REF_SCORE_Y + 13, 210, 14,
			g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font,
			renderer_name(settings->renderer_mode), SETTINGS_REF_SCORE_X + 72,
			SETTINGS_REF_SCORE_Y + 39, 210, 22, g_settings_green, false);
		draw_text_ref(pixels, width, height, layout, font, labels[2],
			SETTINGS_REF_RANK_X + 72, SETTINGS_REF_RANK_Y + 13, 210, 14,
			g_settings_lavender, false);
		draw_text_ref(pixels, width, height, layout, font, "LOCAL ONLY",
			SETTINGS_REF_RANK_X + 72, SETTINGS_REF_RANK_Y + 39, 210, 20,
			g_settings_green, false);
		return ;
	}
	labels[0] = "WALLET";
	labels[1] = "SCORE";
	labels[2] = "RANK";
	snprintf(value, sizeof(value), "%d", settings->profile.wallet_points);
	draw_text_ref(pixels, width, height, layout, font, labels[0],
		SETTINGS_REF_WALLET_X + 72, SETTINGS_REF_WALLET_Y + 13, 210, 14,
		g_settings_lavender, false);
	draw_text_ref(pixels, width, height, layout, font, value,
		SETTINGS_REF_WALLET_X + 72, SETTINGS_REF_WALLET_Y + 39, 210, 23,
		g_settings_green, false);
	snprintf(value, sizeof(value), "%" PRIu64, settings->profile.score);
	draw_text_ref(pixels, width, height, layout, font, labels[1],
		SETTINGS_REF_SCORE_X + 72, SETTINGS_REF_SCORE_Y + 13, 210, 14,
		g_settings_lavender, false);
	draw_text_ref(pixels, width, height, layout, font, value,
		SETTINGS_REF_SCORE_X + 72, SETTINGS_REF_SCORE_Y + 39, 210, 23,
		g_settings_green, false);
	snprintf(value, sizeof(value), "#%d", settings->profile.rank);
	draw_text_ref(pixels, width, height, layout, font, labels[2],
		SETTINGS_REF_RANK_X + 72, SETTINGS_REF_RANK_Y + 13, 210, 14,
		g_settings_lavender, false);
	draw_text_ref(pixels, width, height, layout, font, value,
		SETTINGS_REF_RANK_X + 72, SETTINGS_REF_RANK_Y + 39, 210, 23,
		g_settings_green, false);
}

static void	draw_volume_value(uint32_t *pixels, int width, int height,
	const app_settings_view_model_t *settings,
	const settings_layout_t *layout, struct ncvisual *font)
{
	char	volume[16];

	snprintf(volume, sizeof(volume), "%d%%",
		settings->music_volume * 100 / AUDIO_MAX_VOLUME);
	if (!settings->offline && settings->signed_in)
		draw_text_ref(pixels, width, height, layout, font, volume,
			SETTINGS_REF_PROFILE_X + 180, SETTINGS_REF_PROFILE_Y + 263,
			80, 14, g_settings_green, false);
	else
		draw_text_ref(pixels, width, height, layout, font, volume,
			SETTINGS_REF_WALLET_X + 72, SETTINGS_REF_WALLET_Y + 39,
			210, 22, g_settings_green, false);
}

static void	draw_buttons(uint32_t *pixels, int width, int height,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	const char	*labels[SETTINGS_BUTTON_COUNT];
	const char	*hints[SETTINGS_BUTTON_COUNT];
	color_t		color;
	int		index;
	int		center;

	labels[0] = "BACK";
	labels[1] = "MARKET";
	labels[2] = "VOLUME -";
	labels[3] = "VOLUME +";
	hints[0] = "ESC";
	hints[1] = "M";
	hints[2] = "-";
	hints[3] = "+";
	index = 0;
	while (index < SETTINGS_BUTTON_COUNT)
	{
		bool	focused;
		bool	enabled;

		focused = (index == 0 && state->focus == SETTINGS_FOCUS_BACK)
			|| (index == 1 && state->focus == SETTINGS_FOCUS_MARKETPLACE)
			|| (index == 2 && state->focus == SETTINGS_FOCUS_VOLUME_DOWN)
			|| (index == 3 && state->focus == SETTINGS_FOCUS_VOLUME_UP);
		enabled = index != 1 || settings->signed_in;
		color = !enabled ? g_settings_disabled
			: focused ? g_settings_gold : g_settings_cream;
		center = (index == 0 ? SETTINGS_REF_BUTTON_BACK_X
			: index == 1 ? SETTINGS_REF_BUTTON_MARKET_X
			: index == 2 ? SETTINGS_REF_BUTTON_VOLUME_DOWN_X
			: SETTINGS_REF_BUTTON_VOLUME_UP_X) + SETTINGS_REF_BUTTON_WIDTH / 2;
		draw_text_ref(pixels, width, height, layout, font, labels[index],
			center - 104, SETTINGS_REF_BUTTON_Y + 36, 208, 21, color, true);
		draw_text_ref(pixels, width, height, layout, font, hints[index],
			center - 52, SETTINGS_REF_BUTTON_Y + 77, 104, 15, color, true);
		if (focused)
		{
			draw_text_ref(pixels, width, height, layout, font, ">",
				center - 112, SETTINGS_REF_BUTTON_Y + 38, 18, 20,
				g_settings_gold, true);
			draw_text_ref(pixels, width, height, layout, font, "<",
				center + 94, SETTINGS_REF_BUTTON_Y + 38, 18, 20,
				g_settings_gold, true);
		}
		index++;
	}
	/*
	 * Settings takes no pointer input, so the character arrows and the powers
	 * card need their keys spelled out here or they are undiscoverable.
	 */
	draw_text_ref(pixels, width, height, layout, font,
		settings->signed_in
		? "TAB / ARROWS FOCUS   ENTER SELECT   [ ] CHARACTER   I POWERS   ESC BACK"
		: "TAB / ARROWS FOCUS   ENTER SELECT   ESC BACK", 260,
		SETTINGS_REF_BUTTON_Y + 139, 930, 12, g_settings_lavender, true);
}

static void	draw_portrait(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, const char *path)
{
	struct ncvisual	*portrait;
	ncvgeom			geom;
	uint32_t		pixel;
	int				draw_width;
	int				draw_height;
	int				source_x;
	int				source_y;
	int				x;
	int				y;
	int				origin_x;
	int				origin_y;

	if (path == NULL || path[0] == '\0')
		return ;
	portrait = ncvisual_from_file(path);
	memset(&geom, 0, sizeof(geom));
	if (portrait == NULL || ncvisual_geom(NULL, portrait, NULL, &geom) != 0
		|| geom.pixx == 0 || geom.pixy == 0)
	{
		if (portrait != NULL)
			ncvisual_destroy(portrait);
		return ;
	}
	draw_width = layout->portrait.width;
	draw_height = (int)((uint64_t)draw_width * geom.pixy / geom.pixx);
	if (draw_height > layout->portrait.height)
	{
		draw_height = layout->portrait.height;
		draw_width = (int)((uint64_t)draw_height * geom.pixx / geom.pixy);
	}
	origin_x = layout->portrait.x + (layout->portrait.width - draw_width) / 2;
	origin_y = layout->portrait.y + (layout->portrait.height - draw_height) / 2;
	y = 0;
	while (y < draw_height)
	{
		x = 0;
		while (x < draw_width)
		{
			source_x = x * (int)geom.pixx / draw_width;
			source_y = y * (int)geom.pixy / draw_height;
			if (ncvisual_at_yx(portrait, (unsigned)source_y,
					(unsigned)source_x, &pixel) >= 0
				&& ncpixel_a(pixel) != 0)
			{
				color_t	color;

				color.r = ncpixel_r(pixel);
				color.g = ncpixel_g(pixel);
				color.b = ncpixel_b(pixel);
				put_pixel(pixels, width, height, origin_x + x, origin_y + y,
					color, ncpixel_a(pixel), false);
			}
			x++;
		}
		y++;
	}
	ncvisual_destroy(portrait);
}

static void	draw_character_arrows(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font,
	const settings_state_t *state)
{
	color_t	previous_color;
	color_t	next_color;

	previous_color = state->focus == SETTINGS_FOCUS_CHARACTER_PREVIOUS
		? g_settings_gold : g_settings_cream;
	next_color = state->focus == SETTINGS_FOCUS_CHARACTER_NEXT
		? g_settings_gold : g_settings_cream;
	draw_text_ref(pixels, width, height, layout, font, "<--",
		SETTINGS_REF_CHARACTER_PREVIOUS_X, SETTINGS_REF_CHARACTER_ARROW_Y + 25,
		SETTINGS_REF_CHARACTER_ARROW_WIDTH, 20, previous_color, true);
	draw_text_ref(pixels, width, height, layout, font, "-->",
		SETTINGS_REF_CHARACTER_NEXT_X, SETTINGS_REF_CHARACTER_ARROW_Y + 25,
		SETTINGS_REF_CHARACTER_ARROW_WIDTH, 20, next_color, true);
}

static void	draw_ability_card(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font,
	const app_settings_view_model_t *settings)
{
	const app_catalogue_item_view_model_t	*character;
	color_t						panel;
	char							heading[APP_TEXT_MAX + 32];
	int							x;
	int							y;
	int							index;

	character = equipped_character(settings);
	if (character == NULL)
		return ;
	panel = (color_t){24, 10, 38};
	y = ref_y(layout, 118);
	while (y < ref_y(layout, 488))
	{
		x = ref_x(layout, 570);
		while (x < ref_x(layout, 1222))
		{
			put_pixel(pixels, width, height, x, y, panel, 244u, true);
			x++;
		}
		y++;
	}
	snprintf(heading, sizeof(heading), "%s - CRYSTAL POWERS",
		character->name);
	draw_text_ref(pixels, width, height, layout, font, heading, 590, 136,
		612, 21, g_settings_pink, true);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		snprintf(heading, sizeof(heading), "L%d  %s", index + 1,
			character->abilities[index].name);
		draw_text_ref(pixels, width, height, layout, font, heading, 594,
			182 + index * 73, 592, 13, g_settings_gold, false);
		draw_wrapped_text_ref(pixels, width, height, layout, font,
			character->abilities[index].description, 594,
			203 + index * 73, 592, 9, g_settings_cream, 2);
		index++;
	}
}

static void	draw_wrapped_text_ref(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font, const char *text,
	int ref_x_value, int ref_y_value, int ref_width, int ref_glyph,
	color_t tint, int max_lines)
{
	char	line_text[APP_TEXT_MAX * 2];
	int	line_start;
	int	cursor;
	int	last_space;
	int	line_index;
	int	max_chars;

	max_chars = max_int(12, ref_width / max_int(1, ref_glyph + 3));
	line_start = 0;
	line_index = 0;
	while (text[line_start] != '\0' && line_index < max_lines)
	{
		cursor = line_start;
		last_space = -1;
		while (text[cursor] != '\0' && cursor - line_start < max_chars)
		{
			if (text[cursor] == ' ')
				last_space = cursor;
			cursor++;
		}
		if (text[cursor] != '\0' && last_space >= line_start)
			cursor = last_space;
		snprintf(line_text, sizeof(line_text), "%.*s", cursor - line_start,
			text + line_start);
		draw_text_ref(pixels, width, height, layout, font, line_text,
			ref_x_value, ref_y_value + line_index * (ref_glyph + 7), ref_width,
			ref_glyph, tint, false);
		line_start = cursor;
		while (text[line_start] == ' ')
			line_start++;
		line_index++;
	}
}

static const app_catalogue_item_view_model_t	*equipped_character(
	const app_settings_view_model_t *settings)
{
	int	index;

	if (settings == NULL)
		return (NULL);
	index = 0;
	while (index < settings->characters.count
		&& index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (settings->characters.items[index].equipped)
			return (&settings->characters.items[index]);
		index++;
	}
	return (NULL);
}

static void	draw_text_ref(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font, const char *text,
	int ref_x_value, int ref_y_value, int ref_width, int ref_glyph,
	color_t tint, bool centered)
{
	char	visible[APP_TEXT_MAX * 2];
	int	glyph_size;
	int	spacing;
	int	max_chars;
	int	length;
	int	text_pixels;
	int	x;
	int	y;
	int	shadow;

	if (layout == NULL || font == NULL || text == NULL || ref_width <= 0)
		return ;
	glyph_size = max_int(3, ref_size(layout, ref_glyph));
	spacing = max_int(1, ref_size(layout, SETTINGS_FONT_SPACING_REF));
	shadow = max_int(1, ref_size(layout, SETTINGS_FONT_SHADOW_REF));
	while (glyph_size > max_int(3, ref_size(layout, 7))
		&& text_width(text, glyph_size, spacing) > ref_x(layout, ref_width))
	{
		glyph_size--;
		if (spacing > max_int(1, ref_size(layout, 2)))
			spacing--;
	}
	/* The final glyph has no trailing spacing, so exact-fit labels count it. */
	max_chars = max_int(1, (ref_x(layout, ref_width) + spacing)
		/ max_int(1, glyph_size + spacing));
	length = 0;
	while (text[length] != '\0' && length < max_chars
		&& length + 1 < (int)sizeof(visible))
	{
		visible[length] = (unsigned char)text[length] >= 32
			&& (unsigned char)text[length] < 127 ? text[length] : '?';
		length++;
	}
	visible[length] = '\0';
	text_pixels = text_width(visible, glyph_size, spacing);
	x = ref_x(layout, ref_x_value);
	if (centered)
		x += (ref_x(layout, ref_width) - text_pixels) / 2;
	y = ref_y(layout, ref_y_value);
	draw_text_run(pixels, width, height, layout, font, visible, x + shadow,
		y + shadow, glyph_size, spacing, g_settings_shadow);
	draw_text_run(pixels, width, height, layout, font, visible, x, y,
		glyph_size, spacing, tint);
}

static void	draw_text_run(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font,
	const char *text, int x, int y, int glyph_size, int spacing, color_t tint)
{
	int	glyph;
	int	codepoint;

	glyph = 0;
	while (text[glyph] != '\0')
	{
		codepoint = (unsigned char)text[glyph];
		if (codepoint < 32 || codepoint >= 32
			+ SETTINGS_FONT_COLUMNS * SETTINGS_FONT_ROWS)
			codepoint = '?';
		draw_glyph(pixels, width, height, font, codepoint - 32,
			x + glyph * (glyph_size + spacing), y, glyph_size, tint, layout);
		glyph++;
	}
}

static void	draw_glyph(uint32_t *pixels, int width, int height,
	struct ncvisual *font, int glyph, int x, int y, int glyph_size,
	color_t tint, const settings_layout_t *layout)
{
	uint32_t	source;
	unsigned	alpha;
	int		source_x;
	int		source_y;
	int		dest_x;
	int		dest_y;

	dest_y = 0;
	while (dest_y < glyph_size)
	{
		dest_x = 0;
		while (dest_x < glyph_size)
		{
			source_x = (glyph % SETTINGS_FONT_COLUMNS)
				* SETTINGS_FONT_WIDTH
				+ dest_x * SETTINGS_FONT_WIDTH / glyph_size;
			source_y = (glyph / SETTINGS_FONT_COLUMNS)
				* SETTINGS_FONT_HEIGHT + SETTINGS_FONT_INK_Y
				+ dest_y * SETTINGS_FONT_INK_HEIGHT / glyph_size;
			if (ncvisual_at_yx(font, (unsigned)source_y, (unsigned)source_x,
					&source) >= 0)
			{
				alpha = ncpixel_a(source);
				if (alpha != 0)
					put_pixel(pixels, width, height, x + dest_x, y + dest_y,
						tint, alpha,
						layout->opaque_background);
			}
			dest_x++;
		}
		dest_y++;
	}
}

static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
	color_t tint, unsigned alpha, bool opaque)
{
	uint32_t	*pixel;

	if (x < 0 || x >= width || y < 0 || y >= height)
		return ;
	pixel = &pixels[(size_t)y * width + x];
	if (opaque || ncpixel_a(*pixel) != 0)
		blend_pixel(pixel, tint, alpha);
	else
	{
		*pixel = ncpixel(tint.r, tint.g, tint.b);
		ncpixel_set_a(pixel, alpha);
	}
}

static void	blend_pixel(uint32_t *pixel, color_t tint, unsigned alpha)
{
	unsigned	old_alpha;
	unsigned	out_alpha;
	unsigned	red;
	unsigned	green;
	unsigned	blue;

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

static int	ref_x(const settings_layout_t *layout, int value)
{
	return (value * layout->pixel_width / SETTINGS_REFERENCE_WIDTH);
}

static int	ref_y(const settings_layout_t *layout, int value)
{
	return (value * layout->pixel_height / SETTINGS_REFERENCE_HEIGHT);
}

static int	ref_size(const settings_layout_t *layout, int value)
{
	int	x_size;
	int	y_size;

	x_size = value * layout->pixel_width / SETTINGS_REFERENCE_WIDTH;
	y_size = value * layout->pixel_height / SETTINGS_REFERENCE_HEIGHT;
	return (min_int(x_size, y_size));
}

static int	text_width(const char *text, int glyph_size, int spacing)
{
	int	length;

	length = (int)strlen(text);
	if (length == 0)
		return (0);
	return (length * glyph_size + (length - 1) * spacing);
}

static int	min_int(int left, int right)
{
	return (left < right ? left : right);
}

static int	max_int(int left, int right)
{
	return (left > right ? left : right);
}

static const char	*nonempty(const char *text)
{
	return (text != NULL && text[0] != '\0' ? text : "-");
}

static const char	*status_title(const app_screen_view_model_t *view)
{
	if (view->status == APP_DATA_LOADING)
		return ("LOADING SETTINGS...");
	if (view->status == APP_DATA_EMPTY)
		return ("NO PROFILE DATA");
	if (view->status == APP_DATA_ERROR)
		return ("SETTINGS ERROR");
	return ("SETTINGS UNAVAILABLE");
}

static const char	*status_detail(const app_screen_view_model_t *view)
{
	if (view->status == APP_DATA_LOADING)
		return ("PLEASE WAIT");
	if (view->status == APP_DATA_EMPTY)
		return ("ACCOUNT DATA IS EMPTY");
	if (view->status == APP_DATA_ERROR)
		return ("TRY AGAIN LATER");
	return (nonempty(view->subtitle));
}

static const char	*renderer_name(tetrisu_renderer_mode_t mode)
{
	if (mode == TETRISU_RENDERER_CELL)
		return ("CELL");
	if (mode == TETRISU_RENDERER_STATIONARY)
		return ("STATIONARY");
	if (mode == TETRISU_RENDERER_PIXEL)
		return ("PIXEL");
	return ("AUTO");
}

static uint64_t	settings_hash(const void *data, size_t size, uint64_t hash)
{
	const unsigned char	*bytes;
	size_t				index;

	bytes = data;
	if (hash == 0)
		hash = UINT64_C(1469598103934665603);
	index = 0;
	while (index < size)
	{
		hash ^= bytes[index];
		hash *= UINT64_C(1099511628211);
		index++;
	}
	return (hash == 0 ? 1 : hash);
}

static uint64_t	static_signature(const app_screen_view_model_t *view,
	const settings_layout_t *layout)
{
	app_settings_view_model_t	settings;
	uint64_t					hash;

	settings = view->data.settings;
	settings.music_volume = 0;
	hash = settings_hash(&settings, sizeof(settings), 0);
	hash = settings_hash(&view->status, sizeof(view->status), hash);
	hash = settings_hash(&view->local_preview, sizeof(view->local_preview), hash);
	hash = settings_hash(&layout->pixel_width, sizeof(layout->pixel_width), hash);
	hash = settings_hash(&layout->pixel_height, sizeof(layout->pixel_height), hash);
	return (hash);
}

static bool	settings_render_failed(const char *stage)
{
	fprintf(stderr, "tetrisu: Settings renderer failed at %s\n", stage);
	return (false);
}
