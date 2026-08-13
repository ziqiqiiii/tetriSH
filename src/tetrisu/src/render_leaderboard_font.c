#include "tetrisu.h"

# define LB_REF_WIDTH		1448
# define LB_REF_HEIGHT		1086
# define LB_FONT_SPACING_REF	3
# define LB_FONT_SHADOW_REF	3

static const t_color	g_lb_panel = {14, 7, 32};
static const t_color	g_lb_panel_alt = {24, 12, 45};
static const t_color	g_lb_shadow = {4, 1, 15};
static const t_color	g_lb_pink = {255, 112, 190};
static const t_color	g_lb_gold = {255, 203, 102};
static const t_color	g_lb_silver = {190, 205, 224};
static const t_color	g_lb_bronze = {219, 145, 91};
static const t_color	g_lb_cream = {250, 242, 221};
static const t_color	g_lb_lavender = {190, 155, 218};
static const t_color	g_lb_green = {112, 214, 174};

static bool	compose_pixels(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_leaderboard_pixel_layout *layout, uint32_t **pixels);
static bool	create_pixel_plane(t_render_ctx *ctx, uint32_t *pixels,
				int width, int height);
static int	update_static_layer(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_leaderboard_pixel_layout *layout);
static int	update_controls_layer(t_render_ctx *ctx,
				const t_leaderboard_state *state,
				const t_leaderboard_pixel_layout *layout);
static bool	create_controls_plane(t_render_ctx *ctx,
				const uint32_t *pixels,
				const t_leaderboard_pixel_layout *layout);
static uint64_t	lb_hash(const void *data, size_t size, uint64_t hash);
static uint64_t	static_signature(const t_app_screen_view_model *view,
				const t_leaderboard_pixel_layout *layout);
static void	draw_header(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_app_screen_view_model *view, const t_pixel_asset *font);
static void	draw_ready(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_app_leaderboard_view_model *leaderboard,
				const t_pixel_asset *font);
static void	draw_podium_card(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_app_leaderboard_entry_view_model *entry,
				const t_pixel_asset *font, int place, int ref_x_value,
				int ref_y_value, int ref_width, int ref_height, t_color accent);
static void	draw_rank_rows(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_app_leaderboard_view_model *leaderboard,
				const t_pixel_asset *font);
static void	draw_status(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_app_screen_view_model *view, const t_pixel_asset *font);
static void	draw_controls(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_leaderboard_state *state, const t_pixel_asset *font);
static void	draw_button(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_rect *rect, const t_pixel_asset *font,
				const char *label, bool focused);
static void	fill_ref_rect(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout, int ref_x_value,
				int ref_y_value, int ref_width, int ref_height, t_color tint,
				unsigned alpha);
static void	stroke_ref_rect(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout, int ref_x_value,
				int ref_y_value, int ref_width, int ref_height, int ref_thickness,
				t_color tint, unsigned alpha);
static void	fill_rect(uint32_t *pixels, int width, int height, int x, int y,
				int rect_width, int rect_height, t_color tint, unsigned alpha);
static void	stroke_rect(uint32_t *pixels, int width, int height, int x,
				int y, int rect_width, int rect_height, int thickness,
				t_color tint, unsigned alpha);
static void	draw_diamond_ref(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout, int ref_center_x,
				int ref_center_y, int ref_radius, t_color tint);
static void	draw_text_ref(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_pixel_asset *font, const char *text, int ref_x_value,
				int ref_y_value, int ref_width, int ref_glyph, t_color tint,
				bool centered);
static void	draw_text_box(uint32_t *pixels, int width, int height,
				const t_leaderboard_pixel_layout *layout,
				const t_pixel_asset *font, const char *text, int x, int y,
				int box_width, int box_height, int ref_glyph, t_color tint);
static void	draw_text_run(uint32_t *pixels, int width, int height,
				const t_pixel_asset *font, const char *text, int x, int y,
				int glyph_size, int spacing, t_color tint, unsigned opacity);
static void	draw_glyph(uint32_t *pixels, int width, int height,
				const t_pixel_asset *font, int glyph, int x, int y,
				int glyph_size, t_color tint, unsigned opacity);
static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
				const t_pixel_asset *font, int glyph, int source_x, int source_y,
				int x, int y, int cell_width, int cell_height, t_color tint,
				unsigned opacity);
static void	blend_pixel(uint32_t *pixel, t_color tint, unsigned alpha);
static int	ink_span(int units, int glyph_size);
static int	ref_x(const t_leaderboard_pixel_layout *layout, int value);
static int	ref_y(const t_leaderboard_pixel_layout *layout, int value);
static int	ref_size(const t_leaderboard_pixel_layout *layout, int value);
static int	text_width(const char *text, int glyph_size, int spacing);
static int	fit_glyph_size(const t_leaderboard_pixel_layout *layout,
				const char *text, int pixel_width, int ref_glyph, int *spacing);
static int	min_int(int first, int second);
static int	max_int(int first, int second);
static const char	*status_title(const t_app_screen_view_model *view);
static const char	*status_detail(const t_app_screen_view_model *view);

/**
 * @brief Renders the complete live leaderboard as authored bitmap content.
 */
bool	render_leaderboard_pixel_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_leaderboard_state *state,
	bool rebuild_background)
{
	t_leaderboard_background_action	action;
	t_leaderboard_pixel_layout		layout;
	int								rebuilt;
	int								changed;

	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL
		|| !render_pixels_available(ctx) || !notcurses_canpixel(ctx->nc))
		return (false);
	action = leaderboard_background_action_for(ctx->pixels,
			rebuild_background);
	if (action == LEADERBOARD_BACKGROUND_CELL)
		return (false);
	if ((action == LEADERBOARD_BACKGROUND_REPLACE_EXACT
			|| ctx->bg_plane == NULL)
		&& render_background_replace_exact(ctx,
			ctx->theme_assets.leaderboard_background, false) < 0)
		return (false);
	leaderboard_pixel_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	if (ctx->leaderboard_font.pixels == NULL
		&& !render_font_mask_load(&ctx->leaderboard_font))
		return (false);
	if (rebuild_background)
	{
		ctx->leaderboard_static_signature = 0;
		ctx->leaderboard_controls_signature = 0;
	}
	/*
	 * The header, podium, and rank rows only change when the provider returns
	 * new data, so they live on one full-screen plane written from a cached
	 * canvas. Focus moves between the two buttons, which is all the small
	 * controls plane above it covers.
	 */
	rebuilt = update_static_layer(ctx, view, &layout);
	if (rebuilt < 0)
		return (false);
	changed = update_controls_layer(ctx, state, &layout);
	if (changed < 0)
		return (false);
	changed |= rebuilt;
	ctx->leaderboard_pixel_active = true;
	if (rebuilt > 0)
	{
		ncplane_move_top(ctx->screen_plane);
		if (ctx->leaderboard_controls_plane != NULL)
			ncplane_move_top(ctx->leaderboard_controls_plane);
	}
	render_compatibility_badge_hide(ctx);
	render_notification_raise(ctx);
	if (changed == 0 && ctx->notifications.count == 0)
		return (true);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Recomposes the data-driven layer when the provider view changes.
 *
 * @return 1 when the layer was rebuilt, 0 when it was reused, -1 on failure.
 */
static int	update_static_layer(t_render_ctx *ctx,
	const t_app_screen_view_model *view,
	const t_leaderboard_pixel_layout *layout)
{
	uint64_t	signature;
	uint32_t	*pixels;

	signature = static_signature(view, layout);
	if (ctx->screen_plane != NULL && ctx->leaderboard_pixels != NULL
		&& signature == ctx->leaderboard_static_signature)
		return (0);
	pixels = NULL;
	if (!compose_pixels(ctx, view, layout, &pixels))
		return (-1);
	if (!create_pixel_plane(ctx, pixels, layout->pixel_width,
			layout->pixel_height))
	{
		free(pixels);
		return (-1);
	}
	free(ctx->leaderboard_pixels);
	ctx->leaderboard_pixels = pixels;
	ctx->leaderboard_pixels_width = layout->pixel_width;
	ctx->leaderboard_pixels_height = layout->pixel_height;
	ctx->leaderboard_static_signature = signature;
	return (1);
}

/**
 * @brief Repaints the button strip from a copy of the cached static canvas.
 *
 * Drawing over the canvas rather than into it keeps the cached layer free of
 * focus state, so a later data refresh does not inherit a stale highlight.
 *
 * @return 1 when the strip was rewritten, 0 when it was reused, -1 on failure.
 */
static int	update_controls_layer(t_render_ctx *ctx,
	const t_leaderboard_state *state,
	const t_leaderboard_pixel_layout *layout)
{
	uint64_t	signature;
	uint32_t	*pixels;
	size_t		count;

	signature = lb_hash(&state->focus, sizeof(state->focus),
			ctx->leaderboard_static_signature);
	if (ctx->leaderboard_controls_plane != NULL
		&& signature == ctx->leaderboard_controls_signature)
		return (0);
	if (ctx->leaderboard_pixels == NULL)
		return (-1);
	count = (size_t)layout->pixel_width * (size_t)layout->pixel_height;
	pixels = malloc(count * sizeof(*pixels));
	if (pixels == NULL)
		return (-1);
	memcpy(pixels, ctx->leaderboard_pixels, count * sizeof(*pixels));
	draw_controls(pixels, layout->pixel_width, layout->pixel_height, layout,
		state, &ctx->leaderboard_font);
	if (!create_controls_plane(ctx, pixels, layout))
	{
		free(pixels);
		return (-1);
	}
	free(pixels);
	ctx->leaderboard_controls_signature = signature;
	return (1);
}

/**
 * @brief Refreshes the controls plane from a window of the composed canvas.
 *
 * The window is expanded to cell boundaries so the plane covers whole cells,
 * and is blitted straight out of the canvas using its row stride. An existing
 * plane at the same geometry is written in place; replacing one would damage
 * the cells it held and force the full-screen bitmap below to be resent.
 */
static bool	create_controls_plane(t_render_ctx *ctx, const uint32_t *pixels,
	const t_leaderboard_pixel_layout *layout)
{
	ncplane_options		options;
	struct ncplane		*plane;
	const uint32_t		*origin;
	int					crop_x;
	int					crop_y;
	int					crop_width;
	int					crop_height;

	if (ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| layout->controls.width <= 0 || layout->controls.height <= 0)
		return (false);
	crop_x = layout->controls.x / ctx->cell_px_x * ctx->cell_px_x;
	crop_y = layout->controls.y / ctx->cell_px_y * ctx->cell_px_y;
	crop_width = min_int(((layout->controls.x + layout->controls.width
					+ ctx->cell_px_x - 1) / ctx->cell_px_x) * ctx->cell_px_x,
			layout->pixel_width) - crop_x;
	crop_height = min_int(((layout->controls.y + layout->controls.height
					+ ctx->cell_px_y - 1) / ctx->cell_px_y) * ctx->cell_px_y,
			layout->pixel_height) - crop_y;
	if (crop_width <= 0 || crop_height <= 0)
		return (false);
	memset(&options, 0, sizeof(options));
	options.y = ctx->bg_row + crop_y / ctx->cell_px_y;
	options.x = ctx->bg_col + crop_x / ctx->cell_px_x;
	options.rows = (unsigned)crop_height / (unsigned)ctx->cell_px_y;
	options.cols = (unsigned)crop_width / (unsigned)ctx->cell_px_x;
	origin = pixels + (size_t)crop_y * layout->pixel_width + crop_x;
	if (render_plane_geometry_matches(ctx->leaderboard_controls_plane,
			options.y, options.x, options.rows, options.cols))
		return (render_plane_blit_rgba(ctx, ctx->leaderboard_controls_plane,
				origin, crop_width, crop_height, layout->pixel_width));
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (false);
	if (!render_plane_blit_rgba(ctx, plane, origin, crop_width, crop_height,
			layout->pixel_width))
	{
		ncplane_destroy(plane);
		return (false);
	}
	if (ctx->leaderboard_controls_plane != NULL)
		ncplane_destroy(ctx->leaderboard_controls_plane);
	ctx->leaderboard_controls_plane = plane;
	return (true);
}

/**
 * @brief Folds the data the static layer draws into a signature.
 *
 * Hashing the view model wholesale would fold in its padding and the unused
 * tail of the entry array, either of which would rebuild the layer on frames
 * that changed nothing.
 */
static uint64_t	static_signature(const t_app_screen_view_model *view,
	const t_leaderboard_pixel_layout *layout)
{
	const t_app_leaderboard_entry_view_model	*entry;
	uint64_t									hash;
	int											index;

	hash = UINT64_C(1469598103934665603);
	hash = lb_hash(&view->status, sizeof(view->status), hash);
	hash = lb_hash(&view->local_preview, sizeof(view->local_preview), hash);
	hash = lb_hash(view->title, strlen(view->title), hash);
	hash = lb_hash(view->subtitle, strlen(view->subtitle), hash);
	hash = lb_hash(&layout->pixel_width, sizeof(layout->pixel_width), hash);
	hash = lb_hash(&layout->pixel_height, sizeof(layout->pixel_height), hash);
	hash = lb_hash(&view->data.leaderboard.count,
			sizeof(view->data.leaderboard.count), hash);
	index = 0;
	while (index < view->data.leaderboard.count
		&& index < APP_LEADERBOARD_MAX_ENTRIES)
	{
		entry = &view->data.leaderboard.entries[index];
		hash = lb_hash(&entry->position, sizeof(entry->position), hash);
		hash = lb_hash(entry->username, strlen(entry->username), hash);
		hash = lb_hash(&entry->score, sizeof(entry->score), hash);
		index++;
	}
	return (hash);
}

static uint64_t	lb_hash(const void *data, size_t size, uint64_t hash)
{
	const unsigned char	*bytes;
	size_t				index;

	bytes = data;
	index = 0;
	while (index < size)
	{
		hash ^= bytes[index];
		hash *= UINT64_C(1099511628211);
		index++;
	}
	return (hash);
}

/**
 * @brief Releases leaderboard-only font and composite caches.
 */
void	render_leaderboard_pixel_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	render_font_mask_free(&ctx->leaderboard_font);
	if (ctx->leaderboard_controls_plane != NULL)
	{
		ncplane_destroy(ctx->leaderboard_controls_plane);
		ctx->leaderboard_controls_plane = NULL;
	}
	free(ctx->leaderboard_pixels);
	ctx->leaderboard_pixels = NULL;
	ctx->leaderboard_pixels_width = 0;
	ctx->leaderboard_pixels_height = 0;
	ctx->leaderboard_pixel_active = false;
	ctx->leaderboard_static_signature = 0;
	ctx->leaderboard_controls_signature = 0;
}

/**
 * @brief Composes everything except the focus-sensitive control strip.
 */
static bool	compose_pixels(t_render_ctx *ctx,
	const t_app_screen_view_model *view,
	const t_leaderboard_pixel_layout *layout, uint32_t **pixels)
{
	size_t	count;

	if (layout->pixel_width <= 0 || layout->pixel_height <= 0
		|| (size_t)layout->pixel_width > SIZE_MAX
		/ (size_t)layout->pixel_height)
		return (false);
	count = (size_t)layout->pixel_width * layout->pixel_height;
	if (count > SIZE_MAX / sizeof(**pixels))
		return (false);
	*pixels = calloc(count, sizeof(**pixels));
	if (*pixels == NULL)
		return (false);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY)
	{
		if (ctx->backdrop_pixels == NULL
			|| ctx->backdrop_width != layout->pixel_width
			|| ctx->backdrop_height != layout->pixel_height)
		{
			free(*pixels);
			*pixels = NULL;
			return (false);
		}
		memcpy(*pixels, ctx->backdrop_pixels, count * sizeof(**pixels));
	}
	draw_header(*pixels, layout->pixel_width, layout->pixel_height, layout,
		view, &ctx->leaderboard_font);
	if (view->status == APP_DATA_READY && view->data.leaderboard.count > 0)
		draw_ready(*pixels, layout->pixel_width, layout->pixel_height, layout,
			&view->data.leaderboard, &ctx->leaderboard_font);
	else
		draw_status(*pixels, layout->pixel_width, layout->pixel_height, layout,
			view, &ctx->leaderboard_font);
	return (true);
}

static bool	create_pixel_plane(t_render_ctx *ctx, uint32_t *pixels,
	int width, int height)
{
	ncplane_options		options;
	struct ncplane		*plane;
	nccell				base;

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

static void	draw_header(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_app_screen_view_model *view, const t_pixel_asset *font)
{
	fill_ref_rect(pixels, width, height, layout, 344, 205, 760, 92,
		g_lb_shadow, 190u);
	stroke_ref_rect(pixels, width, height, layout, 344, 205, 760, 92, 3,
		g_lb_pink, 225u);
	draw_diamond_ref(pixels, width, height, layout, 724, 205, 12, g_lb_pink);
	draw_text_ref(pixels, width, height, layout, font, "LEADERBOARD",
		364, 222, 720, 30, g_lb_pink, true);
	draw_text_ref(pixels, width, height, layout, font, view->local_preview
		? "LOCAL UI PREVIEW" : "GLOBAL TOP SCORES", 364, 270, 720, 15,
		view->local_preview ? g_lb_gold : g_lb_green, true);
}

static void	draw_ready(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_app_leaderboard_view_model *leaderboard,
	const t_pixel_asset *font)
{
	draw_podium_card(pixels, width, height, layout,
		leaderboard_entry_for_position(leaderboard, 2), font, 2,
		274, 352, 286, 158, g_lb_silver);
	draw_podium_card(pixels, width, height, layout,
		leaderboard_entry_for_position(leaderboard, 1), font, 1,
		581, 318, 286, 192, g_lb_gold);
	draw_podium_card(pixels, width, height, layout,
		leaderboard_entry_for_position(leaderboard, 3), font, 3,
		888, 352, 286, 158, g_lb_bronze);
	draw_rank_rows(pixels, width, height, layout, leaderboard, font);
}

static void	draw_podium_card(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_app_leaderboard_entry_view_model *entry, const t_pixel_asset *font,
	int place, int ref_x_value, int ref_y_value, int ref_width, int ref_height,
	t_color accent)
{
	char	place_text[16];
	char	score[32];
	int		label_y;
	int		name_y;
	int		score_y;

	if (entry == NULL)
		return ;
	fill_ref_rect(pixels, width, height, layout, ref_x_value + 8,
		ref_y_value + 10, ref_width, ref_height, g_lb_shadow, 175u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value, ref_y_value,
		ref_width, ref_height, accent, 245u);
	fill_ref_rect(pixels, width, height, layout, ref_x_value + 7,
		ref_y_value + 7, ref_width - 14, ref_height - 14, g_lb_panel, 245u);
	stroke_ref_rect(pixels, width, height, layout, ref_x_value + 12,
		ref_y_value + 12, ref_width - 24, ref_height - 24, 2, accent, 180u);
	draw_diamond_ref(pixels, width, height, layout,
		ref_x_value + ref_width / 2, ref_y_value + 9, place == 1 ? 17 : 13,
		accent);
	snprintf(place_text, sizeof(place_text), place == 1 ? "1ST PLACE"
		: (place == 2 ? "2ND PLACE" : "3RD PLACE"));
	label_y = ref_y_value + (place == 1 ? 34 : 28);
	name_y = ref_y_value + (place == 1 ? 84 : 69);
	score_y = ref_y_value + (place == 1 ? 132 : 112);
	draw_text_ref(pixels, width, height, layout, font, place_text,
		ref_x_value + 18, label_y, ref_width - 36, place == 1 ? 19 : 16,
		accent, true);
	draw_text_ref(pixels, width, height, layout, font, entry->username,
		ref_x_value + 18, name_y, ref_width - 36, place == 1 ? 18 : 16,
		g_lb_cream, true);
	snprintf(score, sizeof(score), "%" PRIu64, entry->score);
	draw_text_ref(pixels, width, height, layout, font, score,
		ref_x_value + 18, score_y, ref_width - 36, place == 1 ? 16 : 14,
		g_lb_lavender, true);
}

static void	draw_rank_rows(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_app_leaderboard_view_model *leaderboard, const t_pixel_asset *font)
{
	const t_app_leaderboard_entry_view_model	*entry;
	char										position_text[8];
	char										score[32];
	int											position;
	int											y;

	draw_text_ref(pixels, width, height, layout, font, "RANK",
		350, 530, 110, 14, g_lb_pink, true);
	draw_text_ref(pixels, width, height, layout, font, "PLAYER",
		472, 530, 420, 14, g_lb_pink, true);
	draw_text_ref(pixels, width, height, layout, font, "SCORE",
		910, 530, 188, 14, g_lb_pink, true);
	position = 4;
	while (position <= APP_LEADERBOARD_MAX_ENTRIES)
	{
		entry = leaderboard_entry_for_position(leaderboard, position);
		y = 565 + (position - 4) * 38;
		fill_ref_rect(pixels, width, height, layout, 340, y, 768, 34,
			position % 2 == 0 ? g_lb_panel_alt : g_lb_panel, 225u);
		stroke_ref_rect(pixels, width, height, layout, 340, y, 768, 34, 1,
			position % 2 == 0 ? g_lb_pink : g_lb_lavender, 145u);
		fill_ref_rect(pixels, width, height, layout, 352, y + 4, 58, 26,
			position % 2 == 0 ? g_lb_pink : g_lb_lavender, 110u);
		snprintf(position_text, sizeof(position_text), "%d", position);
		draw_text_ref(pixels, width, height, layout, font, position_text,
			352, y + 9, 58, 12, g_lb_cream, true);
		if (entry != NULL)
		{
			draw_text_ref(pixels, width, height, layout, font,
				entry->username, 434, y + 8, 440, 14, g_lb_cream, false);
			snprintf(score, sizeof(score), "%" PRIu64, entry->score);
			draw_text_ref(pixels, width, height, layout, font, score,
				892, y + 8, 198, 13, g_lb_lavender, true);
		}
		position++;
	}
}

static void	draw_status(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_app_screen_view_model *view, const t_pixel_asset *font)
{
	fill_ref_rect(pixels, width, height, layout, 344, 390, 760, 280,
		g_lb_shadow, 205u);
	stroke_ref_rect(pixels, width, height, layout, 344, 390, 760, 280, 3,
		g_lb_pink, 220u);
	draw_diamond_ref(pixels, width, height, layout, 724, 390, 14, g_lb_gold);
	draw_text_ref(pixels, width, height, layout, font, status_title(view),
		384, 474, 680, 25, g_lb_gold, true);
	draw_text_ref(pixels, width, height, layout, font, status_detail(view),
		384, 548, 680, 16, g_lb_lavender, true);
}

static void	draw_controls(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout,
	const t_leaderboard_state *state, const t_pixel_asset *font)
{
	draw_button(pixels, width, height,
		&layout->buttons[LEADERBOARD_FOCUS_BACK], font, "BACK",
		state->focus == LEADERBOARD_FOCUS_BACK);
	draw_button(pixels, width, height,
		&layout->buttons[LEADERBOARD_FOCUS_REFRESH], font, "REFRESH",
		state->focus == LEADERBOARD_FOCUS_REFRESH);
	draw_text_ref(pixels, width, height, layout, font,
		"ESC BACK  |  R REFRESH  |  LEFT/RIGHT SELECT",
		330, 842, 788, 11, g_lb_lavender, true);
}

static void	draw_button(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_rect *rect, const t_pixel_asset *font,
	const char *label, bool focused)
{
	t_color	accent;
	int		thickness;

	accent = focused ? g_lb_gold : g_lb_lavender;
	thickness = max_int(1, rect->height / 18);
	fill_rect(pixels, width, height, rect->x + thickness,
		rect->y + thickness, rect->width, rect->height, g_lb_shadow, 160u);
	fill_rect(pixels, width, height, rect->x, rect->y, rect->width,
		rect->height, focused ? accent : g_lb_panel_alt,
		focused ? 245u : 235u);
	stroke_rect(pixels, width, height, rect->x, rect->y, rect->width,
		rect->height, thickness, accent, 255u);
	draw_text_box(pixels, width, height, NULL, font, label, rect->x,
		rect->y, rect->width, rect->height, 16,
		focused ? g_lb_panel : g_lb_cream);
}

static void	fill_ref_rect(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout, int ref_x_value, int ref_y_value,
	int ref_width, int ref_height, t_color tint, unsigned alpha)
{
	fill_rect(pixels, width, height, ref_x(layout, ref_x_value),
		ref_y(layout, ref_y_value), ref_x(layout, ref_width),
		ref_y(layout, ref_height), tint, alpha);
}

static void	stroke_ref_rect(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout, int ref_x_value, int ref_y_value,
	int ref_width, int ref_height, int ref_thickness, t_color tint,
	unsigned alpha)
{
	stroke_rect(pixels, width, height, ref_x(layout, ref_x_value),
		ref_y(layout, ref_y_value), ref_x(layout, ref_width),
		ref_y(layout, ref_height), max_int(1, ref_size(layout, ref_thickness)),
		tint, alpha);
}

static void	fill_rect(uint32_t *pixels, int width, int height, int x, int y,
	int rect_width, int rect_height, t_color tint, unsigned alpha)
{
	int	dest_y;
	int	dest_x;

	dest_y = max_int(0, y);
	while (dest_y < min_int(height, y + rect_height))
	{
		dest_x = max_int(0, x);
		while (dest_x < min_int(width, x + rect_width))
		{
			blend_pixel(&pixels[(size_t)dest_y * width + dest_x], tint,
				alpha);
			dest_x++;
		}
		dest_y++;
	}
}

static void	stroke_rect(uint32_t *pixels, int width, int height, int x,
	int y, int rect_width, int rect_height, int thickness, t_color tint,
	unsigned alpha)
{
	fill_rect(pixels, width, height, x, y, rect_width, thickness, tint, alpha);
	fill_rect(pixels, width, height, x, y + rect_height - thickness,
		rect_width, thickness, tint, alpha);
	fill_rect(pixels, width, height, x, y, thickness, rect_height, tint, alpha);
	fill_rect(pixels, width, height, x + rect_width - thickness, y,
		thickness, rect_height, tint, alpha);
}

static void	draw_diamond_ref(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout, int ref_center_x,
	int ref_center_y, int ref_radius, t_color tint)
{
	int	center_x;
	int	center_y;
	int	radius;
	int	y;
	int	x;

	center_x = ref_x(layout, ref_center_x);
	center_y = ref_y(layout, ref_center_y);
	radius = max_int(1, ref_size(layout, ref_radius));
	y = -radius;
	while (y <= radius)
	{
		x = -radius;
		while (x <= radius)
		{
			if (abs(x) + abs(y) <= radius && center_x + x >= 0
				&& center_x + x < width && center_y + y >= 0
				&& center_y + y < height)
				blend_pixel(&pixels[(size_t)(center_y + y) * width
					+ center_x + x], tint, 255u);
			x++;
		}
		y++;
	}
}

static void	draw_text_ref(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout, const t_pixel_asset *font,
	const char *text, int ref_x_value, int ref_y_value, int ref_width,
	int ref_glyph, t_color tint, bool centered)
{
	char	visible[APP_TEXT_MAX + 64];
	int		glyph_size;
	int		spacing;
	int		max_chars;
	int		length;
	int		text_pixels;
	int		x;
	int		y;
	int		shadow;

	if (layout == NULL || font == NULL || font->pixels == NULL || text == NULL
		|| ref_width <= 0)
		return ;
	glyph_size = fit_glyph_size(layout, text, ref_x(layout, ref_width),
		ref_glyph, &spacing);
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
	shadow = max_int(1, ref_size(layout, LB_FONT_SHADOW_REF));
	draw_text_run(pixels, width, height, font, visible, x + shadow,
		y + shadow, glyph_size, spacing, g_lb_shadow, 235u);
	draw_text_run(pixels, width, height, font, visible, x, y, glyph_size,
		spacing, tint, 255u);
}

static void	draw_text_box(uint32_t *pixels, int width, int height,
	const t_leaderboard_pixel_layout *layout, const t_pixel_asset *font,
	const char *text, int x, int y, int box_width, int box_height,
	int ref_glyph, t_color tint)
{
	int	glyph_size;
	int	spacing;
	int	text_pixels;
	int	shadow;

	(void)layout;
	glyph_size = max_int(5, ref_glyph * width / LB_REF_WIDTH);
	spacing = max_int(1, LB_FONT_SPACING_REF * width / LB_REF_WIDTH);
	while (glyph_size > 5
		&& text_width(text, glyph_size, spacing) > box_width - 16)
		glyph_size--;
	text_pixels = text_width(text, glyph_size, spacing);
	shadow = max_int(1, glyph_size / 8);
	x += (box_width - text_pixels) / 2;
	y += (box_height - glyph_size) / 2;
	draw_text_run(pixels, width, height, font, text, x + shadow, y + shadow,
		glyph_size, spacing, g_lb_shadow, 235u);
	draw_text_run(pixels, width, height, font, text, x, y, glyph_size,
		spacing, tint, 255u);
}

static void	draw_text_run(uint32_t *pixels, int width, int height,
	const t_pixel_asset *font, const char *text, int x, int y, int glyph_size,
	int spacing, t_color tint, unsigned opacity)
{
	unsigned	codepoint;
	int			index;

	index = 0;
	while (text[index] != '\0')
	{
		codepoint = (unsigned char)text[index];
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		draw_glyph(pixels, width, height, font, (int)codepoint - 32,
			x + index * (glyph_size + spacing), y, glyph_size, tint, opacity);
		index++;
	}
}

static void	draw_glyph(uint32_t *pixels, int width, int height,
	const t_pixel_asset *font, int glyph, int x, int y, int glyph_size,
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
			draw_glyph_cell(pixels, width, height, font, glyph, source_x,
				source_y, x + source_x * glyph_size / FONT_GLYPH_WIDTH,
				y + ink_span(source_y - FONT_INK_Y, glyph_size),
				(source_x + 1) * glyph_size / FONT_GLYPH_WIDTH
				- source_x * glyph_size / FONT_GLYPH_WIDTH,
				ink_span(source_y + 1 - FONT_INK_Y, glyph_size)
				- ink_span(source_y - FONT_INK_Y, glyph_size), tint, opacity);
			source_x++;
		}
		source_y++;
	}
}

static void	draw_glyph_cell(uint32_t *pixels, int width, int height,
	const t_pixel_asset *font, int glyph, int source_x, int source_y,
	int x, int y, int cell_width, int cell_height, t_color tint,
	unsigned opacity)
{
	unsigned	alpha;
	int			dest_y;
	int			dest_x;

	if (cell_width <= 0 || cell_height <= 0)
		return ;
	alpha = ncpixel_a(font->pixels[(size_t)((glyph / FONT_COLUMNS)
			* FONT_GLYPH_HEIGHT + source_y) * font->width
		+ (glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH + source_x]);
	alpha = (alpha * opacity + 127u) / 255u;
	if (alpha == 0)
		return ;
	dest_y = 0;
	while (dest_y < cell_height)
	{
		dest_x = 0;
		while (dest_x < cell_width)
		{
			if (x + dest_x >= 0 && x + dest_x < width && y + dest_y >= 0
				&& y + dest_y < height)
				blend_pixel(&pixels[(size_t)(y + dest_y) * width + x
					+ dest_x], tint, alpha);
			dest_x++;
		}
		dest_y++;
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

static int	ink_span(int units, int glyph_size)
{
	if (units >= 0)
		return (units * glyph_size / FONT_INK_HEIGHT);
	return (-((-units * glyph_size + FONT_INK_HEIGHT - 1)
			/ FONT_INK_HEIGHT));
}

static int	ref_x(const t_leaderboard_pixel_layout *layout, int value)
{
	return (value * layout->pixel_width / LB_REF_WIDTH);
}

static int	ref_y(const t_leaderboard_pixel_layout *layout, int value)
{
	return (value * layout->pixel_height / LB_REF_HEIGHT);
}

static int	ref_size(const t_leaderboard_pixel_layout *layout, int value)
{
	return (min_int(ref_x(layout, value), ref_y(layout, value)));
}

static int	text_width(const char *text, int glyph_size, int spacing)
{
	int	length;

	length = (int)strlen(text);
	if (length == 0)
		return (0);
	return (length * glyph_size + (length - 1) * spacing);
}

static int	fit_glyph_size(const t_leaderboard_pixel_layout *layout,
	const char *text, int pixel_width, int ref_glyph, int *spacing)
{
	int	glyph_size;
	int	minimum;

	glyph_size = max_int(5, ref_size(layout, ref_glyph));
	minimum = max_int(5, ref_size(layout, 7));
	*spacing = max_int(1, ref_size(layout, LB_FONT_SPACING_REF));
	while (glyph_size > minimum
		&& text_width(text, glyph_size, *spacing) > pixel_width)
	{
		glyph_size--;
		if (*spacing > 1)
			(*spacing)--;
	}
	return (glyph_size);
}

static int	min_int(int first, int second)
{
	if (first < second)
		return (first);
	return (second);
}

static int	max_int(int first, int second)
{
	if (first > second)
		return (first);
	return (second);
}

static const char	*status_title(const t_app_screen_view_model *view)
{
	if (view->status == APP_DATA_LOADING)
		return ("FETCHING THE LATEST SCORES");
	if (view->status == APP_DATA_EMPTY || (view->status == APP_DATA_READY
			&& view->data.leaderboard.count == 0))
		return ("NO SCORES YET");
	if (view->status == APP_DATA_UNAVAILABLE)
		return ("LEADERBOARD OFFLINE");
	return ("SCORES COULD NOT BE LOADED");
}

static const char	*status_detail(const t_app_screen_view_model *view)
{
	if (view->status == APP_DATA_LOADING)
		return ("PLEASE WAIT");
	if (view->status == APP_DATA_EMPTY || (view->status == APP_DATA_READY
			&& view->data.leaderboard.count == 0))
		return ("THE FIRST GREAT RUN COULD BE YOURS");
	if (view->status == APP_DATA_UNAVAILABLE)
		return ("THE SCORE SERVER IS UNAVAILABLE");
	return ("CHECK THE SERVER AND TRY REFRESH");
}
