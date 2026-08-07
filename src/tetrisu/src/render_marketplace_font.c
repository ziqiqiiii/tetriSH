#include "tetrisu.h"

# define MARKET_FONT_COLUMNS	16
# define MARKET_FONT_ROWS	6
# define MARKET_FONT_WIDTH	8
# define MARKET_FONT_HEIGHT	16
# define MARKET_FONT_INK_Y	4
# define MARKET_FONT_SPACING_REF	4
# define MARKET_FONT_SHADOW_REF	3

static const color_t	g_market_cream = {250, 242, 221};
static const color_t	g_market_gold = {255, 203, 102};
static const color_t	g_market_pink = {255, 112, 190};
static const color_t	g_market_lavender = {190, 155, 218};
static const color_t	g_market_green = {112, 214, 174};
static const color_t	g_market_red = {255, 111, 142};
static const color_t	g_market_disabled = {105, 99, 120};
static const color_t	g_market_shadow = {22, 8, 31};
static const color_t	g_market_plate = {26, 12, 42};
static const color_t	g_market_slot_plate = {74, 40, 104};

typedef struct s_market_slot
{
	int	x;
	int	y;
	int	width;
	int	height;
	int	thumb_x;
	int	thumb_width;
	int	thumb_height;
	int	name_y;
	int	price_y;
}	market_slot_t;

/*
 * One glyph size per panel, fitted from the longest caption it will draw.
 * Sizing each caption independently makes neighbouring tiles render at
 * visibly different sizes, which reads as a rendering glitch rather than as
 * typography.
 */
typedef struct s_market_caption_metrics
{
	int	name_glyph;
	int	name_spacing;
	int	price_glyph;
	int	price_spacing;
}	market_caption_metrics_t;

static bool	load_font(render_ctx_t *ctx, struct ncvisual **font);
static bool	refresh_background(render_ctx_t *ctx, bool force);
static bool	cache_background(render_ctx_t *ctx);
static int	update_static_layer(render_ctx_t *ctx,
		const app_screen_view_model_t *view,
		const marketplace_layout_t *layout, struct ncvisual *font);
static int	update_region_layers(render_ctx_t *ctx,
		const app_screen_view_model_t *view, const marketplace_state_t *state,
		const marketplace_layout_t *layout, struct ncvisual *font);
static void	restack_marketplace_planes(render_ctx_t *ctx);
static bool	compose_static(render_ctx_t *ctx,
		const app_screen_view_model_t *view,
		const marketplace_layout_t *layout, struct ncvisual *font);
static bool	compose_inventory(render_ctx_t *ctx,
		const app_marketplace_view_model_t *market,
		const marketplace_state_t *state,
		const marketplace_layout_t *layout, struct ncvisual *font,
		bool characters);
static bool	compose_detail(render_ctx_t *ctx,
		const app_screen_view_model_t *view, const marketplace_state_t *state,
		const marketplace_layout_t *layout, struct ncvisual *font);
static bool	compose_controls(render_ctx_t *ctx,
		const app_marketplace_view_model_t *market,
		const marketplace_state_t *state,
		const marketplace_layout_t *layout, struct ncvisual *font);
static uint32_t	*region_canvas(render_ctx_t *ctx,
		const marketplace_layout_t *layout);
static bool	prefill_background(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height);
static bool	create_static_plane(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height);
static bool	create_region_plane(render_ctx_t *ctx, uint32_t *pixels,
		int width, int height, const marketplace_rect_t *region,
		struct ncplane **slot);
static void	draw_chrome(uint32_t *pixels, int width, int height,
		const app_screen_view_model_t *view,
		const marketplace_layout_t *layout, struct ncvisual *font);
static void	draw_stat_cards(uint32_t *pixels, int width, int height,
		const app_marketplace_view_model_t *market,
		const marketplace_layout_t *layout, struct ncvisual *font);
static void	draw_panel_plate(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, int ref_x_value, int ref_y_value,
		int ref_width, int ref_height, color_t edge);
static void	draw_slot_highlight(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, const market_slot_t *slot);
static void	slot_geometry(bool characters, int index, market_slot_t *slot);
static void	caption_metrics(const app_catalogue_view_model_t *catalogue,
		int visible, bool characters, const marketplace_layout_t *layout,
		int slot_width, market_caption_metrics_t *metrics);
static const char	*slot_label(const app_catalogue_item_view_model_t *item,
		bool characters);
static void	draw_inventory_slot(uint32_t *pixels, int width, int height,
		const app_catalogue_item_view_model_t *item,
		const app_marketplace_view_model_t *market,
		const marketplace_layout_t *layout, struct ncvisual *font,
		render_ctx_t *ctx, const market_slot_t *slot, int cache_slot,
		bool characters, bool focused,
		const market_caption_metrics_t *metrics);
static void	draw_text_fixed(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x_value, int ref_y_value, int ref_width,
		int glyph_size, int spacing, color_t tint, bool centered);
static void	draw_detail_body(uint32_t *pixels, int width, int height,
		const app_catalogue_item_view_model_t *item,
		const marketplace_layout_t *layout, struct ncvisual *font,
		bool characters);
static void	draw_detail_status(uint32_t *pixels, int width, int height,
		const app_catalogue_item_view_model_t *item,
		const app_marketplace_view_model_t *market,
		const marketplace_layout_t *layout, struct ncvisual *font);
static void	draw_buttons(uint32_t *pixels, int width, int height,
		const app_marketplace_view_model_t *market,
		const marketplace_state_t *state,
		const marketplace_layout_t *layout, struct ncvisual *font);
static void	button_caption(const app_marketplace_view_model_t *market,
		const marketplace_state_t *state, char *out, size_t size);
static const char	*price_caption(
		const app_catalogue_item_view_model_t *item, char *out, size_t size);
static color_t	item_colour(const app_catalogue_item_view_model_t *item,
		const app_marketplace_view_model_t *market, bool focused);
static bool	draw_artwork(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, struct ncvisual *font,
		render_ctx_t *ctx, int cache_slot, const char *path, int ref_x_value,
		int ref_y_value, int ref_width, int ref_height, bool owned);
static struct ncvisual	*cached_artwork(render_ctx_t *ctx, int cache_slot,
		const char *path);
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, int ref_x_value, int ref_y_value,
		int ref_width, int ref_height, color_t tint, unsigned alpha);
static void	draw_wrapped_text_ref(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x_value, int ref_y_value, int ref_width,
		int ref_glyph, int ref_line_step, color_t tint, int max_lines);
static void	draw_text_ref(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, struct ncvisual *font,
		const char *text, int ref_x_value, int ref_y_value, int ref_width,
		int ref_glyph, color_t tint, bool centered);
static void	draw_text_run(uint32_t *pixels, int width, int height,
		const marketplace_layout_t *layout, struct ncvisual *font,
		const char *text, int x, int y, int glyph_size, int spacing,
		color_t tint);
static void	draw_glyph(uint32_t *pixels, int width, int height,
		struct ncvisual *font, int glyph, int x, int y, int glyph_size,
		color_t tint, const marketplace_layout_t *layout);
static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
		struct ncvisual *font, int glyph, int source_x, int source_y,
		int x, int y, int cell_width, int cell_height, color_t tint,
		bool opaque);
static int	ink_span(int units, int glyph_size);
static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
		color_t tint, unsigned alpha, bool opaque);
static void	blend_pixel(uint32_t *pixel, color_t tint, unsigned alpha);
static int	ref_x(const marketplace_layout_t *layout, int value);
static int	ref_y(const marketplace_layout_t *layout, int value);
static int	ref_size(const marketplace_layout_t *layout, int value);
static int	text_width(const char *text, int glyph_size, int spacing);
static int	fit_glyph_size(const marketplace_layout_t *layout,
		const char *text, int ref_width, int ref_glyph, int *spacing);
static int	min_int(int left, int right);
static int	max_int(int left, int right);
static const char	*nonempty(const char *text);
static const char	*status_title(const app_screen_view_model_t *view);
static const char	*status_detail(const app_screen_view_model_t *view);
static uint64_t	market_hash(const void *data, size_t size, uint64_t hash);
static uint64_t	static_signature(const app_screen_view_model_t *view,
		const marketplace_layout_t *layout);
static bool	market_render_failed(const char *stage);
static int	market_region_failed(const char *stage);

/**
 * @brief Renders the Marketplace over its authored shop backdrop.
 *
 * Both bitmap tiers take the same route: one full-screen plane carrying the
 * static art - backdrop, title, wallet header, and every panel plate - and
 * four small region planes above it for everything focus can move through.
 * Region planes are written in place rather than replaced, so a keystroke
 * costs one small region instead of the whole screen.
 */
bool	render_marketplace_pixel_show(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const marketplace_state_t *state,
	bool rebuild_background)
{
	marketplace_layout_t	layout;
	struct ncvisual			*font;
	int						rebuilt;
	int						changed;

	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL
		|| !render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc))
		return (false);
	if (!refresh_background(ctx, rebuild_background))
		return (market_render_failed("background"));
	if (rebuild_background)
	{
		render_screen_destroy(ctx);
		ctx->marketplace_static_signature = 0;
		ctx->marketplace_characters_signature = 0;
		ctx->marketplace_themes_signature = 0;
		ctx->marketplace_detail_signature = 0;
		ctx->marketplace_controls_signature = 0;
	}
	marketplace_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	layout.opaque_background = ctx->pixels == TETRISU_PIXELS_STATIONARY;
	if (!load_font(ctx, &font))
		return (market_render_failed("font"));
	rebuilt = update_static_layer(ctx, view, &layout, font);
	if (rebuilt < 0)
		return (false);
	changed = update_region_layers(ctx, view, state, &layout, font);
	if (changed < 0)
		return (false);
	changed |= rebuilt;
	if (rebuilt > 0)
		restack_marketplace_planes(ctx);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	if (changed == 0 && ctx->notifications.count == 0)
		return (true);
	if (notcurses_render(ctx->nc) != 0)
		return (market_render_failed("render"));
	return (true);
}

/**
 * @brief Releases Marketplace-only cached artwork while keeping the backdrop.
 */
void	render_marketplace_pixel_destroy(render_ctx_t *ctx)
{
	int	index;

	if (ctx == NULL)
		return ;
	if (ctx->marketplace_font_visual != NULL)
	{
		ncvisual_destroy(ctx->marketplace_font_visual);
		ctx->marketplace_font_visual = NULL;
	}
	if (ctx->marketplace_characters_plane != NULL)
	{
		ncplane_destroy(ctx->marketplace_characters_plane);
		ctx->marketplace_characters_plane = NULL;
	}
	if (ctx->marketplace_themes_plane != NULL)
	{
		ncplane_destroy(ctx->marketplace_themes_plane);
		ctx->marketplace_themes_plane = NULL;
	}
	if (ctx->marketplace_detail_plane != NULL)
	{
		ncplane_destroy(ctx->marketplace_detail_plane);
		ctx->marketplace_detail_plane = NULL;
	}
	if (ctx->marketplace_controls_plane != NULL)
	{
		ncplane_destroy(ctx->marketplace_controls_plane);
		ctx->marketplace_controls_plane = NULL;
	}
	index = 0;
	while (index < APP_CATALOGUE_MAX_ITEMS * 2)
	{
		if (ctx->marketplace_thumbnail_visuals[index] != NULL)
			ncvisual_destroy(ctx->marketplace_thumbnail_visuals[index]);
		ctx->marketplace_thumbnail_visuals[index] = NULL;
		ctx->marketplace_thumbnail_sources[index][0] = '\0';
		index++;
	}
	free(ctx->marketplace_background_pixels);
	ctx->marketplace_background_pixels = NULL;
	free(ctx->marketplace_static_pixels);
	ctx->marketplace_static_pixels = NULL;
	ctx->marketplace_pixels_width = 0;
	ctx->marketplace_pixels_height = 0;
	ctx->marketplace_background_ready = false;
	ctx->marketplace_background_rows = 0;
	ctx->marketplace_background_cols = 0;
	ctx->marketplace_static_signature = 0;
	ctx->marketplace_characters_signature = 0;
	ctx->marketplace_themes_signature = 0;
	ctx->marketplace_detail_signature = 0;
	ctx->marketplace_controls_signature = 0;
}

/**
 * @brief Recomposes the full-screen static layer when its inputs change.
 *
 * @return 1 when the layer was rebuilt, 0 when it was reused, -1 on failure.
 */
static int	update_static_layer(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const marketplace_layout_t *layout,
	struct ncvisual *font)
{
	uint64_t	signature;

	signature = static_signature(view, layout);
	if (ctx->screen_plane != NULL && ctx->marketplace_static_pixels != NULL
		&& signature == ctx->marketplace_static_signature)
		return (0);
	if (!compose_static(ctx, view, layout, font))
	{
		(void)market_render_failed("static frame");
		return (-1);
	}
	ctx->marketplace_static_signature = signature;
	return (1);
}

/**
 * @brief Refreshes every focus-sensitive region whose signature moved.
 *
 * Every region signature is seeded with the static one. Rewriting the
 * full-screen plane re-emits the bitmap under these planes, which on a
 * stationary protocol overwrites the cells they occupy; a region that did not
 * also recompose would stay blank until something else moved it. A purchase
 * changes the wallet, so it moves the static signature and therefore all four.
 *
 * @return 1 when at least one region was rewritten, 0 when none were, -1 on
 * failure.
 */
static int	update_region_layers(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const marketplace_state_t *state,
	const marketplace_layout_t *layout, struct ncvisual *font)
{
	marketplace_section_t	preview;
	uint64_t				signature;
	int						slot;
	int						changed;

	changed = 0;
	signature = market_hash(&state->section, sizeof(state->section),
			ctx->marketplace_static_signature);
	signature = market_hash(&state->character_slot,
			sizeof(state->character_slot), signature);
	if (ctx->marketplace_characters_plane == NULL
		|| signature != ctx->marketplace_characters_signature)
	{
		if (!compose_inventory(ctx, &view->data.marketplace, state, layout,
				font, true))
			return (market_region_failed("characters"));
		ctx->marketplace_characters_signature = signature;
		changed = 1;
	}
	signature = market_hash(&state->section, sizeof(state->section),
			ctx->marketplace_static_signature);
	signature = market_hash(&state->theme_slot, sizeof(state->theme_slot),
			signature);
	if (ctx->marketplace_themes_plane == NULL
		|| signature != ctx->marketplace_themes_signature)
	{
		if (!compose_inventory(ctx, &view->data.marketplace, state, layout,
				font, false))
			return (market_region_failed("themes"));
		ctx->marketplace_themes_signature = signature;
		changed = 1;
	}
	preview = marketplace_focused_section(state);
	slot = marketplace_focused_slot(state);
	signature = market_hash(&preview, sizeof(preview),
			ctx->marketplace_static_signature);
	signature = market_hash(&slot, sizeof(slot), signature);
	if (ctx->marketplace_detail_plane == NULL
		|| signature != ctx->marketplace_detail_signature)
	{
		if (!compose_detail(ctx, view, state, layout, font))
			return (market_region_failed("detail"));
		ctx->marketplace_detail_signature = signature;
		changed = 1;
	}
	/* The Buy caption tracks the focused item, so it hashes that too. */
	signature = market_hash(&state->focus, sizeof(state->focus),
			ctx->marketplace_static_signature);
	signature = market_hash(&state->section, sizeof(state->section), signature);
	signature = market_hash(&preview, sizeof(preview), signature);
	signature = market_hash(&slot, sizeof(slot), signature);
	if (ctx->marketplace_controls_plane == NULL
		|| signature != ctx->marketplace_controls_signature)
	{
		if (!compose_controls(ctx, &view->data.marketplace, state, layout,
				font))
			return (market_region_failed("controls"));
		ctx->marketplace_controls_signature = signature;
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
static void	restack_marketplace_planes(render_ctx_t *ctx)
{
	ncplane_move_top(ctx->screen_plane);
	if (ctx->marketplace_characters_plane != NULL)
		ncplane_move_top(ctx->marketplace_characters_plane);
	if (ctx->marketplace_themes_plane != NULL)
		ncplane_move_top(ctx->marketplace_themes_plane);
	if (ctx->marketplace_detail_plane != NULL)
		ncplane_move_top(ctx->marketplace_detail_plane);
	if (ctx->marketplace_controls_plane != NULL)
		ncplane_move_top(ctx->marketplace_controls_plane);
}

/**
 * @brief Fits the shop backdrop, reusing it until the geometry changes.
 *
 * The Marketplace art carries no authored frames, so a missing file degrades
 * to the Settings backdrop rather than failing the screen: every plate this
 * renderer draws is opaque and covers the older art's frames anyway.
 */
static bool	refresh_background(render_ctx_t *ctx, bool force)
{
	bool	geometry_changed;

	geometry_changed = !ctx->marketplace_background_ready
		|| ctx->marketplace_background_rows != ctx->bg_rows
		|| ctx->marketplace_background_cols != ctx->bg_cols;
	if (!force && !geometry_changed && ctx->bg_plane != NULL)
		return (true);
	if (render_background_replace_exact(ctx, MARKETPLACE_BACKGROUND_PATH,
			false) < 0 && render_background_replace_exact(ctx,
			SETTINGS_BACKGROUND_PATH, false) < 0)
		return (false);
	/*
	 * Both caches are sized by the fitted geometry, so a resize invalidates
	 * them before anything can prefill from a buffer of the previous size.
	 */
	free(ctx->marketplace_static_pixels);
	ctx->marketplace_static_pixels = NULL;
	ctx->marketplace_pixels_width = 0;
	ctx->marketplace_pixels_height = 0;
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY && !cache_background(ctx))
		return (false);
	if (ctx->pixels != TETRISU_PIXELS_STATIONARY)
	{
		free(ctx->marketplace_background_pixels);
		ctx->marketplace_background_pixels = NULL;
	}
	ctx->marketplace_background_ready = true;
	ctx->marketplace_background_rows = ctx->bg_rows;
	ctx->marketplace_background_cols = ctx->bg_cols;
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
	int				width;
	int				height;
	int				y;
	int				x;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| ctx->bg_cols > INT_MAX / ctx->cell_px_x
		|| ctx->bg_rows > INT_MAX / ctx->cell_px_y)
		return (false);
	width = ctx->bg_cols * ctx->cell_px_x;
	height = ctx->bg_rows * ctx->cell_px_y;
	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height / sizeof(*buffer))
		return (false);
	visual = ncvisual_from_file(MARKETPLACE_BACKGROUND_PATH);
	if (visual == NULL)
		visual = ncvisual_from_file(SETTINGS_BACKGROUND_PATH);
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
	free(ctx->marketplace_background_pixels);
	ctx->marketplace_background_pixels = buffer;
	return (true);
}

static bool	load_font(render_ctx_t *ctx, struct ncvisual **font)
{
	ncvgeom	geom;

	if (ctx->marketplace_font_visual != NULL)
	{
		*font = ctx->marketplace_font_visual;
		return (true);
	}
	*font = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (*font == NULL)
		return (false);
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, *font, NULL, &geom) != 0
		|| geom.pixx != MARKET_FONT_COLUMNS * MARKET_FONT_WIDTH
		|| geom.pixy != MARKET_FONT_ROWS * MARKET_FONT_HEIGHT)
	{
		ncvisual_destroy(*font);
		*font = NULL;
		return (false);
	}
	ctx->marketplace_font_visual = *font;
	return (true);
}

/**
 * @brief Composes the layer nothing focus does can change.
 *
 * The panel plates and their gold edges live here rather than in the regions:
 * every region canvas is a copy of this frame, so a region redraw reproduces
 * its own plate for free and never has to repaint one that did not move.
 */
static bool	compose_static(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const marketplace_layout_t *layout,
	struct ncvisual *font)
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
	draw_chrome(pixels, width, height, view, layout, font);
	free(ctx->marketplace_static_pixels);
	ctx->marketplace_static_pixels = pixels;
	ctx->marketplace_pixels_width = width;
	ctx->marketplace_pixels_height = height;
	return (create_static_plane(ctx, pixels, width, height));
}

/**
 * @brief Draws the title, wallet header, and every panel plate.
 */
static void	draw_chrome(uint32_t *pixels, int width, int height,
	const app_screen_view_model_t *view, const marketplace_layout_t *layout,
	struct ncvisual *font)
{
	const app_marketplace_view_model_t	*market;
	bool								ready;

	market = &view->data.marketplace;
	ready = view->status == APP_DATA_READY && market->signed_in
		&& !market->offline;
	draw_text_ref(pixels, width, height, layout, font, "MARKETPLACE",
		MARKETPLACE_REF_CONTENT_X, MARKETPLACE_REF_TITLE_Y,
		MARKETPLACE_REF_CONTENT_WIDTH, 34, g_market_pink, true);
	if (view->local_preview)
		draw_text_ref(pixels, width, height, layout, font, "LOCAL UI PREVIEW",
			MARKETPLACE_REF_CONTENT_X + MARKETPLACE_REF_CONTENT_WIDTH - 230,
			MARKETPLACE_REF_TITLE_Y + 12, 230, 12, g_market_gold, true);
	draw_stat_cards(pixels, width, height, market, layout, font);
	draw_panel_plate(pixels, width, height, layout,
		MARKETPLACE_REF_CHARACTERS_X, MARKETPLACE_REF_CHARACTERS_Y,
		MARKETPLACE_REF_CHARACTERS_WIDTH, MARKETPLACE_REF_CHARACTERS_HEIGHT,
		g_market_gold);
	draw_panel_plate(pixels, width, height, layout, MARKETPLACE_REF_THEMES_X,
		MARKETPLACE_REF_THEMES_Y, MARKETPLACE_REF_THEMES_WIDTH,
		MARKETPLACE_REF_THEMES_HEIGHT, g_market_gold);
	draw_panel_plate(pixels, width, height, layout, MARKETPLACE_REF_DETAIL_X,
		MARKETPLACE_REF_DETAIL_Y, MARKETPLACE_REF_DETAIL_WIDTH,
		MARKETPLACE_REF_DETAIL_HEIGHT, g_market_gold);
	draw_panel_plate(pixels, width, height, layout, MARKETPLACE_REF_CONTROLS_X,
		MARKETPLACE_REF_CONTROLS_Y, MARKETPLACE_REF_CONTROLS_WIDTH,
		MARKETPLACE_REF_CONTROLS_HEIGHT, g_market_lavender);
	draw_text_ref(pixels, width, height, layout, font, "CHARACTERS",
		MARKETPLACE_REF_CHARACTERS_X + 10,
		MARKETPLACE_REF_CHARACTERS_Y + MARKETPLACE_REF_PANEL_TITLE_Y,
		MARKETPLACE_REF_CHARACTERS_WIDTH - 20,
		MARKETPLACE_REF_PANEL_TITLE_GLYPH, g_market_pink, true);
	draw_text_ref(pixels, width, height, layout, font, "THEMES",
		MARKETPLACE_REF_THEMES_X + 10,
		MARKETPLACE_REF_THEMES_Y + MARKETPLACE_REF_PANEL_TITLE_Y,
		MARKETPLACE_REF_THEMES_WIDTH - 20, MARKETPLACE_REF_PANEL_TITLE_GLYPH,
		g_market_pink, true);
	if (ready)
		return ;
	/*
	 * Nothing is for sale without an account, so the shelves stay empty and
	 * the reason is spelled out where the goods would be.
	 */
	draw_text_ref(pixels, width, height, layout, font, status_title(view),
		MARKETPLACE_REF_DETAIL_X + 20, MARKETPLACE_REF_DETAIL_Y + 88,
		MARKETPLACE_REF_DETAIL_WIDTH - 40, 26, g_market_gold, true);
	draw_text_ref(pixels, width, height, layout, font, status_detail(view),
		MARKETPLACE_REF_DETAIL_X + 20, MARKETPLACE_REF_DETAIL_Y + 140,
		MARKETPLACE_REF_DETAIL_WIDTH - 40, 15, g_market_lavender, true);
}

/**
 * @brief Draws the wallet, score, and rank cards above the shelves.
 */
static void	draw_stat_cards(uint32_t *pixels, int width, int height,
	const app_marketplace_view_model_t *market,
	const marketplace_layout_t *layout, struct ncvisual *font)
{
	static const char	*labels[3] = {"WALLET POINTS", "BEST SCORE", "RANK"};
	char				value[64];
	int					index;
	int					card_x;

	index = 0;
	while (index < 3)
	{
		card_x = MARKETPLACE_REF_CONTENT_X
			+ index * MARKETPLACE_REF_STAT_STEP_X;
		draw_panel_plate(pixels, width, height, layout, card_x,
			MARKETPLACE_REF_STAT_Y, MARKETPLACE_REF_STAT_WIDTH,
			MARKETPLACE_REF_STAT_HEIGHT, g_market_lavender);
		draw_text_ref(pixels, width, height, layout, font, labels[index],
			card_x + 14, MARKETPLACE_REF_STAT_Y + 12,
			MARKETPLACE_REF_STAT_WIDTH - 28, 12, g_market_lavender, false);
		if (market->signed_in && !market->offline)
		{
			if (index == 0)
				snprintf(value, sizeof(value), "%d",
					market->profile.wallet_points);
			else if (index == 1)
				snprintf(value, sizeof(value), "%" PRIu64,
					market->profile.score);
			else
				snprintf(value, sizeof(value), "#%d", market->profile.rank);
		}
		else
			snprintf(value, sizeof(value), "-");
		draw_text_ref(pixels, width, height, layout, font, value,
			card_x + 14, MARKETPLACE_REF_STAT_Y + 36,
			MARKETPLACE_REF_STAT_WIDTH - 28, 24,
			index == 0 ? g_market_gold : g_market_green, false);
		index++;
	}
}

/**
 * @brief Fills one opaque plate with a two-pixel edge in the given colour.
 *
 * The backdrop is a scene rather than an authored frame, so every panel on
 * this screen owns its own plate. The fill is opaque because a stationary
 * protocol cannot write transparency over what is already on the terminal.
 */
static void	draw_panel_plate(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, int ref_x_value, int ref_y_value,
	int ref_width, int ref_height, color_t edge)
{
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, ref_height, g_market_plate, 236u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, 3, edge, 255u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value,
		ref_y_value + ref_height - 3, ref_width, 3, edge, 255u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		3, ref_height, edge, 255u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value + ref_width - 3,
		ref_y_value, 3, ref_height, edge, 255u);
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
	const marketplace_layout_t *layout)
{
	uint32_t	*canvas;
	size_t		count;

	if (layout->pixel_width <= 0 || layout->pixel_height <= 0)
		return (NULL);
	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	if (count > SIZE_MAX / sizeof(uint32_t))
		return (NULL);
	if (ctx->marketplace_static_pixels != NULL
		&& ctx->marketplace_pixels_width == layout->pixel_width
		&& ctx->marketplace_pixels_height == layout->pixel_height)
	{
		canvas = malloc(count * sizeof(*canvas));
		if (canvas != NULL)
			memcpy(canvas, ctx->marketplace_static_pixels,
				count * sizeof(*canvas));
		return (canvas);
	}
	return (calloc(count, sizeof(uint32_t)));
}

/**
 * @brief Repaints one shelf panel with its current focus highlight.
 */
static bool	compose_inventory(render_ctx_t *ctx,
	const app_marketplace_view_model_t *market,
	const marketplace_state_t *state, const marketplace_layout_t *layout,
	struct ncvisual *font, bool characters)
{
	const app_catalogue_view_model_t	*catalogue;
	market_caption_metrics_t			metrics;
	marketplace_rect_t					region;
	market_slot_t						slot;
	struct ncplane						**plane;
	uint32_t							*pixels;
	int									visible;
	int									index;
	int									focused_slot;

	plane = characters ? &ctx->marketplace_characters_plane
		: &ctx->marketplace_themes_plane;
	region = characters ? layout->characters : layout->themes;
	catalogue = characters ? &market->characters : &market->themes;
	visible = 0;
	if (market->signed_in && !market->offline)
		visible = settings_catalogue_count(catalogue, characters
				? MARKETPLACE_CHARACTER_SLOTS : MARKETPLACE_THEME_SLOTS);
	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	focused_slot = -1;
	if (state->section == (characters ? MARKETPLACE_SECTION_CHARACTERS
			: MARKETPLACE_SECTION_THEMES))
		focused_slot = characters ? state->character_slot : state->theme_slot;
	slot_geometry(characters, 0, &slot);
	caption_metrics(catalogue, visible, characters, layout, slot.width,
		&metrics);
	index = 0;
	while (index < visible)
	{
		slot_geometry(characters, index, &slot);
		draw_inventory_slot(pixels, layout->pixel_width, layout->pixel_height,
			&catalogue->items[index], market, layout, font, ctx, &slot,
			(characters ? 0 : APP_CATALOGUE_MAX_ITEMS) + index, characters,
			index == focused_slot, &metrics);
		index++;
	}
	if (visible == 0)
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height,
			layout, font, "NOTHING IN STOCK",
			(characters ? MARKETPLACE_REF_CHARACTERS_X
				: MARKETPLACE_REF_THEMES_X) + 10,
			(characters ? MARKETPLACE_REF_CHARACTERS_Y
				: MARKETPLACE_REF_THEMES_Y) + 130,
			MARKETPLACE_REF_CHARACTERS_WIDTH - 20, 14, g_market_lavender,
			true);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Resolves one drawn slot's reference-space rectangle.
 *
 * Both panels are four columns wide, so the column arithmetic here and the
 * focus arithmetic in marketplace_screen.c must agree on the same count.
 */
static void	slot_geometry(bool characters, int index, market_slot_t *slot)
{
	int	panel_x;
	int	panel_y;
	int	panel_width;
	int	row;
	int	column;

	panel_x = characters ? MARKETPLACE_REF_CHARACTERS_X
		: MARKETPLACE_REF_THEMES_X;
	panel_y = characters ? MARKETPLACE_REF_CHARACTERS_Y
		: MARKETPLACE_REF_THEMES_Y;
	panel_width = characters ? MARKETPLACE_REF_CHARACTERS_WIDTH
		: MARKETPLACE_REF_THEMES_WIDTH;
	row = index / MARKETPLACE_INVENTORY_COLUMNS;
	column = index % MARKETPLACE_INVENTORY_COLUMNS;
	slot->width = (panel_width - 2 * MARKETPLACE_REF_SLOT_INSET)
		/ MARKETPLACE_INVENTORY_COLUMNS;
	slot->x = panel_x + MARKETPLACE_REF_SLOT_INSET + column * slot->width;
	slot->y = panel_y + MARKETPLACE_REF_SLOT_FIRST_Y + row * (characters
			? MARKETPLACE_REF_CHARACTER_STEP_Y : MARKETPLACE_REF_THEME_STEP_Y);
	slot->thumb_width = slot->width - (characters
			? MARKETPLACE_REF_CHARACTER_THUMB_INSET
			: MARKETPLACE_REF_THEME_THUMB_INSET);
	slot->thumb_x = slot->x + (slot->width - slot->thumb_width) / 2;
	slot->thumb_height = characters ? MARKETPLACE_REF_CHARACTER_THUMB_HEIGHT
		: MARKETPLACE_REF_THEME_THUMB_HEIGHT;
	slot->name_y = characters ? MARKETPLACE_REF_CHARACTER_NAME_Y
		: MARKETPLACE_REF_THEME_NAME_Y;
	slot->price_y = characters ? MARKETPLACE_REF_CHARACTER_PRICE_Y
		: MARKETPLACE_REF_THEME_PRICE_Y;
	slot->height = slot->price_y + MARKETPLACE_REF_SLOT_PRICE_GLYPH;
}

/**
 * @brief Draws one shelf tile: art, caption, and what it would cost.
 */
static void	draw_inventory_slot(uint32_t *pixels, int width, int height,
	const app_catalogue_item_view_model_t *item,
	const app_marketplace_view_model_t *market,
	const marketplace_layout_t *layout, struct ncvisual *font,
	render_ctx_t *ctx, const market_slot_t *slot, int cache_slot,
	bool characters, bool focused, const market_caption_metrics_t *metrics)
{
	char	caption[APP_TEXT_MAX + 16];

	if (focused)
		draw_slot_highlight(pixels, width, height, layout, slot);
	draw_artwork(pixels, width, height, layout, font, ctx, cache_slot,
		item->portrait_asset, slot->thumb_x, slot->y + 2, slot->thumb_width,
		slot->thumb_height, item->owned);
	if (item->equipped)
		draw_text_ref(pixels, width, height, layout, font, "*",
			slot->thumb_x + 4, slot->y + 6, 16, 15,
			focused ? g_market_gold : g_market_green, false);
	draw_text_fixed(pixels, width, height, layout, font,
		slot_label(item, characters), slot->x + 2, slot->y + slot->name_y,
		slot->width - 4, metrics->name_glyph, metrics->name_spacing,
		item_colour(item, market, focused), true);
	draw_text_fixed(pixels, width, height, layout, font,
		price_caption(item, caption, sizeof(caption)), slot->x + 2,
		slot->y + slot->price_y, slot->width - 4, metrics->price_glyph,
		metrics->price_spacing, item->owned ? g_market_green
		: (marketplace_can_afford(market, item) ? g_market_gold
			: g_market_red), true);
}

/**
 * @brief Keeps four-column slot captions readable below their thumbnails.
 *
 * The full canonical name stays in the detail card. Only the compact caption
 * inside a 98-reference-pixel tile is shortened, and only where the canonical
 * name is long enough to be shrunk into illegibility.
 */
static const char	*slot_label(const app_catalogue_item_view_model_t *item,
	bool characters)
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
 * @brief Fits one caption size and one price size for a whole panel.
 *
 * Both are measured from the longest string the panel will draw, so every tile
 * in it renders at the same size however long its own caption is.
 */
static void	caption_metrics(const app_catalogue_view_model_t *catalogue,
	int visible, bool characters, const marketplace_layout_t *layout,
	int slot_width, market_caption_metrics_t *metrics)
{
	const char	*longest;
	int			index;

	longest = "";
	index = 0;
	while (index < visible && index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (strlen(slot_label(&catalogue->items[index], characters))
			> strlen(longest))
			longest = slot_label(&catalogue->items[index], characters);
		index++;
	}
	metrics->name_glyph = fit_glyph_size(layout, longest, slot_width - 4,
			MARKETPLACE_REF_SLOT_NAME_GLYPH, &metrics->name_spacing);
	/* "EQUIPPED" is the widest price line any tile can show. */
	metrics->price_glyph = fit_glyph_size(layout, "EQUIPPED", slot_width - 4,
			MARKETPLACE_REF_SLOT_PRICE_GLYPH, &metrics->price_spacing);
}

/**
 * @brief Draws the selection plate: a soft fill inside a gold outline.
 *
 * The plate is deliberately darker than the panel rather than brighter, so the
 * gold caption stays the brightest thing in the tile and the eye lands on the
 * text rather than on the marker.
 */
static void	draw_slot_highlight(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, const market_slot_t *slot)
{
	int	focus_x;
	int	focus_y;
	int	focus_width;
	int	focus_height;

	focus_x = slot->x + MARKETPLACE_REF_SLOT_PAD_X;
	focus_y = slot->y - MARKETPLACE_REF_SLOT_PAD_Y;
	focus_width = slot->width - 2 * MARKETPLACE_REF_SLOT_PAD_X;
	focus_height = slot->height + 2 * MARKETPLACE_REF_SLOT_PAD_Y;
	fill_ref_rect(pixels, width, height, layout, focus_x, focus_y, focus_width,
		focus_height, g_market_slot_plate, 224u);
	fill_ref_rect(pixels, width, height, layout, focus_x, focus_y, focus_width,
		2, g_market_gold, 255u);
	fill_ref_rect(pixels, width, height, layout, focus_x,
		focus_y + focus_height - 2, focus_width, 2, g_market_gold, 255u);
	fill_ref_rect(pixels, width, height, layout, focus_x, focus_y, 2,
		focus_height, g_market_gold, 255u);
	fill_ref_rect(pixels, width, height, layout, focus_x + focus_width - 2,
		focus_y, 2, focus_height, g_market_gold, 255u);
}

/**
 * @brief Repaints the detail card for whichever item the cursor last touched.
 */
static bool	compose_detail(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const marketplace_state_t *state,
	const marketplace_layout_t *layout, struct ncvisual *font)
{
	const app_marketplace_view_model_t		*market;
	const app_catalogue_item_view_model_t	*item;
	marketplace_rect_t						region;
	uint32_t								*pixels;
	int										cache_slot;
	bool									characters;

	market = &view->data.marketplace;
	item = marketplace_focused_item(market, state);
	region = layout->detail;
	if (item == NULL)
	{
		if (ctx->marketplace_detail_plane != NULL)
		{
			ncplane_destroy(ctx->marketplace_detail_plane);
			ctx->marketplace_detail_plane = NULL;
		}
		return (true);
	}
	characters = marketplace_focused_is_character(state);
	cache_slot = (characters ? 0 : APP_CATALOGUE_MAX_ITEMS)
		+ marketplace_focused_slot(state);
	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_artwork(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, ctx, cache_slot, item->portrait_asset,
		MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_PREVIEW_X,
		MARKETPLACE_REF_DETAIL_Y + MARKETPLACE_REF_DETAIL_PREVIEW_Y,
		MARKETPLACE_REF_DETAIL_PREVIEW_SIZE,
		MARKETPLACE_REF_DETAIL_PREVIEW_SIZE, item->owned);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, item->name,
		MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_TEXT_X,
		MARKETPLACE_REF_DETAIL_Y + MARKETPLACE_REF_DETAIL_NAME_Y,
		MARKETPLACE_REF_DETAIL_TEXT_WIDTH, 26, g_market_cream, false);
	draw_detail_status(pixels, layout->pixel_width, layout->pixel_height, item,
		market, layout, font);
	draw_detail_body(pixels, layout->pixel_width, layout->pixel_height, item,
		layout, font, characters);
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->marketplace_detail_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief States the price against the balance so the outcome is never a guess.
 */
static void	draw_detail_status(uint32_t *pixels, int width, int height,
	const app_catalogue_item_view_model_t *item,
	const app_marketplace_view_model_t *market,
	const marketplace_layout_t *layout, struct ncvisual *font)
{
	char	line[APP_TEXT_MAX + 96];
	color_t	tint;

	if (item->equipped)
	{
		snprintf(line, sizeof(line), "OWNED - EQUIPPED");
		tint = g_market_green;
	}
	else if (item->owned)
	{
		snprintf(line, sizeof(line), "OWNED - PRESS E TO EQUIP");
		tint = g_market_green;
	}
	else if (marketplace_can_afford(market, item))
	{
		snprintf(line, sizeof(line), "PRICE %d P    WALLET %d P    AFTER %d P",
			item->price, market->profile.wallet_points,
			market->profile.wallet_points - item->price);
		tint = g_market_gold;
	}
	else
	{
		snprintf(line, sizeof(line), "PRICE %d P    WALLET %d P    NEED %d MORE",
			item->price, market->profile.wallet_points,
			item->price - market->profile.wallet_points);
		tint = g_market_red;
	}
	draw_text_ref(pixels, width, height, layout, font, line,
		MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_TEXT_X,
		MARKETPLACE_REF_DETAIL_Y + MARKETPLACE_REF_DETAIL_STATUS_Y,
		MARKETPLACE_REF_DETAIL_TEXT_WIDTH, 15, tint, false);
}

/**
 * @brief Describes what the money actually buys.
 *
 * Characters are sold on their four crystal powers, so those are listed the
 * way the Settings powers card lists them. A theme has no powers, so the card
 * says what a theme changes instead of leaving two thirds of it blank.
 */
static void	draw_detail_body(uint32_t *pixels, int width, int height,
	const app_catalogue_item_view_model_t *item,
	const marketplace_layout_t *layout, struct ncvisual *font, bool characters)
{
	static const char	*theme_lines[3] = {
		"A theme restyles the board, the tetromino tiles, and the music.",
		"Owned themes can be equipped from here or from the Settings screen.",
		"Equipping a theme never changes the character you play as."
	};
	char				heading[APP_TEXT_MAX + 32];
	int					index;
	int					body_y;

	body_y = MARKETPLACE_REF_DETAIL_Y + MARKETPLACE_REF_DETAIL_BODY_Y;
	if (!characters)
	{
		index = 0;
		while (index < 3)
		{
			draw_text_ref(pixels, width, height, layout, font,
				theme_lines[index],
				MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_TEXT_X,
				body_y + index * MARKETPLACE_REF_DETAIL_STEP_Y,
				MARKETPLACE_REF_DETAIL_TEXT_WIDTH, 11, g_market_lavender,
				false);
			index++;
		}
		return ;
	}
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		snprintf(heading, sizeof(heading), "L%d  %s", index + 1,
			item->abilities[index].name);
		draw_text_ref(pixels, width, height, layout, font, heading,
			MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_TEXT_X,
			body_y + index * MARKETPLACE_REF_DETAIL_STEP_Y,
			MARKETPLACE_REF_DETAIL_TEXT_WIDTH,
			MARKETPLACE_REF_DETAIL_NAME_GLYPH, g_market_gold, false);
		draw_wrapped_text_ref(pixels, width, height, layout, font,
			item->abilities[index].description,
			MARKETPLACE_REF_DETAIL_X + MARKETPLACE_REF_DETAIL_TEXT_X + 20,
			body_y + index * MARKETPLACE_REF_DETAIL_STEP_Y
			+ MARKETPLACE_REF_DETAIL_DESC_Y,
			MARKETPLACE_REF_DETAIL_TEXT_WIDTH - 20,
			MARKETPLACE_REF_DETAIL_DESC_GLYPH,
			MARKETPLACE_REF_DETAIL_DESC_STEP, g_market_cream, 2);
		index++;
	}
}

static bool	compose_controls(render_ctx_t *ctx,
	const app_marketplace_view_model_t *market,
	const marketplace_state_t *state, const marketplace_layout_t *layout,
	struct ncvisual *font)
{
	marketplace_rect_t	region;
	uint32_t			*pixels;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_buttons(pixels, layout->pixel_width, layout->pixel_height, market,
		state, layout, font);
	region = layout->controls;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->marketplace_controls_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Draws the four control buttons and the key legend below them.
 */
static void	draw_buttons(uint32_t *pixels, int width, int height,
	const app_marketplace_view_model_t *market,
	const marketplace_state_t *state, const marketplace_layout_t *layout,
	struct ncvisual *font)
{
	static const char	*labels[MARKETPLACE_BUTTON_COUNT] = {
		"BACK", "BUY", "VOLUME -", "VOLUME +"
	};
	char				captions[MARKETPLACE_BUTTON_COUNT][32];
	color_t				colour;
	int					index;
	int					button_x;
	bool				focused;
	bool				enabled;

	snprintf(captions[0], sizeof(captions[0]), "ESC");
	button_caption(market, state, captions[1], sizeof(captions[1]));
	snprintf(captions[2], sizeof(captions[2]), "-");
	snprintf(captions[3], sizeof(captions[3]), "+");
	index = 0;
	while (index < MARKETPLACE_BUTTON_COUNT)
	{
		button_x = MARKETPLACE_REF_BUTTON_FIRST_X
			+ index * MARKETPLACE_REF_BUTTON_STEP_X;
		focused = index == (int)state->focus
			&& state->section == MARKETPLACE_SECTION_CONTROLS;
		enabled = index != 1 || market->signed_in;
		colour = !enabled ? g_market_disabled
			: (focused ? g_market_gold : g_market_cream);
		fill_ref_rect(pixels, width, height, layout, button_x,
			MARKETPLACE_REF_BUTTON_Y, MARKETPLACE_REF_BUTTON_WIDTH,
			MARKETPLACE_REF_BUTTON_HEIGHT, focused ? g_market_slot_plate
			: g_market_plate, focused ? 236u : 200u);
		fill_ref_rect(pixels, width, height, layout, button_x,
			MARKETPLACE_REF_BUTTON_Y, MARKETPLACE_REF_BUTTON_WIDTH, 2, colour,
			255u);
		fill_ref_rect(pixels, width, height, layout, button_x,
			MARKETPLACE_REF_BUTTON_Y + MARKETPLACE_REF_BUTTON_HEIGHT - 2,
			MARKETPLACE_REF_BUTTON_WIDTH, 2, colour, 255u);
		fill_ref_rect(pixels, width, height, layout, button_x,
			MARKETPLACE_REF_BUTTON_Y, 2, MARKETPLACE_REF_BUTTON_HEIGHT,
			colour, 255u);
		fill_ref_rect(pixels, width, height, layout,
			button_x + MARKETPLACE_REF_BUTTON_WIDTH - 2,
			MARKETPLACE_REF_BUTTON_Y, 2, MARKETPLACE_REF_BUTTON_HEIGHT,
			colour, 255u);
		draw_text_ref(pixels, width, height, layout, font, labels[index],
			button_x + 12, MARKETPLACE_REF_BUTTON_Y + 20,
			MARKETPLACE_REF_BUTTON_WIDTH - 24, 20, colour, true);
		draw_text_ref(pixels, width, height, layout, font, captions[index],
			button_x + 12, MARKETPLACE_REF_BUTTON_Y + 56,
			MARKETPLACE_REF_BUTTON_WIDTH - 24, 13,
			enabled ? g_market_lavender : g_market_disabled, true);
		index++;
	}
	/*
	 * The Marketplace takes no pointer input, so the grids and the purchase
	 * keys need spelling out here or they are undiscoverable.
	 */
	draw_text_ref(pixels, width, height, layout, font,
		market->signed_in
		? "ARROWS MOVE   UP ENTERS SHELVES   ENTER BUYS   E EQUIPS   ESC BACK"
		: "ARROWS MOVE   ENTER SELECT   ESC BACK", MARKETPLACE_REF_CONTENT_X,
		MARKETPLACE_REF_HINT_Y, MARKETPLACE_REF_CONTENT_WIDTH, 12,
		g_market_lavender, true);
}

/**
 * @brief Writes the second Buy-button line: what pressing it would do now.
 */
static void	button_caption(const app_marketplace_view_model_t *market,
	const marketplace_state_t *state, char *out, size_t size)
{
	const app_catalogue_item_view_model_t	*item;

	item = marketplace_focused_item(market, state);
	if (item == NULL)
		snprintf(out, size, "ENTER");
	else if (item->equipped)
		snprintf(out, size, "EQUIPPED");
	else if (item->owned)
		snprintf(out, size, "OWNED");
	else if (marketplace_can_afford(market, item))
		snprintf(out, size, "%d P", item->price);
	else
		snprintf(out, size, "TOO COSTLY");
}

/**
 * @brief Writes a shelf tile's price line.
 */
static const char	*price_caption(
	const app_catalogue_item_view_model_t *item, char *out, size_t size)
{
	if (item->equipped)
		snprintf(out, size, "EQUIPPED");
	else if (item->owned)
		snprintf(out, size, "OWNED");
	else if (item->price <= 0)
		snprintf(out, size, "FREE");
	else
		snprintf(out, size, "%d P", item->price);
	return (out);
}

/**
 * @brief Colours a shelf caption by focus first, then by what it costs.
 */
static color_t	item_colour(const app_catalogue_item_view_model_t *item,
	const app_marketplace_view_model_t *market, bool focused)
{
	if (focused)
		return (g_market_gold);
	if (item->owned)
		return (g_market_cream);
	if (marketplace_can_afford(market, item))
		return (g_market_lavender);
	return (g_market_disabled);
}

static bool	prefill_background(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height)
{
	if (ctx->marketplace_background_pixels == NULL
		|| ctx->bg_cols * ctx->cell_px_x != width
		|| ctx->bg_rows * ctx->cell_px_y != height)
		return (false);
	memcpy(pixels, ctx->marketplace_background_pixels,
		(size_t)width * (size_t)height * sizeof(*pixels));
	return (true);
}

static bool	create_static_plane(render_ctx_t *ctx, uint32_t *pixels,
	int width, int height)
{
	ncplane_options	options;
	struct ncplane	*plane;
	nccell			base;

	if (render_plane_geometry_matches(ctx->screen_plane, ctx->bg_row,
			ctx->bg_col, (unsigned)ctx->bg_rows, (unsigned)ctx->bg_cols))
		return (render_plane_blit_rgba(ctx, ctx->screen_plane, pixels, width,
				height, width));
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
	int width, int height, const marketplace_rect_t *region,
	struct ncplane **slot)
{
	ncplane_options	options;
	struct ncplane	*plane;
	const uint32_t	*origin;
	int				crop_x;
	int				crop_y;
	int				crop_width;
	int				crop_height;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0 || region->x < 0
		|| region->y < 0 || region->width <= 0 || region->height <= 0
		|| region->x + region->width > width
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
	if (render_plane_geometry_matches(*slot, options.y, options.x,
			options.rows, options.cols))
		return (render_plane_blit_rgba(ctx, *slot, origin, crop_width,
				crop_height, width));
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (false);
	if (!render_plane_blit_rgba(ctx, plane, origin, crop_width, crop_height,
			width))
	{
		ncplane_destroy(plane);
		return (false);
	}
	if (*slot != NULL)
		ncplane_destroy(*slot);
	*slot = plane;
	return (true);
}

/**
 * @brief Draws catalogue artwork without making it a hard dependency.
 *
 * Preview art is optional during development and in cell-only installations.
 * A framed placeholder keeps the tile geometry and focus treatment intact when
 * a file is missing or cannot be decoded. Locked items are desaturated so the
 * shelf reads as stock rather than as inventory.
 */
static bool	draw_artwork(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, struct ncvisual *font,
	render_ctx_t *ctx, int cache_slot, const char *path, int ref_x_value,
	int ref_y_value, int ref_width, int ref_height, bool owned)
{
	struct ncvisual	*visual;
	ncvgeom			geom;
	color_t			tint;
	uint32_t		pixel;
	unsigned		gray;
	int				draw_width;
	int				draw_height;
	int				origin_x;
	int				origin_y;
	int				x;
	int				y;

	ref_width = max_int(1, ref_width);
	ref_height = max_int(1, ref_height);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, ref_height, (color_t){39, 24, 62}, 255u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, 2, owned ? g_market_lavender : g_market_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value,
		ref_y_value + ref_height - 2, ref_width, 2,
		owned ? g_market_lavender : g_market_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value, 2,
		ref_height, owned ? g_market_lavender : g_market_disabled, 220u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value + ref_width - 2,
		ref_y_value, 2, ref_height,
		owned ? g_market_lavender : g_market_disabled, 220u);
	visual = cached_artwork(ctx, cache_slot, path);
	memset(&geom, 0, sizeof(geom));
	if (visual == NULL || ncvisual_geom(NULL, visual, NULL, &geom) != 0
		|| geom.pixx == 0 || geom.pixy == 0)
	{
		if (font != NULL)
			draw_text_ref(pixels, width, height, layout, font, "?",
				ref_x_value, ref_y_value + ref_height / 3, ref_width, 16,
				owned ? g_market_lavender : g_market_disabled, true);
		return (false);
	}
	draw_width = max_int(1, ref_x(layout, ref_width) - 6);
	draw_height = max_int(1, ref_y(layout, ref_height) - 6);
	if ((uint64_t)draw_width * geom.pixy > (uint64_t)draw_height * geom.pixx)
		draw_width = max_int(1,
				(int)((uint64_t)draw_height * geom.pixx / geom.pixy));
	else
		draw_height = max_int(1,
				(int)((uint64_t)draw_width * geom.pixy / geom.pixx));
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
			if (ncvisual_at_yx(visual,
					(unsigned)(y * (int)geom.pixy / draw_height),
					(unsigned)(x * (int)geom.pixx / draw_width), &pixel) >= 0
				&& ncpixel_a(pixel) != 0)
			{
				tint.r = ncpixel_r(pixel);
				tint.g = ncpixel_g(pixel);
				tint.b = ncpixel_b(pixel);
				if (!owned)
				{
					gray = (30u * tint.r + 59u * tint.g + 11u * tint.b) / 100u;
					tint.r = gray * 2u / 3u;
					tint.g = gray * 2u / 3u;
					tint.b = gray * 2u / 3u;
				}
				put_pixel(pixels, width, height, origin_x + x, origin_y + y,
					tint, ncpixel_a(pixel), layout->opaque_background);
			}
			x++;
		}
		y++;
	}
	return (true);
}

/**
 * @brief Returns the decoded artwork for a path, decoding it at most once.
 *
 * A focus move recomposes a region, so re-reading and re-decoding the PNG each
 * time would put file I/O straight onto the input path.
 */
static struct ncvisual	*cached_artwork(render_ctx_t *ctx, int cache_slot,
	const char *path)
{
	struct ncvisual	*visual;

	if (ctx == NULL || cache_slot < 0
		|| cache_slot >= APP_CATALOGUE_MAX_ITEMS * 2)
		return (NULL);
	if (path == NULL || path[0] == '\0')
	{
		if (ctx->marketplace_thumbnail_visuals[cache_slot] != NULL)
		{
			ncvisual_destroy(ctx->marketplace_thumbnail_visuals[cache_slot]);
			ctx->marketplace_thumbnail_visuals[cache_slot] = NULL;
		}
		ctx->marketplace_thumbnail_sources[cache_slot][0] = '\0';
		return (NULL);
	}
	if (strcmp(ctx->marketplace_thumbnail_sources[cache_slot], path) == 0)
		return (ctx->marketplace_thumbnail_visuals[cache_slot]);
	if (ctx->marketplace_thumbnail_visuals[cache_slot] != NULL)
		ncvisual_destroy(ctx->marketplace_thumbnail_visuals[cache_slot]);
	snprintf(ctx->marketplace_thumbnail_sources[cache_slot],
		sizeof(ctx->marketplace_thumbnail_sources[cache_slot]), "%s", path);
	visual = ncvisual_from_file(path);
	ctx->marketplace_thumbnail_visuals[cache_slot] = visual;
	return (visual);
}

/**
 * @brief Fills a reference-space rectangle.
 */
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, int ref_x_value, int ref_y_value,
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

static void	draw_wrapped_text_ref(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, struct ncvisual *font,
	const char *text, int ref_x_value, int ref_y_value, int ref_width,
	int ref_glyph, int ref_line_step, color_t tint, int max_lines)
{
	char	line_text[APP_ABILITY_TEXT_MAX + 4];
	int		line_start;
	int		cursor;
	int		last_space;
	int		line_index;
	int		max_chars;

	if (text == NULL)
		return ;
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
			ref_x_value, ref_y_value + line_index * ref_line_step, ref_width,
			ref_glyph, tint, false);
		line_start = cursor;
		while (text[line_start] == ' ')
			line_start++;
		line_index++;
	}
}

/**
 * @brief Shrinks a glyph size until the text fits its reference-space box.
 */
static int	fit_glyph_size(const marketplace_layout_t *layout,
	const char *text, int ref_width, int ref_glyph, int *spacing)
{
	int	glyph_size;

	glyph_size = max_int(3, ref_size(layout, ref_glyph));
	*spacing = max_int(1, ref_size(layout, MARKET_FONT_SPACING_REF));
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
	const marketplace_layout_t *layout, struct ncvisual *font,
	const char *text, int ref_x_value, int ref_y_value, int ref_width,
	int ref_glyph, color_t tint, bool centered)
{
	int	glyph_size;
	int	spacing;

	if (layout == NULL || font == NULL || text == NULL || ref_width <= 0)
		return ;
	glyph_size = fit_glyph_size(layout, text, ref_width, ref_glyph, &spacing);
	draw_text_fixed(pixels, width, height, layout, font, text, ref_x_value,
		ref_y_value, ref_width, glyph_size, spacing, tint, centered);
}

/**
 * @brief Draws text at a caller-chosen size, clipping it to its box.
 *
 * Split out so a caller drawing a row of related captions can size them all
 * from the longest one rather than letting each shrink independently.
 */
static void	draw_text_fixed(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, struct ncvisual *font,
	const char *text, int ref_x_value, int ref_y_value, int ref_width,
	int glyph_size, int spacing, color_t tint, bool centered)
{
	char	visible[APP_ABILITY_TEXT_MAX + 4];
	int		max_chars;
	int		length;
	int		x;
	int		y;
	int		shadow;

	if (layout == NULL || font == NULL || text == NULL || ref_width <= 0)
		return ;
	shadow = max_int(1, ref_size(layout, MARKET_FONT_SHADOW_REF));
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
	x = ref_x(layout, ref_x_value);
	if (centered)
		x += (ref_x(layout, ref_width)
				- text_width(visible, glyph_size, spacing)) / 2;
	y = ref_y(layout, ref_y_value);
	draw_text_run(pixels, width, height, layout, font, visible, x + shadow,
		y + shadow, glyph_size, spacing, g_market_shadow);
	draw_text_run(pixels, width, height, layout, font, visible, x, y,
		glyph_size, spacing, tint);
}

static void	draw_text_run(uint32_t *pixels, int width, int height,
	const marketplace_layout_t *layout, struct ncvisual *font,
	const char *text, int x, int y, int glyph_size, int spacing, color_t tint)
{
	int	glyph;
	int	codepoint;

	glyph = 0;
	while (text[glyph] != '\0')
	{
		codepoint = (unsigned char)text[glyph];
		if (codepoint < 32
			|| codepoint >= 32 + MARKET_FONT_COLUMNS * MARKET_FONT_ROWS)
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
	color_t tint, const marketplace_layout_t *layout)
{
	int	source_y;
	int	source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < MARKET_FONT_WIDTH)
		{
			draw_glyph_cell(pixels, width, height, font, glyph, source_x,
				source_y, x + source_x * glyph_size / MARKET_FONT_WIDTH,
				y + ink_span(source_y - MARKET_FONT_INK_Y, glyph_size),
				(source_x + 1) * glyph_size / MARKET_FONT_WIDTH
				- source_x * glyph_size / MARKET_FONT_WIDTH,
				ink_span(source_y + 1 - MARKET_FONT_INK_Y, glyph_size)
				- ink_span(source_y - MARKET_FONT_INK_Y, glyph_size),
				tint, layout->opaque_background);
			source_x++;
		}
		source_y++;
	}
}

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
	if (ncvisual_at_yx(font, (unsigned)((glyph / MARKET_FONT_COLUMNS)
				* MARKET_FONT_HEIGHT + source_y),
			(unsigned)((glyph % MARKET_FONT_COLUMNS)
				* MARKET_FONT_WIDTH + source_x), &source) < 0)
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
			put_pixel(pixels, width, height, x + dest_x, y + dest_y, tint,
				alpha, opaque);
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

static int	ref_x(const marketplace_layout_t *layout, int value)
{
	return (value * layout->pixel_width / MARKETPLACE_REFERENCE_WIDTH);
}

static int	ref_y(const marketplace_layout_t *layout, int value)
{
	return (value * layout->pixel_height / MARKETPLACE_REFERENCE_HEIGHT);
}

static int	ref_size(const marketplace_layout_t *layout, int value)
{
	int	x_size;
	int	y_size;

	x_size = value * layout->pixel_width / MARKETPLACE_REFERENCE_WIDTH;
	y_size = value * layout->pixel_height / MARKETPLACE_REFERENCE_HEIGHT;
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
		return ("OPENING THE SHOP...");
	if (view->status == APP_DATA_EMPTY)
		return ("NOTHING FOR SALE");
	if (view->status == APP_DATA_ERROR)
		return ("MARKETPLACE ERROR");
	if (view->data.marketplace.offline || !view->data.marketplace.signed_in)
		return ("MARKETPLACE NEEDS AN ACCOUNT");
	return ("MARKETPLACE UNAVAILABLE");
}

static const char	*status_detail(const app_screen_view_model_t *view)
{
	if (view->status == APP_DATA_LOADING)
		return ("PLEASE WAIT");
	if (view->status == APP_DATA_EMPTY)
		return ("THE CATALOGUE IS EMPTY");
	if (view->status == APP_DATA_ERROR)
		return ("TRY AGAIN LATER");
	if (view->data.marketplace.offline || !view->data.marketplace.signed_in)
		return (nonempty(view->data.marketplace.local_status));
	return (nonempty(view->subtitle));
}

static uint64_t	market_hash(const void *data, size_t size, uint64_t hash)
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

/**
 * @brief Hashes everything the static layer draws from.
 *
 * Fields are hashed one at a time rather than as a struct: hashing a struct
 * wholesale folds in its padding bytes, which are never written and make the
 * signature unstable, so the layer would recompose on keystrokes that changed
 * nothing. The wallet is included, so a purchase rebuilds the header.
 */
static uint64_t	static_signature(const app_screen_view_model_t *view,
	const marketplace_layout_t *layout)
{
	const app_marketplace_view_model_t	*market;
	uint64_t							hash;
	int									index;

	market = &view->data.marketplace;
	hash = market_hash(&view->status, sizeof(view->status), 0);
	hash = market_hash(&view->local_preview, sizeof(view->local_preview), hash);
	hash = market_hash(&market->signed_in, sizeof(market->signed_in), hash);
	hash = market_hash(&market->offline, sizeof(market->offline), hash);
	hash = market_hash(&market->profile.wallet_points,
			sizeof(market->profile.wallet_points), hash);
	hash = market_hash(&market->profile.score, sizeof(market->profile.score),
			hash);
	hash = market_hash(&market->profile.rank, sizeof(market->profile.rank),
			hash);
	hash = market_hash(&market->characters.count,
			sizeof(market->characters.count), hash);
	hash = market_hash(&market->themes.count, sizeof(market->themes.count),
			hash);
	index = 0;
	while (index < APP_CATALOGUE_MAX_ITEMS)
	{
		hash = market_hash(&market->characters.items[index].owned,
				sizeof(bool), hash);
		hash = market_hash(&market->characters.items[index].equipped,
				sizeof(bool), hash);
		hash = market_hash(&market->themes.items[index].owned, sizeof(bool),
				hash);
		hash = market_hash(&market->themes.items[index].equipped, sizeof(bool),
				hash);
		index++;
	}
	hash = market_hash(&layout->pixel_width, sizeof(layout->pixel_width), hash);
	hash = market_hash(&layout->pixel_height, sizeof(layout->pixel_height),
			hash);
	return (hash);
}

static bool	market_render_failed(const char *stage)
{
	fprintf(stderr, "tetrisu: Marketplace renderer failed at %s\n", stage);
	return (false);
}

/**
 * @brief Reports a failed region refresh in the tri-state form regions use.
 */
static int	market_region_failed(const char *stage)
{
	(void)market_render_failed(stage);
	return (-1);
}
