#include "tetrisu.h"

# define LB_BACK_TEXT		"  BACK  "
# define LB_REFRESH_TEXT	" REFRESH "
# define LB_HINT_TEXT		"ESC BACK  |  R REFRESH  |  LEFT/RIGHT SELECT"
# define LB_BG_R			16
# define LB_BG_G			9
# define LB_BG_B			31
# define LB_INNER_R			25
# define LB_INNER_G			14
# define LB_INNER_B			46
# define LB_PINK_R			255
# define LB_PINK_G			112
# define LB_PINK_B			190
# define LB_GOLD_R			255
# define LB_GOLD_G			203
# define LB_GOLD_B			102
# define LB_CREAM_R			250
# define LB_CREAM_G			242
# define LB_CREAM_B			221
# define LB_LAVENDER_R		190
# define LB_LAVENDER_G		155
# define LB_LAVENDER_B		218
# define LB_GREEN_R			112
# define LB_GREEN_G			214
# define LB_GREEN_B			174
# define LB_SILVER_R			190
# define LB_SILVER_G			205
# define LB_SILVER_B			224
# define LB_BRONZE_R			219
# define LB_BRONZE_G			145
# define LB_BRONZE_B			91

static bool	create_panel(render_ctx_t *ctx, bool compatibility,
				bool *compact);
static void	draw_leaderboard(render_ctx_t *ctx,
				const app_screen_view_model_t *view,
				const leaderboard_state_t *state, bool compatibility,
				bool compact);
static void	draw_frame(struct ncplane *plane, int rows, int cols,
				bool compatibility);
static void	draw_cell_backdrop(struct ncplane *plane, int rows, int cols);
static void	draw_header(struct ncplane *plane,
				const app_screen_view_model_t *view, int cols);
static void	draw_status(struct ncplane *plane,
				const app_screen_view_model_t *view, int rows, int cols);
static void	draw_rankings(struct ncplane *plane,
				const app_leaderboard_view_model_t *leaderboard,
				int rows, int cols, bool compact);
static void	draw_podium(struct ncplane *plane,
				const app_leaderboard_view_model_t *leaderboard, int cols);
static void	draw_podium_entry(struct ncplane *plane,
				const app_leaderboard_entry_view_model_t *entry,
				int center, int width, int place);
static void	fill_podium_card(struct ncplane *plane, int row, int center,
				int width, int place);
static void	draw_rank_list(struct ncplane *plane,
				const app_leaderboard_view_model_t *leaderboard,
				int first_row, int last_row, int cols);
static void	draw_compact_list(struct ncplane *plane,
				const app_leaderboard_view_model_t *leaderboard,
				int rows, int cols);
static void	draw_controls(struct ncplane *plane,
				const leaderboard_state_t *state, int rows, int cols);
static void	draw_button(struct ncplane *plane, int row, int x,
				const char *text, bool focused);
static void	button_geometry(const struct ncplane *plane, int *row,
				int *back_x, int *refresh_x);
static void	put_centered(struct ncplane *plane, int row, const char *text,
				int cols, bool bold);
static void	put_centered_range(struct ncplane *plane, int row,
				const char *text, int center, int width, bool bold);
static bool	button_hit(const ncinput *input, int row, int x, int width,
				int plane_y, int plane_x);
static int	min_int(int first, int second);

/**
 * @brief Draws the dedicated leaderboard over its authored competition art.
 *
 * Bitmap-capable terminals compose the backdrop, live data, and controls into
 * one shared-font pixel surface. Cell sessions, or bitmap sessions where that
 * presentation cannot load, draw a self-contained generic fallback with the
 * same data and controls.
 */
bool	render_leaderboard_show(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const leaderboard_state_t *state,
	bool rebuild_background)
{
	bool							compact;
	bool							compatibility;

	if (ctx == NULL || ctx->std == NULL || view == NULL || state == NULL)
		return (false);
	render_menu_destroy(ctx);
	if (!render_compatibility_mode(ctx)
		&& render_leaderboard_pixel_show(ctx, view, state,
			rebuild_background))
		return (true);
	render_screen_destroy(ctx);
	render_leaderboard_pixel_destroy(ctx);
	render_background_destroy(ctx);
	render_backdrop_forget(ctx);
	compatibility = true;
	if (!create_panel(ctx, compatibility, &compact))
		return (false);
	draw_leaderboard(ctx, view, state, compatibility, compact);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Resolves pointer hover/click coordinates to a leaderboard button.
 */
bool	render_leaderboard_hit_test(const render_ctx_t *ctx,
	const ncinput *input, leaderboard_focus_t *focus)
{
	int	plane_y;
	int	plane_x;
	int	row;
	int	back_x;
	int	refresh_x;

	if (ctx == NULL || ctx->screen_plane == NULL || input == NULL
		|| focus == NULL)
		return (false);
	if (ctx->leaderboard_pixel_active)
	{
		leaderboard_pixel_layout_t	layout;

		leaderboard_pixel_layout_build(ctx->bg_row, ctx->bg_col,
			ctx->bg_rows, ctx->bg_cols, ctx->cell_px_y, ctx->cell_px_x,
			&layout);
		return (leaderboard_pixel_hit_test(&layout, input->y, input->x,
				focus));
	}
	ncplane_yx(ctx->screen_plane, &plane_y, &plane_x);
	button_geometry(ctx->screen_plane, &row, &back_x, &refresh_x);
	if (button_hit(input, row, back_x, (int)strlen(LB_BACK_TEXT),
			plane_y, plane_x))
	{
		*focus = LEADERBOARD_FOCUS_BACK;
		return (true);
	}
	if (button_hit(input, row, refresh_x, (int)strlen(LB_REFRESH_TEXT),
			plane_y, plane_x))
	{
		*focus = LEADERBOARD_FOCUS_REFRESH;
		return (true);
	}
	return (false);
}

/**
 * @brief Removes the leaderboard plane.
 */
void	render_leaderboard_destroy(render_ctx_t *ctx)
{
	render_screen_destroy(ctx);
	render_leaderboard_pixel_destroy(ctx);
}

static bool	create_panel(render_ctx_t *ctx, bool compatibility, bool *compact)
{
	ncplane_options	options;
	leaderboard_layout_t	layout;
	uint64_t		channels;
	unsigned		std_rows;
	unsigned		std_cols;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	if (!leaderboard_layout_resolve((int)std_rows, (int)std_cols,
			compatibility, &layout))
		return (false);
	memset(&options, 0, sizeof(options));
	options.y = layout.y;
	options.x = layout.x;
	options.rows = layout.rows;
	options.cols = layout.cols;
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	*compact = layout.compact;
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, LB_CREAM_R, LB_CREAM_G,
		LB_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, LB_BG_R, LB_BG_G, LB_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	return (true);
}

static void	draw_leaderboard(render_ctx_t *ctx,
	const app_screen_view_model_t *view, const leaderboard_state_t *state,
	bool compatibility, bool compact)
{
	struct ncplane	*plane;
	int				rows;
	int				cols;

	plane = ctx->screen_plane;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	draw_frame(plane, rows, cols, compatibility);
	draw_header(plane, view, cols);
	if (view->status == APP_DATA_READY && view->data.leaderboard.count > 0)
		draw_rankings(plane, &view->data.leaderboard, rows, cols, compact);
	else
		draw_status(plane, view, rows, cols);
	draw_controls(plane, state, rows, cols);
}

static void	draw_frame(struct ncplane *plane, int rows, int cols,
	bool compatibility)
{
	int	x;
	int	y;

	(void)ncplane_set_fg_rgb8(plane, LB_PINK_R, LB_PINK_G, LB_PINK_B);
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
	if (compatibility)
		draw_cell_backdrop(plane, rows, cols);
}

/**
 * @brief Adds a restrained, bitmap-free competition motif to cell mode.
 */
static void	draw_cell_backdrop(struct ncplane *plane, int rows, int cols)
{
	int	x;

	(void)ncplane_set_fg_rgb8(plane, LB_LAVENDER_R, LB_LAVENDER_G,
		LB_LAVENDER_B);
	x = 2;
	while (x < cols - 2)
	{
		(void)ncplane_putchar_yx(plane, 3, x, x % 4 == 0 ? '+' : '.');
		x++;
	}
	(void)ncplane_set_fg_rgb8(plane, LB_PINK_R, LB_PINK_G, LB_PINK_B);
	(void)ncplane_putstr_yx(plane, 5, 2, "[]");
	(void)ncplane_putstr_yx(plane, 8, cols - 6, "[][]");
	(void)ncplane_putstr_yx(plane, rows - 7, 2, "[][]");
	(void)ncplane_putstr_yx(plane, rows - 9, cols - 5, "[]");
	(void)ncplane_set_fg_rgb8(plane, LB_GOLD_R, LB_GOLD_G, LB_GOLD_B);
	(void)ncplane_putchar_yx(plane, 3, cols / 2, '*');
	(void)ncplane_putchar_yx(plane, rows - 3, cols / 2, '*');
}

static void	draw_header(struct ncplane *plane,
	const app_screen_view_model_t *view, int cols)
{
	(void)ncplane_set_fg_rgb8(plane, LB_PINK_R, LB_PINK_G, LB_PINK_B);
	put_centered(plane, 1, ":: LEADERBOARD ::", cols, true);
	if (view->local_preview)
	{
		(void)ncplane_set_fg_rgb8(plane, LB_GOLD_R, LB_GOLD_G, LB_GOLD_B);
		put_centered(plane, 2, "LOCAL UI PREVIEW", cols, true);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, LB_LAVENDER_R, LB_LAVENDER_G,
			LB_LAVENDER_B);
		put_centered(plane, 2, "GLOBAL TOP SCORES", cols, false);
	}
}

static void	draw_status(struct ncplane *plane,
	const app_screen_view_model_t *view, int rows, int cols)
{
	const char	*title;
	const char	*detail;

	title = "SCORES COULD NOT BE LOADED";
	detail = "CHECK THE SERVER AND TRY REFRESH";
	if (view->status == APP_DATA_LOADING)
	{
		title = "FETCHING THE LATEST SCORES...";
		detail = "PLEASE WAIT";
	}
	else if (view->status == APP_DATA_EMPTY
		|| (view->status == APP_DATA_READY
			&& view->data.leaderboard.count == 0))
	{
		title = "NO SCORES YET";
		detail = "THE FIRST GREAT RUN COULD BE YOURS";
	}
	else if (view->status == APP_DATA_UNAVAILABLE)
	{
		title = "LEADERBOARD OFFLINE";
		detail = "THE SCORE SERVER IS UNAVAILABLE";
	}
	(void)ncplane_set_fg_rgb8(plane, LB_GOLD_R, LB_GOLD_G, LB_GOLD_B);
	put_centered(plane, rows / 2 - 1, title, cols, true);
	(void)ncplane_set_fg_rgb8(plane, LB_LAVENDER_R, LB_LAVENDER_G,
		LB_LAVENDER_B);
	put_centered(plane, rows / 2 + 1, detail, cols, false);
}

static void	draw_rankings(struct ncplane *plane,
	const app_leaderboard_view_model_t *leaderboard, int rows, int cols,
	bool compact)
{
	if (!compact)
	{
		draw_podium(plane, leaderboard, cols);
		draw_rank_list(plane, leaderboard, 12, rows - 6, cols);
	}
	else
		draw_compact_list(plane, leaderboard, rows, cols);
}

static void	draw_podium(struct ncplane *plane,
	const app_leaderboard_view_model_t *leaderboard, int cols)
{
	int	width;

	width = min_int(24, cols / 3 - 2);
	draw_podium_entry(plane, leaderboard_entry_for_position(leaderboard, 2),
		cols / 4, width, 2);
	draw_podium_entry(plane, leaderboard_entry_for_position(leaderboard, 1),
		cols / 2, width, 1);
	draw_podium_entry(plane, leaderboard_entry_for_position(leaderboard, 3),
		cols * 3 / 4, width, 3);
}

static void	draw_podium_entry(struct ncplane *plane,
	const app_leaderboard_entry_view_model_t *entry, int center, int width,
	int place)
{
	char	score[32];
	char	label[16];
	int		row;

	if (entry == NULL)
		return ;
	row = place == 1 ? 4 : 5;
	fill_podium_card(plane, row, center, width, place);
	if (place == 1)
		(void)ncplane_set_fg_rgb8(plane, LB_GOLD_R, LB_GOLD_G, LB_GOLD_B);
	else if (place == 2)
		(void)ncplane_set_fg_rgb8(plane, LB_SILVER_R, LB_SILVER_G,
			LB_SILVER_B);
	else
		(void)ncplane_set_fg_rgb8(plane, LB_BRONZE_R, LB_BRONZE_G,
			LB_BRONZE_B);
	snprintf(label, sizeof(label), "[ %d%s ]", place,
		place == 1 ? "ST" : (place == 2 ? "ND" : "RD"));
	put_centered_range(plane, row, label, center, width, true);
	(void)ncplane_set_fg_rgb8(plane, LB_CREAM_R, LB_CREAM_G, LB_CREAM_B);
	put_centered_range(plane, row + 1, entry->username, center, width, true);
	snprintf(score, sizeof(score), "%" PRIu64, entry->score);
	(void)ncplane_set_fg_rgb8(plane, LB_LAVENDER_R, LB_LAVENDER_G,
		LB_LAVENDER_B);
	put_centered_range(plane, row + 2, score, center, width, false);
	(void)ncplane_set_fg_rgb8(plane, place == 1 ? LB_GOLD_R
		: (place == 2 ? LB_SILVER_R : LB_BRONZE_R), place == 1 ? LB_GOLD_G
		: (place == 2 ? LB_SILVER_G : LB_BRONZE_G), place == 1 ? LB_GOLD_B
		: (place == 2 ? LB_SILVER_B : LB_BRONZE_B));
	put_centered_range(plane, row + 3, place == 1
		? "==========" : "--------", center, width, false);
	(void)ncplane_set_bg_rgb8(plane, LB_BG_R, LB_BG_G, LB_BG_B);
}

static void	fill_podium_card(struct ncplane *plane, int row, int center,
	int width, int place)
{
	int	start;
	int	end;
	int	x;
	int	y;

	if (place == 1)
		(void)ncplane_set_bg_rgb8(plane, 69, 48, 20);
	else if (place == 2)
		(void)ncplane_set_bg_rgb8(plane, 38, 43, 61);
	else
		(void)ncplane_set_bg_rgb8(plane, 65, 33, 32);
	start = center - width / 2;
	end = start + width;
	y = row;
	while (y <= row + 3)
	{
		x = start;
		while (x < end)
		{
			(void)ncplane_putchar_yx(plane, y, x, ' ');
			x++;
		}
		y++;
	}
}

static void	draw_rank_list(struct ncplane *plane,
	const app_leaderboard_view_model_t *leaderboard, int first_row,
	int last_row, int cols)
{
	const app_leaderboard_entry_view_model_t	*entry;
	char										line[APP_TEXT_MAX + 48];
	int											position;
	int											row;

	(void)ncplane_set_fg_rgb8(plane, LB_PINK_R, LB_PINK_G, LB_PINK_B);
	put_centered(plane, first_row, "RANK       PLAYER              SCORE",
		cols, true);
	position = 4;
	row = first_row + 1;
	while (position <= APP_LEADERBOARD_MAX_ENTRIES && row <= last_row)
	{
		entry = leaderboard_entry_for_position(leaderboard, position);
		if (entry != NULL)
		{
			snprintf(line, sizeof(line), "%2d         %-18.18s %12" PRIu64,
				entry->position, entry->username, entry->score);
			(void)ncplane_set_fg_rgb8(plane, position % 2 == 0
				? LB_CREAM_R : LB_LAVENDER_R, position % 2 == 0
				? LB_CREAM_G : LB_LAVENDER_G, position % 2 == 0
				? LB_CREAM_B : LB_LAVENDER_B);
			put_centered(plane, row, line, cols, false);
		}
		position++;
		row++;
	}
}

static void	draw_compact_list(struct ncplane *plane,
	const app_leaderboard_view_model_t *leaderboard, int rows, int cols)
{
	const app_leaderboard_entry_view_model_t	*entry;
	char										line[APP_TEXT_MAX + 40];
	int											position;
	int											row;
	int											last_row;

	(void)ncplane_set_fg_rgb8(plane, LB_PINK_R, LB_PINK_G, LB_PINK_B);
	put_centered(plane, 4, "RANK   PLAYER                 SCORE",
		cols, true);
	position = 1;
	row = 5;
	last_row = rows - 5;
	while (position <= APP_LEADERBOARD_MAX_ENTRIES && row <= last_row)
	{
		entry = leaderboard_entry_for_position(leaderboard, position);
		if (entry != NULL)
		{
			snprintf(line, sizeof(line), "%2d     %-18.18s %12" PRIu64,
				entry->position, entry->username, entry->score);
			if (position <= 3)
				(void)ncplane_set_fg_rgb8(plane, position == 1 ? LB_GOLD_R
					: (position == 2 ? LB_SILVER_R : LB_BRONZE_R),
					position == 1 ? LB_GOLD_G : (position == 2 ? LB_SILVER_G
					: LB_BRONZE_G),
					position == 1 ? LB_GOLD_B : (position == 2 ? LB_SILVER_B
					: LB_BRONZE_B));
			else
				(void)ncplane_set_fg_rgb8(plane, position % 2 == 0
					? LB_CREAM_R : LB_LAVENDER_R, position % 2 == 0
					? LB_CREAM_G : LB_LAVENDER_G, position % 2 == 0
					? LB_CREAM_B : LB_LAVENDER_B);
			put_centered(plane, row, line, cols, position <= 3);
			row++;
		}
		position++;
	}
}

static void	draw_controls(struct ncplane *plane,
	const leaderboard_state_t *state, int rows, int cols)
{
	int	row;
	int	back_x;
	int	refresh_x;

	button_geometry(plane, &row, &back_x, &refresh_x);
	draw_button(plane, row, back_x, LB_BACK_TEXT,
		state->focus == LEADERBOARD_FOCUS_BACK);
	draw_button(plane, row, refresh_x, LB_REFRESH_TEXT,
		state->focus == LEADERBOARD_FOCUS_REFRESH);
	(void)ncplane_set_fg_rgb8(plane, LB_LAVENDER_R, LB_LAVENDER_G,
		LB_LAVENDER_B);
	(void)ncplane_set_bg_rgb8(plane, LB_BG_R, LB_BG_G, LB_BG_B);
	put_centered(plane, rows - 2, LB_HINT_TEXT, cols, false);
}

static void	draw_button(struct ncplane *plane, int row, int x,
	const char *text, bool focused)
{
	if (focused)
	{
		(void)ncplane_set_fg_rgb8(plane, LB_BG_R, LB_BG_G, LB_BG_B);
		(void)ncplane_set_bg_rgb8(plane, LB_GOLD_R, LB_GOLD_G, LB_GOLD_B);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, LB_CREAM_R, LB_CREAM_G,
			LB_CREAM_B);
		(void)ncplane_set_bg_rgb8(plane, LB_INNER_R, LB_INNER_G,
			LB_INNER_B);
	}
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, text);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	button_geometry(const struct ncplane *plane, int *row,
	int *back_x, int *refresh_x)
{
	int	rows;
	int	cols;
	int	gap;

	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	*row = rows - 4;
	gap = 8;
	*back_x = (cols - (int)strlen(LB_BACK_TEXT)
			- (int)strlen(LB_REFRESH_TEXT) - gap) / 2;
	*refresh_x = *back_x + (int)strlen(LB_BACK_TEXT) + gap;
}

static void	put_centered(struct ncplane *plane, int row, const char *text,
	int cols, bool bold)
{
	put_centered_range(plane, row, text, cols / 2, cols - 4, bold);
}

static void	put_centered_range(struct ncplane *plane, int row,
	const char *text, int center, int width, bool bold)
{
	char	clipped[APP_TEXT_MAX + 64];
	int		rows;
	int		cols;
	int		x;

	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	if (text == NULL || row < 0 || row >= rows || width < 1)
		return ;
	snprintf(clipped, sizeof(clipped), "%.*s", min_int(width,
			(int)sizeof(clipped) - 1), text);
	x = center - (int)strlen(clipped) / 2;
	if (x < 1)
		x = 1;
	if (x + (int)strlen(clipped) >= cols)
		x = cols - (int)strlen(clipped) - 1;
	if (bold)
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, clipped);
	if (bold)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static bool	button_hit(const ncinput *input, int row, int x, int width,
	int plane_y, int plane_x)
{
	return (input->y == plane_y + row && input->x >= plane_x + x
		&& input->x < plane_x + x + width);
}

static int	min_int(int first, int second)
{
	if (first < second)
		return (first);
	return (second);
}
