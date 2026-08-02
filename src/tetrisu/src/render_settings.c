#include "tetrisu.h"

# define SETTINGS_BG_R		16
# define SETTINGS_BG_G		9
# define SETTINGS_BG_B		31
# define SETTINGS_INNER_R	25
# define SETTINGS_INNER_G	14
# define SETTINGS_INNER_B	46
# define SETTINGS_PINK_R	255
# define SETTINGS_PINK_G	112
# define SETTINGS_PINK_B	190
# define SETTINGS_GOLD_R	255
# define SETTINGS_GOLD_G	203
# define SETTINGS_GOLD_B	102
# define SETTINGS_CREAM_R	250
# define SETTINGS_CREAM_G	242
# define SETTINGS_CREAM_B	221
# define SETTINGS_LAVENDER_R	190
# define SETTINGS_LAVENDER_G	155
# define SETTINGS_LAVENDER_B	218
# define SETTINGS_GREEN_R	112
# define SETTINGS_GREEN_G	214
# define SETTINGS_GREEN_B	174
# define SETTINGS_RED_R	255
# define SETTINGS_RED_G	111
# define SETTINGS_RED_B	142

static bool	create_panel(render_ctx_t *ctx);
static void	draw_settings(render_ctx_t *ctx,
			const app_screen_view_model_t *view,
			const settings_state_t *state);
static void	draw_frame(struct ncplane *plane, int rows, int cols,
			bool compatibility);
static void	draw_profile(struct ncplane *plane,
			const app_settings_view_model_t *settings, int rows, int cols);
static void	draw_offline(struct ncplane *plane,
			const app_settings_view_model_t *settings, int rows, int cols);
static void	draw_ability_compat(struct ncplane *plane,
			const app_settings_view_model_t *settings, int rows, int cols);
static void	draw_controls(struct ncplane *plane,
			const app_settings_view_model_t *settings,
			const settings_state_t *state);
static void	put_line(struct ncplane *plane, int row, int x, int width,
			const char *text, bool bold);
static void	put_centered(struct ncplane *plane, int row, int cols,
			const char *text, bool bold);
static void	draw_button(struct ncplane *plane, int row, int x,
			const char *text, bool focused, bool enabled);
static void	button_geometry(const struct ncplane *plane, int *row,
			int *back_x, int *market_x, int *down_x, int *up_x);
static bool	button_hit(const ncinput *input, int row, int x, int width,
			int plane_y, int plane_x);
static const char *renderer_mode_name(tetrisu_renderer_mode_t mode);
static int	min_int(int first, int second);

/**
 * @brief Draws Settings through the pixel compositor or the cell fallback.
 *
 * The cell branch remains intentionally self-contained for deterministic
 * terminals and TETRISU_RENDERER=cell. Every other tier uses the authored
 * Settings bitmap and bitmap-font overlays.
 */
bool	render_settings_show(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state,
	bool rebuild_background)
{
	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL)
		return (false);
	render_menu_destroy(ctx);
	if (!render_compatibility_mode(ctx))
		return (render_settings_pixel_show(ctx, view, state,
			rebuild_background));
	render_screen_destroy(ctx);
	render_settings_pixel_destroy(ctx);
	render_background_destroy(ctx);
	if (!create_panel(ctx))
		return (false);
	draw_settings(ctx, view, state);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Maps pointer coordinates to the same focus order as the keyboard.
 */
bool	render_settings_hit_test(const render_ctx_t *ctx, const ncinput *input,
	settings_focus_t *focus)
{
	settings_layout_t	layout;
	int				button_index;
	int	plane_y;
	int	plane_x;
	int	row;
	int	back_x;
	int	market_x;
	int	down_x;
	int	up_x;

	if (ctx == NULL || input == NULL
		|| focus == NULL)
		return (false);
	if (!render_compatibility_mode(ctx))
	{
		settings_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
			ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
		if (!settings_layout_hit_test(&layout, input->y, input->x,
				&button_index))
			return (false);
		if (button_index == 0)
			*focus = SETTINGS_FOCUS_BACK;
		else if (button_index == 1)
			*focus = SETTINGS_FOCUS_MARKETPLACE;
		else if (button_index == 2)
			*focus = SETTINGS_FOCUS_VOLUME_DOWN;
		else if (button_index == 3)
			*focus = SETTINGS_FOCUS_VOLUME_UP;
		else if (button_index == 4)
			*focus = SETTINGS_FOCUS_CHARACTER_PREVIOUS;
		else
			*focus = SETTINGS_FOCUS_CHARACTER_NEXT;
		return (true);
	}
	if (ctx->screen_plane == NULL)
		return (false);
	ncplane_yx(ctx->screen_plane, &plane_y, &plane_x);
	button_geometry(ctx->screen_plane, &row, &back_x, &market_x,
		&down_x, &up_x);
	if (button_hit(input, row, back_x, 8, plane_y, plane_x))
		*focus = SETTINGS_FOCUS_BACK;
	else if (button_hit(input, row, market_x, 10, plane_y, plane_x))
		*focus = SETTINGS_FOCUS_MARKETPLACE;
	else if (button_hit(input, row, down_x, 9, plane_y, plane_x))
		*focus = SETTINGS_FOCUS_VOLUME_DOWN;
	else if (button_hit(input, row, up_x, 9, plane_y, plane_x))
		*focus = SETTINGS_FOCUS_VOLUME_UP;
	else
		return (false);
	return (true);
}

/**
 * @brief Reports pointer presence over the live character portrait.
 */
bool	render_settings_portrait_hit_test(const render_ctx_t *ctx,
	const ncinput *input)
{
	settings_layout_t	layout;
	settings_rect_t		*portrait;
	int				left;
	int				top;
	int				right;
	int				bottom;

	if (ctx == NULL || input == NULL || render_compatibility_mode(ctx))
		return (false);
	settings_layout_build(ctx->bg_row, ctx->bg_col, ctx->bg_rows,
		ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x, &layout);
	portrait = &layout.portrait;
	left = layout.origin_x + portrait->x / layout.cell_px_x;
	top = layout.origin_y + portrait->y / layout.cell_px_y;
	right = layout.origin_x + (portrait->x + portrait->width
		+ layout.cell_px_x - 1) / layout.cell_px_x;
	bottom = layout.origin_y + (portrait->y + portrait->height
		+ layout.cell_px_y - 1) / layout.cell_px_y;
	return (input->x >= left && input->x < right
		&& input->y >= top && input->y < bottom);
}

/**
 * @brief Releases Settings-owned planes without touching the backdrop.
 */
void	render_settings_destroy(render_ctx_t *ctx)
{
	if (ctx == NULL)
		return ;
	render_screen_destroy(ctx);
	render_settings_pixel_destroy(ctx);
}

static bool	create_panel(render_ctx_t *ctx)
{
	ncplane_options	options;
	uint64_t		channels;
	unsigned		std_rows;
	unsigned		std_cols;
	int				rows;
	int				cols;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	if (std_rows < 14 || std_cols < 32)
		return (false);
	memset(&options, 0, sizeof(options));
	if (render_compatibility_mode(ctx))
	{
		options.y = std_rows > 20 ? 1 : 0;
		options.x = 0;
		options.rows = (int)std_rows - options.y;
		options.cols = (int)std_cols;
	}
	else
	{
		rows = min_int((int)std_rows - 2, 34);
		cols = min_int((int)std_cols - 2, 112);
		options.rows = rows;
		options.cols = cols;
		options.y = ((int)std_rows - rows) / 2;
		options.x = ((int)std_cols - cols) / 2;
	}
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, SETTINGS_CREAM_R,
		SETTINGS_CREAM_G, SETTINGS_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, SETTINGS_BG_R, SETTINGS_BG_G,
		SETTINGS_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	return (true);
}

static void	draw_settings(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const settings_state_t *state)
{
	struct ncplane	*plane;
	int			rows;
	int			cols;

	plane = ctx->screen_plane;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	draw_frame(plane, rows, cols, render_compatibility_mode(ctx));
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
		SETTINGS_PINK_B);
	put_centered(plane, 1, cols, ":: SETTINGS / PROFILE ::", true);
	if (view->local_preview)
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
			SETTINGS_GOLD_B);
		put_centered(plane, 2, cols, "LOCAL UI PREVIEW", true);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_LAVENDER_R,
			SETTINGS_LAVENDER_G, SETTINGS_LAVENDER_B);
		put_centered(plane, 2, cols, view->data.settings.offline
			? "OFFLINE LOCAL SETTINGS" : "PROFILE STATUS UNAVAILABLE", false);
	}
	if (view->status != APP_DATA_READY)
	{
		const char *status_title;
		const char *status_detail;

		status_title = "SETTINGS UNAVAILABLE";
		status_detail = view->subtitle;
		if (view->status == APP_DATA_LOADING)
		{
			status_title = "LOADING SETTINGS...";
			status_detail = "PLEASE WAIT";
		}
		else if (view->status == APP_DATA_EMPTY)
		{
			status_title = "NO PROFILE DATA";
			status_detail = "ACCOUNT DATA IS EMPTY";
		}
		else if (view->status == APP_DATA_ERROR)
		{
			status_title = "SETTINGS ERROR";
			status_detail = "TRY AGAIN LATER";
		}
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
			SETTINGS_GOLD_B);
		put_centered(plane, rows / 2 - 1, cols,
			status_title, true);
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_LAVENDER_R,
			SETTINGS_LAVENDER_G, SETTINGS_LAVENDER_B);
		put_centered(plane, rows / 2 + 1, cols, status_detail, false);
	}
	else if (view->data.settings.offline || !view->data.settings.signed_in)
		draw_offline(plane, &view->data.settings, rows, cols);
	else
	{
		draw_profile(plane, &view->data.settings, rows, cols);
		if (state->ability_info_visible)
			draw_ability_compat(plane, &view->data.settings, rows, cols);
	}
	draw_controls(plane, &view->data.settings, state);
}

static void	draw_profile(struct ncplane *plane,
	const app_settings_view_model_t *settings, int rows, int cols)
{
	char		line[APP_TEXT_MAX + 96];
	int		index;
	int		info_x;
	int		character_row;
	int		theme_row;

	info_x = cols >= 72 ? 28 : 3;
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
		SETTINGS_CREAM_B);
	snprintf(line, sizeof(line), "PROFILE PORTRAIT: %s",
		settings->profile.character);
	put_line(plane, 3, 3, cols - 6, line, true);
	put_line(plane, 4, info_x, cols - info_x - 2, "USERNAME: ", false);
	put_line(plane, 4, info_x + 10, cols - info_x - 12,
		settings->profile.username, true);
	put_line(plane, 5, info_x, cols - info_x - 2, "EQUIPPED CHARACTER: ", false);
	put_line(plane, 5, info_x + 20, cols - info_x - 22,
		settings->profile.character, true);
	put_line(plane, 6, info_x, cols - info_x - 2, "EQUIPPED THEME: ", false);
	put_line(plane, 6, info_x + 16, cols - info_x - 18,
		settings->profile.theme, true);
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
		SETTINGS_PINK_B);
	put_line(plane, 8, 3, cols - 6, "OWNED CHARACTERS", true);
	character_row = 9;
	index = 0;
	while (index < settings->characters.count && character_row < rows - 6)
	{
		if (settings->characters.items[index].owned)
		{
			snprintf(line, sizeof(line), "%s%s",
				settings->characters.items[index].equipped ? "> " : "  ",
				settings->characters.items[index].name);
			put_line(plane, character_row, 4, cols - 8, line,
				settings->characters.items[index].equipped);
			character_row++;
		}
		index++;
	}
	if (character_row == 9)
	{
		put_line(plane, character_row, 4, cols - 8,
			"No owned characters", false);
		character_row++;
	}
	theme_row = character_row + 1;
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
		SETTINGS_PINK_B);
	put_line(plane, theme_row, 3, cols - 6, "OWNED THEMES", true);
	theme_row++;
	index = 0;
	while (index < settings->themes.count && theme_row < rows - 5)
	{
		if (settings->themes.items[index].owned)
		{
			snprintf(line, sizeof(line), "%s%s",
				settings->themes.items[index].equipped ? "> " : "  ",
				settings->themes.items[index].name);
			put_line(plane, theme_row, 4, cols - 8, line,
				settings->themes.items[index].equipped);
			theme_row++;
		}
		index++;
	}
	if (theme_row == character_row + 2)
		put_line(plane, theme_row, 4, cols - 8, "No owned themes", false);
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_GREEN_R, SETTINGS_GREEN_G,
		SETTINGS_GREEN_B);
	snprintf(line, sizeof(line), "WALLET POINTS: %d    SCORE: %" PRIu64
		"    RANK: #%d", settings->profile.wallet_points,
		settings->profile.score, settings->profile.rank);
	put_line(plane, rows >= 24 ? 17 : rows - 7, 3, cols - 6, line, true);
	snprintf(line, sizeof(line), "MUSIC VOLUME: %d%%    RENDERER: %s",
		settings->music_volume * 100 / AUDIO_MAX_VOLUME,
		renderer_mode_name(settings->renderer_mode));
	put_line(plane, rows >= 24 ? 18 : rows - 6, 3, cols - 6, line, false);
}

static void	draw_offline(struct ncplane *plane,
	const app_settings_view_model_t *settings, int rows, int cols)
{
	char	line[APP_TEXT_MAX + 64];

	(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
		SETTINGS_CREAM_B);
	put_line(plane, 5, 3, cols - 6, "ACCOUNT PROFILE: NOT SIGNED IN", true);
	put_line(plane, 7, 3, cols - 6,
		"Username, wallet, score, rank, and inventory are unavailable offline.",
		false);
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_GREEN_R, SETTINGS_GREEN_G,
		SETTINGS_GREEN_B);
	snprintf(line, sizeof(line), "MUSIC VOLUME: %d%%    RENDERER: %s",
		settings->music_volume * 100 / AUDIO_MAX_VOLUME,
		renderer_mode_name(settings->renderer_mode));
	put_line(plane, rows >= 24 ? 11 : rows - 7, 3, cols - 6, line, true);
	put_line(plane, rows >= 24 ? 13 : rows - 6, 3, cols - 6,
		settings->local_status, false);
	put_line(plane, rows >= 24 ? 15 : rows - 5, 3, cols - 6,
		"Use +/- to adjust music volume for this run.", false);
}

static void	draw_ability_compat(struct ncplane *plane,
	const app_settings_view_model_t *settings, int rows, int cols)
{
	const app_catalogue_item_view_model_t	*character;
	char							line[APP_ABILITY_TEXT_MAX + 80];
	int							index;
	int							row;
	int							x;

	character = NULL;
	index = 0;
	while (index < settings->characters.count)
	{
		if (settings->characters.items[index].equipped)
			character = &settings->characters.items[index];
		index++;
	}
	if (character == NULL)
		return ;
	(void)ncplane_set_bg_rgb8(plane, SETTINGS_INNER_R, SETTINGS_INNER_G,
		SETTINGS_INNER_B);
	row = 3;
	while (row < rows - 5)
	{
		x = 2;
		while (x < cols - 2)
		{
			(void)ncplane_putchar_yx(plane, row, x, ' ');
			x++;
		}
		row++;
	}
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
		SETTINGS_PINK_B);
	snprintf(line, sizeof(line), "%s - CRYSTAL POWERS", character->name);
	put_centered(plane, 3, cols, line, true);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT && 5 + index * 2 < rows - 5)
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
			SETTINGS_GOLD_B);
		snprintf(line, sizeof(line), "L%d %s", index + 1,
			character->abilities[index].name);
		put_line(plane, 5 + index * 2, 4, cols - 8, line, true);
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
			SETTINGS_CREAM_B);
		put_line(plane, 6 + index * 2, 6, cols - 12,
			character->abilities[index].description, false);
		index++;
	}
}

static void	draw_controls(struct ncplane *plane,
	const app_settings_view_model_t *settings, const settings_state_t *state)
{
	int	row;
	int	back_x;
	int	market_x;
	int	down_x;
	int	up_x;

	button_geometry(plane, &row, &back_x, &market_x, &down_x, &up_x);
	draw_button(plane, row, back_x, "BACK",
		state->focus == SETTINGS_FOCUS_BACK, true);
	draw_button(plane, row, market_x, "MARKET",
		state->focus == SETTINGS_FOCUS_MARKETPLACE, settings->signed_in);
	draw_button(plane, row, down_x, "VOL -",
		state->focus == SETTINGS_FOCUS_VOLUME_DOWN, true);
	draw_button(plane, row, up_x, "VOL +",
		state->focus == SETTINGS_FOCUS_VOLUME_UP, true);
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_LAVENDER_R,
		SETTINGS_LAVENDER_G, SETTINGS_LAVENDER_B);
	if (settings->signed_in)
		put_centered(plane, row - 1, (int)ncplane_dim_x(plane),
			"[ / ] PREVIOUS / NEXT CHARACTER    I POWER INFO", false);
	put_centered(plane, row + 2, (int)ncplane_dim_x(plane),
		settings->signed_in
		? "TAB/ARROWS FOCUS  ENTER SELECT  M MARKETPLACE  +/- VOLUME  ESC BACK"
		: "TAB/ARROWS FOCUS  ENTER SELECT  +/- VOLUME  ESC BACK", false);
}

static void	draw_frame(struct ncplane *plane, int rows, int cols,
	bool compatibility)
{
	int	x;
	int	y;

	(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
		SETTINGS_PINK_B);
	x = 0;
	while (x < cols)
	{
		(void)ncplane_putchar_yx(plane, 0, x,
			x == 0 || x == cols - 1 ? '+' : '=');
		(void)ncplane_putchar_yx(plane, rows - 1, x,
			x == 0 || x == cols - 1 ? '+' : '=');
		x++;
	}
	y = 1;
	while (y < rows - 1)
	{
		(void)ncplane_putchar_yx(plane, y, 0, '|');
		(void)ncplane_putchar_yx(plane, y, cols - 1, '|');
		y++;
	}
	if (compatibility && cols >= 20 && rows >= 8)
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_LAVENDER_R,
			SETTINGS_LAVENDER_G, SETTINGS_LAVENDER_B);
		(void)ncplane_putstr_yx(plane, 3, 3, "[] []");
		(void)ncplane_putstr_yx(plane, rows - 4, cols - 9, "[][]");
	}
}

static void	put_line(struct ncplane *plane, int row, int x, int width,
	const char *text, bool bold)
{
	char	clipped[APP_TEXT_MAX + 112];
	int	rows;
	int	cols;

	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	if (text == NULL || row < 0 || row >= rows || x < 0 || x >= cols
		|| width < 1)
		return ;
	if (width > cols - x)
		width = cols - x;
	snprintf(clipped, sizeof(clipped), "%.*s", min_int(width,
		(int)sizeof(clipped) - 1), text);
	if (bold)
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, clipped);
	if (bold)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	put_centered(struct ncplane *plane, int row, int cols,
	const char *text, bool bold)
{
	int	width;
	int	x;

	if (text == NULL || row < 0 || row >= (int)ncplane_dim_y(plane))
		return ;
	width = (int)strlen(text);
	if (width > cols - 2)
		width = cols - 2;
	x = (cols - width) / 2;
	put_line(plane, row, x, width, text, bold);
}

static void	draw_button(struct ncplane *plane, int row, int x,
	const char *text, bool focused, bool enabled)
{
	char	label[16];

	snprintf(label, sizeof(label), "[ %-6s ]", text);
	if (!enabled)
	{
		(void)ncplane_set_fg_rgb8(plane, 116, 111, 132);
		(void)ncplane_set_bg_rgb8(plane, SETTINGS_BG_R, SETTINGS_BG_G,
			SETTINGS_BG_B);
	}
	else if (focused)
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_BG_R, SETTINGS_BG_G,
			SETTINGS_BG_B);
		(void)ncplane_set_bg_rgb8(plane, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
			SETTINGS_GOLD_B);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
			SETTINGS_CREAM_B);
		(void)ncplane_set_bg_rgb8(plane, SETTINGS_INNER_R,
			SETTINGS_INNER_G, SETTINGS_INNER_B);
	}
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, label);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	button_geometry(const struct ncplane *plane, int *row,
	int *back_x, int *market_x, int *down_x, int *up_x)
{
	int	cols;
	int	total;
	int	start;

	cols = (int)ncplane_dim_x(plane);
	*row = (int)ncplane_dim_y(plane) - 4;
	total = 8 + 2 + 10 + 2 + 9 + 2 + 9;
	start = (cols - total) / 2;
	if (start < 1)
		start = 1;
	*back_x = start;
	*market_x = *back_x + 10;
	*down_x = *market_x + 12;
	*up_x = *down_x + 11;
}

static bool	button_hit(const ncinput *input, int row, int x, int width,
	int plane_y, int plane_x)
{
	return (input->y == plane_y + row && input->x >= plane_x + x
		&& input->x < plane_x + x + width);
}

static const char *renderer_mode_name(tetrisu_renderer_mode_t mode)
{
	if (mode == TETRISU_RENDERER_CELL)
		return ("CELL");
	if (mode == TETRISU_RENDERER_STATIONARY)
		return ("STATIONARY");
	if (mode == TETRISU_RENDERER_PIXEL)
		return ("PIXEL");
	return ("AUTO");
}

static int	min_int(int first, int second)
{
	return (first < second ? first : second);
}
