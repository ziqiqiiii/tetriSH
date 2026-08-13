#include "tetrisu.h"

# define NOTIFICATION_COLS		38
# define NOTIFICATION_ROWS		4
# define NOTIFICATION_TOP		1
# define NOTIFICATION_RIGHT		1
# define NOTIFICATION_GAP		1
# define NOTIFICATION_TEXT_LEFT	13
# define NOTIFICATION_MIN_COLS	24

/*
 * Authored size of both notification cards. The plane is sized from this so
 * the card is blitted at its own aspect: a plane picked without reference to
 * it stretched the plaque and dragged the lettering off its own border.
 */
# define NOTIFICATION_ART_WIDTH		640
# define NOTIFICATION_ART_HEIGHT	138

/*
 * Text boxes measured from the artwork, in per-mille of the card, covering
 * the flat interior of the plaque only. The ownership card spends its left
 * edge on the marketplace stall, so its box starts further right.
 */
# define OWNERSHIP_BOX_X		320
# define OWNERSHIP_BOX_WIDTH	600
# define OWNERSHIP_BOX_Y		330
# define OWNERSHIP_BOX_HEIGHT	520
# define VOLUME_BOX_X			285
# define VOLUME_BOX_WIDTH		620
# define VOLUME_BOX_Y			330
# define VOLUME_BOX_HEIGHT		550
# define NOTIFICATION_BAND		46
# define NOTIFICATION_BAND_GAP	8

typedef struct s_notif_box
{
	int	x;
	int	y;
	int	width;
	int	height;
}	t_notif_box;

static const t_color	g_notification_gold = {255, 210, 118};
static const t_color	g_notification_cream = {255, 244, 250};
static const t_color	g_notification_filled = {255, 94, 167};
static const t_color	g_notification_empty = {116, 76, 142};

static void	refresh_notifications(t_render_ctx *ctx, uint64_t now_ms);
static int	art_columns(const t_render_ctx *ctx);
static bool	notification_position(t_render_ctx *ctx, int index, int cols,
						int *y, int *x);
static struct ncplane	*create_art_plane(t_render_ctx *ctx, int y, int x,
						const t_ui_notification *notification, int opacity,
						bool *content_embedded);
static uint32_t	*build_canvas(t_render_ctx *ctx, int y, int x,
						const t_ui_notification *notification, int opacity,
						int width, int height);
static struct ncplane	*blit_canvas(t_render_ctx *ctx, const uint32_t *canvas,
						int width, int height, int y, int x, int cols);
static void	fill_backdrop(t_render_ctx *ctx, uint32_t *canvas, int width,
						int height, int y, int x);
static void	copy_backdrop_row(uint32_t *dest, int width,
						const uint32_t *source, int source_width,
						int source_y, int source_x);
static void	compose_card(uint32_t *canvas, int width, int height,
						struct ncvisual *visual, int opacity);
static void	blend_over(uint32_t *dest, unsigned red, unsigned green,
						unsigned blue, unsigned alpha);
static void	fill_rect(uint32_t *canvas, int width, int height,
						const t_notif_box *rect, t_color tint, unsigned alpha);
static const t_pixel_asset	*notification_font(t_render_ctx *ctx);
static t_notif_box	text_box(const t_ui_notification *notification,
						int width, int height);
static t_notif_box	band(const t_notif_box *box, int index);
static t_notif_box	place_line(const t_notif_box *band_box, const char *text,
						int limit);
static int	cap_row(int band_y, int glyph_size);
static void	draw_card_text(t_render_ctx *ctx, uint32_t *canvas, int width,
						int height, const t_ui_notification *notification);
static void	draw_ownership_text(uint32_t *canvas, int width, int height,
						const t_pixel_asset *font,
						const t_ui_notification *notification,
						const t_notif_box *box);
static void	draw_volume_text(uint32_t *canvas, int width, int height,
						const t_pixel_asset *font,
						const t_ui_notification *notification,
						const t_notif_box *box);
static void	draw_volume_bar(uint32_t *canvas, int width, int height,
						const t_notif_box *band_box, int percent);
static void	draw_atlas_text(uint32_t *canvas, int width, int height,
						const t_pixel_asset *font, const char *text,
						const t_notif_box *at, t_color tint);
static void	draw_atlas_glyph(uint32_t *canvas, int width, int height,
						const t_pixel_asset *font, int glyph, int x, int y,
						int glyph_size, t_color tint);
static void	draw_atlas_texel(uint32_t *canvas, int width, int height,
						unsigned alpha, const t_notif_box *cell, t_color tint);
static int	ink_span(int units, int glyph_size);
static int	glyph_spacing(int glyph_size);
static int	text_pixels(const char *text, int glyph_size);
static int	fit_glyph(const char *text, int box_width, int glyph_size);
static struct ncplane	*create_text_plane(t_render_ctx *ctx, int y, int x,
						const t_ui_notification *notification, int opacity,
						bool has_art);
static const char	*notification_asset_path(
						const t_ui_notification *notification);
static void	draw_notification(struct ncplane *plane,
						const t_ui_notification *notification, int opacity,
						int row_offset);
static void	draw_bar(struct ncplane *plane, int percent, int opacity,
						int row);
static void	draw_fallback_frame(struct ncplane *plane,
						const t_ui_notification *notification, int opacity);
static void	set_transparent_base(struct ncplane *plane);
static void	set_color(struct ncplane *plane, unsigned r, unsigned g,
						unsigned b, int opacity);
static unsigned	fade_component(unsigned component, int opacity);
static void	destroy_notification_planes(t_render_ctx *ctx);
static void	raise_planes(t_render_ctx *ctx);
static bool	can_refresh_stationary_in_place(const t_render_ctx *ctx,
						bool content_changed);

/**
 * @brief Shows or refreshes the global music-volume notification.
 *
 * Every bitmap tier uses authored pixel art. Stationary/Sixel terminals keep
 * the art opaque, refresh matching content in place, and retransmit it after a
 * full-screen bitmap redraw; true cell mode uses the framed presentation.
 *
 * @param ctx Active render context.
 * @param volume Mixer volume in the application range.
 */
void	render_notification_show_volume(t_render_ctx *ctx, int volume)
{
	if (ctx == NULL || ctx->nc == NULL)
		return ;
	render_notification_queue_volume(ctx, volume);
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Refreshes volume notification planes for a caller-owned render.
 */
void	render_notification_queue_volume(t_render_ctx *ctx, int volume)
{
	uint64_t	now_ms;
	bool		content_changed;

	if (ctx == NULL || ctx->nc == NULL)
		return ;
	now_ms = ui_notification_now_ms();
	content_changed = ui_notification_show(&ctx->notifications, "MUSIC",
		ui_notification_volume_percent(volume), now_ms);
	if (can_refresh_stationary_in_place(ctx, content_changed))
	{
		raise_planes(ctx);
		return ;
	}
	refresh_notifications(ctx, now_ms);
}

/**
 * @brief Queues the fixed Marketplace guidance shown for a locked item.
 */
/**
 * @brief Shows one incoming-ability card, right now.
 *
 * Shown rather than queued: this is the only warning a player gets that
 * something has been done to them, and it arrives while they are placing a
 * piece. A card held back until the next stationary repaint would arrive after
 * the effect it describes had already cost them something.
 *
 * @param ctx Render context.
 * @param title What landed.
 * @param message What it does.
 */
void	render_notification_show_effect(t_render_ctx *ctx, const char *title,
		const char *message)
{
	uint64_t	now_ms;

	if (ctx == NULL || ctx->nc == NULL)
		return ;
	now_ms = ui_notification_now_ms();
	(void)ui_notification_show_effect(&ctx->notifications, title, message,
		now_ms);
	refresh_notifications(ctx, now_ms);
}

void	render_notification_queue_ownership(t_render_ctx *ctx)
{
	uint64_t	now_ms;
	bool		content_changed;

	if (ctx == NULL || ctx->nc == NULL)
		return ;
	now_ms = ui_notification_now_ms();
	content_changed = ui_notification_show_ownership(&ctx->notifications,
			now_ms);
	if (can_refresh_stationary_in_place(ctx, content_changed))
	{
		raise_planes(ctx);
		return ;
	}
	refresh_notifications(ctx, now_ms);
}

/**
 * @brief Advances notification fade and expiry state.
 *
 * @param ctx Active render context.
 */
void	render_notification_tick(t_render_ctx *ctx)
{
	uint64_t	now_ms;
	bool		changed;

	if (ctx == NULL || ctx->nc == NULL || ctx->notifications.count == 0)
		return ;
	now_ms = ui_notification_now_ms();
	changed = ui_notification_update(&ctx->notifications, now_ms);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY && !changed)
		return ;
	refresh_notifications(ctx, now_ms);
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Returns the delay until the global overlay needs another frame.
 *
 * @param ctx Active render context.
 * @return Delay in milliseconds, or -1 with no visible notifications.
 */
int	render_notification_next_wake_ms(const t_render_ctx *ctx)
{
	if (ctx == NULL)
		return (-1);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY)
		return (ui_notification_next_expiry_ms(&ctx->notifications,
				ui_notification_now_ms()));
	return (ui_notification_next_wake_ms(&ctx->notifications,
			ui_notification_now_ms()));
}

/**
 * @brief Rebuilds active notifications after a terminal resize.
 *
 * @param ctx Active render context.
 */
void	render_notification_reflow(t_render_ctx *ctx)
{
	if (ctx == NULL || ctx->nc == NULL || ctx->notifications.count == 0)
		return ;
	refresh_notifications(ctx, ui_notification_now_ms());
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Keeps each text plane above its matching pixel-art plane.
 *
 * @param ctx Active render context.
 */
void	render_notification_raise(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	if (ctx->notifications.count > 0
		&& tetrisu_pixel_policy_notification_needs_reemit(ctx->pixels))
	{
		refresh_notifications(ctx, ui_notification_now_ms());
		return ;
	}
	raise_planes(ctx);
}

static void	raise_planes(t_render_ctx *ctx)
{
	int	index;

	index = ctx->notifications.count - 1;
	while (index >= 0)
	{
		if (ctx->notification_art_planes[index] != NULL)
			ncplane_move_top(ctx->notification_art_planes[index]);
		if (ctx->notification_planes[index] != NULL)
			ncplane_move_top(ctx->notification_planes[index]);
		index--;
	}
}

static bool	can_refresh_stationary_in_place(const t_render_ctx *ctx,
	bool content_changed)
{
	return (ctx->pixels == TETRISU_PIXELS_STATIONARY && !content_changed
		&& ctx->notifications.count > 0
		&& ctx->notification_art_planes[0] != NULL);
}

/**
 * @brief Releases every global notification plane and state entry.
 *
 * @param ctx Render context being torn down.
 */
void	render_notification_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	destroy_notification_planes(ctx);
	render_font_mask_free(&ctx->notification_font);
	ui_notification_stack_init(&ctx->notifications);
}

/**
 * @brief Reports whether a screen owes a repaint because of a notification.
 *
 * @param ctx Active render context.
 * @return true when notification planes have appeared or gone since the last
 *         screen repaint.
 */
bool	render_notification_repaint_pending(const t_render_ctx *ctx)
{
	return (ctx != NULL && ctx->notification_repaint);
}

/**
 * @brief Takes the pending repaint, so the screen about to draw owns it.
 *
 * @param ctx Active render context.
 * @return What was pending; the flag is cleared.
 */
bool	render_notification_take_repaint(t_render_ctx *ctx)
{
	bool	pending;

	if (ctx == NULL)
		return (false);
	pending = ctx->notification_repaint;
	ctx->notification_repaint = false;
	return (pending);
}

/*
** Every appearance and disappearance of a card passes through here, which is
** why the repaint is flagged here and nowhere else.
**
** A card is a bitmap and so is most of what it covers. Notcurses cannot
** overlap two sprixels, so it wipes the cells of the one underneath - and the
** screens cache their panels by signature, so once the card expires nothing
** considers those panels stale and they never come back. Changing the volume
** left a room with a background, a title, a footer and no panels.
*/
static void	refresh_notifications(t_render_ctx *ctx, uint64_t now_ms)
{
	int		index;
	int		opacity;
	int		cols;
	int		y;
	int		x;
	bool	content_embedded;

	ctx->notification_repaint = true;
	destroy_notification_planes(ctx);
	cols = art_columns(ctx);
	index = 0;
	while (index < ctx->notifications.count)
	{
		opacity = ui_notification_opacity(&ctx->notifications.items[index],
				now_ms);
		if (opacity > 0 && notification_position(ctx, index, cols, &y, &x))
		{
			ctx->notification_art_planes[index] = create_art_plane(ctx,
					y, x, &ctx->notifications.items[index], opacity,
					&content_embedded);
			/*
			 * The cell card is a fixed width, so right-align it inside the
			 * band the art plane reserved rather than letting it start off
			 * screen when the art plane is the wider of the two.
			 */
			if (ctx->notification_art_planes[index] == NULL
				|| !content_embedded)
				ctx->notification_planes[index] = create_text_plane(ctx, y,
						x + cols - NOTIFICATION_COLS > 0
						? x + cols - NOTIFICATION_COLS : 0,
						&ctx->notifications.items[index], opacity,
						ctx->notification_art_planes[index] != NULL);
		}
		index++;
	}
	raise_planes(ctx);
}

/**
 * @brief Sizes the card plane so the artwork keeps its authored aspect.
 *
 * Cells are taller than they are wide, so the column count that matches a
 * 640x138 card depends on the terminal's cell geometry rather than on any
 * fixed width. Tiers without art keep the terminal-native card width.
 *
 * @param ctx Active render context.
 * @return Plane width in columns.
 */
static int	art_columns(const t_render_ctx *ctx)
{
	unsigned	rows;
	unsigned	columns;
	int			cols;

	if (!tetrisu_pixel_policy_supports_notification_art(ctx->pixels)
		|| ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0)
		return (NOTIFICATION_COLS);
	cols = (NOTIFICATION_ROWS * ctx->cell_px_y * NOTIFICATION_ART_WIDTH
			+ NOTIFICATION_ART_HEIGHT * ctx->cell_px_x / 2)
		/ (NOTIFICATION_ART_HEIGHT * ctx->cell_px_x);
	/*
	 * A terminal too narrow for the true aspect gets a squatter card rather
	 * than no card: the notification is the only channel some of these
	 * messages have.
	 */
	ncplane_dim_yx(ctx->std, &rows, &columns);
	if (cols > (int)columns - NOTIFICATION_RIGHT)
		cols = (int)columns - NOTIFICATION_RIGHT;
	if (cols < NOTIFICATION_MIN_COLS)
		cols = NOTIFICATION_MIN_COLS;
	return (cols);
}

static bool	notification_position(t_render_ctx *ctx, int index, int cols,
	int *y, int *x)
{
	unsigned	rows;
	unsigned	columns;

	ncplane_dim_yx(ctx->std, &rows, &columns);
	if (rows < NOTIFICATION_TOP + NOTIFICATION_ROWS
		|| (int)columns < cols + NOTIFICATION_RIGHT)
		return (false);
	*y = NOTIFICATION_TOP
		+ index * (NOTIFICATION_ROWS + NOTIFICATION_GAP);
	*x = (int)columns - cols - NOTIFICATION_RIGHT;
	return (*y + NOTIFICATION_ROWS <= (int)rows);
}

/**
 * @brief Composes and blits the authored card for one notification.
 *
 * The card is assembled in an RGBA canvas rather than mutated as a visual so
 * the backdrop, the artwork and the lettering all share one blend path, and
 * so the stationary tier can flatten the result into opaque pixels.
 */
static struct ncplane	*create_art_plane(t_render_ctx *ctx, int y, int x,
	const t_ui_notification *notification, int opacity,
	bool *content_embedded)
{
	uint32_t		*canvas;
	struct ncplane	*plane;
	int				cols;
	int				width;
	int				height;

	*content_embedded = false;
	if (!tetrisu_pixel_policy_supports_notification_art(ctx->pixels)
		|| ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0)
		return (NULL);
	cols = art_columns(ctx);
	width = cols * ctx->cell_px_x;
	height = NOTIFICATION_ROWS * ctx->cell_px_y;
	canvas = build_canvas(ctx, y, x, notification, opacity, width, height);
	if (canvas == NULL)
		return (NULL);
	*content_embedded = true;
	plane = blit_canvas(ctx, canvas, width, height, y, x, cols);
	free(canvas);
	if (plane == NULL)
		*content_embedded = false;
	return (plane);
}

static uint32_t	*build_canvas(t_render_ctx *ctx, int y, int x,
	const t_ui_notification *notification, int opacity, int width, int height)
{
	uint32_t		*canvas;
	struct ncvisual	*visual;

	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height / sizeof(*canvas))
		return (NULL);
	visual = ncvisual_from_file(notification_asset_path(notification));
	if (visual == NULL)
		return (NULL);
	if (ncvisual_resize_noninterpolative(visual, height, width) != 0)
	{
		ncvisual_destroy(visual);
		return (NULL);
	}
	canvas = calloc((size_t)width * (size_t)height, sizeof(*canvas));
	if (canvas == NULL)
	{
		ncvisual_destroy(visual);
		return (NULL);
	}
	fill_backdrop(ctx, canvas, width, height, y, x);
	if (ctx->pixels == TETRISU_PIXELS_STATIONARY)
		opacity = 255;
	compose_card(canvas, width, height, visual, opacity);
	ncvisual_destroy(visual);
	draw_card_text(ctx, canvas, width, height, notification);
	return (canvas);
}

/**
 * @brief Seeds the canvas with whatever the card is about to cover.
 *
 * Stationary bitmap protocols clear the cells an overlay occupies, so a
 * transparent card pixel would expose the terminal background instead of the
 * screen behind it. Painting the backdrop into the card first is what removes
 * that rectangle. Tiers that really composite need none of this.
 */
static void	fill_backdrop(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, int y, int x)
{
	const uint32_t	*source;
	int				source_width;
	int				source_height;
	int				origin_y;
	int				origin_x;
	int				row;

	if (!tetrisu_pixel_policy_notification_needs_reemit(ctx->pixels))
		return ;
	source = render_backdrop_pixels(ctx, &source_width, &source_height);
	if (source == NULL)
		return ;
	origin_y = (y - ctx->bg_row) * ctx->cell_px_y;
	origin_x = (x - ctx->bg_col) * ctx->cell_px_x;
	row = 0;
	while (row < height)
	{
		if (origin_y + row >= 0 && origin_y + row < source_height)
			copy_backdrop_row(canvas + (size_t)row * width, width, source,
				source_width, origin_y + row, origin_x);
		row++;
	}
}

static void	copy_backdrop_row(uint32_t *dest, int width,
	const uint32_t *source, int source_width, int source_y, int source_x)
{
	int	column;

	column = 0;
	while (column < width)
	{
		if (source_x + column >= 0 && source_x + column < source_width)
			dest[column] = source[(size_t)source_y * source_width
				+ source_x + column];
		column++;
	}
}

static void	compose_card(uint32_t *canvas, int width, int height,
	struct ncvisual *visual, int opacity)
{
	uint32_t	source;
	int			y;
	int			x;

	y = 0;
	while (y < height)
	{
		x = 0;
		while (x < width)
		{
			if (ncvisual_at_yx(visual, (unsigned)y, (unsigned)x, &source) >= 0)
				blend_over(&canvas[(size_t)y * width + x], ncpixel_r(source),
					ncpixel_g(source), ncpixel_b(source),
					ncpixel_a(source) * (unsigned)opacity / 255u);
			x++;
		}
		y++;
	}
}

/**
 * @brief Source-over blend of one colour into a possibly transparent pixel.
 */
static void	blend_over(uint32_t *dest, unsigned red, unsigned green,
	unsigned blue, unsigned alpha)
{
	unsigned	old_alpha;
	unsigned	out_alpha;

	if (alpha == 0)
		return ;
	old_alpha = ncpixel_a(*dest);
	if (alpha >= 255u || old_alpha == 0)
	{
		*dest = ncpixel(red, green, blue);
		ncpixel_set_a(dest, alpha);
		return ;
	}
	out_alpha = alpha + old_alpha * (255u - alpha) / 255u;
	if (out_alpha == 0)
		return ;
	*dest = ncpixel(
			(red * alpha + ncpixel_r(*dest) * old_alpha
				* (255u - alpha) / 255u) / out_alpha,
			(green * alpha + ncpixel_g(*dest) * old_alpha
				* (255u - alpha) / 255u) / out_alpha,
			(blue * alpha + ncpixel_b(*dest) * old_alpha
				* (255u - alpha) / 255u) / out_alpha);
	ncpixel_set_a(dest, out_alpha);
}

static void	fill_rect(uint32_t *canvas, int width, int height,
	const t_notif_box *rect, t_color tint, unsigned alpha)
{
	int	y;
	int	x;

	y = rect->y;
	while (y < rect->y + rect->height)
	{
		x = rect->x;
		while (x < rect->x + rect->width)
		{
			if (y >= 0 && y < height && x >= 0 && x < width)
				blend_over(&canvas[(size_t)y * width + x], tint.r, tint.g,
					tint.b, alpha);
			x++;
		}
		y++;
	}
}

static struct ncplane	*blit_canvas(t_render_ctx *ctx, const uint32_t *canvas,
	int width, int height, int y, int x, int cols)
{
	ncplane_options			options;
	struct ncvisual_options	vopts;
	struct ncvisual			*ncv;
	struct ncplane			*plane;

	memset(&options, 0, sizeof(options));
	options.y = y;
	options.x = x;
	options.rows = NOTIFICATION_ROWS;
	options.cols = (unsigned)cols;
	plane = ncplane_create(ctx->std, &options);
	if (plane == NULL)
		return (NULL);
	set_transparent_base(plane);
	ncv = ncvisual_from_rgba(canvas, height,
			width * (int)sizeof(*canvas), width);
	if (ncv == NULL)
	{
		ncplane_destroy(plane);
		return (NULL);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = plane;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NOINTERPOLATE;
	if (ncvisual_blit(ctx->nc, ncv, &vopts) == NULL)
	{
		ncvisual_destroy(ncv);
		ncplane_destroy(plane);
		return (NULL);
	}
	ncvisual_destroy(ncv);
	return (plane);
}

/**
 * @brief Lazily decodes the shared glyph sheet used by the card lettering.
 *
 * The cards used to carry their own 3x5 pattern table, which could not tell
 * M, N and W apart at card size. Sharing the atlas the rest of the UI already
 * draws with removes both the ambiguity and a second font to maintain.
 */
static const t_pixel_asset	*notification_font(t_render_ctx *ctx)
{
	if (ctx->notification_font.pixels != NULL)
		return (&ctx->notification_font);
	if (!render_font_mask_load(&ctx->notification_font))
		return (NULL);
	return (&ctx->notification_font);
}

/**
 * @brief Maps the measured per-mille text box onto the drawn card.
 */
static t_notif_box	text_box(const t_ui_notification *notification,
	int width, int height)
{
	t_notif_box	box;

	if (notification->kind == UI_NOTIFICATION_OWNERSHIP)
	{
		box.x = width * OWNERSHIP_BOX_X / 1000;
		box.width = width * OWNERSHIP_BOX_WIDTH / 1000;
		box.y = height * OWNERSHIP_BOX_Y / 1000;
		box.height = height * OWNERSHIP_BOX_HEIGHT / 1000;
		return (box);
	}
	box.x = width * VOLUME_BOX_X / 1000;
	box.width = width * VOLUME_BOX_WIDTH / 1000;
	box.y = height * VOLUME_BOX_Y / 1000;
	box.height = height * VOLUME_BOX_HEIGHT / 1000;
	return (box);
}

/**
 * @brief Returns one of the two stacked bands inside a card's text box.
 */
static t_notif_box	band(const t_notif_box *box, int index)
{
	t_notif_box	result;

	result.x = box->x;
	result.width = box->width;
	result.height = box->height * NOTIFICATION_BAND / 100;
	result.y = box->y;
	if (index > 0)
		result.y += box->height - result.height;
	return (result);
}

static void	draw_card_text(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, const t_ui_notification *notification)
{
	const t_pixel_asset	*font;
	t_notif_box			box;

	font = notification_font(ctx);
	if (font == NULL)
		return ;
	box = text_box(notification, width, height);
	if (box.width <= 0 || box.height <= 0)
		return ;
	if (notification->kind == UI_NOTIFICATION_OWNERSHIP)
		draw_ownership_text(canvas, width, height, font, notification, &box);
	else
		draw_volume_text(canvas, width, height, font, notification, &box);
}

/**
 * @brief Fits a line inside a band and returns its cap-row draw position.
 *
 * A band has to hold the whole ink height, not just the cap height, or the
 * ascender dots ride up onto the plaque's border. The glyph size therefore
 * comes from the ink ratio, and the cap row is pushed down by exactly the
 * rows that sit above it.
 *
 * @param band_box Band the line must stay inside.
 * @param text Line being placed.
 * @param limit Widest run allowed, normally the band width.
 * @return Box whose y is the cap row and whose height is the glyph size.
 */
static t_notif_box	place_line(const t_notif_box *band_box, const char *text,
	int limit)
{
	t_notif_box	at;

	at = *band_box;
	at.height = at.height * FONT_INK_HEIGHT
		/ (FONT_INK_BOTTOM - FONT_INK_TOP);
	at.height = fit_glyph(text, limit, at.height);
	at.y = cap_row(band_box->y, at.height);
	return (at);
}

/**
 * @brief Returns the cap row that keeps a glyph's ascenders inside the band.
 */
static int	cap_row(int band_y, int glyph_size)
{
	return (band_y + (FONT_INK_Y - FONT_INK_TOP) * glyph_size
		/ FONT_INK_HEIGHT);
}

/**
 * @brief Centres the locked-item title and its Marketplace hint in the box.
 */
static void	draw_ownership_text(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, const t_ui_notification *notification,
	const t_notif_box *box)
{
	t_notif_box	band_box;
	t_notif_box	at;

	band_box = band(box, 0);
	at = place_line(&band_box, notification->title, box->width);
	at.x += (box->width - text_pixels(notification->title, at.height)) / 2;
	draw_atlas_text(canvas, width, height, font, notification->title, &at,
		g_notification_gold);
	band_box = band(box, 1);
	at = place_line(&band_box, notification->message, box->width);
	at.x += (box->width - text_pixels(notification->message, at.height)) / 2;
	draw_atlas_text(canvas, width, height, font, notification->message, &at,
		g_notification_cream);
}

/**
 * @brief Draws the volume title with a right-aligned percentage and its bar.
 */
static void	draw_volume_text(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, const t_ui_notification *notification,
	const t_notif_box *box)
{
	char		percent[8];
	t_notif_box	band_box;
	t_notif_box	at;

	snprintf(percent, sizeof(percent), "%d%%", notification->percent);
	band_box = band(box, 0);
	at = place_line(&band_box, notification->title, box->width / 2);
	/* Both halves of the line share the smaller size so they sit level. */
	at.height = fit_glyph(percent, box->width / 2, at.height);
	at.y = cap_row(band_box.y, at.height);
	draw_atlas_text(canvas, width, height, font, notification->title, &at,
		g_notification_gold);
	at.x = box->x + box->width - text_pixels(percent, at.height);
	draw_atlas_text(canvas, width, height, font, percent, &at,
		g_notification_cream);
	band_box = band(box, 1);
	draw_volume_bar(canvas, width, height, &band_box, notification->percent);
}

static void	draw_volume_bar(uint32_t *canvas, int width, int height,
	const t_notif_box *band_box, int percent)
{
	t_notif_box	segment;
	int			gap;
	int			step;
	int			filled_steps;
	int			index;

	gap = band_box->width / 100;
	if (gap < 1)
		gap = 1;
	step = (band_box->width - gap * (UI_NOTIFICATION_BAR_STEPS - 1))
		/ UI_NOTIFICATION_BAR_STEPS;
	if (step < 1)
		return ;
	filled_steps = (percent * UI_NOTIFICATION_BAR_STEPS + 50) / 100;
	segment.height = band_box->height * 70 / 100;
	if (segment.height < 1)
		segment.height = 1;
	segment.y = band_box->y + (band_box->height - segment.height) / 2;
	segment.width = step;
	index = 0;
	while (index < UI_NOTIFICATION_BAR_STEPS)
	{
		segment.x = band_box->x + index * (step + gap);
		fill_rect(canvas, width, height, &segment,
			index < filled_steps ? g_notification_filled
			: g_notification_empty, 255u);
		index++;
	}
}

static void	draw_atlas_text(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, const char *text, const t_notif_box *at,
	t_color tint)
{
	unsigned	codepoint;
	int			index;

	index = 0;
	while (text[index] != '\0')
	{
		codepoint = (unsigned char)text[index];
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		draw_atlas_glyph(canvas, width, height, font, (int)codepoint - 32,
			at->x + index * (at->height + glyph_spacing(at->height)),
			at->y, at->height, tint);
		index++;
	}
}

/**
 * @brief Draws one atlas glyph, ascender dots and descender tails included.
 */
static void	draw_atlas_glyph(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, int glyph, int x, int y, int glyph_size,
	t_color tint)
{
	t_notif_box	cell;
	int			source_y;
	int			source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < FONT_GLYPH_WIDTH)
		{
			cell.x = x + source_x * glyph_size / FONT_GLYPH_WIDTH;
			cell.width = (source_x + 1) * glyph_size / FONT_GLYPH_WIDTH
				- source_x * glyph_size / FONT_GLYPH_WIDTH;
			cell.y = y + ink_span(source_y - FONT_INK_Y, glyph_size);
			cell.height = ink_span(source_y + 1 - FONT_INK_Y, glyph_size)
				- ink_span(source_y - FONT_INK_Y, glyph_size);
			draw_atlas_texel(canvas, width, height,
				ncpixel_a(font->pixels[(size_t)((glyph / FONT_COLUMNS)
						* FONT_GLYPH_HEIGHT + source_y) * font->width
					+ (glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH + source_x]),
				&cell, tint);
			source_x++;
		}
		source_y++;
	}
}

static void	draw_atlas_texel(uint32_t *canvas, int width, int height,
	unsigned alpha, const t_notif_box *cell, t_color tint)
{
	if (alpha == 0 || cell->width <= 0 || cell->height <= 0)
		return ;
	fill_rect(canvas, width, height, cell, tint, alpha);
}

/**
 * @brief Scales a signed offset in atlas rows into destination pixels.
 */
static int	ink_span(int units, int glyph_size)
{
	if (units >= 0)
		return (units * glyph_size / FONT_INK_HEIGHT);
	return (-((-units * glyph_size + FONT_INK_HEIGHT - 1) / FONT_INK_HEIGHT));
}

static int	glyph_spacing(int glyph_size)
{
	if (glyph_size / 4 < 1)
		return (1);
	return (glyph_size / 4);
}

static int	text_pixels(const char *text, int glyph_size)
{
	int	length;

	length = (int)strlen(text);
	if (length <= 0)
		return (0);
	return (length * (glyph_size + glyph_spacing(glyph_size))
		- glyph_spacing(glyph_size));
}

/**
 * @brief Shrinks a glyph size until its run fits inside the plaque interior.
 *
 * Nothing clips the lettering afterwards, so a size that does not fit would
 * be drawn straight over the card's border and out past the plane edge.
 */
static int	fit_glyph(const char *text, int box_width, int glyph_size)
{
	while (glyph_size > 4 && text_pixels(text, glyph_size) > box_width)
		glyph_size--;
	return (glyph_size);
}

static struct ncplane	*create_text_plane(t_render_ctx *ctx, int y, int x,
	const t_ui_notification *notification, int opacity, bool has_art)
{
	ncplane_options	opts;
	struct ncplane	*plane;

	memset(&opts, 0, sizeof(opts));
	opts.y = y;
	opts.x = x;
	opts.rows = NOTIFICATION_ROWS;
	opts.cols = NOTIFICATION_COLS;
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
		return (NULL);
	if (has_art)
		set_transparent_base(plane);
	else
		draw_fallback_frame(plane, notification, opacity);
	draw_notification(plane, notification, opacity, has_art ? 1 : 0);
	return (plane);
}

static void	draw_notification(struct ncplane *plane,
	const t_ui_notification *notification, int opacity, int row_offset)
{
	char	percent[8];
	int		percent_x;

	(void)ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
	set_color(plane, 255, 210, 118, opacity);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, 1 + row_offset, NOTIFICATION_TEXT_LEFT,
		notification->title);
	if (notification->kind == UI_NOTIFICATION_OWNERSHIP)
	{
		set_color(plane, 255, 244, 250, opacity);
		(void)ncplane_putstr_yx(plane, 2 + row_offset,
			NOTIFICATION_TEXT_LEFT, notification->message);
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
		return ;
	}
	snprintf(percent, sizeof(percent), "%d%%", notification->percent);
	percent_x = NOTIFICATION_COLS - (int)strlen(percent) - 3;
	set_color(plane, 255, 244, 250, opacity);
	(void)ncplane_putstr_yx(plane, 1 + row_offset, percent_x, percent);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	draw_bar(plane, notification->percent, opacity, 2 + row_offset);
}

static void	draw_bar(struct ncplane *plane, int percent, int opacity, int row)
{
	int	filled_steps;
	int	bar_x;
	int	index;

	filled_steps = (percent * UI_NOTIFICATION_BAR_STEPS + 50) / 100;
	bar_x = NOTIFICATION_TEXT_LEFT
		+ (NOTIFICATION_COLS - NOTIFICATION_TEXT_LEFT
			- UI_NOTIFICATION_BAR_STEPS - 2) / 2;
	set_color(plane, 220, 175, 255, opacity);
	(void)ncplane_putstr_yx(plane, row, bar_x, "[");
	(void)ncplane_putstr_yx(plane, row,
		bar_x + UI_NOTIFICATION_BAR_STEPS + 1, "]");
	index = 0;
	while (index < UI_NOTIFICATION_BAR_STEPS)
	{
		if (index < filled_steps)
		{
			set_color(plane, 255, 94, 167, opacity);
			(void)ncplane_putstr_yx(plane, row, bar_x + 1 + index, "◆");
		}
		else
		{
			set_color(plane, 116, 76, 142, opacity);
			(void)ncplane_putstr_yx(plane, row, bar_x + 1 + index, "◇");
		}
		index++;
	}
}

static void	draw_fallback_frame(struct ncplane *plane,
	const t_ui_notification *notification, int opacity)
{
	uint64_t	channels;
	int			x;

	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels,
		fade_component(255, opacity), fade_component(115, opacity),
		fade_component(179, opacity));
	(void)ncchannels_set_bg_rgb8(&channels,
		fade_component(31, opacity), fade_component(17, opacity),
		fade_component(39, opacity));
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	set_color(plane, 255, 115, 179, opacity);
	x = 1;
	while (x < NOTIFICATION_COLS - 1)
	{
		(void)ncplane_putstr_yx(plane, 0, x, "─");
		(void)ncplane_putstr_yx(plane, NOTIFICATION_ROWS - 1, x, "─");
		x++;
	}
	(void)ncplane_putstr_yx(plane, 0, 0, "╭");
	(void)ncplane_putstr_yx(plane, 0, NOTIFICATION_COLS - 1, "╮");
	(void)ncplane_putstr_yx(plane, NOTIFICATION_ROWS - 1, 0, "╰");
	(void)ncplane_putstr_yx(plane, NOTIFICATION_ROWS - 1,
		NOTIFICATION_COLS - 1, "╯");
	if (notification != NULL
		&& notification->kind == UI_NOTIFICATION_OWNERSHIP)
	{
		(void)ncplane_putstr_yx(plane, 1, 0, "│  .---.   ");
		(void)ncplane_putstr_yx(plane, 2, 0, "│  |LOCK|  ");
	}
	else
	{
		(void)ncplane_putstr_yx(plane, 1, 0, "│ /\\_/\\");
		(void)ncplane_putstr_yx(plane, 2, 0, "│( ^.^ )");
	}
	(void)ncplane_putstr_yx(plane, 1, NOTIFICATION_COLS - 1, "│");
	(void)ncplane_putstr_yx(plane, 2, NOTIFICATION_COLS - 1, "│");
}

static const char	*notification_asset_path(
	const t_ui_notification *notification)
{
	if (notification != NULL
		&& notification->kind == UI_NOTIFICATION_OWNERSHIP)
		return (OWNERSHIP_NOTIFICATION_PATH);
	return (VOLUME_NOTIFICATION_PATH);
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

static void	set_color(struct ncplane *plane, unsigned r, unsigned g,
	unsigned b, int opacity)
{
	(void)ncplane_set_fg_rgb8(plane, fade_component(r, opacity),
		fade_component(g, opacity), fade_component(b, opacity));
}

static unsigned	fade_component(unsigned component, int opacity)
{
	if (opacity < 0)
		opacity = 0;
	if (opacity > 255)
		opacity = 255;
	return ((component * (unsigned)opacity) / 255u);
}

static void	destroy_notification_planes(t_render_ctx *ctx)
{
	int	index;

	index = 0;
	while (index < UI_NOTIFICATION_STACK_MAX)
	{
		if (ctx->notification_planes[index] != NULL)
		{
			ncplane_destroy(ctx->notification_planes[index]);
			ctx->notification_planes[index] = NULL;
		}
		if (ctx->notification_art_planes[index] != NULL)
		{
			ncplane_destroy(ctx->notification_art_planes[index]);
			ctx->notification_art_planes[index] = NULL;
		}
		index++;
	}
}
