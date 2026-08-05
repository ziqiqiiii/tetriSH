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

/*
 * draw_button() always emits "[ %-6s ]", so every control is exactly
 * SETTINGS_BUTTON_CELLS wide and the row spans SETTINGS_BUTTON_SPAN. The
 * minimum panel size is derived from that span plus the frame columns, so the
 * compatibility panel is never asked to draw controls it cannot fit.
 */
# define SETTINGS_BUTTON_CELLS	10
# define SETTINGS_BUTTON_GAP	1
# define SETTINGS_BUTTON_SPAN	(4 * SETTINGS_BUTTON_CELLS \
	+ 3 * SETTINGS_BUTTON_GAP)
# define SETTINGS_COMPAT_MIN_COLS	(SETTINGS_BUTTON_SPAN + 3)
# define SETTINGS_COMPAT_MIN_ROWS	14
/* draw_controls() owns rows-5..rows-2; the two rows above carry the summary. */
# define SETTINGS_CONTROLS_ROWS	5
# define SETTINGS_SUMMARY_ROWS	2

static bool	create_panel(render_ctx_t *ctx);
static bool	show_too_small(render_ctx_t *ctx);
static void	draw_settings(render_ctx_t *ctx,
			const app_screen_view_model_t *view,
			const settings_state_t *state);
static void	draw_frame(struct ncplane *plane, int rows, int cols,
			bool compatibility);
static void	draw_profile(struct ncplane *plane,
			const app_settings_view_model_t *settings,
			const settings_state_t *state, int rows, int cols);
static void	slot_prefix(char *out, size_t size, bool equipped, bool focused);
static void	set_slot_colour(struct ncplane *plane, bool focused);
static void	draw_offline(struct ncplane *plane,
			const app_settings_view_model_t *settings, int rows, int cols);
static void	draw_ability_compat(struct ncplane *plane,
			const app_settings_view_model_t *settings,
			const settings_state_t *state, int rows, int cols);
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
	if (!render_compatibility_mode(ctx)
		&& render_settings_pixel_show(ctx, view, state, rebuild_background))
		return (true);
	/*
	 * A failed bitmap composition degrades to the cell panel instead of
	 * failing the screen: run_settings_screen() treats false as fatal, and
	 * opening Settings must never be able to quit the client.
	 */
	render_screen_destroy(ctx);
	render_settings_pixel_destroy(ctx);
	render_background_destroy(ctx);
	if (!create_panel(ctx))
		return (show_too_small(ctx));
	draw_settings(ctx, view, state);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
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
	if (std_rows < SETTINGS_COMPAT_MIN_ROWS
		|| std_cols < SETTINGS_COMPAT_MIN_COLS)
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

/**
 * @brief Explains that the terminal is smaller than the Settings panel needs.
 *
 * Shown instead of the panel so a small window degrades to a readable notice
 * the user can resize or leave, rather than failing the screen.
 */
static bool	show_too_small(render_ctx_t *ctx)
{
	ncplane_options	options;
	uint64_t		channels;
	unsigned		std_rows;
	unsigned		std_cols;
	char			notice[64];

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	if (std_rows == 0 || std_cols == 0)
		return (false);
	memset(&options, 0, sizeof(options));
	options.rows = (int)std_rows;
	options.cols = (int)std_cols;
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
	snprintf(notice, sizeof(notice), "SETTINGS NEEDS %dx%d",
		SETTINGS_COMPAT_MIN_COLS, SETTINGS_COMPAT_MIN_ROWS);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, SETTINGS_GOLD_R,
		SETTINGS_GOLD_G, SETTINGS_GOLD_B);
	put_centered(ctx->screen_plane, 0, (int)std_cols, notice, true);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, SETTINGS_LAVENDER_R,
		SETTINGS_LAVENDER_G, SETTINGS_LAVENDER_B);
	put_centered(ctx->screen_plane, 1, (int)std_cols, "RESIZE OR ESC", false);
	ncplane_move_top(ctx->screen_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
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
		draw_profile(plane, &view->data.settings, state, rows, cols);
		if (settings_card_visible(state))
			draw_ability_compat(plane, &view->data.settings, state, rows,
				cols);
	}
	draw_controls(plane, &view->data.settings, state);
}

static void	draw_profile(struct ncplane *plane,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	int rows, int cols)
{
	char		line[APP_TEXT_MAX + 96];
	char		prefix[4];
	bool		focused;
	int		index;
	int		slot;
	int		info_x;
	int		character_row;
	int		theme_row;
	int		theme_first;
	int		content_limit;
	int		summary_row;

	info_x = cols >= 72 ? 28 : 3;
	/* Inventory grows downwards, so it must stop before the two summary rows
	 * and the control block rather than at fixed rows the lists can reach. */
	content_limit = rows - SETTINGS_CONTROLS_ROWS - SETTINGS_SUMMARY_ROWS;
	theme_row = content_limit - 1;
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
	if (8 < content_limit)
	{
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
			SETTINGS_PINK_B);
		put_line(plane, 8, 3, cols - 6, "OWNED CHARACTERS", true);
		character_row = 9;
		index = 0;
		slot = 0;
		while (index < settings->characters.count
			&& character_row < content_limit)
		{
			if (settings->characters.items[index].owned)
			{
				focused = state->section == SETTINGS_SECTION_CHARACTERS
					&& state->character_slot == slot;
				slot_prefix(prefix, sizeof(prefix),
					settings->characters.items[index].equipped, focused);
				snprintf(line, sizeof(line), "%s%s", prefix,
					settings->characters.items[index].name);
				set_slot_colour(plane, focused);
				put_line(plane, character_row, 4, cols - 8, line, focused);
				character_row++;
				slot++;
			}
			index++;
		}
		if (character_row == 9 && character_row < content_limit)
		{
			put_line(plane, character_row, 4, cols - 8,
				"No owned characters", false);
			character_row++;
		}
		theme_row = character_row + 1;
		if (theme_row < content_limit)
		{
			(void)ncplane_set_fg_rgb8(plane, SETTINGS_PINK_R, SETTINGS_PINK_G,
				SETTINGS_PINK_B);
			put_line(plane, theme_row, 3, cols - 6, "OWNED THEMES", true);
			theme_row++;
		}
		theme_first = theme_row;
		index = 0;
		slot = 0;
		while (index < settings->themes.count && theme_row < content_limit)
		{
			if (settings->themes.items[index].owned)
			{
				focused = state->section == SETTINGS_SECTION_THEMES
					&& state->theme_slot == slot;
				slot_prefix(prefix, sizeof(prefix),
					settings->themes.items[index].equipped, focused);
				snprintf(line, sizeof(line), "%s%s", prefix,
					settings->themes.items[index].name);
				set_slot_colour(plane, focused);
				put_line(plane, theme_row, 4, cols - 8, line, focused);
				theme_row++;
				slot++;
			}
			index++;
		}
		if (theme_row == theme_first && theme_row < content_limit)
		{
			put_line(plane, theme_row, 4, cols - 8, "No owned themes", false);
			theme_row++;
		}
	}
	summary_row = theme_row + 1;
	if (summary_row > content_limit)
		summary_row = content_limit;
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_GREEN_R, SETTINGS_GREEN_G,
		SETTINGS_GREEN_B);
	snprintf(line, sizeof(line), "WALLET POINTS: %d    SCORE: %" PRIu64
		"    RANK: #%d", settings->profile.wallet_points,
		settings->profile.score, settings->profile.rank);
	put_line(plane, summary_row, 3, cols - 6, line, true);
	snprintf(line, sizeof(line), "MUSIC VOLUME: %d%%    RENDERER: %s",
		settings->music_volume * 100 / AUDIO_MAX_VOLUME,
		renderer_mode_name(settings->renderer_mode));
	put_line(plane, summary_row + 1, 3, cols - 6, line, false);
}

/**
 * @brief Builds the two-column marker that opens an inventory line.
 *
 * The cell panel has no colour budget to spare, so focus and equipped state
 * are encoded positionally: the cursor column, then the equipped column.
 */
static void	slot_prefix(char *out, size_t size, bool equipped, bool focused)
{
	snprintf(out, size, "%c%c", focused ? '>' : ' ', equipped ? '*' : ' ');
}

/**
 * @brief Colours one inventory line: gold marks the cursor and nothing else.
 *
 * A panel the cursor has left draws entirely in cream, so only one line on the
 * screen is ever gold.
 */
static void	set_slot_colour(struct ncplane *plane, bool focused)
{
	if (focused)
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
			SETTINGS_GOLD_B);
	else
		(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
			SETTINGS_CREAM_B);
}

static void	draw_offline(struct ncplane *plane,
	const app_settings_view_model_t *settings, int rows, int cols)
{
	char	line[APP_TEXT_MAX + 64];
	int		last_row;
	int		step;
	int		info_row;

	(void)ncplane_set_fg_rgb8(plane, SETTINGS_CREAM_R, SETTINGS_CREAM_G,
		SETTINGS_CREAM_B);
	put_line(plane, 5, 3, cols - 6, "ACCOUNT PROFILE: NOT SIGNED IN", true);
	put_line(plane, 7, 3, cols - 6,
		"Username, wallet, score, rank, and inventory are unavailable offline.",
		false);
	/* Space the three local-control lines only while they still clear the
	 * fixed text above and the control block below; drop what cannot fit. */
	last_row = rows - SETTINGS_CONTROLS_ROWS - 1;
	step = rows >= 24 ? 2 : 1;
	info_row = last_row - step * 2;
	if (info_row > 11)
		info_row = 11;
	if (info_row < 8)
		info_row = 8;
	(void)ncplane_set_fg_rgb8(plane, SETTINGS_GREEN_R, SETTINGS_GREEN_G,
		SETTINGS_GREEN_B);
	snprintf(line, sizeof(line), "MUSIC VOLUME: %d%%    RENDERER: %s",
		settings->music_volume * 100 / AUDIO_MAX_VOLUME,
		renderer_mode_name(settings->renderer_mode));
	if (info_row <= last_row)
		put_line(plane, info_row, 3, cols - 6, line, true);
	if (info_row + step <= last_row)
		put_line(plane, info_row + step, 3, cols - 6,
			settings->local_status, false);
	if (info_row + step * 2 <= last_row)
		put_line(plane, info_row + step * 2, 3, cols - 6,
			"Use +/- to adjust music volume for this run.", false);
}

static void	draw_ability_compat(struct ncplane *plane,
	const app_settings_view_model_t *settings, const settings_state_t *state,
	int rows, int cols)
{
	const app_catalogue_item_view_model_t	*character;
	char							line[APP_ABILITY_TEXT_MAX + 80];
	int							index;
	int							row;
	int							x;

	character = settings_card_character(settings, state);
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
			"UP INTO INVENTORY    ENTER EQUIPS    I POWER INFO", false);
	put_centered(plane, row + 2, (int)ncplane_dim_x(plane),
		settings->signed_in
		? "ARROWS MOVE  ENTER SELECT  M MARKETPLACE  +/- VOLUME  ESC BACK"
		: "ARROWS MOVE  ENTER SELECT  +/- VOLUME  ESC BACK", false);
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
	int	start;

	cols = (int)ncplane_dim_x(plane);
	*row = (int)ncplane_dim_y(plane) - 4;
	start = (cols - SETTINGS_BUTTON_SPAN) / 2;
	if (start < 1)
		start = 1;
	*back_x = start;
	*market_x = *back_x + SETTINGS_BUTTON_CELLS + SETTINGS_BUTTON_GAP;
	*down_x = *market_x + SETTINGS_BUTTON_CELLS + SETTINGS_BUTTON_GAP;
	*up_x = *down_x + SETTINGS_BUTTON_CELLS + SETTINGS_BUTTON_GAP;
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
