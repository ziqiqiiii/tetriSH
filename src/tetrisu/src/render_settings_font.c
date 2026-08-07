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
static bool	compose_inventory(render_ctx_t *ctx,
		const app_settings_view_model_t *settings,
		const settings_state_t *state, const settings_layout_t *layout,
		struct ncvisual *font, bool characters);
static int	update_static_layer(render_ctx_t *ctx,
		const app_screen_view_model_t *view, const settings_state_t *state,
		const settings_layout_t *layout, struct ncvisual *font);
static int	update_region_layers(render_ctx_t *ctx,
		const app_screen_view_model_t *view, const settings_state_t *state,
		const settings_layout_t *layout, struct ncvisual *font);
static void	restack_settings_planes(render_ctx_t *ctx);
static int	settings_region_failed(const char *stage);
static uint32_t	*region_canvas(render_ctx_t *ctx,
		const settings_layout_t *layout);
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
		const settings_state_t *state, const settings_layout_t *layout,
		struct ncvisual *font, bool characters, render_ctx_t *ctx);
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, int ref_x_value, int ref_y_value,
		int ref_width, int ref_height, color_t tint, unsigned alpha);
static void	draw_slot_highlight(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, int ref_label_x, int ref_label_y,
		int ref_slot_width, int ref_slot_height);
static const char	*inventory_slot_label(
		const app_catalogue_item_view_model_t *item, bool characters);
static bool	draw_thumbnail(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		render_ctx_t *ctx, int cache_slot,
		const char *path, int ref_x_value, int ref_y_value, int ref_width,
		int ref_height, bool owned);
static void	draw_stats(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_volume_value(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_buttons(uint32_t *pixels, int width, int height,
		const app_settings_view_model_t *settings, const settings_state_t *state,
		const settings_layout_t *layout, struct ncvisual *font);
static void	draw_portrait(render_ctx_t *ctx, uint32_t *pixels, int width,
		int height, const settings_layout_t *layout, const char *path);
static struct ncvisual	*cached_thumbnail(render_ctx_t *ctx, int cache_slot,
		const char *path);
static void	draw_ability_card(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const app_catalogue_item_view_model_t *character);
static void	draw_wrapped_text_ref(uint32_t *pixels, int width, int height,
		const settings_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x, int ref_y, int ref_width,
		int ref_glyph, color_t tint, int max_lines);
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
static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
		struct ncvisual *font, int glyph, int source_x, int source_y,
		int x, int y, int cell_width, int cell_height, color_t tint,
		bool opaque);
static int	ink_span(int units, int glyph_size);
static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
		color_t tint, unsigned alpha, bool opaque);
static void	blend_pixel(uint32_t *pixel, color_t tint, unsigned alpha);
static int	ref_x(const settings_layout_t *layout, int value);
static int	ref_y(const settings_layout_t *layout, int value);
static int	ref_size(const settings_layout_t *layout, int value);
static int	text_width(const char *text, int glyph_size, int spacing);
static int	fit_glyph_size(const settings_layout_t *layout, const char *text,
		int ref_width, int ref_glyph, int *spacing);
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
	struct ncvisual		*font;
	int					rebuilt;
	int					changed;

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
		ctx->settings_characters_signature = 0;
		ctx->settings_themes_signature = 0;
		ctx->settings_volume_signature = 0;
		ctx->settings_ability_signature = 0;
	}
	settings_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	layout.opaque_background = ctx->pixels == TETRISU_PIXELS_STATIONARY;
	if (!load_font(ctx, &font))
		return (settings_render_failed("font"));
	/*
	 * Both bitmap tiers take the same route: one full-screen plane carrying the
	 * static art, and small region planes above it for everything focus can
	 * move through. The stationary tier used to flatten all of that into a
	 * single frame because replacing a region plane damages the cells it held
	 * and forces the bitmap underneath to be retransmitted. Region planes are
	 * now written in place instead of replaced, so that no longer happens and a
	 * keystroke costs one small region rather than the whole screen.
	 */
	rebuilt = update_static_layer(ctx, view, state, &layout, font);
	if (rebuilt < 0)
		return (false);
	changed = update_region_layers(ctx, view, state, &layout, font);
	if (changed < 0)
		return (false);
	changed |= rebuilt;
	if (rebuilt > 0)
		restack_settings_planes(ctx);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	if (changed == 0 && ctx->notifications.count == 0)
		return (true);
	if (notcurses_render(ctx->nc) != 0)
		return (settings_render_failed("render"));
	return (true);
}

/**
 * @brief Recomposes the full-screen static layer when its inputs change.
 *
 * @return 1 when the layer was rebuilt, 0 when it was reused, -1 on failure.
 */
static int	update_static_layer(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	uint64_t	signature;

	signature = static_signature(view, layout);
	if (ctx->screen_plane != NULL && ctx->settings_static_pixels != NULL
		&& signature == ctx->settings_static_signature)
		return (0);
	if (!compose_settings(ctx, view, state, layout, font))
	{
		(void)settings_render_failed("static frame");
		return (-1);
	}
	ctx->settings_static_signature = signature;
	return (1);
}

/**
 * @brief Refreshes every focus-sensitive region whose signature moved.
 *
 * @return 1 when at least one region was rewritten, 0 when none were, -1 on
 * failure.
 */
static int	update_region_layers(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	uint64_t	geometry;
	uint64_t	signature;
	int			changed;

	/*
	 * Every region signature is seeded with the static one. Rewriting the
	 * full-screen plane re-emits the bitmap under these planes, which on a
	 * stationary protocol overwrites the cells they occupy; a region that did
	 * not also recompose would stay blank until something else moved it.
	 */
	geometry = settings_hash(&layout->pixel_width, sizeof(layout->pixel_width),
			ctx->settings_static_signature);
	geometry = settings_hash(&layout->pixel_height,
			sizeof(layout->pixel_height), geometry);
	changed = 0;
	signature = settings_hash(&state->focus, sizeof(state->focus), geometry);
	signature = settings_hash(&state->section, sizeof(state->section),
			signature);
	signature = settings_hash(&view->data.settings.signed_in,
			sizeof(view->data.settings.signed_in), signature);
	if (ctx->settings_controls_plane == NULL
		|| signature != ctx->settings_controls_signature)
	{
		if (!compose_controls(ctx, &view->data.settings, state, layout, font))
			return (settings_region_failed("controls"));
		ctx->settings_controls_signature = signature;
		changed = 1;
	}
	signature = settings_hash(&state->character_slot,
			sizeof(state->character_slot), ctx->settings_static_signature);
	signature = settings_hash(&state->section, sizeof(state->section),
			signature);
	if (signature != ctx->settings_characters_signature)
	{
		if (!compose_inventory(ctx, &view->data.settings, state, layout, font,
				true))
			return (settings_region_failed("characters"));
		ctx->settings_characters_signature = signature;
		changed = 1;
	}
	signature = settings_hash(&state->theme_slot, sizeof(state->theme_slot),
			ctx->settings_static_signature);
	signature = settings_hash(&state->section, sizeof(state->section),
			signature);
	if (signature != ctx->settings_themes_signature)
	{
		if (!compose_inventory(ctx, &view->data.settings, state, layout, font,
				false))
			return (settings_region_failed("themes"));
		ctx->settings_themes_signature = signature;
		changed = 1;
	}
	signature = settings_hash(&view->data.settings.music_volume,
			sizeof(view->data.settings.music_volume), geometry);
	if (ctx->settings_volume_plane == NULL
		|| signature != ctx->settings_volume_signature)
	{
		if (!compose_volume(ctx, &view->data.settings, layout, font))
			return (settings_region_failed("volume"));
		ctx->settings_volume_signature = signature;
		changed = 1;
	}
	signature = settings_hash(&state->ability_info_visible,
			sizeof(state->ability_info_visible), ctx->settings_static_signature);
	signature = settings_hash(&state->section, sizeof(state->section),
			signature);
	signature = settings_hash(&state->character_slot,
			sizeof(state->character_slot), signature);
	if (signature != ctx->settings_ability_signature)
	{
		if (!compose_ability(ctx, &view->data.settings, state, layout, font))
			return (settings_region_failed("ability"));
		ctx->settings_ability_signature = signature;
		changed = 1;
	}
	return (changed);
}

/**
 * @brief Restores region planes above a freshly created static plane.
 *
 * Only called when the static layer was replaced. Restacking damages every
 * plane it touches, so a frame that merely rewrote a region leaves the order
 * alone: creation order already puts the regions on top.
 */
static void	restack_settings_planes(render_ctx_t *ctx)
{
	ncplane_move_top(ctx->screen_plane);
	if (ctx->settings_controls_plane != NULL)
		ncplane_move_top(ctx->settings_controls_plane);
	if (ctx->settings_characters_plane != NULL)
		ncplane_move_top(ctx->settings_characters_plane);
	if (ctx->settings_themes_plane != NULL)
		ncplane_move_top(ctx->settings_themes_plane);
	if (ctx->settings_volume_plane != NULL)
		ncplane_move_top(ctx->settings_volume_plane);
	if (ctx->settings_ability_plane != NULL)
		ncplane_move_top(ctx->settings_ability_plane);
}

/**
 * @brief Releases Settings-only cached artwork while retaining the backdrop.
 */
void	render_settings_pixel_destroy(render_ctx_t *ctx)
{
	int	index;

	if (ctx == NULL)
		return ;
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
	if (ctx->settings_characters_plane != NULL)
	{
		ncplane_destroy(ctx->settings_characters_plane);
		ctx->settings_characters_plane = NULL;
	}
	if (ctx->settings_themes_plane != NULL)
	{
		ncplane_destroy(ctx->settings_themes_plane);
		ctx->settings_themes_plane = NULL;
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
	if (ctx->settings_portrait_visual != NULL)
	{
		ncvisual_destroy(ctx->settings_portrait_visual);
		ctx->settings_portrait_visual = NULL;
	}
	ctx->settings_portrait_source[0] = '\0';
	index = 0;
	while (index < APP_CATALOGUE_MAX_ITEMS * 2)
	{
		if (ctx->settings_thumbnail_visuals[index] != NULL)
			ncvisual_destroy(ctx->settings_thumbnail_visuals[index]);
		ctx->settings_thumbnail_visuals[index] = NULL;
		ctx->settings_thumbnail_sources[index][0] = '\0';
		index++;
	}
	free(ctx->settings_background_pixels);
	ctx->settings_background_pixels = NULL;
	free(ctx->settings_static_pixels);
	ctx->settings_static_pixels = NULL;
	ctx->settings_pixels_width = 0;
	ctx->settings_pixels_height = 0;
	ctx->settings_background_ready = false;
	ctx->settings_background_rows = 0;
	ctx->settings_background_cols = 0;
	ctx->settings_static_signature = 0;
	ctx->settings_controls_signature = 0;
	ctx->settings_characters_signature = 0;
	ctx->settings_themes_signature = 0;
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
		< 0 && render_background_replace_exact(ctx,
			SETTINGS_BACKGROUND_LEGACY_PATH, false) < 0)
		return (false);
	/*
	 * Both caches are sized by the fitted geometry, so a resize invalidates
	 * them before anything can prefill from a buffer of the previous size.
	 */
	free(ctx->settings_static_pixels);
	ctx->settings_static_pixels = NULL;
	ctx->settings_pixels_width = 0;
	ctx->settings_pixels_height = 0;
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY && !cache_background(ctx))
		return (false);
	if (ctx->pixels != TETRISU_PIXELS_STATIONARY)
	{
		free(ctx->settings_background_pixels);
		ctx->settings_background_pixels = NULL;
	}
	ctx->settings_background_ready = true;
	ctx->settings_background_rows = ctx->bg_rows;
	ctx->settings_background_cols = ctx->bg_cols;
	return (true);
}

/**
 * @brief Flattens the fitted backdrop into a reusable opaque RGBA buffer.
 *
 * ncvisual_at_yx() is a per-call lookup, so walking a full-screen visual costs
 * millions of them. Doing that walk once per geometry change and keeping the
 * result turns every later frame prefill into a memcpy.
 */
static bool	cache_background(render_ctx_t *ctx)
{
	struct ncvisual	*visual;
	uint32_t		*buffer;
	uint32_t		pixel;
	int			width;
	int			height;
	int			y;
	int			x;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y)
		return (false);
	width = ctx->bg_cols * ctx->cell_px_x;
	height = ctx->bg_rows * ctx->cell_px_y;
	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height / sizeof(*buffer))
		return (false);
	visual = ncvisual_from_file(SETTINGS_BACKGROUND_PATH);
	if (visual == NULL)
		visual = ncvisual_from_file(SETTINGS_BACKGROUND_LEGACY_PATH);
	if (visual == NULL || ncvisual_resize(visual, height, width) != 0)
	{
		if (visual != NULL)
			ncvisual_destroy(visual);
		return (false);
	}
	buffer = malloc((size_t)width * (size_t)height * sizeof(*buffer));
	if (buffer == NULL)
	{
		ncvisual_destroy(visual);
		return (false);
	}
	y = 0;
	while (y < height)
	{
		x = 0;
		while (x < width)
		{
			if (ncvisual_at_yx(visual, (unsigned)y, (unsigned)x, &pixel) < 0)
				pixel = ncpixel(8, 8, 31);
			ncpixel_set_a(&pixel, 255u);
			buffer[(size_t)y * width + x] = pixel;
			x++;
		}
		y++;
	}
	ncvisual_destroy(visual);
	free(ctx->settings_background_pixels);
	ctx->settings_background_pixels = buffer;
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
	(void)state;
	draw_profile(pixels, width, height, view, layout, font);
	if (view->status == APP_DATA_READY && view->data.settings.signed_in
		&& !view->data.settings.offline)
		draw_portrait(ctx, pixels, width, height, layout,
			view->data.settings.profile.portrait_asset);
	draw_stats(pixels, width, height, &view->data.settings, layout, font);
	/*
	 * Keep the composed layer. On the movable tier every region plane is cut
	 * from it, so each one lands on the artwork genuinely underneath; on the
	 * stationary tier it is the base each full frame is stamped from, which is
	 * what keeps the portrait scaling and profile text off the input path.
	 */
	free(ctx->settings_static_pixels);
	ctx->settings_static_pixels = pixels;
	ctx->settings_pixels_width = width;
	ctx->settings_pixels_height = height;
	return (create_settings_plane(ctx, pixels, width, height));
}

/**
 * @brief Allocates a full-frame canvas seeded with the composed static frame.
 *
 * Region composition draws onto a copy of what is already on screen, so the
 * cropped result is opaque wherever the frame is. That is what lets the
 * stationary tier use small planes at all: Sixel cannot write transparency
 * over existing content, but it can overwrite it.
 */
static uint32_t	*region_canvas(render_ctx_t *ctx,
	const settings_layout_t *layout)
{
	size_t	count;

	if (layout->pixel_width <= 0 || layout->pixel_height <= 0)
		return (NULL);
	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	if (count > SIZE_MAX / sizeof(uint32_t))
		return (NULL);
	if (ctx->settings_static_pixels != NULL
		&& ctx->settings_pixels_width == layout->pixel_width
		&& ctx->settings_pixels_height == layout->pixel_height)
	{
		uint32_t	*canvas;

		canvas = malloc(count * sizeof(*canvas));
		if (canvas != NULL)
			memcpy(canvas, ctx->settings_static_pixels,
				count * sizeof(*canvas));
		return (canvas);
	}
	return (calloc(count, sizeof(uint32_t)));
}

static bool	compose_controls(render_ctx_t *ctx,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font)
{
	settings_rect_t	region;
	uint32_t		*pixels;

	pixels = region_canvas(ctx, layout);
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

/**
 * @brief Repaints one inventory panel with its current focus highlight.
 *
 * Both panels live on their own region planes rather than in the static frame
 * because focus moves through them: baking them into the frame would make
 * every arrow key a full-screen recomposition.
 */
static bool	compose_inventory(render_ctx_t *ctx,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font, bool characters)
{
	settings_rect_t	region;
	uint32_t		*pixels;
	struct ncplane	**slot;

	slot = characters ? &ctx->settings_characters_plane
		: &ctx->settings_themes_plane;
	if (!settings->signed_in || settings->offline)
	{
		if (*slot != NULL)
		{
			ncplane_destroy(*slot);
			*slot = NULL;
		}
		return (true);
	}
	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_inventory(pixels, layout->pixel_width, layout->pixel_height,
		characters ? &settings->characters : &settings->themes, state, layout,
		font, characters, ctx);
	region.x = ref_x(layout, characters ? SETTINGS_REF_CHARACTERS_X
		: SETTINGS_REF_THEMES_X);
	region.y = ref_y(layout, characters ? SETTINGS_REF_CHARACTERS_Y
		: SETTINGS_REF_THEMES_Y);
	region.width = ref_x(layout, characters ? SETTINGS_REF_CHARACTERS_WIDTH
		: SETTINGS_REF_THEMES_WIDTH);
	region.height = ref_y(layout, characters ? SETTINGS_REF_CHARACTERS_HEIGHT
		: SETTINGS_REF_THEMES_HEIGHT);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, slot))
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

	pixels = region_canvas(ctx, layout);
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

	if (!settings_card_visible(state)
		|| settings_card_character(settings, state) == NULL)
	{
		if (ctx->settings_ability_plane != NULL)
		{
			ncplane_destroy(ctx->settings_ability_plane);
			ctx->settings_ability_plane = NULL;
		}
		return (true);
	}
	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_ability_card(pixels, layout->pixel_width, layout->pixel_height,
		layout, font, settings_card_character(settings, state));
	region.x = ref_x(layout, SETTINGS_REF_CARD_X);
	region.y = ref_y(layout, SETTINGS_REF_CARD_Y);
	region.width = ref_x(layout, SETTINGS_REF_CARD_WIDTH);
	region.height = ref_y(layout, SETTINGS_REF_CARD_HEIGHT);
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
	if (ctx->settings_background_pixels == NULL
		|| ctx->bg_cols * ctx->cell_px_x != width
		|| ctx->bg_rows * ctx->cell_px_y != height)
		return (false);
	memcpy(pixels, ctx->settings_background_pixels,
		(size_t)width * (size_t)height * sizeof(*pixels));
	return (true);
}

static bool	create_settings_plane(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height)
{
	ncplane_options		options;
	struct ncplane		*plane;
	nccell				base;

	if (render_plane_geometry_matches(ctx->screen_plane, ctx->bg_row, ctx->bg_col,
			(unsigned)ctx->bg_rows, (unsigned)ctx->bg_cols))
		return (render_plane_blit_rgba(ctx, ctx->screen_plane, pixels, width, height,
				width));
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row;
	options.x = ctx->bg_col;
	options.rows = ctx->bg_rows;
	options.cols = ctx->bg_cols;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (false);
	nccell_init(&base);
	if (nccell_load(plane, &base, " ") >= 0)
	{
		nccell_set_fg_alpha(&base, NCALPHA_TRANSPARENT);
		nccell_set_bg_alpha(&base, NCALPHA_TRANSPARENT);
		ncplane_set_base_cell(plane, &base);
		nccell_release(plane, &base);
	}
	ncplane_erase(plane);
	if (!render_plane_blit_rgba(ctx, plane, pixels, width, height, width))
	{
		ncplane_destroy(plane);
		return (false);
	}
	if (ctx->screen_plane != NULL)
		ncplane_destroy(ctx->screen_plane);
	ctx->screen_plane = plane;
	return (true);
}

/**
 * @brief Refreshes one region plane from a window of the composed canvas.
 *
 * The window is expanded to cell boundaries so the plane covers whole cells,
 * and is blitted straight out of the canvas using its row stride rather than
 * being copied into a compact buffer first. An existing plane at the same
 * geometry is written in place; only a geometry change replaces it, because
 * destroying a sprixel plane forces the bitmap beneath it to be retransmitted.
 */
static bool	create_region_plane(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height, const settings_rect_t *region,
	struct ncplane **slot)
{
	ncplane_options		options;
	struct ncplane		*plane;
	const uint32_t		*origin;
	int					crop_x;
	int					crop_y;
	int					crop_width;
	int					crop_height;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| region->x < 0 || region->y < 0 || region->width <= 0
		|| region->height <= 0 || region->x + region->width > width
		|| region->y + region->height > height)
		return (false);
	crop_x = region->x / ctx->cell_px_x * ctx->cell_px_x;
	crop_y = region->y / ctx->cell_px_y * ctx->cell_px_y;
	crop_width = min_int(((region->x + region->width + ctx->cell_px_x - 1)
				/ ctx->cell_px_x) * ctx->cell_px_x, width) - crop_x;
	crop_height = min_int(((region->y + region->height + ctx->cell_px_y - 1)
				/ ctx->cell_px_y) * ctx->cell_px_y, height) - crop_y;
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row + crop_y / ctx->cell_px_y;
	options.x = ctx->bg_col + crop_x / ctx->cell_px_x;
	options.rows = (unsigned)(crop_height + ctx->cell_px_y - 1)
		/ (unsigned)ctx->cell_px_y;
	options.cols = (unsigned)(crop_width + ctx->cell_px_x - 1)
		/ (unsigned)ctx->cell_px_x;
	origin = pixels + (size_t)crop_y * width + crop_x;
	if (render_plane_geometry_matches(*slot, options.y, options.x, options.rows,
			options.cols))
		return (render_plane_blit_rgba(ctx, *slot, origin, crop_width, crop_height,
				width));
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (false);
	if (!render_plane_blit_rgba(ctx, plane, origin, crop_width, crop_height, width))
	{
		ncplane_destroy(plane);
		return (false);
	}
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
	/*
	 * The inventory panels are deliberately absent here: they carry the focus
	 * highlight, so they are composed onto their own region planes instead.
	 */
}

static void	draw_inventory(uint32_t *pixels, int width, int height,
	const app_catalogue_view_model_t *catalogue, const settings_state_t *state,
	const settings_layout_t *layout, struct ncvisual *font, bool characters,
	render_ctx_t *ctx)
{
	int		visible;
	int		index;
	int		slot;
	int		focused_slot;
	int		focused_index;
	int		ref_panel_x;
	int		ref_panel_y;
	int		ref_panel_width;
	int		ref_slot_width;
	int		ref_slot_x;
	int		ref_slot_y;
	int		ref_slot_step;
	int		ref_slot_thumb_width;
	int		ref_slot_thumb_height;
	int		ref_slot_name_y;
	int		ref_slot_content_height;
	int		row;
	int		column;
	int		limit;
	bool	focused;
	char	focused_name[APP_TEXT_MAX + 16];

	ref_panel_x = characters ? SETTINGS_REF_CHARACTERS_X
		: SETTINGS_REF_THEMES_X;
	ref_panel_y = characters ? SETTINGS_REF_CHARACTERS_Y : SETTINGS_REF_THEMES_Y;
	ref_panel_width = characters ? SETTINGS_REF_CHARACTERS_WIDTH
		: SETTINGS_REF_THEMES_WIDTH;
	limit = characters ? SETTINGS_CHARACTER_SLOTS : SETTINGS_THEME_SLOTS;
	ref_slot_width = (ref_panel_width - 20) / SETTINGS_INVENTORY_COLUMNS;
	ref_slot_step = characters ? SETTINGS_REF_CHARACTER_SLOT_STEP_Y
		: SETTINGS_REF_THEME_SLOT_STEP_Y;
	ref_slot_thumb_width = characters
		? SETTINGS_REF_CHARACTER_SLOT_THUMB_WIDTH
		: SETTINGS_REF_THEME_SLOT_THUMB_WIDTH;
	ref_slot_thumb_height = characters
		? SETTINGS_REF_CHARACTER_SLOT_THUMB_HEIGHT
		: SETTINGS_REF_THEME_SLOT_THUMB_HEIGHT;
	ref_slot_name_y = characters ? SETTINGS_REF_CHARACTER_SLOT_NAME_Y
		: SETTINGS_REF_THEME_SLOT_NAME_Y;
	ref_slot_content_height = max_int(ref_slot_thumb_height,
			ref_slot_name_y + SETTINGS_REF_SLOT_GLYPH);
	visible = settings_catalogue_count(catalogue, limit);
	focused_slot = -1;
	focused_index = -1;
	if (state->section == (characters ? SETTINGS_SECTION_CHARACTERS
			: SETTINGS_SECTION_THEMES))
		focused_slot = characters ? state->character_slot : state->theme_slot;
	if (focused_slot >= 0 && focused_slot < visible)
		focused_index = focused_slot;
	draw_text_ref(pixels, width, height, layout, font,
		characters ? "CHARACTERS" : "THEMES", ref_panel_x + 10,
		ref_panel_y + SETTINGS_REF_INVENTORY_TITLE_Y, ref_panel_width - 20,
		SETTINGS_REF_INVENTORY_TITLE_GLYPH, g_settings_pink, true);
	if (focused_index >= 0)
	{
		if (catalogue->items[focused_index].owned)
			snprintf(focused_name, sizeof(focused_name), "%s",
				catalogue->items[focused_index].name);
		else
			snprintf(focused_name, sizeof(focused_name), "LOCKED - %s",
				catalogue->items[focused_index].name);
		draw_text_ref(pixels, width, height, layout, font,
			focused_name, ref_panel_x + 10,
			ref_panel_y + SETTINGS_REF_INVENTORY_DETAIL_Y,
			ref_panel_width - 20, SETTINGS_REF_INVENTORY_DETAIL_GLYPH,
			catalogue->items[focused_index].owned ? g_settings_gold
				: g_settings_disabled, true);
	}
	else if (visible > 0)
		draw_text_ref(pixels, width, height, layout, font,
			"ARROWS TO INSPECT", ref_panel_x + 10,
			ref_panel_y + SETTINGS_REF_INVENTORY_DETAIL_Y,
			ref_panel_width - 20, SETTINGS_REF_INVENTORY_HINT_GLYPH,
			g_settings_lavender, true);
	slot = 0;
	index = 0;
	while (index < visible)
	{
		focused = slot == focused_slot;
		row = slot / SETTINGS_INVENTORY_COLUMNS;
		column = slot % SETTINGS_INVENTORY_COLUMNS;
		ref_slot_x = ref_panel_x + SETTINGS_REF_SLOT_INSET
			+ column * ref_slot_width;
		ref_slot_y = ref_panel_y + (characters
			? SETTINGS_REF_CHARACTER_SLOT_FIRST_Y
			: SETTINGS_REF_THEME_SLOT_FIRST_Y) + row * ref_slot_step;
		if (focused)
			draw_slot_highlight(pixels, width, height, layout,
				ref_slot_x, ref_slot_y, ref_slot_width,
				ref_slot_content_height);
		draw_thumbnail(pixels, width, height, layout, font, ctx,
			(characters ? 0 : APP_CATALOGUE_MAX_ITEMS) + slot,
			catalogue->items[index].portrait_asset, ref_slot_x + 14,
			ref_slot_y + 1, min_int(ref_slot_thumb_width,
				ref_slot_width - 28), ref_slot_thumb_height,
			catalogue->items[index].owned);
		/*
		 * The badge belongs inside the thumbnail frame, which starts at
		 * ref_slot_x + 14. Drawing it left of that hung half the marker off
		 * the tile, where it read as a stray blob rather than as "equipped".
		 */
		if (catalogue->items[index].equipped)
			draw_text_ref(pixels, width, height, layout, font, "*",
				ref_slot_x + 17, ref_slot_y + 5, 16, 16,
				focused ? g_settings_gold : g_settings_green, false);
		draw_text_ref(pixels, width, height, layout, font,
			inventory_slot_label(&catalogue->items[index], characters),
			ref_slot_x + 2,
			ref_slot_y + ref_slot_name_y, ref_slot_width - 4,
			SETTINGS_REF_SLOT_GLYPH,
			catalogue->items[index].owned
				? (focused ? g_settings_gold : g_settings_cream)
				: g_settings_disabled, true);
		slot++;
		index++;
	}
	if (visible == 0)
		draw_text_ref(pixels, width, height, layout, font, "NONE OWNED",
			ref_panel_x + 10, ref_panel_y + 70, ref_panel_width - 20, 13,
			g_settings_lavender, true);
}

/**
 * @brief Draws the selection row: a soft plate with a gold edge on its left.
 *
 * The plate is deliberately darker than the panel rather than brighter, so the
 * gold label stays the brightest thing in the row and the eye lands on the
 * text rather than on the marker.
 */
static void	draw_slot_highlight(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, int ref_label_x, int ref_label_y,
	int ref_slot_width, int ref_slot_height)
{
	const color_t	plate = {74, 40, 104};
	int			focus_x;
	int			focus_y;
	int			focus_width;
	int			focus_height;

	focus_x = ref_label_x + SETTINGS_REF_SLOT_PAD_X;
	focus_y = ref_label_y - SETTINGS_REF_SLOT_PAD_Y;
	focus_width = ref_slot_width - 2 * SETTINGS_REF_SLOT_PAD_X;
	focus_height = ref_slot_height + 2 * SETTINGS_REF_SLOT_PAD_Y;
	fill_ref_rect(pixels, width, height, layout,
		focus_x, focus_y, focus_width, focus_height, plate, 224u);
	/* A complete border keeps the focus weight centred around the slot. */
	fill_ref_rect(pixels, width, height, layout,
		focus_x, focus_y, focus_width, 2, g_settings_gold, 255u);
	fill_ref_rect(pixels, width, height, layout,
		focus_x, focus_y + focus_height - 2, focus_width, 2,
		g_settings_gold, 255u);
	fill_ref_rect(pixels, width, height, layout,
		focus_x, focus_y, 2, focus_height, g_settings_gold, 255u);
	fill_ref_rect(pixels, width, height, layout,
		focus_x + focus_width - 2, focus_y, 2, focus_height,
		g_settings_gold, 255u);
}

/**
 * @brief Keeps four-column slot captions readable below their thumbnails.
 *
 * The full canonical item name remains in the focused panel header. Only the
 * compact caption inside a 99-reference-pixel theme slot is shortened.
 */
static const char	*inventory_slot_label(
	const app_catalogue_item_view_model_t *item, bool characters)
{
	if (item == NULL)
		return ("");
	if (characters)
		return (item->name);
	if (strcmp(item->id, "design_ai_university") == 0)
		return ("Design AI");
	if (strcmp(item->id, "snowman") == 0)
		return ("Snowman");
	if (strcmp(item->id, "al_merqaedes") == 0)
		return ("Al-Merq.");
	if (strcmp(item->id, "nuclear_ghandi") == 0)
		return ("Nuclear G.");
	return (item->name);
}

/**
 * @brief Draws a catalogue thumbnail without making artwork a hard dependency.
 *
 * Preview art is optional during development and in cell-only installations.
 * A framed placeholder keeps the slot geometry and focus treatment intact
 * when a file is missing or cannot be decoded.
 */
static bool	draw_thumbnail(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font, render_ctx_t *ctx,
	int cache_slot, const char *path, int ref_x_value, int ref_y_value,
	int ref_width, int ref_height, bool owned)
{
	struct ncvisual	*visual;
	ncvgeom		geom;
	uint32_t	pixel;
	color_t		fallback;
	int		draw_width;
	int		draw_height;
	int		source_x;
	int		source_y;
	int		origin_x;
	int		origin_y;
	int		x;
	int		y;
	bool		drawn;

	ref_width = max_int(1, ref_width);
	ref_height = max_int(1, ref_height);
	fallback = (color_t){39, 24, 62};
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, ref_height, fallback, 255u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, 2, owned ? g_settings_lavender : g_settings_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value,
		ref_y_value + ref_height - 2, ref_width, 2,
		owned ? g_settings_lavender : g_settings_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		2, ref_height, owned ? g_settings_lavender : g_settings_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout,
		ref_x_value + ref_width - 2, ref_y_value, 2, ref_height,
		owned ? g_settings_lavender : g_settings_disabled, 220u);
	visual = cached_thumbnail(ctx, cache_slot, path);
	drawn = false;
	if (visual != NULL)
	{
		memset(&geom, 0, sizeof(geom));
		if (ncvisual_geom(NULL, visual, NULL, &geom) == 0
			&& geom.pixx > 0 && geom.pixy > 0)
		{
			draw_width = ref_x(layout, ref_width) - 4;
			draw_height = ref_y(layout, ref_height) - 4;
			if (draw_width < 1)
				draw_width = 1;
			if (draw_height < 1)
				draw_height = 1;
			if ((uint64_t)draw_width * geom.pixy
				> (uint64_t)draw_height * geom.pixx)
				draw_width = (int)((uint64_t)draw_height * geom.pixx
					/ geom.pixy);
			else
				draw_height = (int)((uint64_t)draw_width * geom.pixy
					/ geom.pixx);
			if (draw_width < 1)
				draw_width = 1;
			if (draw_height < 1)
				draw_height = 1;
			origin_x = ref_x(layout, ref_x_value)
				+ (ref_x(layout, ref_width) - draw_width) / 2;
			origin_y = ref_y(layout, ref_y_value)
				+ (ref_y(layout, ref_height) - draw_height) / 2;
			y = 0;
			while (y < draw_height)
			{
				x = 0;
				while (x < draw_width)
				{
					source_x = x * (int)geom.pixx / draw_width;
					source_y = y * (int)geom.pixy / draw_height;
					if (ncvisual_at_yx(visual, (unsigned)source_y,
							(unsigned)source_x, &pixel) >= 0
						&& ncpixel_a(pixel) != 0)
					{
						color_t tint;

						tint.r = ncpixel_r(pixel);
						tint.g = ncpixel_g(pixel);
						tint.b = ncpixel_b(pixel);
						if (!owned)
						{
							unsigned gray;

							gray = (30u * tint.r + 59u * tint.g
								+ 11u * tint.b) / 100u;
							tint.r = gray * 2u / 3u;
							tint.g = gray * 2u / 3u;
							tint.b = gray * 2u / 3u;
						}
						put_pixel(pixels, width, height, origin_x + x,
							origin_y + y, tint, ncpixel_a(pixel),
							layout->opaque_background);
					}
					x++;
				}
				y++;
			}
			drawn = true;
		}
	}
	if (!drawn && font != NULL)
		draw_text_ref(pixels, width, height, layout, font, "?",
			ref_x_value, ref_y_value + 6, ref_width, 16,
			owned ? g_settings_lavender : g_settings_disabled,
			true);
	return (drawn);
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
		center = index == 0 ? SETTINGS_REF_BUTTON_BACK_CENTER_X
			: index == 1 ? SETTINGS_REF_BUTTON_MARKET_CENTER_X
			: index == 2 ? SETTINGS_REF_BUTTON_VOLUME_DOWN_CENTER_X
			: SETTINGS_REF_BUTTON_VOLUME_UP_CENTER_X;
		draw_text_ref(pixels, width, height, layout, font, labels[index],
			center - 104, SETTINGS_REF_BUTTON_Y + 36, 208, 21, color, true);
		draw_text_ref(pixels, width, height, layout, font, hints[index],
			center - 52, SETTINGS_REF_BUTTON_Y + 77, 104, 15, color, true);
		/*
		 * No chevrons. The buttons sit 240 reference units apart and the label
		 * box is 208 wide, so a bracket on either side lands close enough to
		 * the neighbouring label to read as belonging to it. Gold already
		 * means focus everywhere else on this screen, so it carries it here.
		 */
		index++;
	}
	/*
	 * Settings takes no pointer input, so the inventory grids and the powers
	 * card need their keys spelled out here or they are undiscoverable.
	 */
	draw_text_ref(pixels, width, height, layout, font,
		settings->signed_in
		? "ARROWS MOVE   UP ENTERS INVENTORY   ENTER EQUIPS   I POWERS   ESC BACK"
		: "ARROWS MOVE   ENTER SELECT   ESC BACK", 260,
		SETTINGS_REF_BUTTON_Y + 139, 930, 12, g_settings_lavender, true);
}

/**
 * @brief Returns the decoded portrait for a path, decoding it at most once.
 *
 * The stationary tier redraws the whole frame per keystroke, so re-reading and
 * re-decoding the PNG each time would put file I/O on the input path.
 */
static struct ncvisual	*cached_portrait(render_ctx_t *ctx, const char *path)
{
	if (path == NULL || path[0] == '\0')
		return (NULL);
	if (ctx->settings_portrait_visual != NULL
		&& strcmp(ctx->settings_portrait_source, path) == 0)
		return (ctx->settings_portrait_visual);
	if (ctx->settings_portrait_visual != NULL)
	{
		ncvisual_destroy(ctx->settings_portrait_visual);
		ctx->settings_portrait_visual = NULL;
	}
	ctx->settings_portrait_visual = ncvisual_from_file(path);
	if (ctx->settings_portrait_visual == NULL)
	{
		ctx->settings_portrait_source[0] = '\0';
		return (NULL);
	}
	snprintf(ctx->settings_portrait_source,
		sizeof(ctx->settings_portrait_source), "%s", path);
	return (ctx->settings_portrait_visual);
}

static struct ncvisual	*cached_thumbnail(render_ctx_t *ctx, int cache_slot,
	const char *path)
{
	struct ncvisual	*visual;

	if (ctx == NULL || cache_slot < 0
		|| cache_slot >= APP_CATALOGUE_MAX_ITEMS * 2)
		return (NULL);
	if (path == NULL || path[0] == '\0')
	{
		if (ctx->settings_thumbnail_visuals[cache_slot] != NULL)
		{
			ncvisual_destroy(ctx->settings_thumbnail_visuals[cache_slot]);
			ctx->settings_thumbnail_visuals[cache_slot] = NULL;
		}
		ctx->settings_thumbnail_sources[cache_slot][0] = '\0';
		return (NULL);
	}
	if (strcmp(ctx->settings_thumbnail_sources[cache_slot], path) == 0)
		return (ctx->settings_thumbnail_visuals[cache_slot]);
	if (ctx->settings_thumbnail_visuals[cache_slot] != NULL)
	{
		ncvisual_destroy(ctx->settings_thumbnail_visuals[cache_slot]);
		ctx->settings_thumbnail_visuals[cache_slot] = NULL;
	}
	snprintf(ctx->settings_thumbnail_sources[cache_slot],
		sizeof(ctx->settings_thumbnail_sources[cache_slot]), "%s", path);
	visual = ncvisual_from_file(path);
	ctx->settings_thumbnail_visuals[cache_slot] = visual;
	return (visual);
}

static void	draw_portrait(render_ctx_t *ctx, uint32_t *pixels, int width,
	int height, const settings_layout_t *layout, const char *path)
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

	portrait = cached_portrait(ctx, path);
	memset(&geom, 0, sizeof(geom));
	if (portrait == NULL || ncvisual_geom(NULL, portrait, NULL, &geom) != 0
		|| geom.pixx == 0 || geom.pixy == 0)
		return ;
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
}

/**
 * @brief Fills a reference-space rectangle, used for the focus highlight bar.
 */
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, int ref_x_value, int ref_y_value,
	int ref_width, int ref_height, color_t tint, unsigned alpha)
{
	int	left;
	int	top;
	int	right;
	int	bottom;
	int	y;
	int	x;

	left = ref_x(layout, ref_x_value);
	top = ref_y(layout, ref_y_value);
	right = left + ref_x(layout, ref_width);
	bottom = top + ref_y(layout, ref_height);
	y = top;
	while (y < bottom)
	{
		x = left;
		while (x < right)
		{
			put_pixel(pixels, width, height, x, y, tint, alpha,
				layout->opaque_background);
			x++;
		}
		y++;
	}
}

static void	draw_ability_card(uint32_t *pixels, int width, int height,
	const settings_layout_t *layout, struct ncvisual *font,
	const app_catalogue_item_view_model_t *character)
{
	color_t						panel;
	char							heading[APP_TEXT_MAX + 32];
	int							x;
	int							y;
	int							index;

	if (character == NULL)
		return ;
	/*
	 * The card is inset inside the authored profile frame rather than sized to
	 * the region around it, so its fill stops short of the gold border instead
	 * of painting over it. Every row below is measured from the same inset.
	 */
	panel = (color_t){24, 10, 38};
	y = ref_y(layout, SETTINGS_REF_CARD_Y);
	while (y < ref_y(layout, SETTINGS_REF_CARD_Y + SETTINGS_REF_CARD_HEIGHT))
	{
		x = ref_x(layout, SETTINGS_REF_CARD_X);
		while (x < ref_x(layout, SETTINGS_REF_CARD_X + SETTINGS_REF_CARD_WIDTH))
		{
			put_pixel(pixels, width, height, x, y, panel, 255u, true);
			x++;
		}
		y++;
	}
	snprintf(heading, sizeof(heading), "%s - CRYSTAL POWERS",
		character->name);
	draw_text_ref(pixels, width, height, layout, font, heading,
		SETTINGS_REF_CARD_X + 10, SETTINGS_REF_CARD_Y + 12,
		SETTINGS_REF_CARD_WIDTH - 20, 19, g_settings_pink, true);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		snprintf(heading, sizeof(heading), "L%d  %s", index + 1,
			character->abilities[index].name);
		draw_text_ref(pixels, width, height, layout, font, heading,
			SETTINGS_REF_CARD_X + 14,
			SETTINGS_REF_CARD_Y + 46 + index * SETTINGS_REF_CARD_STEP,
			SETTINGS_REF_CARD_WIDTH - 28, 13, g_settings_gold, false);
		draw_wrapped_text_ref(pixels, width, height, layout, font,
			character->abilities[index].description,
			SETTINGS_REF_CARD_X + 24,
			SETTINGS_REF_CARD_Y + 65 + index * SETTINGS_REF_CARD_STEP,
			SETTINGS_REF_CARD_WIDTH - 48, 9, g_settings_cream, 2);
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


/**
 * @brief Shrinks a glyph size until the text fits its reference-space box.
 *
 * Split out so a caller drawing a column of related labels can size them all
 * from the longest one. Sizing each label independently makes them visibly
 * change size as their text changes, which reads as a rendering glitch.
 */
static int	fit_glyph_size(const settings_layout_t *layout, const char *text,
	int ref_width, int ref_glyph, int *spacing)
{
	int	glyph_size;

	glyph_size = max_int(3, ref_size(layout, ref_glyph));
	*spacing = max_int(1, ref_size(layout, SETTINGS_FONT_SPACING_REF));
	while (glyph_size > max_int(3, ref_size(layout, 7))
		&& text_width(text, glyph_size, *spacing) > ref_x(layout, ref_width))
	{
		glyph_size--;
		if (*spacing > max_int(1, ref_size(layout, 2)))
			*spacing -= 1;
	}
	return (glyph_size);
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
	glyph_size = fit_glyph_size(layout, text, ref_width, ref_glyph, &spacing);
	shadow = max_int(1, ref_size(layout, SETTINGS_FONT_SHADOW_REF));
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

/**
 * @brief Draws a prepared glyph run with the supplied colour and spacing.
 */
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

/**
 * @brief Scales a signed offset in atlas rows into destination pixels.
 *
 * Rows above the cap band are negative, and C division truncates towards zero,
 * which would sample the wrong source row for them. Flooring keeps the two
 * halves of a glyph on the same grid.
 */
static int	ink_span(int units, int glyph_size)
{
	if (units >= 0)
		return (units * glyph_size / FONT_INK_HEIGHT);
	return (-((-units * glyph_size + FONT_INK_HEIGHT - 1) / FONT_INK_HEIGHT));
}

/**
 * @brief Draws one atlas glyph with its ascender dots and descender tails.
 *
 * Iteration runs over source rows rather than destination rows so the whole
 * ink band maps onto the same scale as the cap band. The cap row still lands
 * on y, so extending the band moves no existing text.
 */
static void	draw_glyph(uint32_t *pixels, int width, int height,
	struct ncvisual *font, int glyph, int x, int y, int glyph_size,
	color_t tint, const settings_layout_t *layout)
{
	int	source_y;
	int	source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < SETTINGS_FONT_WIDTH)
		{
			draw_glyph_cell(pixels, width, height, font, glyph,
				source_x, source_y, x + source_x * glyph_size
				/ SETTINGS_FONT_WIDTH,
				y + ink_span(source_y - SETTINGS_FONT_INK_Y, glyph_size),
				(source_x + 1) * glyph_size / SETTINGS_FONT_WIDTH
				- source_x * glyph_size / SETTINGS_FONT_WIDTH,
				ink_span(source_y + 1 - SETTINGS_FONT_INK_Y, glyph_size)
				- ink_span(source_y - SETTINGS_FONT_INK_Y, glyph_size),
				tint, layout->opaque_background);
			source_x++;
		}
		source_y++;
	}
}

/**
 * @brief Expands one atlas texel into its destination rectangle.
 */
static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
	struct ncvisual *font, int glyph, int source_x, int source_y,
	int x, int y, int cell_width, int cell_height, color_t tint, bool opaque)
{
	uint32_t	source;
	unsigned	alpha;
	int			dest_y;
	int			dest_x;

	if (cell_width <= 0 || cell_height <= 0)
		return ;
	if (ncvisual_at_yx(font, (unsigned)((glyph / SETTINGS_FONT_COLUMNS)
				* SETTINGS_FONT_HEIGHT + source_y),
			(unsigned)((glyph % SETTINGS_FONT_COLUMNS)
				* SETTINGS_FONT_WIDTH + source_x), &source) < 0)
		return ;
	alpha = ncpixel_a(source);
	if (alpha == 0)
		return ;
	dest_y = 0;
	while (dest_y < cell_height)
	{
		dest_x = 0;
		while (dest_x < cell_width)
		{
			put_pixel(pixels, width, height, x + dest_x, y + dest_y,
				tint, alpha, opaque);
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

/**
 * @brief Reports a failed region refresh in the tri-state form regions use.
 */
static int	settings_region_failed(const char *stage)
{
	(void)settings_render_failed(stage);
	return (-1);
}
