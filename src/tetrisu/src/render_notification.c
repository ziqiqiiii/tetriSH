#include "tetrisu.h"

# define NOTIFICATION_WIDTH		22
# define NOTIFICATION_ROWS		2
# define NOTIFICATION_TOP		1
# define NOTIFICATION_RIGHT		1
# define NOTIFICATION_GAP		1

static void	refresh_notifications(render_ctx_t *ctx, uint64_t now_ms);
static struct ncplane	*create_notification_plane(render_ctx_t *ctx,
					const ui_notification_t *notification, int index,
					int opacity);
static void	draw_notification(struct ncplane *plane,
					const ui_notification_t *notification, int opacity);
static void	draw_bar(struct ncplane *plane, int percent, int opacity);
static void	set_color(struct ncplane *plane, unsigned r, unsigned g,
					unsigned b, int opacity);
static unsigned	fade_component(unsigned component, int opacity);
static void	destroy_notification_planes(render_ctx_t *ctx);

/**
 * @brief Shows or refreshes the global music-volume notification.
 *
 * The notification is terminal-native on every renderer and remains available
 * when SDL audio is disabled.
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
 * Callers only need to invoke this at render_notification_next_wake_ms().
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
 * @brief Keeps the global overlay above newly created screen planes.
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
	int	index;
	int	opacity;

	destroy_notification_planes(ctx);
	index = 0;
	while (index < ctx->notifications.count)
	{
		opacity = ui_notification_opacity(&ctx->notifications.items[index],
				now_ms);
		if (opacity > 0)
			ctx->notification_planes[index] = create_notification_plane(ctx,
					&ctx->notifications.items[index], index, opacity);
		index++;
	}
	render_notification_raise(ctx);
}

static struct ncplane	*create_notification_plane(render_ctx_t *ctx,
	const ui_notification_t *notification, int index, int opacity)
{
	ncplane_options	opts;
	struct ncplane	*plane;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < NOTIFICATION_ROWS || cols < NOTIFICATION_WIDTH)
		return (NULL);
	memset(&opts, 0, sizeof(opts));
	opts.y = NOTIFICATION_TOP
		+ index * (NOTIFICATION_ROWS + NOTIFICATION_GAP);
	opts.x = (int)cols - NOTIFICATION_WIDTH - NOTIFICATION_RIGHT;
	if (opts.x < 0 || opts.y + NOTIFICATION_ROWS > (int)rows)
		return (NULL);
	opts.rows = NOTIFICATION_ROWS;
	opts.cols = NOTIFICATION_WIDTH;
	plane = ncplane_create(ctx->std, &opts);
	if (plane == NULL)
		return (NULL);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels,
		fade_component(245, opacity), fade_component(238, opacity),
		fade_component(248, opacity));
	(void)ncchannels_set_bg_rgb8(&channels,
		fade_component(28, opacity), fade_component(13, opacity),
		fade_component(39, opacity));
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	draw_notification(plane, notification, opacity);
	return (plane);
}

static void	draw_notification(struct ncplane *plane,
	const ui_notification_t *notification, int opacity)
{
	char	percent[8];
	int		percent_x;

	set_color(plane, 255, 203, 102, opacity);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, 0, 2, notification->title);
	snprintf(percent, sizeof(percent), "%d%%", notification->percent);
	percent_x = NOTIFICATION_WIDTH - (int)strlen(percent) - 2;
	set_color(plane, 245, 238, 248, opacity);
	(void)ncplane_putstr_yx(plane, 0, percent_x, percent);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	draw_bar(plane, notification->percent, opacity);
}

static void	draw_bar(struct ncplane *plane, int percent, int opacity)
{
	char	filled[UI_NOTIFICATION_BAR_STEPS + 1];
	char	empty[UI_NOTIFICATION_BAR_STEPS + 1];
	int		filled_steps;
	int		bar_x;

	filled_steps = (percent * UI_NOTIFICATION_BAR_STEPS + 50) / 100;
	memset(filled, '#', (size_t)filled_steps);
	filled[filled_steps] = '\0';
	memset(empty, '-', (size_t)(UI_NOTIFICATION_BAR_STEPS - filled_steps));
	empty[UI_NOTIFICATION_BAR_STEPS - filled_steps] = '\0';
	bar_x = (NOTIFICATION_WIDTH - UI_NOTIFICATION_BAR_STEPS - 2) / 2;
	set_color(plane, 220, 175, 255, opacity);
	(void)ncplane_putstr_yx(plane, 1, bar_x, "[");
	(void)ncplane_putstr_yx(plane, 1,
		bar_x + UI_NOTIFICATION_BAR_STEPS + 1, "]");
	set_color(plane, 255, 78, 153, opacity);
	if (filled_steps > 0)
		(void)ncplane_putstr_yx(plane, 1, bar_x + 1, filled);
	set_color(plane, 104, 66, 132, opacity);
	if (filled_steps < UI_NOTIFICATION_BAR_STEPS)
		(void)ncplane_putstr_yx(plane, 1, bar_x + 1 + filled_steps, empty);
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
		index++;
	}
}
