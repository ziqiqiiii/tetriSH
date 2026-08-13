#include "tetrisu.h"

# define MP_FONT_COLUMNS	16
# define MP_FONT_ROWS	6
# define MP_FONT_WIDTH	8
# define MP_FONT_HEIGHT	16
# define MP_FONT_INK_Y	4
# define MP_FONT_SPACING_REF	4
# define MP_FONT_SHADOW_REF	3
/* The backdrop's decorated frame reaches this far in from each edge. */
# define MP_BAND_X	96

static const t_color	g_mp_cream = {250, 242, 221};
static const t_color	g_mp_gold = {255, 203, 102};
static const t_color	g_mp_pink = {255, 112, 190};
static const t_color	g_mp_lavender = {190, 155, 218};
static const t_color	g_mp_green = {112, 214, 174};
static const t_color	g_mp_amber = {255, 176, 84};
static const t_color	g_mp_red = {255, 111, 142};
static const t_color	g_mp_disabled = {105, 99, 120};
static const t_color	g_mp_shadow = {22, 8, 31};
static const t_color	g_mp_plate = {26, 12, 42};
static const t_color	g_mp_focus_plate = {74, 40, 104};

// Static Functions
static bool	load_font(t_render_ctx *ctx, struct ncvisual **font);
static const char	*background_path(const t_render_ctx *ctx,
				t_app_screen screen);
static bool	refresh_background(t_render_ctx *ctx, bool force,
				t_app_screen screen);
static bool	cache_background(t_render_ctx *ctx, t_app_screen screen);
static void	forget_regions(t_render_ctx *ctx);
static int	update_static_layer(t_render_ctx *ctx,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static int	update_region_layers(t_render_ctx *ctx,
				const t_app_screen_view_model *view, const void *state,
				const t_mp_layout *layout, struct ncvisual *font);
static void	restack_mp_planes(t_render_ctx *ctx);
static bool	compose_static(t_render_ctx *ctx,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static void	draw_mode_chrome(uint32_t *pixels, int width, int height,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static void	draw_lobby_chrome(uint32_t *pixels, int width, int height,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static void	draw_create_chrome(uint32_t *pixels, int width, int height,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static void	draw_room_chrome(uint32_t *pixels, int width, int height,
				const t_app_screen_view_model *view, const t_mp_layout *layout,
				struct ncvisual *font);
static int	compose_mode_regions(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_mp_mode_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static int	compose_lobby_regions(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_lobby_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static int	compose_create_regions(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_create_room_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static int	compose_room_regions(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_waiting_room_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static bool	compose_cards(t_render_ctx *ctx, const t_mp_mode_state *state,
				const t_mp_layout *layout, struct ncvisual *font);
static bool	compose_list(t_render_ctx *ctx,
				const t_app_lobby_view_model *lobby, const t_lobby_state *state,
				const t_mp_layout *layout, struct ncvisual *font);
static bool	compose_field(t_render_ctx *ctx, const t_lobby_state *state,
				const t_mp_layout *layout, struct ncvisual *font);
static bool	compose_lobby_status(t_render_ctx *ctx,
				const t_lobby_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static bool	compose_options(t_render_ctx *ctx,
				const t_create_room_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static bool	compose_slots(t_render_ctx *ctx,
					const t_app_room_view_model *room,
					const t_waiting_room_state *state,
					const t_mp_layout *layout, struct ncvisual *font);
static bool	compose_room_status(t_render_ctx *ctx,
				const t_app_room_view_model *room,
				const t_waiting_room_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static bool	compose_chat(t_render_ctx *ctx, const t_app_room_view_model *room,
				const t_waiting_room_state *state, const t_mp_layout *layout,
				struct ncvisual *font);
static void	draw_list_row(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, struct ncvisual *font,
				const t_app_room_summary_view_model *room, int row,
				bool focused);
static void	draw_identity(uint32_t *pixels, int width, int height,
				const t_app_profile_view_model *profile,
				const t_mp_layout *layout, struct ncvisual *font);
static uint32_t	*region_canvas(t_render_ctx *ctx, const t_mp_layout *layout);
static bool	prefill_background(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height);
static bool	create_static_plane(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height);
static bool	create_region_plane(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height, const t_mp_rect *region,
				struct ncplane **slot);
static void	draw_plate(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, const t_mp_rect *ref_rect,
				t_color edge);
static void	draw_rule(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, int ref_x_value, int ref_y_value,
				int ref_width, t_color tint);
static void	draw_band(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, int ref_y_value, int ref_height);
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, int ref_x_value, int ref_y_value,
				int ref_width, int ref_height, t_color tint, unsigned alpha);
static void	draw_text_ref(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, struct ncvisual *font,
				const char *text, int ref_x_value, int ref_y_value,
				int ref_width, int ref_glyph, t_color tint, bool centered);
static void	draw_text_fixed(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, struct ncvisual *font,
				const char *text, int ref_x_value, int ref_y_value,
				int ref_width, int glyph_size, int spacing, t_color tint,
				bool centered);
static void	draw_text_run(uint32_t *pixels, int width, int height,
				const t_mp_layout *layout, struct ncvisual *font,
				const char *text, int x, int y, int glyph_size, int spacing,
				t_color tint);
static void	draw_glyph(uint32_t *pixels, int width, int height,
				struct ncvisual *font, int glyph, int x, int y, int glyph_size,
				t_color tint, const t_mp_layout *layout);
static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
				struct ncvisual *font, int glyph, int source_x, int source_y,
				int x, int y, int cell_width, int cell_height, t_color tint,
				bool opaque);
static int	ink_span(int units, int glyph_size);
static void	put_pixel(uint32_t *pixels, int width, int height, int x, int y,
				t_color tint, unsigned alpha, bool opaque);
static void	blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha);
static int	ref_x(const t_mp_layout *layout, int value);
static int	ref_y(const t_mp_layout *layout, int value);
static int	ref_size(const t_mp_layout *layout, int value);
static int	text_width(const char *text, int glyph_size, int spacing);
static int	fit_glyph_size(const t_mp_layout *layout, const char *text,
				int ref_width, int ref_glyph, int *spacing);
static int	min_int(int left, int right);
static int	max_int(int left, int right);
static const char	*nonempty(const char *text);
static t_color	room_state_colour(const t_app_room_summary_view_model *room);
static uint64_t	mp_hash(const void *data, size_t size, uint64_t hash);
static uint64_t	mp_hash_text(const char *text, uint64_t hash);
static uint64_t	static_signature(const t_app_screen_view_model *view,
				const t_mp_layout *layout);
static uint64_t	model_signature(const t_app_screen_view_model *view,
				uint64_t hash);
static uint64_t	room_players_signature(const t_app_room_view_model *room,
				uint64_t hash);
static uint64_t	room_chat_signature(const t_app_room_view_model *room,
				uint64_t hash);
static bool	mp_render_failed(const char *stage);
static int	mp_region_failed(const char *stage);

/**
 * @brief Renders whichever multiplayer surface the view model names.
 *
 * All four screens take the same route both bitmap tiers do elsewhere: one
 * full-screen plane carrying everything nothing the user does can move, and
 * small region planes above it for everything else. Nothing here raises a
 * plane over the screen, so no action can blank it.
 *
 * @param ctx Render context.
 * @param view Loaded model for the screen being drawn.
 * @param state Screen-specific input state, matching view->screen.
 * @param rebuild_background true after a resize or on first entry.
 * @return true when the frame was presented or deliberately skipped.
 */
bool	render_mp_pixel_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const void *state,
	bool rebuild_background)
{
	t_mp_layout		layout;
	struct ncvisual	*font;
	bool			screen_changed;
	int				rebuilt;
	int				changed;

	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL
		|| !render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc))
		return (false);
	screen_changed = ctx->mp_screen != view->screen;
	if (!refresh_background(ctx, rebuild_background || screen_changed,
			view->screen))
		return (mp_render_failed("background"));
	if (rebuild_background || screen_changed)
	{
		render_screen_destroy(ctx);
		forget_regions(ctx);
		ctx->mp_screen = view->screen;
	}
	mp_layout_build(view->screen, ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	layout.opaque_background = ctx->pixels == TETRISU_PIXELS_STATIONARY;
	if (!load_font(ctx, &font))
		return (mp_render_failed("font"));
	rebuilt = update_static_layer(ctx, view, &layout, font);
	if (rebuilt < 0)
		return (false);
	changed = update_region_layers(ctx, view, state, &layout, font);
	if (changed < 0)
		return (false);
	changed |= rebuilt;
	if (rebuilt > 0)
		restack_mp_planes(ctx);
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	if (changed == 0 && ctx->notifications.count == 0)
		return (true);
	if (notcurses_render(ctx->nc) != 0)
		return (mp_render_failed("render"));
	return (true);
}

/**
 * @brief Releases multiplayer-only cached artwork while keeping the backdrop.
 *
 * @param ctx Render context.
 */
void	render_mp_pixel_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->mp_font_visual != NULL)
	{
		ncvisual_destroy(ctx->mp_font_visual);
		ctx->mp_font_visual = NULL;
	}
	forget_regions(ctx);
	free(ctx->mp_background_pixels);
	ctx->mp_background_pixels = NULL;
	free(ctx->mp_static_pixels);
	ctx->mp_static_pixels = NULL;
	ctx->mp_pixels_width = 0;
	ctx->mp_pixels_height = 0;
	ctx->mp_background_ready = false;
	ctx->mp_background_rows = 0;
	ctx->mp_background_cols = 0;
	ctx->mp_background_source[0] = '\0';
	ctx->mp_screen = APP_SCREEN_ENTRY;
	ctx->mp_static_signature = 0;
}

/**
 * @brief Destroys every region plane and clears the signatures that cache them.
 *
 * The four screens share one set of planes but use different subsets of it, so
 * leaving a plane behind when switching screens would strand a region of the
 * previous screen on top of the new one.
 */
static void	forget_regions(t_render_ctx *ctx)
{
	struct ncplane	**planes[7];
	int				index;

	planes[0] = &ctx->mp_cards_plane;
	planes[1] = &ctx->mp_list_plane;
	planes[2] = &ctx->mp_field_plane;
	planes[3] = &ctx->mp_status_plane;
	planes[4] = &ctx->mp_options_plane;
	planes[5] = &ctx->mp_slots_plane;
	planes[6] = &ctx->mp_chat_plane;
	index = 0;
	while (index < 7)
	{
		if (*planes[index] != NULL)
			ncplane_destroy(*planes[index]);
		*planes[index] = NULL;
		index++;
	}
	ctx->mp_static_signature = 0;
	ctx->mp_cards_signature = 0;
	ctx->mp_list_signature = 0;
	ctx->mp_field_signature = 0;
	ctx->mp_status_signature = 0;
	ctx->mp_options_signature = 0;
	ctx->mp_slots_signature = 0;
	ctx->mp_chat_signature = 0;
}

/**
 * @brief Returns the backdrop each multiplayer screen is drawn over.
 *
 * Every multiplayer surface uses the selected theme's multiplayer artwork.
 * The homepage is reserved for the actual home screen and remains only the
 * fallback when a themed multiplayer asset cannot be loaded.
 */
static const char	*background_path(const t_render_ctx *ctx,
	t_app_screen screen)
{
	(void)screen;
	return (ctx->theme_assets.multiplayer_background);
}

/**
 * @brief Fits the backdrop, reusing it until the geometry or the screen moves.
 */
static bool	refresh_background(t_render_ctx *ctx, bool force,
	t_app_screen screen)
{
	const char	*path;
	bool		stale;

	path = background_path(ctx, screen);
	stale = !ctx->mp_background_ready
		|| ctx->mp_background_rows != ctx->bg_rows
		|| ctx->mp_background_cols != ctx->bg_cols
		|| strncmp(ctx->mp_background_source, path,
			sizeof(ctx->mp_background_source)) != 0;
	if (!force && !stale && ctx->bg_plane != NULL)
		return (true);
	if (render_background_replace_exact(ctx, path, false) < 0
		&& render_background_replace_exact(ctx,
			ctx->theme_assets.homepage, false) < 0)
		return (false);
	/*
	 * Both caches are sized by the fitted geometry, so a resize invalidates
	 * them before anything can prefill from a buffer of the previous size.
	 */
	free(ctx->mp_static_pixels);
	ctx->mp_static_pixels = NULL;
	ctx->mp_pixels_width = 0;
	ctx->mp_pixels_height = 0;
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY
		&& !cache_background(ctx, screen))
		return (false);
	if (ctx->pixels != TETRISU_PIXELS_STATIONARY)
	{
		free(ctx->mp_background_pixels);
		ctx->mp_background_pixels = NULL;
	}
	ctx->mp_background_ready = true;
	ctx->mp_background_rows = ctx->bg_rows;
	ctx->mp_background_cols = ctx->bg_cols;
	snprintf(ctx->mp_background_source, sizeof(ctx->mp_background_source),
		"%s", path);
	return (true);
}

/**
 * @brief Flattens the fitted backdrop into a reusable opaque RGBA buffer.
 *
 * ncvisual_at_yx() is a per-call lookup, so walking a full-screen visual costs
 * millions of them. Doing that walk once per geometry change and keeping the
 * result turns every later frame prefill into a memcpy.
 */
static bool	cache_background(t_render_ctx *ctx, t_app_screen screen)
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
	visual = ncvisual_from_file(background_path(ctx, screen));
	if (visual == NULL)
		visual = ncvisual_from_file(ctx->theme_assets.homepage);
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
	free(ctx->mp_background_pixels);
	ctx->mp_background_pixels = buffer;
	return (true);
}

static bool	load_font(t_render_ctx *ctx, struct ncvisual **font)
{
	ncvgeom	geom;

	if (ctx->mp_font_visual != NULL)
	{
		*font = ctx->mp_font_visual;
		return (true);
	}
	*font = ncvisual_from_file(SHARED_FONT_MASK_PATH);
	if (*font == NULL)
		return (false);
	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, *font, NULL, &geom) != 0
		|| geom.pixx != MP_FONT_COLUMNS * MP_FONT_WIDTH
		|| geom.pixy != MP_FONT_ROWS * MP_FONT_HEIGHT)
	{
		ncvisual_destroy(*font);
		*font = NULL;
		return (false);
	}
	ctx->mp_font_visual = *font;
	return (true);
}

/**
 * @brief Recomposes the full-screen static layer when its inputs change.
 *
 * @return 1 when the layer was rebuilt, 0 when it was reused, -1 on failure.
 */
static int	update_static_layer(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
	struct ncvisual *font)
{
	uint64_t	signature;

	signature = static_signature(view, layout);
	if (ctx->screen_plane != NULL && ctx->mp_static_pixels != NULL
		&& signature == ctx->mp_static_signature)
		return (0);
	if (!compose_static(ctx, view, layout, font))
	{
		(void)mp_render_failed("static frame");
		return (-1);
	}
	ctx->mp_static_signature = signature;
	return (1);
}

/**
 * @brief Refreshes every region whose signature moved, for the current screen.
 *
 * @return 1 when at least one region was rewritten, 0 when none were, -1 on
 * failure.
 */
static int	update_region_layers(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const void *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	if (view->screen == APP_SCREEN_MULTIPLAYER_MODE)
		return (compose_mode_regions(ctx, view, state, layout, font));
	if (view->screen == APP_SCREEN_LOBBY)
		return (compose_lobby_regions(ctx, view, state, layout, font));
	if (view->screen == APP_SCREEN_CREATE_ROOM_MODAL)
		return (compose_create_regions(ctx, view, state, layout, font));
	if (view->screen == APP_SCREEN_WAITING_ROOM)
		return (compose_room_regions(ctx, view, state, layout, font));
	return (0);
}

/**
 * @brief Restores region planes above a freshly created static plane.
 *
 * Only called when the static layer was replaced. Restacking damages every
 * plane it touches, so a frame that merely rewrote a region leaves the order
 * alone: creation order already puts the regions on top.
 */
static void	restack_mp_planes(t_render_ctx *ctx)
{
	struct ncplane	*planes[7];
	int				index;

	ncplane_move_top(ctx->screen_plane);
	planes[0] = ctx->mp_cards_plane;
	planes[1] = ctx->mp_list_plane;
	planes[2] = ctx->mp_field_plane;
	planes[3] = ctx->mp_status_plane;
	planes[4] = ctx->mp_options_plane;
	planes[5] = ctx->mp_slots_plane;
	planes[6] = ctx->mp_chat_plane;
	index = 0;
	while (index < 7)
	{
		if (planes[index] != NULL)
			ncplane_move_top(planes[index]);
		index++;
	}
}

/**
 * @brief Composes the layer nothing the user does on this screen can change.
 */
static bool	compose_static(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
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
	if (view->screen == APP_SCREEN_MULTIPLAYER_MODE)
		draw_mode_chrome(pixels, width, height, view, layout, font);
	else if (view->screen == APP_SCREEN_LOBBY)
		draw_lobby_chrome(pixels, width, height, view, layout, font);
	else if (view->screen == APP_SCREEN_CREATE_ROOM_MODAL)
		draw_create_chrome(pixels, width, height, view, layout, font);
	else if (view->screen == APP_SCREEN_WAITING_ROOM)
		draw_room_chrome(pixels, width, height, view, layout, font);
	free(ctx->mp_static_pixels);
	ctx->mp_static_pixels = pixels;
	ctx->mp_pixels_width = width;
	ctx->mp_pixels_height = height;
	return (create_static_plane(ctx, pixels, width, height));
}

/**
 * @brief Draws the mode picker's panel, heading and standing copy.
 *
 * The home artwork is busy across its whole upper half, so the panel is opaque
 * and sits low where the art is plain.
 */
static void	draw_mode_chrome(uint32_t *pixels, int width, int height,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
	struct ncvisual *font)
{
	(void)view;
	draw_plate(pixels, width, height, layout, &layout->panel, g_mp_pink);
	draw_text_ref(pixels, width, height, layout, font, "MULTIPLAYER",
		MP_MODE_REF_PANEL_X, MP_MODE_REF_TITLE_Y, MP_MODE_REF_PANEL_WIDTH,
		MP_MODE_REF_TITLE_HEIGHT - 20, g_mp_pink, true);
	draw_text_ref(pixels, width, height, layout, font,
		"HOW DO YOU WANT TO PLAY?", MP_MODE_REF_PANEL_X,
		MP_MODE_REF_SUBTITLE_Y, MP_MODE_REF_PANEL_WIDTH, 16, g_mp_lavender,
		true);
}

/**
 * @brief Draws the lobby's title strip, identity, plates and standing copy.
 */
static void	draw_lobby_chrome(uint32_t *pixels, int width, int height,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
	struct ncvisual *font)
{
	draw_band(pixels, width, height, layout, LOBBY_REF_BAND_TOP_Y,
		LOBBY_REF_BAND_TOP_HEIGHT);
	draw_band(pixels, width, height, layout, LOBBY_REF_BAND_BOTTOM_Y,
		LOBBY_REF_BAND_BOTTOM_HEIGHT);
	draw_text_ref(pixels, width, height, layout, font, "tetriSH  LOBBY",
		LOBBY_REF_CONTENT_X, LOBBY_REF_TITLE_Y, 620,
		LOBBY_REF_TITLE_HEIGHT - 22, g_mp_pink, false);
	draw_identity(pixels, width, height, &view->data.lobby.profile, layout,
		font);
	draw_rule(pixels, width, height, layout, LOBBY_REF_CONTENT_X,
		LOBBY_REF_DIVIDER_Y, LOBBY_REF_CONTENT_WIDTH, g_mp_lavender);
	draw_text_ref(pixels, width, height, layout, font,
		"D - DOUBLE     BR - BATTLE ROYALE", LOBBY_REF_CONTENT_X,
		LOBBY_REF_LEGEND_Y, LOBBY_REF_CONTENT_WIDTH, 12, g_mp_lavender, false);
	draw_rule(pixels, width, height, layout, LOBBY_REF_CONTENT_X,
		LOBBY_REF_CONTROLS_Y - 16, LOBBY_REF_CONTENT_WIDTH, g_mp_lavender);
	draw_text_ref(pixels, width, height, layout, font,
		"[UP/DOWN] SELECT   [ENTER] JOIN   [C] CREATE   [R] REFRESH   "
		"[M] MODE   [B] BACK", LOBBY_REF_CONTENT_X, LOBBY_REF_CONTROLS_Y,
		LOBBY_REF_CONTENT_WIDTH, 13, g_mp_cream, true);
}

/**
 * @brief Draws the create-room panel and its standing copy.
 */
static void	draw_create_chrome(uint32_t *pixels, int width, int height,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
	struct ncvisual *font)
{
	(void)view;
	/*
	 * The lobby underneath is dimmed rather than redrawn: this screen stands in
	 * for a modal, and a wash reads as one without needing the lobby's model.
	 */
	fill_ref_rect(pixels, width, height, layout, 0, 0,
		MULTIPLAYER_REFERENCE_WIDTH, MULTIPLAYER_REFERENCE_HEIGHT,
		g_mp_shadow, 170u);
	draw_plate(pixels, width, height, layout, &layout->panel, g_mp_green);
	draw_text_ref(pixels, width, height, layout, font, "CREATE ROOM",
		CREATE_REF_PANEL_X + 40, CREATE_REF_TITLE_Y,
		CREATE_REF_PANEL_WIDTH - 80, 26, g_mp_green, false);
	draw_text_ref(pixels, width, height, layout, font, "SELECT MODE",
		CREATE_REF_PANEL_X + 40, CREATE_REF_PROMPT_Y,
		CREATE_REF_PANEL_WIDTH - 80, 14, g_mp_lavender, false);
	draw_rule(pixels, width, height, layout, CREATE_REF_PANEL_X + 40,
		CREATE_REF_DIVIDER_Y, CREATE_REF_PANEL_WIDTH - 80, g_mp_lavender);
	draw_text_ref(pixels, width, height, layout, font,
		"[1/2] SELECT     [ENTER] CREATE     [ESC] CANCEL",
		CREATE_REF_PANEL_X + 40, CREATE_REF_CONTROLS_Y,
		CREATE_REF_PANEL_WIDTH - 80, 13, g_mp_cream, true);
}

/**
 * @brief Draws the waiting room's heading, plates and standing copy.
 */
static void	draw_room_chrome(uint32_t *pixels, int width, int height,
	const t_app_screen_view_model *view, const t_mp_layout *layout,
	struct ncvisual *font)
{
	const t_app_room_view_model	*room;
	char						line[APP_TEXT_MAX * 2];

	room = &view->data.room;
	draw_band(pixels, width, height, layout, ROOM_REF_BAND_TOP_Y,
		ROOM_REF_BAND_TOP_HEIGHT);
	draw_band(pixels, width, height, layout, ROOM_REF_BAND_BOTTOM_Y,
		ROOM_REF_BAND_BOTTOM_HEIGHT);
	snprintf(line, sizeof(line), "ROOM: %s (%s)", nonempty(room->id),
		room->mode == APP_GAME_MODE_BATTLE_ROYALE ? "BATTLE ROYALE"
		: "DOUBLE");
	draw_text_ref(pixels, width, height, layout, font, line,
		ROOM_REF_CONTENT_X, ROOM_REF_TITLE_Y, 900,
		ROOM_REF_TITLE_HEIGHT - 22, g_mp_pink, false);
	snprintf(line, sizeof(line),
		"Share this id so a friend can join:  %s", nonempty(room->id));
	draw_text_ref(pixels, width, height, layout, font, line,
		ROOM_REF_CONTENT_X, ROOM_REF_SHARE_Y, 900, 13, g_mp_lavender, false);
	draw_rule(pixels, width, height, layout, ROOM_REF_CONTENT_X,
		ROOM_REF_DIVIDER_Y, MULTIPLAYER_REFERENCE_WIDTH
		- 2 * ROOM_REF_CONTENT_X, g_mp_lavender);
	draw_rule(pixels, width, height, layout, ROOM_REF_CONTENT_X,
		ROOM_REF_CONTROLS_Y - 16, MULTIPLAYER_REFERENCE_WIDTH
		- 2 * ROOM_REF_CONTENT_X, g_mp_lavender);
	draw_text_ref(pixels, width, height, layout, font,
		waiting_room_legend(0),
		ROOM_REF_CONTENT_X, ROOM_REF_CONTROLS_Y,
		MULTIPLAYER_REFERENCE_WIDTH - 2 * ROOM_REF_CONTENT_X, 13, g_mp_cream,
		true);
}

/**
 * @brief Draws the lobby's username, score and rank across the title row.
 */
static void	draw_identity(uint32_t *pixels, int width, int height,
	const t_app_profile_view_model *profile, const t_mp_layout *layout,
	struct ncvisual *font)
{
	char	line[APP_TEXT_MAX];

	draw_text_ref(pixels, width, height, layout, font,
		nonempty(profile->username), 760, LOBBY_REF_IDENTITY_Y, 280,
		LOBBY_REF_IDENTITY_HEIGHT - 16, g_mp_cream, false);
	snprintf(line, sizeof(line), "%" PRIu64 " pts", profile->score);
	draw_text_ref(pixels, width, height, layout, font, line, 1050,
		LOBBY_REF_IDENTITY_Y, 180, LOBBY_REF_IDENTITY_HEIGHT - 16, g_mp_green,
		false);
	snprintf(line, sizeof(line), "#%d", profile->rank);
	draw_text_ref(pixels, width, height, layout, font, line, 1244,
		LOBBY_REF_IDENTITY_Y, 84, LOBBY_REF_IDENTITY_HEIGHT - 16, g_mp_gold,
		false);
}

/**
 * @brief Refreshes the mode picker's single region when focus moves.
 */
static int	compose_mode_regions(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_mp_mode_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	uint64_t	signature;

	signature = model_signature(view, ctx->mp_static_signature);
	signature = mp_hash(&state->focus, sizeof(state->focus), signature);
	signature = mp_hash(&state->feedback, sizeof(state->feedback), signature);
	signature = mp_hash(&state->feedback_value, sizeof(state->feedback_value),
			signature);
	if (ctx->mp_cards_plane != NULL && signature == ctx->mp_cards_signature)
		return (0);
	if (!compose_cards(ctx, state, layout, font))
		return (mp_region_failed("mode cards"));
	ctx->mp_cards_signature = signature;
	return (1);
}

/**
 * @brief Refreshes the lobby's list, join field and status line.
 *
 * Every signature is seeded with the static one and with the model. The static
 * seed covers a full-screen rebuild re-emitting the bitmap under these planes;
 * the model seed covers a refresh changing the room list without moving the
 * cursor.
 */
static int	compose_lobby_regions(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_lobby_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	char		line[APP_TEXT_MAX * 2];
	uint64_t	base;
	uint64_t	signature;
	int			changed;

	base = model_signature(view, ctx->mp_static_signature);
	changed = 0;
	signature = mp_hash(&state->selected, sizeof(state->selected), base);
	signature = mp_hash(&state->list_offset, sizeof(state->list_offset),
			signature);
	signature = mp_hash(&state->section, sizeof(state->section), signature);
	signature = mp_hash(&state->filter, sizeof(state->filter), signature);
	signature = mp_hash(&state->visible_count, sizeof(state->visible_count),
			signature);
	if (ctx->mp_list_plane == NULL || signature != ctx->mp_list_signature)
	{
		if (!compose_list(ctx, &view->data.lobby, state, layout, font))
			return (mp_region_failed("room list"));
		ctx->mp_list_signature = signature;
		changed = 1;
	}
	/*
	 * The field and the status line draw nothing from the room list, so they
	 * seed from the static layer alone. Only the table needs the list in its
	 * seed, and giving it to all three would repaint the whole screen on every
	 * refresh.
	 */
	signature = mp_hash(&state->section, sizeof(state->section),
			ctx->mp_static_signature);
	signature = mp_hash_text(state->room_id, signature);
	if (ctx->mp_field_plane == NULL || signature != ctx->mp_field_signature)
	{
		if (!compose_field(ctx, state, layout, font))
			return (mp_region_failed("join field"));
		ctx->mp_field_signature = signature;
		changed = 1;
	}
	/*
	 * Hashed from the rendered line rather than from the fields behind it. The
	 * unknown-id message quotes the id being typed, so hashing the id directly
	 * would repaint this plane on every keystroke in the join field even though
	 * the line it draws has not moved.
	 */
	lobby_feedback_text(state, line, sizeof(line));
	signature = mp_hash_text(line, ctx->mp_static_signature);
	signature = mp_hash(&state->feedback, sizeof(state->feedback), signature);
	if (ctx->mp_status_plane == NULL || signature != ctx->mp_status_signature)
	{
		if (!compose_lobby_status(ctx, state, layout, font))
			return (mp_region_failed("lobby status"));
		ctx->mp_status_signature = signature;
		changed = 1;
	}
	return (changed);
}

/**
 * @brief Refreshes the create-room panel's single region.
 */
static int	compose_create_regions(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_create_room_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	uint64_t	signature;

	signature = mp_hash(&state->mode, sizeof(state->mode),
			ctx->mp_static_signature);
	signature = mp_hash(&state->feedback, sizeof(state->feedback), signature);
	signature = mp_hash(&state->feedback_value, sizeof(state->feedback_value),
			signature);
	(void)view;
	if (ctx->mp_options_plane != NULL
		&& signature == ctx->mp_options_signature)
		return (0);
	if (!compose_options(ctx, state, layout, font))
		return (mp_region_failed("create room options"));
	ctx->mp_options_signature = signature;
	return (1);
}

/**
 * @brief Refreshes the waiting room's seats, status and chat column.
 */
static int	compose_room_regions(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_waiting_room_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	const t_app_room_view_model	*room;
	uint64_t					base;
	uint64_t					signature;
	int							changed;

	room = &view->data.room;
	/*
	 * The seats and the transcript seed different regions. Seeding both from
	 * one model hash is safe but wasteful: every chat message would repaint the
	 * seat list and every ready flag would repaint the transcript.
	 */
	base = room_players_signature(room, ctx->mp_static_signature);
	signature = mp_hash(&state->roster_offset, sizeof(state->roster_offset),
			base);
	changed = 0;
	if (ctx->mp_slots_plane == NULL || signature != ctx->mp_slots_signature)
	{
		if (!compose_slots(ctx, room, state, layout, font))
			return (mp_region_failed("room slots"));
		ctx->mp_slots_signature = signature;
		changed = 1;
	}
	signature = mp_hash(&state->counting_down, sizeof(state->counting_down),
			base);
	signature = mp_hash(&state->countdown, sizeof(state->countdown), signature);
	signature = mp_hash(&state->feedback, sizeof(state->feedback), signature);
	signature = mp_hash(&state->feedback_value, sizeof(state->feedback_value),
			signature);
	if (ctx->mp_status_plane == NULL || signature != ctx->mp_status_signature)
	{
		if (!compose_room_status(ctx, room, state, layout, font))
			return (mp_region_failed("room status"));
		ctx->mp_status_signature = signature;
		changed = 1;
	}
	signature = room_chat_signature(room, ctx->mp_static_signature);
	signature = mp_hash(&state->chatting, sizeof(state->chatting), signature);
	signature = mp_hash_text(state->compose, signature);
	if (ctx->mp_chat_plane == NULL || signature != ctx->mp_chat_signature)
	{
		if (!compose_chat(ctx, room, state, layout, font))
			return (mp_region_failed("room chat"));
		ctx->mp_chat_signature = signature;
		changed = 1;
	}
	return (changed);
}

/**
 * @brief Repaints the two mode cards, the control legend and the result line.
 */
static bool	compose_cards(t_render_ctx *ctx, const t_mp_mode_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX];
	t_color		edge;
	int			card_x;
	int			index;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	index = 0;
	while (index < MP_MODE_CARD_COUNT)
	{
		card_x = MP_MODE_REF_CARDS_X + index * MP_MODE_REF_CARD_STEP_X;
		edge = (int)state->focus == index ? g_mp_gold : g_mp_disabled;
		fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
			card_x, MP_MODE_REF_CARDS_Y, MP_MODE_REF_CARD_WIDTH,
			MP_MODE_REF_CARDS_HEIGHT,
			(int)state->focus == index ? g_mp_focus_plate : g_mp_plate, 240u);
		fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
			card_x, MP_MODE_REF_CARDS_Y, MP_MODE_REF_CARD_WIDTH, 3, edge, 255u);
		fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
			card_x, MP_MODE_REF_CARDS_Y + MP_MODE_REF_CARDS_HEIGHT - 3,
			MP_MODE_REF_CARD_WIDTH, 3, edge, 255u);
		fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
			card_x, MP_MODE_REF_CARDS_Y, 3, MP_MODE_REF_CARDS_HEIGHT, edge,
			255u);
		fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
			card_x + MP_MODE_REF_CARD_WIDTH - 3, MP_MODE_REF_CARDS_Y, 3,
			MP_MODE_REF_CARDS_HEIGHT, edge, 255u);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_name(index), card_x + 20,
			MP_MODE_REF_CARDS_Y + MP_MODE_REF_CARD_NAME_Y,
			MP_MODE_REF_CARD_WIDTH - 40, MP_MODE_REF_CARD_NAME_GLYPH,
			(int)state->focus == index ? g_mp_gold : g_mp_cream, false);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_players(index), card_x + 20,
			MP_MODE_REF_CARDS_Y + MP_MODE_REF_CARD_PLAYERS_Y,
			MP_MODE_REF_CARD_WIDTH - 40, 13, g_mp_green, false);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_line(index, 0), card_x + 20,
			MP_MODE_REF_CARDS_Y + MP_MODE_REF_CARD_BODY_Y,
			MP_MODE_REF_CARD_WIDTH - 40, MP_MODE_REF_CARD_BODY_GLYPH,
			g_mp_lavender, false);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_line(index, 1), card_x + 20,
			MP_MODE_REF_CARDS_Y + MP_MODE_REF_CARD_BODY_Y
			+ MP_MODE_REF_CARD_BODY_STEP, MP_MODE_REF_CARD_WIDTH - 40,
			MP_MODE_REF_CARD_BODY_GLYPH, g_mp_lavender, false);
		index++;
	}
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "[LEFT/RIGHT] CHOOSE     [ENTER] CONTINUE     [B] BACK",
		MP_MODE_REF_CARDS_X, MP_MODE_REF_CONTROLS_Y, MP_MODE_REF_CARDS_WIDTH,
		13, g_mp_cream, true);
	if (mp_feedback_text(state->feedback, state->feedback_value, line,
			sizeof(line))[0] != '\0')
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, MP_MODE_REF_CARDS_X, MP_MODE_REF_CARDS_Y
			+ MP_MODE_REF_CARDS_HEIGHT + 14, MP_MODE_REF_CARDS_WIDTH, 13,
			g_mp_gold, true);
	region = layout->cards;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_cards_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the room table: its filter heading, columns and rows.
 */
static bool	compose_list(t_render_ctx *ctx,
	const t_app_lobby_view_model *lobby, const t_lobby_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	const t_app_room_summary_view_model	*room;
	t_mp_rect							region;
	uint32_t							*pixels;
	int									row;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_plate(pixels, layout->pixel_width, layout->pixel_height, layout,
		&layout->rooms_plate, g_mp_gold);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, lobby_filter_name(state->filter), LOBBY_REF_LIST_X,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_TITLE_Y, LOBBY_REF_LIST_WIDTH,
		LOBBY_REF_HEADING_GLYPH, g_mp_pink, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "ID", LOBBY_REF_LIST_X + LOBBY_REF_COL_ID,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_HEADER_Y, LOBBY_REF_COL_ID_WIDTH, 11,
		g_mp_lavender, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "MODE", LOBBY_REF_LIST_X + LOBBY_REF_COL_MODE,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_HEADER_Y, LOBBY_REF_COL_MODE_WIDTH,
		11, g_mp_lavender, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "PLAYERS", LOBBY_REF_LIST_X + LOBBY_REF_COL_PLAYERS,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_HEADER_Y,
		LOBBY_REF_COL_PLAYERS_WIDTH, 11, g_mp_lavender, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "STATE", LOBBY_REF_LIST_X + LOBBY_REF_COL_STATE,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_HEADER_Y, LOBBY_REF_COL_STATE_WIDTH,
		11, g_mp_lavender, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "OWNER", LOBBY_REF_LIST_X + LOBBY_REF_COL_OWNER,
		LOBBY_REF_LIST_Y + LOBBY_REF_LIST_HEADER_Y, LOBBY_REF_COL_OWNER_WIDTH,
		11, g_mp_lavender, false);
	row = 0;
	while (row < LOBBY_VISIBLE_ROOMS)
	{
		room = lobby_visible_room(lobby, state->filter,
			state->list_offset + row);
		if (room == NULL)
			break ;
		draw_list_row(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, room, row, state->section == LOBBY_SECTION_ROOMS
			&& state->selected == state->list_offset + row);
		row++;
	}
	if (row == 0)
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, "No rooms here yet - press C to open one.",
			LOBBY_REF_LIST_X + LOBBY_REF_COL_ID, LOBBY_REF_LIST_Y
			+ LOBBY_REF_LIST_FIRST_ROW_Y + 10, LOBBY_REF_LIST_WIDTH - 40, 12,
			g_mp_disabled, false);
	region = layout->list;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_list_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Draws one room row, highlighting it when the cursor is on it.
 */
static void	draw_list_row(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, struct ncvisual *font,
	const t_app_room_summary_view_model *room, int row, bool focused)
{
	char	cell[APP_TEXT_MAX];
	t_color	tint;
	int		y;

	y = LOBBY_REF_LIST_Y + LOBBY_REF_LIST_FIRST_ROW_Y
		+ row * LOBBY_REF_LIST_ROW_STEP;
	tint = g_mp_cream;
	if (focused)
	{
		fill_ref_rect(pixels, width, height, layout, LOBBY_REF_LIST_X, y - 8,
			LOBBY_REF_LIST_WIDTH, LOBBY_REF_LIST_ROW_HEIGHT, g_mp_focus_plate,
			235u);
		draw_text_ref(pixels, width, height, layout, font, ">",
			LOBBY_REF_LIST_X + LOBBY_REF_COL_MARK, y, 24,
			LOBBY_REF_ROW_GLYPH, g_mp_gold, false);
		tint = g_mp_gold;
	}
	draw_text_ref(pixels, width, height, layout, font, nonempty(room->id),
		LOBBY_REF_LIST_X + LOBBY_REF_COL_ID, y, LOBBY_REF_COL_ID_WIDTH,
		LOBBY_REF_ROW_GLYPH, tint, false);
	draw_text_ref(pixels, width, height, layout, font,
		lobby_mode_tag(room->mode), LOBBY_REF_LIST_X + LOBBY_REF_COL_MODE, y,
		LOBBY_REF_COL_MODE_WIDTH, LOBBY_REF_ROW_GLYPH, g_mp_lavender, false);
	snprintf(cell, sizeof(cell), "%d/%d", room->players, room->capacity);
	draw_text_ref(pixels, width, height, layout, font, cell,
		LOBBY_REF_LIST_X + LOBBY_REF_COL_PLAYERS, y,
		LOBBY_REF_COL_PLAYERS_WIDTH, LOBBY_REF_ROW_GLYPH,
		room->capacity > 0 && room->players >= room->capacity
		? g_mp_red : g_mp_green, false);
	draw_text_ref(pixels, width, height, layout, font,
		lobby_state_tag(room->state), LOBBY_REF_LIST_X + LOBBY_REF_COL_STATE, y,
		LOBBY_REF_COL_STATE_WIDTH, LOBBY_REF_ROW_GLYPH, room_state_colour(room),
		false);
	draw_text_ref(pixels, width, height, layout, font, nonempty(room->owner),
		LOBBY_REF_LIST_X + LOBBY_REF_COL_OWNER, y, LOBBY_REF_COL_OWNER_WIDTH,
		LOBBY_REF_ROW_GLYPH, g_mp_cream, false);
}

/**
 * @brief Repaints the join-by-id field, showing the caret only when focused.
 */
static bool	compose_field(t_render_ctx *ctx, const t_lobby_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[LOBBY_ROOM_ID_MAX + 2];
	bool		focused;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_plate(pixels, layout->pixel_width, layout->pixel_height, layout,
		&layout->join_plate, g_mp_gold);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "JOIN BY ROOM ID", LOBBY_REF_JOIN_X + 16,
		LOBBY_REF_JOIN_TITLE_Y, LOBBY_REF_JOIN_WIDTH - 32,
		LOBBY_REF_HEADING_GLYPH, g_mp_pink, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "Ask a friend for their room", LOBBY_REF_JOIN_X + 16,
		LOBBY_REF_JOIN_HINT_Y, LOBBY_REF_JOIN_WIDTH - 32, 11, g_mp_lavender,
		false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "id to join it directly.", LOBBY_REF_JOIN_X + 16,
		LOBBY_REF_JOIN_HINT_Y + 30, LOBBY_REF_JOIN_WIDTH - 32, 11,
		g_mp_lavender, false);
	focused = state->section == LOBBY_SECTION_JOIN;
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "Enter room id", LOBBY_REF_FIELD_X, LOBBY_REF_FIELD_Y,
		LOBBY_REF_FIELD_WIDTH, 12, g_mp_lavender, false);
	fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
		LOBBY_REF_FIELD_X, LOBBY_REF_FIELD_Y + LOBBY_REF_FIELD_BOX_Y,
		LOBBY_REF_FIELD_WIDTH, LOBBY_REF_FIELD_BOX_HEIGHT,
		focused ? g_mp_focus_plate : g_mp_plate, 240u);
	fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
		LOBBY_REF_FIELD_X, LOBBY_REF_FIELD_Y + LOBBY_REF_FIELD_BOX_Y,
		LOBBY_REF_FIELD_WIDTH, 3, focused ? g_mp_gold : g_mp_disabled, 255u);
	fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
		LOBBY_REF_FIELD_X, LOBBY_REF_FIELD_Y + LOBBY_REF_FIELD_BOX_Y
		+ LOBBY_REF_FIELD_BOX_HEIGHT - 3, LOBBY_REF_FIELD_WIDTH, 3,
		focused ? g_mp_gold : g_mp_disabled, 255u);
	snprintf(line, sizeof(line), "%s%s", state->room_id, focused ? "_" : "");
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, line[0] == '\0' ? "..." : line, LOBBY_REF_FIELD_X + 16,
		LOBBY_REF_FIELD_Y + LOBBY_REF_FIELD_BOX_Y + 20,
		LOBBY_REF_FIELD_WIDTH - 32, 15,
		line[0] == '\0' ? g_mp_disabled : g_mp_cream, false);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, focused ? "[ENTER] JOIN" : "[RIGHT] TYPE AN ID",
		LOBBY_REF_FIELD_X, LOBBY_REF_FIELD_Y + LOBBY_REF_FIELD_BOX_Y
		+ LOBBY_REF_FIELD_BOX_HEIGHT + 22, LOBBY_REF_FIELD_WIDTH, 12,
		focused ? g_mp_gold : g_mp_disabled, false);
	region = layout->field;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_field_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the lobby's inline result line.
 */
static bool	compose_lobby_status(t_render_ctx *ctx, const t_lobby_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX * 2];

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	lobby_feedback_text(state, line, sizeof(line));
	if (line[0] != '\0')
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, LOBBY_REF_STATUS_X, LOBBY_REF_STATUS_Y + 18,
			LOBBY_REF_STATUS_WIDTH, 14,
			state->feedback == LOBBY_FEEDBACK_FULL
			|| state->feedback == LOBBY_FEEDBACK_IN_GAME
			|| state->feedback == LOBBY_FEEDBACK_UNKNOWN_ID
			? g_mp_red : g_mp_gold, true);
	region = layout->status;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_status_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the create-room panel's two option rows.
 */
static bool	compose_options(t_render_ctx *ctx, const t_create_room_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX];
	bool		focused;
	int			index;
	int			y;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	index = 0;
	while (index < MP_MODE_CARD_COUNT)
	{
		focused = create_room_focused_index(state) == index;
		y = CREATE_REF_OPTIONS_Y + index * CREATE_REF_OPTION_STEP_Y;
		if (focused)
			fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height,
				layout, CREATE_REF_OPTIONS_X, y, CREATE_REF_OPTIONS_WIDTH,
				CREATE_REF_OPTION_HEIGHT, g_mp_focus_plate, 235u);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, focused ? ">" : " ", CREATE_REF_OPTIONS_X + 12, y + 24, 24,
			15, g_mp_gold, false);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_name(index),
			CREATE_REF_OPTIONS_X + CREATE_REF_OPTION_NAME_X, y + 24, 330, 16,
			focused ? g_mp_gold : g_mp_cream, false);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, mp_mode_card_players(index),
			CREATE_REF_OPTIONS_X + CREATE_REF_OPTION_COUNT_X, y + 26, 210, 13,
			g_mp_green, false);
		if (index == 0)
			draw_text_ref(pixels, layout->pixel_width, layout->pixel_height,
				layout, font, "(default)",
				CREATE_REF_OPTIONS_X + CREATE_REF_OPTION_TAG_X, y + 26, 190, 12,
				g_mp_lavender, false);
		index++;
	}
	if (mp_feedback_text(state->feedback, state->feedback_value, line,
			sizeof(line))[0] != '\0')
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, CREATE_REF_OPTIONS_X, CREATE_REF_OPTIONS_Y
			+ CREATE_REF_OPTIONS_FEEDBACK_Y, CREATE_REF_OPTIONS_WIDTH, 13,
			g_mp_gold, true);
	region = layout->options;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_options_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the seat list and its ready badges.
 */
static bool	compose_slots(t_render_ctx *ctx, const t_app_room_view_model *room,
	const t_waiting_room_state *state, const t_mp_layout *layout,
	struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX * 2];
	const char	*badge;
	int			seats;
	int			visible;
	int			start;
	int			slot;
	int			index;
	int			y;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_plate(pixels, layout->pixel_width, layout->pixel_height, layout,
		&layout->slots_plate, g_mp_gold);
	seats = waiting_room_slot_count(room);
	visible = waiting_room_visible_slot_count(room);
	start = state->roster_offset;
	snprintf(line, sizeof(line), "SLOTS (%d/%d) - %d-%d", room->player_count,
		seats, visible > 0 ? start + 1 : 0, start + visible);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, line, ROOM_REF_SLOTS_X, ROOM_REF_SLOTS_Y
		+ ROOM_REF_SLOTS_HEADING_Y, ROOM_REF_SLOTS_WIDTH,
		ROOM_REF_HEADING_GLYPH, g_mp_pink, false);
	index = 0;
	while (index < visible)
	{
		slot = start + index;
		y = ROOM_REF_SLOTS_Y + ROOM_REF_SLOT_FIRST_Y
			+ index * ROOM_REF_SLOT_STEP_Y;
		if (waiting_room_seat_index(room, slot) == room->local_slot
			&& slot < room->player_count)
			fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height,
				layout, ROOM_REF_SLOTS_X - 8, y - 8, ROOM_REF_SLOTS_WIDTH,
				ROOM_REF_SLOT_HEIGHT, g_mp_focus_plate, 220u);
		waiting_room_slot_label(room, slot, line, sizeof(line));
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, ROOM_REF_SLOTS_X + 12, y, ROOM_REF_SLOT_BADGE_X - 24,
			ROOM_REF_ROW_GLYPH, slot < room->player_count
			? g_mp_cream : g_mp_disabled, false);
		badge = waiting_room_badge_text(room, slot);
		if (badge[0] != '\0')
			draw_text_ref(pixels, layout->pixel_width, layout->pixel_height,
				layout, font, badge, ROOM_REF_SLOTS_X + ROOM_REF_SLOT_BADGE_X,
				y, ROOM_REF_SLOTS_WIDTH - ROOM_REF_SLOT_BADGE_X - 12,
				ROOM_REF_ROW_GLYPH, waiting_room_seat_ready(room, slot)
				? g_mp_green : g_mp_amber, false);
		index++;
	}
	region = layout->slots;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_slots_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the status line, the countdown, and the last result.
 *
 * The countdown is the only value in the client that moves without a keystroke,
 * so it lives here alone: one small plane repaints per second and nothing else
 * on the screen is touched.
 */
static bool	compose_room_status(t_render_ctx *ctx,
	const t_app_room_view_model *room, const t_waiting_room_state *state,
	const t_mp_layout *layout, struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX * 2];
	t_color		tint;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	waiting_room_status_text(room, state, line, sizeof(line));
	tint = g_mp_amber;
	if (state->counting_down)
		tint = g_mp_gold;
	else if (waiting_room_can_start(room))
		tint = g_mp_green;
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, line, ROOM_REF_STATUS_X, ROOM_REF_STATUS_Y + 8,
		ROOM_REF_STATUS_WIDTH, 17, tint, false);
	waiting_room_feedback_text(state, line, sizeof(line));
	if (line[0] != '\0')
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, ROOM_REF_STATUS_X, ROOM_REF_STATUS_Y
			+ ROOM_REF_STATUS_FEEDBACK_Y, ROOM_REF_STATUS_WIDTH, 12,
			state->feedback == ROOM_FEEDBACK_READY ? g_mp_green : g_mp_gold,
			false);
	region = layout->status;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_status_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Repaints the chat transcript and the composer line beneath it.
 *
 * The transcript is drawn bottom-up from the newest message, so the newest line
 * is always visible however long the room has been open.
 */
static bool	compose_chat(t_render_ctx *ctx, const t_app_room_view_model *room,
	const t_waiting_room_state *state, const t_mp_layout *layout,
	struct ncvisual *font)
{
	t_mp_rect	region;
	uint32_t	*pixels;
	char		line[APP_TEXT_MAX + APP_ROOM_CHAT_TEXT_MAX + 4];
	int			body_height;
	int			visible;
	int			first;
	int			index;
	int			y;

	pixels = region_canvas(ctx, layout);
	if (pixels == NULL)
		return (false);
	draw_plate(pixels, layout->pixel_width, layout->pixel_height, layout,
		&layout->chat_plate, g_mp_gold);
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, "ROOM CHAT", ROOM_REF_CHAT_PLATE_X + 16,
		ROOM_REF_CHAT_TITLE_Y, ROOM_REF_CHAT_PLATE_WIDTH - 32,
		ROOM_REF_HEADING_GLYPH, g_mp_pink, false);
	body_height = ROOM_REF_CHAT_HEIGHT - ROOM_REF_CHAT_COMPOSE_HEIGHT;
	visible = body_height / ROOM_REF_CHAT_LINE_STEP;
	first = room->chat_count > visible ? room->chat_count - visible : 0;
	index = first;
	while (index < room->chat_count && index < APP_ROOM_CHAT_MAX)
	{
		y = ROOM_REF_CHAT_Y + (index - first) * ROOM_REF_CHAT_LINE_STEP;
		if (room->chat[index].system)
			snprintf(line, sizeof(line), "* %s", room->chat[index].text);
		else
			snprintf(line, sizeof(line), "%s: %s", room->chat[index].author,
				room->chat[index].text);
		draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
			font, line, ROOM_REF_CHAT_X + 8, y, ROOM_REF_CHAT_WIDTH - 16,
			ROOM_REF_CHAT_GLYPH,
			room->chat[index].system ? g_mp_disabled : g_mp_cream, false);
		index++;
	}
	y = ROOM_REF_CHAT_Y + body_height;
	fill_ref_rect(pixels, layout->pixel_width, layout->pixel_height, layout,
		ROOM_REF_CHAT_X, y, ROOM_REF_CHAT_WIDTH, 2,
		state->chatting ? g_mp_gold : g_mp_disabled, 255u);
	if (state->chatting)
		snprintf(line, sizeof(line), "> %s_", state->compose);
	else
		snprintf(line, sizeof(line), "[C] type a message");
	draw_text_ref(pixels, layout->pixel_width, layout->pixel_height, layout,
		font, line, ROOM_REF_CHAT_X + 8, y + 20, ROOM_REF_CHAT_WIDTH - 16,
		ROOM_REF_CHAT_GLYPH + 1,
		state->chatting ? g_mp_gold : g_mp_disabled, false);
	region = layout->chat;
	if (!create_region_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height, &region, &ctx->mp_chat_plane))
	{
		free(pixels);
		return (false);
	}
	free(pixels);
	return (true);
}

/**
 * @brief Allocates a full-frame canvas seeded with the composed static frame.
 *
 * Region composition draws onto a copy of what is already on screen, so the
 * cropped result is opaque wherever the frame is. That is what lets the
 * stationary tier use small planes at all: Sixel cannot write transparency
 * over existing content, but it can overwrite it.
 */
static uint32_t	*region_canvas(t_render_ctx *ctx, const t_mp_layout *layout)
{
	uint32_t	*canvas;
	size_t		count;

	if (layout->pixel_width <= 0 || layout->pixel_height <= 0)
		return (NULL);
	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	if (count > SIZE_MAX / sizeof(uint32_t))
		return (NULL);
	if (ctx->mp_static_pixels != NULL
		&& ctx->mp_pixels_width == layout->pixel_width
		&& ctx->mp_pixels_height == layout->pixel_height)
	{
		canvas = malloc(count * sizeof(*canvas));
		if (canvas != NULL)
			memcpy(canvas, ctx->mp_static_pixels, count * sizeof(*canvas));
		return (canvas);
	}
	return (calloc(count, sizeof(uint32_t)));
}

static bool	prefill_background(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height)
{
	if (ctx->mp_background_pixels == NULL
		|| ctx->bg_cols * ctx->cell_px_x != width
		|| ctx->bg_rows * ctx->cell_px_y != height)
		return (false);
	memcpy(pixels, ctx->mp_background_pixels,
		(size_t)width * (size_t)height * sizeof(*pixels));
	return (true);
}

static bool	create_static_plane(t_render_ctx *ctx, uint32_t *pixels,
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
static bool	create_region_plane(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height, const t_mp_rect *region, struct ncplane **slot)
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
 * @brief Fills one opaque plate with a three-pixel edge in the given colour.
 *
 * The backdrops are scenes rather than authored frames, so every panel on these
 * screens owns its plate. The fill is opaque because a stationary protocol
 * cannot write transparency over what is already on the terminal.
 */
static void	draw_plate(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, const t_mp_rect *ref_rect, t_color edge)
{
	int	edge_px;
	int	y;
	int	x;

	/*
	 * Plates are drawn from the fitted rectangle rather than from reference
	 * units so a plate and the region plane cropped out of it round the same
	 * way; a plate one pixel short of its region leaves a seam the stationary
	 * tier has nothing to fill with.
	 */
	if (ref_rect->width <= 0 || ref_rect->height <= 0)
		return ;
	edge_px = max_int(1, ref_size(layout, 3));
	y = ref_rect->y;
	while (y < ref_rect->y + ref_rect->height)
	{
		x = ref_rect->x;
		while (x < ref_rect->x + ref_rect->width)
		{
			if (y < ref_rect->y + edge_px
				|| y >= ref_rect->y + ref_rect->height - edge_px
				|| x < ref_rect->x + edge_px
				|| x >= ref_rect->x + ref_rect->width - edge_px)
				put_pixel(pixels, width, height, x, y, edge, 255u,
					layout->opaque_background);
			else
				put_pixel(pixels, width, height, x, y, g_mp_plate, 236u,
					layout->opaque_background);
			x++;
		}
		y++;
	}
}

/**
 * @brief Fills a full-width band behind a strip of loose text.
 *
 * The duel-hall backdrop is only quiet in its middle: a skull frieze runs along
 * the top and stacked tetrominoes along the bottom, and text laid straight onto
 * either is hard to read. The panels carry their own plates, so only the strips
 * outside them - the title row and the control legend - need this.
 */
static void	draw_band(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, int ref_y_value, int ref_height)
{
	fill_ref_rect(pixels, width, height, layout, MP_BAND_X, ref_y_value,
		MULTIPLAYER_REFERENCE_WIDTH - 2 * MP_BAND_X, ref_height, g_mp_plate,
		238u);
}

/**
 * @brief Draws a one-unit horizontal rule in reference space.
 */
static void	draw_rule(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, int ref_x_value, int ref_y_value, int ref_width,
	t_color tint)
{
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, 3, tint, 190u);
}

/**
 * @brief Fills a reference-space rectangle.
 */
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, int ref_x_value, int ref_y_value, int ref_width,
	int ref_height, t_color tint, unsigned alpha)
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

/**
 * @brief Shrinks a glyph size until the text fits its reference-space box.
 */
static int	fit_glyph_size(const t_mp_layout *layout, const char *text,
	int ref_width, int ref_glyph, int *spacing)
{
	int	glyph_size;

	glyph_size = max_int(3, ref_size(layout, ref_glyph));
	*spacing = max_int(1, ref_size(layout, MP_FONT_SPACING_REF));
	while (glyph_size > max_int(3, ref_size(layout, 6))
		&& text_width(text, glyph_size, *spacing) > ref_x(layout, ref_width))
	{
		glyph_size--;
		if (*spacing > max_int(1, ref_size(layout, 2)))
			*spacing -= 1;
	}
	return (glyph_size);
}

static void	draw_text_ref(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, struct ncvisual *font, const char *text,
	int ref_x_value, int ref_y_value, int ref_width, int ref_glyph,
	t_color tint, bool centered)
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
 */
static void	draw_text_fixed(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, struct ncvisual *font, const char *text,
	int ref_x_value, int ref_y_value, int ref_width, int glyph_size,
	int spacing, t_color tint, bool centered)
{
	char	visible[APP_TEXT_MAX + APP_ROOM_CHAT_TEXT_MAX + 8];
	int		max_chars;
	int		length;
	int		x;
	int		y;
	int		shadow;

	if (layout == NULL || font == NULL || text == NULL || ref_width <= 0)
		return ;
	shadow = max_int(1, ref_size(layout, MP_FONT_SHADOW_REF));
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
		y + shadow, glyph_size, spacing, g_mp_shadow);
	draw_text_run(pixels, width, height, layout, font, visible, x, y,
		glyph_size, spacing, tint);
}

static void	draw_text_run(uint32_t *pixels, int width, int height,
	const t_mp_layout *layout, struct ncvisual *font, const char *text, int x,
	int y, int glyph_size, int spacing, t_color tint)
{
	int	glyph;
	int	codepoint;

	glyph = 0;
	while (text[glyph] != '\0')
	{
		codepoint = (unsigned char)text[glyph];
		if (codepoint < 32
			|| codepoint >= 32 + MP_FONT_COLUMNS * MP_FONT_ROWS)
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
 */
static void	draw_glyph(uint32_t *pixels, int width, int height,
	struct ncvisual *font, int glyph, int x, int y, int glyph_size,
	t_color tint, const t_mp_layout *layout)
{
	int	source_y;
	int	source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < MP_FONT_WIDTH)
		{
			draw_glyph_cell(pixels, width, height, font, glyph, source_x,
				source_y, x + source_x * glyph_size / MP_FONT_WIDTH,
				y + ink_span(source_y - MP_FONT_INK_Y, glyph_size),
				(source_x + 1) * glyph_size / MP_FONT_WIDTH
				- source_x * glyph_size / MP_FONT_WIDTH,
				ink_span(source_y + 1 - MP_FONT_INK_Y, glyph_size)
				- ink_span(source_y - MP_FONT_INK_Y, glyph_size),
				tint, layout->opaque_background);
			source_x++;
		}
		source_y++;
	}
}

static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
	struct ncvisual *font, int glyph, int source_x, int source_y, int x, int y,
	int cell_width, int cell_height, t_color tint, bool opaque)
{
	uint32_t	source;
	unsigned	alpha;
	int			dest_y;
	int			dest_x;

	if (cell_width <= 0 || cell_height <= 0)
		return ;
	if (ncvisual_at_yx(font, (unsigned)((glyph / MP_FONT_COLUMNS)
				* MP_FONT_HEIGHT + source_y),
			(unsigned)((glyph % MP_FONT_COLUMNS)
				* MP_FONT_WIDTH + source_x), &source) < 0)
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
	t_color tint, unsigned alpha, bool opaque)
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

static void	blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha)
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

static int	ref_x(const t_mp_layout *layout, int value)
{
	return (value * layout->pixel_width / MULTIPLAYER_REFERENCE_WIDTH);
}

static int	ref_y(const t_mp_layout *layout, int value)
{
	return (value * layout->pixel_height / MULTIPLAYER_REFERENCE_HEIGHT);
}

static int	ref_size(const t_mp_layout *layout, int value)
{
	int	x_size;
	int	y_size;

	x_size = value * layout->pixel_width / MULTIPLAYER_REFERENCE_WIDTH;
	y_size = value * layout->pixel_height / MULTIPLAYER_REFERENCE_HEIGHT;
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

static t_color	room_state_colour(const t_app_room_summary_view_model *room)
{
	if (room->state == APP_ROOM_STATE_IN_GAME
		|| room->state == APP_ROOM_STATE_FINISHED)
		return (g_mp_disabled);
	if (room->capacity > 0 && room->players >= room->capacity)
		return (g_mp_red);
	return (g_mp_green);
}

static uint64_t	mp_hash(const void *data, size_t size, uint64_t hash)
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
 * @brief Hashes a NUL-terminated string, terminator included.
 *
 * The terminator is folded in so "ab" and "ab\0junk" in a longer buffer cannot
 * collide, which matters for the typed fields: their tails are never cleared.
 */
static uint64_t	mp_hash_text(const char *text, uint64_t hash)
{
	size_t	index;

	if (text == NULL)
		return (mp_hash("", 1, hash));
	index = 0;
	while (text[index] != '\0')
		index++;
	return (mp_hash(text, index + 1, hash));
}

/**
 * @brief Hashes everything the static layer draws from - and nothing else.
 *
 * Fields are hashed one at a time rather than as a struct: hashing a struct
 * wholesale folds in its padding bytes, which are never written and make the
 * signature unstable, so the layer would recompose on keystrokes that changed
 * nothing.
 *
 * Only facts settled before the screen opened appear here. Ready flags, chat,
 * the room list and every typed field are deliberately absent: they all move
 * while the screen is open, and this layer is a full-screen bitmap a stationary
 * protocol would re-emit over every region plane above it.
 */
static uint64_t	static_signature(const t_app_screen_view_model *view,
	const t_mp_layout *layout)
{
	uint64_t	hash;

	hash = mp_hash(&view->screen, sizeof(view->screen), 0);
	hash = mp_hash(&view->status, sizeof(view->status), hash);
	hash = mp_hash(&view->local_preview, sizeof(view->local_preview), hash);
	hash = mp_hash(&layout->pixel_width, sizeof(layout->pixel_width), hash);
	hash = mp_hash(&layout->pixel_height, sizeof(layout->pixel_height), hash);
	hash = mp_hash(&layout->opaque_background,
			sizeof(layout->opaque_background), hash);
	if (view->screen == APP_SCREEN_LOBBY)
	{
		hash = mp_hash_text(view->data.lobby.profile.username, hash);
		hash = mp_hash(&view->data.lobby.profile.score,
				sizeof(view->data.lobby.profile.score), hash);
		hash = mp_hash(&view->data.lobby.profile.rank,
				sizeof(view->data.lobby.profile.rank), hash);
	}
	else if (view->screen == APP_SCREEN_WAITING_ROOM)
	{
		hash = mp_hash_text(view->data.room.id, hash);
		hash = mp_hash(&view->data.room.mode, sizeof(view->data.room.mode),
				hash);
	}
	return (hash);
}

/**
 * @brief Hashes the model fields every region on this screen draws from.
 *
 * This is the trap on the other side of keeping the static layer still. Once
 * the static signature stops carrying the room list, the ready flags or the
 * transcript, a data change no longer dirties anything through the static seed
 * - so every region seeds from this instead, and a refresh that leaves the
 * cursor alone still repaints the rows underneath it.
 */
static uint64_t	model_signature(const t_app_screen_view_model *view,
	uint64_t hash)
{
	const t_app_room_view_model	*room;
	int							index;

	if (view->screen == APP_SCREEN_LOBBY)
	{
		hash = mp_hash(&view->data.lobby.count, sizeof(view->data.lobby.count),
				hash);
		index = 0;
		while (index < view->data.lobby.count && index < APP_LOBBY_MAX_ROOMS)
		{
			hash = mp_hash_text(view->data.lobby.rooms[index].id, hash);
			hash = mp_hash_text(view->data.lobby.rooms[index].owner, hash);
			hash = mp_hash(&view->data.lobby.rooms[index].mode,
					sizeof(view->data.lobby.rooms[index].mode), hash);
			hash = mp_hash(&view->data.lobby.rooms[index].state,
					sizeof(view->data.lobby.rooms[index].state), hash);
			hash = mp_hash(&view->data.lobby.rooms[index].players,
					sizeof(view->data.lobby.rooms[index].players), hash);
			hash = mp_hash(&view->data.lobby.rooms[index].capacity,
					sizeof(view->data.lobby.rooms[index].capacity), hash);
			index++;
		}
		return (hash);
	}
	if (view->screen != APP_SCREEN_WAITING_ROOM)
		return (hash);
	room = &view->data.room;
	hash = room_players_signature(room, hash);
	return (room_chat_signature(room, hash));
}

/**
 * @brief Hashes the seats, for the regions that draw from them.
 *
 * Split from the transcript so posting a message does not repaint the seat
 * list. The seed still has to cover everything a region draws that it does not
 * hash directly - it just must not cover more than that, or every region on the
 * screen repaints whenever any of them changes.
 */
static uint64_t	room_players_signature(const t_app_room_view_model *room,
	uint64_t hash)
{
	int	index;

	hash = mp_hash(&room->player_count, sizeof(room->player_count), hash);
	hash = mp_hash(&room->capacity, sizeof(room->capacity), hash);
	hash = mp_hash(&room->local_slot, sizeof(room->local_slot), hash);
	hash = mp_hash(&room->mode, sizeof(room->mode), hash);
	hash = mp_hash(&room->state, sizeof(room->state), hash);
	index = 0;
	while (index < room->player_count && index < APP_ROOM_MAX_PLAYERS)
	{
		hash = mp_hash_text(room->players[index].username, hash);
		hash = mp_hash(&room->players[index].owner,
				sizeof(room->players[index].owner), hash);
		hash = mp_hash(&room->players[index].ready,
				sizeof(room->players[index].ready), hash);
		index++;
	}
	return (hash);
}

/**
 * @brief Hashes the transcript, for the one region that draws it.
 */
static uint64_t	room_chat_signature(const t_app_room_view_model *room,
	uint64_t hash)
{
	int	index;

	hash = mp_hash(&room->chat_count, sizeof(room->chat_count), hash);
	index = 0;
	while (index < room->chat_count && index < APP_ROOM_CHAT_MAX)
	{
		hash = mp_hash_text(room->chat[index].author, hash);
		hash = mp_hash_text(room->chat[index].text, hash);
		index++;
	}
	return (hash);
}

static bool	mp_render_failed(const char *stage)
{
	fprintf(stderr, "tetrisu: multiplayer renderer failed at %s\n", stage);
	return (false);
}

/**
 * @brief Reports a failed region refresh in the tri-state form regions use.
 */
static int	mp_region_failed(const char *stage)
{
	(void)mp_render_failed(stage);
	return (-1);
}
