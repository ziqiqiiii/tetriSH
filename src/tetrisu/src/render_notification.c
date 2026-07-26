#include "tetrisu.h"

# define NOTIFICATION_COLS		38
# define NOTIFICATION_ROWS		4
# define NOTIFICATION_TOP		1
# define NOTIFICATION_RIGHT		1
# define NOTIFICATION_GAP		1
# define NOTIFICATION_TEXT_LEFT	13

static void	refresh_notifications(render_ctx_t *ctx, uint64_t now_ms);
static bool	notification_position(render_ctx_t *ctx, int index,
					int *y, int *x);
static struct ncplane	*create_art_plane(render_ctx_t *ctx, int y, int x,
					const ui_notification_t *notification, int opacity,
					bool *content_embedded);
static struct ncplane	*create_text_plane(render_ctx_t *ctx, int y, int x,
					const ui_notification_t *notification, int opacity,
					bool has_art);
static void	draw_visual_content(struct ncvisual *visual,
					const ui_notification_t *notification,
					int pixel_rows, int pixel_cols);
static void	draw_pixel_text(struct ncvisual *visual, int y, int x,
					const char *text, int scale, uint32_t color);
static void	draw_pixel_glyph(struct ncvisual *visual, int y, int x,
					char glyph, int scale, uint32_t color);
static uint16_t	glyph_pattern(char glyph);
static void	draw_pixel_bar(struct ncvisual *visual, int y, int x,
					int percent, int scale);
static void	draw_pixel_rect(struct ncvisual *visual, int y, int x,
					int height, int width, uint32_t color);
static void	fade_visual(struct ncvisual *visual, int opacity);
static void	draw_notification(struct ncplane *plane,
					const ui_notification_t *notification, int opacity,
					int row_offset);
static void	draw_bar(struct ncplane *plane, int percent, int opacity,
					int row);
static void	draw_fallback_frame(struct ncplane *plane, int opacity);
static void	set_transparent_base(struct ncplane *plane);
static void	set_color(struct ncplane *plane, unsigned r, unsigned g,
					unsigned b, int opacity);
static unsigned	fade_component(unsigned component, int opacity);
static void	destroy_notification_planes(render_ctx_t *ctx);

/**
 * @brief Shows or refreshes the global music-volume notification.
 *
 * The notification uses authored pixel art where Kitty graphics are safe and
 * the same PNG through a dense cell blitter in compatibility mode. Dynamic
 * terminal text keeps the percentage and bar crisp on both paths.
 *
 * @param ctx Active render context.
 * @param volume Mixer volume in the application range.
 */
void	render_notification_show_volume(render_ctx_t *ctx, int volume)
{
	uint64_t	now_ms;

	if (ctx == NULL || ctx->nc == NULL)
		return ;
	now_ms = ui_notification_now_ms();
	ui_notification_show(&ctx->notifications, "MUSIC",
		ui_notification_volume_percent(volume), now_ms);
	refresh_notifications(ctx, now_ms);
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Advances notification fade and expiry state.
 *
 * @param ctx Active render context.
 */
void	render_notification_tick(render_ctx_t *ctx)
{
	uint64_t	now_ms;

	if (ctx == NULL || ctx->nc == NULL || ctx->notifications.count == 0)
		return ;
	now_ms = ui_notification_now_ms();
	(void)ui_notification_update(&ctx->notifications, now_ms);
	refresh_notifications(ctx, now_ms);
	(void)notcurses_render(ctx->nc);
}

/**
 * @brief Returns the delay until the global overlay needs another frame.
 *
 * @param ctx Active render context.
 * @return Delay in milliseconds, or -1 with no visible notifications.
 */
int	render_notification_next_wake_ms(const render_ctx_t *ctx)
{
	if (ctx == NULL)
		return (-1);
	return (ui_notification_next_wake_ms(&ctx->notifications,
			ui_notification_now_ms()));
}

/**
 * @brief Rebuilds active notifications after a terminal resize.
 *
 * @param ctx Active render context.
 */
void	render_notification_reflow(render_ctx_t *ctx)
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
void	render_notification_raise(render_ctx_t *ctx)
{
	int	index;

	if (ctx == NULL)
		return ;
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

/**
 * @brief Releases every global notification plane and state entry.
 *
 * @param ctx Render context being torn down.
 */
void	render_notification_destroy(render_ctx_t *ctx)
{
	if (ctx == NULL)
		return ;
	destroy_notification_planes(ctx);
	ui_notification_stack_init(&ctx->notifications);
}

static void	refresh_notifications(render_ctx_t *ctx, uint64_t now_ms)
{
	int		index;
	int		opacity;
	int		y;
	int		x;
	bool	has_art;
	bool	content_embedded;

	destroy_notification_planes(ctx);
	index = 0;
	while (index < ctx->notifications.count)
	{
		opacity = ui_notification_opacity(&ctx->notifications.items[index],
				now_ms);
		if (opacity > 0 && notification_position(ctx, index, &y, &x))
		{
			ctx->notification_art_planes[index] = create_art_plane(ctx,
					y, x, &ctx->notifications.items[index], opacity,
					&content_embedded);
			has_art = ctx->notification_art_planes[index] != NULL;
			if (!has_art || !content_embedded)
				ctx->notification_planes[index] = create_text_plane(ctx,
						y, x, &ctx->notifications.items[index],
						opacity, has_art);
		}
		index++;
	}
	render_notification_raise(ctx);
}

static bool	notification_position(render_ctx_t *ctx, int index,
	int *y, int *x)
{
	unsigned	rows;
	unsigned	cols;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < NOTIFICATION_TOP + NOTIFICATION_ROWS
		|| cols < NOTIFICATION_COLS + NOTIFICATION_RIGHT)
		return (false);
	*y = NOTIFICATION_TOP
		+ index * (NOTIFICATION_ROWS + NOTIFICATION_GAP);
	*x = (int)cols - NOTIFICATION_COLS - NOTIFICATION_RIGHT;
	return (*y + NOTIFICATION_ROWS <= (int)rows);
}

static struct ncplane	*create_art_plane(render_ctx_t *ctx, int y, int x,
	const ui_notification_t *notification, int opacity,
	bool *content_embedded)
{
	struct ncvisual			*visual;
	struct ncvisual_options	opts;
	struct ncplane			*plane;
	ncblitter_e				blitter;
	int						pixel_rows;
	int						pixel_cols;

	visual = ncvisual_from_file(VOLUME_NOTIFICATION_PATH);
	*content_embedded = false;
	if (visual == NULL)
		return (NULL);
	blitter = NCBLIT_4x2;
	pixel_rows = NOTIFICATION_ROWS * 4;
	pixel_cols = NOTIFICATION_COLS * 2;
	if (!render_compatibility_mode(ctx)
		&& render_pixel_planes_reliable(ctx)
		&& ctx->cell_px_y > 0 && ctx->cell_px_x > 0)
	{
		blitter = NCBLIT_PIXEL;
		*content_embedded = true;
		pixel_rows = NOTIFICATION_ROWS * ctx->cell_px_y;
		pixel_cols = NOTIFICATION_COLS * ctx->cell_px_x;
	}
	if (ncvisual_resize_noninterpolative(visual,
			pixel_rows, pixel_cols) != 0)
	{
		ncvisual_destroy(visual);
		return (NULL);
	}
	if (*content_embedded)
		draw_visual_content(visual, notification, pixel_rows, pixel_cols);
	if (opacity < 255)
		fade_visual(visual, opacity);
	memset(&opts, 0, sizeof(opts));
	opts.n = ctx->std;
	opts.scaling = NCSCALE_NONE;
	opts.y = y;
	opts.x = x;
	opts.blitter = blitter;
	opts.flags = NCVISUAL_OPTION_CHILDPLANE
		| NCVISUAL_OPTION_NOINTERPOLATE;
	plane = ncvisual_blit(ctx->nc, visual, &opts);
	ncvisual_destroy(visual);
	return (plane);
}

static void	draw_visual_content(struct ncvisual *visual,
	const ui_notification_t *notification, int pixel_rows, int pixel_cols)
{
	char		percent[8];
	int			scale;
	int			title_x;
	int			percent_x;
	int			text_y;
	int			bar_y;
	int			text_width;
	uint32_t	gold;
	uint32_t	cream;

	scale = pixel_rows / 20;
	if (scale < 1)
		scale = 1;
	title_x = pixel_cols * 30 / 100;
	text_y = pixel_rows * 25 / 100;
	bar_y = pixel_rows * 66 / 100;
	gold = ncpixel(255, 210, 118);
	ncpixel_set_a(&gold, 255);
	cream = ncpixel(255, 244, 250);
	ncpixel_set_a(&cream, 255);
	draw_pixel_text(visual, text_y, title_x, notification->title,
		scale, gold);
	snprintf(percent, sizeof(percent), "%d%%", notification->percent);
	text_width = ((int)strlen(percent) * 4 - 1) * scale;
	percent_x = pixel_cols * 92 / 100 - text_width;
	draw_pixel_text(visual, text_y, percent_x, percent, scale, cream);
	draw_pixel_bar(visual, bar_y, title_x, notification->percent, scale);
}

static void	draw_pixel_text(struct ncvisual *visual, int y, int x,
	const char *text, int scale, uint32_t color)
{
	int	index;

	index = 0;
	while (text[index] != '\0')
	{
		draw_pixel_glyph(visual, y, x + index * 4 * scale,
			text[index], scale, color);
		index++;
	}
}

static void	draw_pixel_glyph(struct ncvisual *visual, int y, int x,
	char glyph, int scale, uint32_t color)
{
	uint16_t	pattern;
	int			row;
	int			col;
	int			bit;

	pattern = glyph_pattern(glyph);
	row = 0;
	while (row < 5)
	{
		col = 0;
		while (col < 3)
		{
			bit = 14 - (row * 3 + col);
			if ((pattern & (1u << bit)) != 0)
				draw_pixel_rect(visual, y + row * scale,
					x + col * scale, scale, scale, color);
			col++;
		}
		row++;
	}
}

static uint16_t	glyph_pattern(char glyph)
{
	static const uint16_t	digits[10] = {
		0x7B6F, 0x2492, 0x73E7, 0x73CF, 0x5BC9,
		0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF
	};

	if (glyph >= '0' && glyph <= '9')
		return (digits[glyph - '0']);
	if (glyph == 'M')
		return (0x5FED);
	if (glyph == 'U')
		return (0x5B6F);
	if (glyph == 'S')
		return (0x79CF);
	if (glyph == 'I')
		return (0x7497);
	if (glyph == 'C')
		return (0x7927);
	if (glyph == '%')
		return (0x5225);
	return (0);
}

static void	draw_pixel_bar(struct ncvisual *visual, int y, int x,
	int percent, int scale)
{
	uint32_t	filled;
	uint32_t	empty;
	int			filled_steps;
	int			segment_width;
	int			gap;
	int			bar_width;
	int			index;

	filled = ncpixel(255, 94, 167);
	ncpixel_set_a(&filled, 255);
	empty = ncpixel(116, 76, 142);
	ncpixel_set_a(&empty, 255);
	filled_steps = (percent * UI_NOTIFICATION_BAR_STEPS + 50) / 100;
	segment_width = scale * 2;
	if (segment_width < 1)
		segment_width = 1;
	gap = scale / 2;
	if (gap < 1)
		gap = 1;
	bar_width = UI_NOTIFICATION_BAR_STEPS * segment_width
		+ (UI_NOTIFICATION_BAR_STEPS - 1) * gap;
	x += (NOTIFICATION_COLS * scale * 2 * 7 / 10 - bar_width) / 2;
	index = 0;
	while (index < UI_NOTIFICATION_BAR_STEPS)
	{
		draw_pixel_rect(visual, y, x + index * (segment_width + gap),
			scale * 2, segment_width,
			index < filled_steps ? filled : empty);
		index++;
	}
}

static void	draw_pixel_rect(struct ncvisual *visual, int y, int x,
	int height, int width, uint32_t color)
{
	ncvgeom	geom;
	int		row;
	int		col;

	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, visual, NULL, &geom) < 0)
		return ;
	row = 0;
	while (row < height && y + row < (int)geom.pixy)
	{
		col = 0;
		while (col < width && x + col < (int)geom.pixx)
		{
			if (y + row >= 0 && x + col >= 0)
				(void)ncvisual_set_yx(visual,
					(unsigned)(y + row), (unsigned)(x + col), color);
			col++;
		}
		row++;
	}
}

static struct ncplane	*create_text_plane(render_ctx_t *ctx, int y, int x,
	const ui_notification_t *notification, int opacity, bool has_art)
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
		draw_fallback_frame(plane, opacity);
	draw_notification(plane, notification, opacity, has_art ? 1 : 0);
	return (plane);
}

static void	fade_visual(struct ncvisual *visual, int opacity)
{
	ncvgeom	geom;
	uint32_t	pixel;
	unsigned	y;
	unsigned	x;
	unsigned	alpha;

	memset(&geom, 0, sizeof(geom));
	if (ncvisual_geom(NULL, visual, NULL, &geom) < 0)
		return ;
	y = 0;
	while (y < geom.pixy)
	{
		x = 0;
		while (x < geom.pixx)
		{
			if (ncvisual_at_yx(visual, y, x, &pixel) == 0)
			{
				alpha = ncpixel_a(pixel);
				ncpixel_set_a(&pixel, alpha * (unsigned)opacity / 255u);
				(void)ncvisual_set_yx(visual, y, x, pixel);
			}
			x++;
		}
		y++;
	}
}

static void	draw_notification(struct ncplane *plane,
	const ui_notification_t *notification, int opacity, int row_offset)
{
	char	percent[8];
	int		percent_x;

	(void)ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
	set_color(plane, 255, 210, 118, opacity);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, 1 + row_offset, NOTIFICATION_TEXT_LEFT,
		notification->title);
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

static void	draw_fallback_frame(struct ncplane *plane, int opacity)
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
	(void)ncplane_putstr_yx(plane, 1, 0, "│ /\\_/\\");
	(void)ncplane_putstr_yx(plane, 2, 0, "│( ^.^ )");
	(void)ncplane_putstr_yx(plane, 1, NOTIFICATION_COLS - 1, "│");
	(void)ncplane_putstr_yx(plane, 2, NOTIFICATION_COLS - 1, "│");
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

static void	destroy_notification_planes(render_ctx_t *ctx)
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
