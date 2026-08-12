#include "tetrisu.h"

# define MARKET_BG_R	16
# define MARKET_BG_G	9
# define MARKET_BG_B	31
# define MARKET_INNER_R	25
# define MARKET_INNER_G	14
# define MARKET_INNER_B	46
# define MARKET_PINK_R	255
# define MARKET_PINK_G	112
# define MARKET_PINK_B	190
# define MARKET_GOLD_R	255
# define MARKET_GOLD_G	203
# define MARKET_GOLD_B	102
# define MARKET_CREAM_R	250
# define MARKET_CREAM_G	242
# define MARKET_CREAM_B	221
# define MARKET_LAVENDER_R	190
# define MARKET_LAVENDER_G	155
# define MARKET_LAVENDER_B	218
# define MARKET_GREEN_R	112
# define MARKET_GREEN_G	214
# define MARKET_GREEN_B	174
# define MARKET_RED_R	255
# define MARKET_RED_G	111
# define MARKET_RED_B	142
# define MARKET_DISABLED_R	105
# define MARKET_DISABLED_G	99
# define MARKET_DISABLED_B	120

/*
 * draw_button() always emits "[ %-8s ]", so every control is exactly
 * MARKET_BUTTON_CELLS wide and the row spans MARKET_BUTTON_SPAN. The minimum
 * panel size is derived from that span plus the frame columns, so the
 * compatibility panel is never asked to draw controls it cannot fit.
 */
# define MARKET_BUTTON_CELLS	12
# define MARKET_BUTTON_GAP	1
# define MARKET_BUTTON_SPAN	(4 * MARKET_BUTTON_CELLS + 3 * MARKET_BUTTON_GAP)
# define MARKET_COMPAT_MIN_COLS	(MARKET_BUTTON_SPAN + 3)
# define MARKET_COMPAT_MIN_ROWS	16
/* draw_controls() owns rows-5..rows-2; the row above carries the key legend. */
# define MARKET_CONTROLS_ROWS	5
# define MARKET_DETAIL_ROWS	7

static bool	create_panel(t_render_ctx *ctx);
static bool	show_too_small(t_render_ctx *ctx);
static void	draw_marketplace(t_render_ctx *ctx,
				const t_app_screen_view_model *view,
				const t_marketplace_state *state);
static void	draw_frame(struct ncplane *plane, int rows, int cols,
				bool compatibility);
static void	draw_header(struct ncplane *plane,
				const t_app_screen_view_model *view, int cols);
static int	draw_catalogue(struct ncplane *plane,
				const t_app_marketplace_view_model *market,
				const t_marketplace_state *state, int row, int cols,
				bool characters);
static void	draw_detail(struct ncplane *plane,
				const t_app_marketplace_view_model *market,
				const t_marketplace_state *state, int rows, int cols);
static void	draw_unavailable(struct ncplane *plane,
				const t_app_screen_view_model *view, int rows, int cols);
static void	draw_controls(struct ncplane *plane,
				const t_app_marketplace_view_model *market,
				const t_marketplace_state *state);
static void	slot_prefix(char *out, size_t size, bool equipped, bool focused,
				bool owned);
static void	slot_suffix(const t_app_catalogue_item_view_model *item,
				const t_app_marketplace_view_model *market, char *out,
				size_t size);
static void	set_slot_colour(struct ncplane *plane,
				const t_app_catalogue_item_view_model *item,
				const t_app_marketplace_view_model *market, bool focused);
static void	put_wrapped(struct ncplane *plane, int row, int x, int width,
				const char *text, int max_lines);
static void	put_line(struct ncplane *plane, int row, int x, int width,
				const char *text, bool bold);
static void	put_centered(struct ncplane *plane, int row, int cols,
				const char *text, bool bold);
static void	draw_button(struct ncplane *plane, int row, int x,
				const char *text, bool focused, bool enabled);
static void	button_geometry(const struct ncplane *plane, int *row,
				int *back_x, int *buy_x, int *down_x, int *up_x);
static void	set_feedback_colour(struct ncplane *plane,
				const t_marketplace_state *state);
static int	min_int(int first, int second);

/**
 * @brief Draws the Marketplace through the compositor or the cell fallback.
 *
 * The cell branch is intentionally self-contained for deterministic terminals
 * and TETRISU_RENDERER=cell. Every other tier uses the authored shop backdrop
 * and the bitmap-font overlays.
 */
bool	render_marketplace_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_marketplace_state *state,
	bool rebuild_background)
{
	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL)
		return (false);
	/*
	 * A notification card is a bitmap over bitmaps, and notcurses wipes the
	 * sprixel underneath rather than overlapping it. The panels it covered
	 * are cached by signature, so nothing else would ever consider them
	 * stale - the screen has to be told to rebuild instead of trusting it.
	 */
	if (render_notification_take_repaint(ctx))
		rebuild_background = true;
	render_menu_destroy(ctx);
	if (!render_compatibility_mode(ctx)
		&& render_marketplace_pixel_show(ctx, view, state, rebuild_background))
		return (true);
	/*
	 * A failed bitmap composition degrades to the cell panel instead of
	 * failing the screen: run_marketplace_screen() treats false as fatal, and
	 * opening the Marketplace must never be able to quit the client.
	 */
	render_screen_destroy(ctx);
	render_marketplace_pixel_destroy(ctx);
	render_background_destroy(ctx);
	if (!create_panel(ctx))
		return (show_too_small(ctx));
	draw_marketplace(ctx, view, state);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Releases Marketplace-owned planes without touching the backdrop.
 */
void	render_marketplace_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	render_screen_destroy(ctx);
	render_marketplace_pixel_destroy(ctx);
}

static bool	create_panel(t_render_ctx *ctx)
{
	ncplane_options	options;
	uint64_t		channels;
	unsigned		std_rows;
	unsigned		std_cols;
	int				rows;
	int				cols;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	if (std_rows < MARKET_COMPAT_MIN_ROWS || std_cols < MARKET_COMPAT_MIN_COLS)
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
		rows = min_int((int)std_rows - 2, 36);
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
	(void)ncchannels_set_fg_rgb8(&channels, MARKET_CREAM_R, MARKET_CREAM_G,
		MARKET_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, MARKET_BG_R, MARKET_BG_G,
		MARKET_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	return (true);
}

/**
 * @brief Explains that the terminal is smaller than the panel needs.
 *
 * Shown instead of the panel so a small window degrades to a readable notice
 * the user can resize or leave, rather than failing the screen.
 */
static bool	show_too_small(t_render_ctx *ctx)
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
	(void)ncchannels_set_fg_rgb8(&channels, MARKET_CREAM_R, MARKET_CREAM_G,
		MARKET_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, MARKET_BG_R, MARKET_BG_G,
		MARKET_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	snprintf(notice, sizeof(notice), "MARKETPLACE NEEDS %dx%d",
		MARKET_COMPAT_MIN_COLS, MARKET_COMPAT_MIN_ROWS);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, MARKET_GOLD_R, MARKET_GOLD_G,
		MARKET_GOLD_B);
	put_centered(ctx->screen_plane, 0, (int)std_cols, notice, true);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, MARKET_LAVENDER_R,
		MARKET_LAVENDER_G, MARKET_LAVENDER_B);
	put_centered(ctx->screen_plane, 1, (int)std_cols, "RESIZE OR ESC", false);
	ncplane_move_top(ctx->screen_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Lays the cell panel out top-down, clipping whatever will not fit.
 *
 * The two catalogues grow downwards, so they must stop before the detail block
 * and the control block rather than at fixed rows the lists can reach.
 */
static void	draw_marketplace(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_marketplace_state *state)
{
	const t_app_marketplace_view_model	*market;
	struct ncplane						*plane;
	int									rows;
	int									cols;
	int									row;

	plane = ctx->screen_plane;
	market = &view->data.marketplace;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	draw_frame(plane, rows, cols, render_compatibility_mode(ctx));
	draw_header(plane, view, cols);
	if (view->status != APP_DATA_READY || !market->signed_in || market->offline)
	{
		draw_unavailable(plane, view, rows, cols);
		draw_controls(plane, market, state);
		return ;
	}
	row = draw_catalogue(plane, market, state, 5,
			cols, true);
	row = draw_catalogue(plane, market, state, row + 1, cols, false);
	draw_detail(plane, market, state, rows, cols);
	draw_controls(plane, market, state);
}

/**
 * @brief Draws the title and the wallet the whole screen spends from.
 */
static void	draw_header(struct ncplane *plane,
	const t_app_screen_view_model *view, int cols)
{
	const t_app_marketplace_view_model	*market;
	char								line[APP_TEXT_MAX + 96];

	market = &view->data.marketplace;
	(void)ncplane_set_fg_rgb8(plane, MARKET_PINK_R, MARKET_PINK_G,
		MARKET_PINK_B);
	put_centered(plane, 1, cols, ":: MARKETPLACE ::", true);
	if (view->local_preview)
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
			MARKET_GOLD_B);
		put_centered(plane, 2, cols, "LOCAL UI PREVIEW", true);
	}
	if (!market->signed_in || market->offline)
		return ;
	(void)ncplane_set_fg_rgb8(plane, MARKET_GREEN_R, MARKET_GREEN_G,
		MARKET_GREEN_B);
	snprintf(line, sizeof(line), "WALLET: %d P    BEST SCORE: %" PRIu64
		"    RANK: #%d", market->profile.wallet_points, market->profile.score,
		market->profile.rank);
	put_centered(plane, 3, cols, line, true);
}

/**
 * @brief Draws one catalogue as a titled list and returns the next free row.
 */
static int	draw_catalogue(struct ncplane *plane,
	const t_app_marketplace_view_model *market,
	const t_marketplace_state *state, int row, int cols, bool characters)
{
	const t_app_catalogue_view_model	*catalogue;
	char								line[APP_TEXT_MAX + 96];
	char								prefix[4];
	char								suffix[32];
	int									limit;
	int									index;
	int									rows;
	bool								focused;

	catalogue = characters ? &market->characters : &market->themes;
	rows = (int)ncplane_dim_y(plane);
	limit = rows - MARKET_CONTROLS_ROWS - MARKET_DETAIL_ROWS;
	if (row >= limit)
		return (row);
	(void)ncplane_set_fg_rgb8(plane, MARKET_PINK_R, MARKET_PINK_G,
		MARKET_PINK_B);
	put_line(plane, row, 3, cols - 6, characters ? "CHARACTERS" : "THEMES",
		true);
	row++;
	index = 0;
	while (index < catalogue->count && index < APP_CATALOGUE_MAX_ITEMS
		&& row < limit)
	{
		focused = state->section == (characters ? MARKETPLACE_SECTION_CHARACTERS
				: MARKETPLACE_SECTION_THEMES)
			&& (characters ? state->character_slot : state->theme_slot)
			== index;
		slot_prefix(prefix, sizeof(prefix), catalogue->items[index].equipped,
			focused, catalogue->items[index].owned);
		slot_suffix(&catalogue->items[index], market, suffix, sizeof(suffix));
		snprintf(line, sizeof(line), "%s%-28.28s %s", prefix,
			catalogue->items[index].name, suffix);
		set_slot_colour(plane, &catalogue->items[index], market, focused);
		put_line(plane, row, 4, cols - 8, line, focused);
		row++;
		index++;
	}
	if (index == 0 && row < limit)
	{
		put_line(plane, row, 4, cols - 8, "Nothing in stock", false);
		row++;
	}
	return (row);
}

/**
 * @brief Describes the focused item just above the control block.
 */
static void	draw_detail(struct ncplane *plane,
	const t_app_marketplace_view_model *market,
	const t_marketplace_state *state, int rows, int cols)
{
	const t_app_catalogue_item_view_model	*item;
	char									line[APP_ABILITY_TEXT_MAX + 96];
	int										row;
	int										index;

	item = marketplace_focused_item(market, state);
	row = rows - MARKET_CONTROLS_ROWS - MARKET_DETAIL_ROWS;
	if (item == NULL || row < 4)
		return ;
	(void)ncplane_set_fg_rgb8(plane, MARKET_PINK_R, MARKET_PINK_G,
		MARKET_PINK_B);
	snprintf(line, sizeof(line), "SELECTED: %s", item->name);
	put_line(plane, row, 3, cols - 6, line, true);
	if (item->equipped)
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_GREEN_R, MARKET_GREEN_G,
			MARKET_GREEN_B);
		snprintf(line, sizeof(line), "OWNED - EQUIPPED");
	}
	else if (item->owned)
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_GREEN_R, MARKET_GREEN_G,
			MARKET_GREEN_B);
		snprintf(line, sizeof(line), "OWNED - PRESS E TO EQUIP");
	}
	else if (marketplace_can_afford(market, item))
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
			MARKET_GOLD_B);
		snprintf(line, sizeof(line), "PRICE %d P   WALLET %d P   AFTER %d P",
			item->price, market->profile.wallet_points,
			market->profile.wallet_points - item->price);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_RED_R, MARKET_RED_G,
			MARKET_RED_B);
		snprintf(line, sizeof(line), "PRICE %d P   WALLET %d P   NEED %d MORE",
			item->price, market->profile.wallet_points,
			item->price - market->profile.wallet_points);
	}
	put_line(plane, row + 1, 3, cols - 6, line, false);
	(void)ncplane_set_fg_rgb8(plane, MARKET_CREAM_R, MARKET_CREAM_G,
		MARKET_CREAM_B);
	if (!marketplace_focused_is_character(state))
	{
		put_wrapped(plane, row + 2, 4, cols - 8,
			"A theme restyles the board, the tetromino tiles, and the music.",
			min_int(2, MARKET_DETAIL_ROWS - 2));
		return ;
	}
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT
		&& row + 2 + index < rows - MARKET_CONTROLS_ROWS)
	{
		snprintf(line, sizeof(line), "L%d %s: %s", index + 1,
			item->abilities[index].name, item->abilities[index].description);
		put_line(plane, row + 2 + index, 4, cols - 8, line, false);
		index++;
	}
}

/**
 * @brief States why nothing is on the shelves.
 */
static void	draw_unavailable(struct ncplane *plane,
	const t_app_screen_view_model *view, int rows, int cols)
{
	const char	*title;
	const char	*detail;

	title = "MARKETPLACE UNAVAILABLE";
	detail = view->subtitle;
	if (view->status == APP_DATA_LOADING)
	{
		title = "OPENING THE SHOP...";
		detail = "PLEASE WAIT";
	}
	else if (view->status == APP_DATA_EMPTY)
	{
		title = "NOTHING FOR SALE";
		detail = "THE CATALOGUE IS EMPTY";
	}
	else if (view->status == APP_DATA_ERROR)
	{
		title = "MARKETPLACE ERROR";
		detail = "TRY AGAIN LATER";
	}
	else if (view->data.marketplace.offline
		|| !view->data.marketplace.signed_in)
	{
		title = "MARKETPLACE NEEDS AN ACCOUNT";
		detail = view->data.marketplace.local_status;
	}
	(void)ncplane_set_fg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
		MARKET_GOLD_B);
	put_centered(plane, rows / 2 - 1, cols, title, true);
	(void)ncplane_set_fg_rgb8(plane, MARKET_LAVENDER_R, MARKET_LAVENDER_G,
		MARKET_LAVENDER_B);
	put_centered(plane, rows / 2 + 1, cols, detail, false);
}

static void	draw_controls(struct ncplane *plane,
	const t_app_marketplace_view_model *market,
	const t_marketplace_state *state)
{
	char	feedback[64];
	int		row;
	int		back_x;
	int		buy_x;
	int		down_x;
	int		up_x;

	button_geometry(plane, &row, &back_x, &buy_x, &down_x, &up_x);
	draw_button(plane, row, back_x, "BACK",
		state->focus == MARKETPLACE_FOCUS_BACK
		&& state->section == MARKETPLACE_SECTION_CONTROLS, true);
	draw_button(plane, row, buy_x, "BUY",
		state->focus == MARKETPLACE_FOCUS_BUY
		&& state->section == MARKETPLACE_SECTION_CONTROLS, market->signed_in);
	draw_button(plane, row, down_x, "VOL -",
		state->focus == MARKETPLACE_FOCUS_VOLUME_DOWN
		&& state->section == MARKETPLACE_SECTION_CONTROLS, true);
	draw_button(plane, row, up_x, "VOL +",
		state->focus == MARKETPLACE_FOCUS_VOLUME_UP
		&& state->section == MARKETPLACE_SECTION_CONTROLS, true);
	if (marketplace_feedback_text(state, feedback, sizeof(feedback)) != NULL)
	{
		set_feedback_colour(plane, state);
		put_centered(plane, row - 1, (int)ncplane_dim_x(plane), feedback,
			true);
	}
	else if (market->signed_in)
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_LAVENDER_R, MARKET_LAVENDER_G,
			MARKET_LAVENDER_B);
		put_centered(plane, row - 1, (int)ncplane_dim_x(plane),
			"UP INTO THE SHELVES    ENTER BUYS    E EQUIPS", false);
	}
	(void)ncplane_set_fg_rgb8(plane, MARKET_LAVENDER_R, MARKET_LAVENDER_G,
		MARKET_LAVENDER_B);
	put_centered(plane, row + 2, (int)ncplane_dim_x(plane),
		market->signed_in
		? "ARROWS MOVE  ENTER BUY  E EQUIP  B BUY  +/- VOLUME  ESC BACK"
		: "ARROWS MOVE  ENTER SELECT  +/- VOLUME  ESC BACK", false);
}

static void	draw_frame(struct ncplane *plane, int rows, int cols,
	bool compatibility)
{
	int	x;
	int	y;

	(void)ncplane_set_fg_rgb8(plane, MARKET_PINK_R, MARKET_PINK_G,
		MARKET_PINK_B);
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
		(void)ncplane_set_fg_rgb8(plane, MARKET_LAVENDER_R, MARKET_LAVENDER_G,
			MARKET_LAVENDER_B);
		(void)ncplane_putstr_yx(plane, 2, 3, "[] []");
		(void)ncplane_putstr_yx(plane, rows - 4, cols - 9, "[][]");
	}
}

/**
 * @brief Builds the two-column marker that opens a catalogue line.
 *
 * The cell panel has no colour budget to spare, so focus and ownership are
 * encoded positionally: the cursor column, then the ownership column.
 */
static void	slot_prefix(char *out, size_t size, bool equipped, bool focused,
	bool owned)
{
	snprintf(out, size, "%c%c", focused ? '>' : ' ',
		owned ? (equipped ? '*' : ' ') : '!');
}

/**
 * @brief Writes what one catalogue line would cost, or that it is already had.
 */
static void	slot_suffix(const t_app_catalogue_item_view_model *item,
	const t_app_marketplace_view_model *market, char *out, size_t size)
{
	if (item->equipped)
		snprintf(out, size, "[EQUIPPED]");
	else if (item->owned)
		snprintf(out, size, "[OWNED]");
	else if (item->price <= 0)
		snprintf(out, size, "[FREE]");
	else if (marketplace_can_afford(market, item))
		snprintf(out, size, "%d P", item->price);
	else
		snprintf(out, size, "%d P (SHORT)", item->price);
}

/**
 * @brief Colours one catalogue line: gold marks the cursor and nothing else.
 */
static void	set_slot_colour(struct ncplane *plane,
	const t_app_catalogue_item_view_model *item,
	const t_app_marketplace_view_model *market, bool focused)
{
	if (focused)
		(void)ncplane_set_fg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
			MARKET_GOLD_B);
	else if (item->owned)
		(void)ncplane_set_fg_rgb8(plane, MARKET_CREAM_R, MARKET_CREAM_G,
			MARKET_CREAM_B);
	else if (marketplace_can_afford(market, item))
		(void)ncplane_set_fg_rgb8(plane, MARKET_LAVENDER_R, MARKET_LAVENDER_G,
			MARKET_LAVENDER_B);
	else
		(void)ncplane_set_fg_rgb8(plane, MARKET_DISABLED_R, MARKET_DISABLED_G,
			MARKET_DISABLED_B);
}

/**
 * @brief Word-wraps text across at most max_lines rows of the cell panel.
 */
static void	put_wrapped(struct ncplane *plane, int row, int x, int width,
	const char *text, int max_lines)
{
	char	chunk[APP_ABILITY_TEXT_MAX + 4];
	int		start;
	int		cursor;
	int		last_space;
	int		drawn;

	if (text == NULL || width < 1)
		return ;
	start = 0;
	drawn = 0;
	while (text[start] != '\0' && drawn < max_lines)
	{
		cursor = start;
		last_space = -1;
		while (text[cursor] != '\0' && cursor - start < width)
		{
			if (text[cursor] == ' ')
				last_space = cursor;
			cursor++;
		}
		if (text[cursor] != '\0' && last_space > start)
			cursor = last_space;
		snprintf(chunk, sizeof(chunk), "%.*s", cursor - start, text + start);
		put_line(plane, row + drawn, x, width, chunk, false);
		start = cursor;
		while (text[start] == ' ')
			start++;
		drawn++;
	}
}

static void	put_line(struct ncplane *plane, int row, int x, int width,
	const char *text, bool bold)
{
	char	clipped[APP_ABILITY_TEXT_MAX + 112];
	int		rows;
	int		cols;

	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	if (text == NULL || row < 0 || row >= rows || x < 0 || x >= cols
		|| width < 1)
		return ;
	if (width > cols - x)
		width = cols - x;
	snprintf(clipped, sizeof(clipped), "%.*s",
		min_int(width, (int)sizeof(clipped) - 1), text);
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

	snprintf(label, sizeof(label), "[ %-8s ]", text);
	if (!enabled)
	{
		(void)ncplane_set_fg_rgb8(plane, 116, 111, 132);
		(void)ncplane_set_bg_rgb8(plane, MARKET_BG_R, MARKET_BG_G,
			MARKET_BG_B);
	}
	else if (focused)
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_BG_R, MARKET_BG_G,
			MARKET_BG_B);
		(void)ncplane_set_bg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
			MARKET_GOLD_B);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, MARKET_CREAM_R, MARKET_CREAM_G,
			MARKET_CREAM_B);
		(void)ncplane_set_bg_rgb8(plane, MARKET_INNER_R, MARKET_INNER_G,
			MARKET_INNER_B);
	}
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, label);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	button_geometry(const struct ncplane *plane, int *row,
	int *back_x, int *buy_x, int *down_x, int *up_x)
{
	int	cols;
	int	start;

	cols = (int)ncplane_dim_x(plane);
	*row = (int)ncplane_dim_y(plane) - 4;
	start = (cols - MARKET_BUTTON_SPAN) / 2;
	if (start < 1)
		start = 1;
	*back_x = start;
	*buy_x = *back_x + MARKET_BUTTON_CELLS + MARKET_BUTTON_GAP;
	*down_x = *buy_x + MARKET_BUTTON_CELLS + MARKET_BUTTON_GAP;
	*up_x = *down_x + MARKET_BUTTON_CELLS + MARKET_BUTTON_GAP;
}

/**
 * @brief Colours the result line by whether the action went through.
 */
static void	set_feedback_colour(struct ncplane *plane,
	const t_marketplace_state *state)
{
	if (state->feedback == MARKETPLACE_FEEDBACK_BOUGHT
		|| state->feedback == MARKETPLACE_FEEDBACK_EQUIPPED)
		(void)ncplane_set_fg_rgb8(plane, MARKET_GREEN_R, MARKET_GREEN_G,
			MARKET_GREEN_B);
	else if (state->feedback == MARKETPLACE_FEEDBACK_INSUFFICIENT
		|| state->feedback == MARKETPLACE_FEEDBACK_LOCKED)
		(void)ncplane_set_fg_rgb8(plane, MARKET_RED_R, MARKET_RED_G,
			MARKET_RED_B);
	else
		(void)ncplane_set_fg_rgb8(plane, MARKET_GOLD_R, MARKET_GOLD_G,
			MARKET_GOLD_B);
}

static int	min_int(int first, int second)
{
	return (first < second ? first : second);
}
