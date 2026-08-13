#include "tetrisu.h"

# define MP_BG_R	16
# define MP_BG_G	9
# define MP_BG_B	31
# define MP_PINK_R	255
# define MP_PINK_G	112
# define MP_PINK_B	190
# define MP_GOLD_R	255
# define MP_GOLD_G	203
# define MP_GOLD_B	102
# define MP_CREAM_R	250
# define MP_CREAM_G	242
# define MP_CREAM_B	221
# define MP_LAVENDER_R	190
# define MP_LAVENDER_G	155
# define MP_LAVENDER_B	218
# define MP_GREEN_R	112
# define MP_GREEN_G	214
# define MP_GREEN_B	174
# define MP_AMBER_R	255
# define MP_AMBER_G	176
# define MP_AMBER_B	84
# define MP_RED_R	255
# define MP_RED_G	111
# define MP_RED_B	142
# define MP_DISABLED_R	105
# define MP_DISABLED_G	99
# define MP_DISABLED_B	120

/*
 * The lobby table is the widest thing any of these screens draws, and the
 * minimum below is exactly what its columns plus the frame occupy. Cell mode
 * stacks the join field under the table rather than beside it, which is what
 * keeps that minimum inside a conventional 80-column terminal.
 */
/* MP_COMPAT_MIN_COLS is in tetrisu.h: the legend has to fit inside it. */
# define MP_COMPAT_MIN_ROWS	20
# define MP_COL_ID		4
# define MP_COL_MODE	18
# define MP_COL_PLAYERS	24
# define MP_COL_STATE	33
# define MP_COL_OWNER	44

// Static Functions
static bool	show_screen(t_render_ctx *ctx,
				const t_app_screen_view_model *view, const void *state,
				bool rebuild_background);
static bool	create_panel(t_render_ctx *ctx);
static bool	show_too_small(t_render_ctx *ctx);
static void	draw_frame(struct ncplane *plane, int rows, int cols,
				bool compatibility);
static void	draw_mode(struct ncplane *plane, const t_mp_mode_state *state,
				int rows, int cols);
static void	draw_lobby(struct ncplane *plane,
				const t_app_screen_view_model *view, const t_lobby_state *state,
				int rows, int cols);
static void	draw_create_room(struct ncplane *plane,
				const t_create_room_state *state, int rows, int cols);
static void	draw_waiting_room(struct ncplane *plane,
				const t_app_screen_view_model *view,
				const t_waiting_room_state *state, int rows, int cols);
static int	draw_room_table(struct ncplane *plane,
				const t_app_lobby_view_model *lobby, const t_lobby_state *state,
				int row, int cols, int limit);
static void	draw_join_field(struct ncplane *plane, const t_lobby_state *state,
				int row, int cols);
static int	draw_chat(struct ncplane *plane, const t_app_room_view_model *room,
				const t_waiting_room_state *state, int row, int cols,
				int limit);
static void	set_colour(struct ncplane *plane, int red, int green, int blue);
static void	set_room_state_colour(struct ncplane *plane,
				const t_app_room_summary_view_model *room);
static void	put_line(struct ncplane *plane, int row, int x, int width,
				const char *text);
static void	put_centered(struct ncplane *plane, int row, int cols,
				const char *text, bool bold);
static void	put_rule(struct ncplane *plane, int row, int cols);
static int	min_int(int first, int second);

/**
 * @brief Draws the mode picker through the compositor or the cell fallback.
 */
bool	render_mp_mode_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_mp_mode_state *state,
	bool rebuild_background)
{
	return (show_screen(ctx, view, state, rebuild_background));
}

/**
 * @brief Draws the lobby through the compositor or the cell fallback.
 */
bool	render_lobby_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_lobby_state *state,
	bool rebuild_background)
{
	return (show_screen(ctx, view, state, rebuild_background));
}

/**
 * @brief Draws the create-room panel through the compositor or the fallback.
 */
bool	render_create_room_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_create_room_state *state,
	bool rebuild_background)
{
	return (show_screen(ctx, view, state, rebuild_background));
}

/**
 * @brief Draws the waiting room through the compositor or the cell fallback.
 */
bool	render_waiting_room_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view, const t_waiting_room_state *state,
	bool rebuild_background)
{
	return (show_screen(ctx, view, state, rebuild_background));
}

/**
 * @brief Releases multiplayer-owned planes without touching the backdrop.
 */
void	render_multiplayer_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	render_screen_destroy(ctx);
	render_mp_pixel_destroy(ctx);
}

/**
 * @brief Routes one multiplayer frame to the bitmap or the cell renderer.
 *
 * The cell branch is intentionally self-contained for deterministic terminals
 * and TETRISU_RENDERER=cell. A failed bitmap composition degrades to it rather
 * than failing the screen: the loops treat false as fatal, and opening a lobby
 * must never be able to quit the client.
 */
static bool	show_screen(t_render_ctx *ctx, const t_app_screen_view_model *view,
	const void *state, bool rebuild_background)
{
	int	rows;
	int	cols;

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
		&& render_mp_pixel_show(ctx, view, state, rebuild_background))
		return (true);
	render_screen_destroy(ctx);
	render_mp_pixel_destroy(ctx);
	render_background_destroy(ctx);
	if (!create_panel(ctx))
		return (show_too_small(ctx));
	rows = (int)ncplane_dim_y(ctx->screen_plane);
	cols = (int)ncplane_dim_x(ctx->screen_plane);
	draw_frame(ctx->screen_plane, rows, cols, render_compatibility_mode(ctx));
	if (view->screen == APP_SCREEN_MULTIPLAYER_MODE)
		draw_mode(ctx->screen_plane, state, rows, cols);
	else if (view->screen == APP_SCREEN_LOBBY)
		draw_lobby(ctx->screen_plane, view, state, rows, cols);
	else if (view->screen == APP_SCREEN_CREATE_ROOM_MODAL)
		draw_create_room(ctx->screen_plane, state, rows, cols);
	else if (view->screen == APP_SCREEN_WAITING_ROOM)
		draw_waiting_room(ctx->screen_plane, view, state, rows, cols);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
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
	if (std_rows < MP_COMPAT_MIN_ROWS || std_cols < MP_COMPAT_MIN_COLS)
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
	(void)ncchannels_set_fg_rgb8(&channels, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, MP_BG_R, MP_BG_G, MP_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	return (true);
}

/**
 * @brief Explains that the terminal is smaller than the panel needs.
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
	(void)ncchannels_set_fg_rgb8(&channels, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	(void)ncchannels_set_bg_rgb8(&channels, MP_BG_R, MP_BG_G, MP_BG_B);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	snprintf(notice, sizeof(notice), "MULTIPLAYER NEEDS %dx%d",
		MP_COMPAT_MIN_COLS, MP_COMPAT_MIN_ROWS);
	set_colour(ctx->screen_plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
	put_centered(ctx->screen_plane, 0, (int)std_cols, notice, true);
	set_colour(ctx->screen_plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	put_centered(ctx->screen_plane, 1, (int)std_cols, "RESIZE OR ESC", false);
	ncplane_move_top(ctx->screen_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Draws the panel border, in ASCII when the terminal cannot do better.
 */
static void	draw_frame(struct ncplane *plane, int rows, int cols,
	bool compatibility)
{
	uint64_t	channels;

	if (rows < 2 || cols < 2)
		return ;
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	(void)ncchannels_set_bg_rgb8(&channels, MP_BG_R, MP_BG_G, MP_BG_B);
	if (compatibility)
		(void)ncplane_perimeter_rounded(plane, 0, channels, 0);
	else
		(void)ncplane_perimeter_double(plane, 0, channels, 0);
}

/**
 * @brief Draws the mode picker's two options as a cell list.
 */
static void	draw_mode(struct ncplane *plane, const t_mp_mode_state *state,
	int rows, int cols)
{
	char	line[APP_TEXT_MAX * 2];
	int		index;
	int		row;

	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	put_centered(plane, 1, cols, ":: MULTIPLAYER ::", true);
	set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	put_centered(plane, 2, cols, "HOW DO YOU WANT TO PLAY?", false);
	row = 4;
	index = 0;
	while (index < MP_MODE_CARD_COUNT && row + 2 < rows - 2)
	{
		if ((int)state->focus == index)
			set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		else
			set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
		snprintf(line, sizeof(line), "%s %-20s %s",
			(int)state->focus == index ? ">" : " ", mp_mode_card_name(index),
			mp_mode_card_players(index));
		put_line(plane, row, 2, cols - 4, line);
		set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
		put_line(plane, row + 1, 4, cols - 6, mp_mode_card_line(index, 0));
		row += 3;
		index++;
	}
	mp_feedback_text(state->feedback, state->feedback_value, line,
		sizeof(line));
	if (line[0] != '\0')
	{
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		put_centered(plane, rows - 3, cols, line, false);
	}
	set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	put_centered(plane, rows - 2, cols,
		"[UP/DOWN] CHOOSE  [ENTER] CONTINUE  [B] BACK", false);
}

/**
 * @brief Draws the lobby stacked: identity, room table, join field, status.
 */
static void	draw_lobby(struct ncplane *plane,
	const t_app_screen_view_model *view, const t_lobby_state *state, int rows,
	int cols)
{
	const t_app_profile_view_model	*profile;
	char							line[APP_TEXT_MAX * 3];
	int								row;
	int								limit;

	profile = &view->data.lobby.profile;
	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	put_centered(plane, 1, cols, ":: TETRISH LOBBY ::", true);
	set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	snprintf(line, sizeof(line), "%s    %" PRIu64 " pts    #%d",
		profile->username[0] != '\0' ? profile->username : "-",
		profile->score, profile->rank);
	put_centered(plane, 2, cols, line, false);
	put_rule(plane, 3, cols);
	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	put_line(plane, 4, 2, cols - 4, lobby_filter_name(state->filter));
	set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	put_line(plane, 5, 2 + MP_COL_ID, cols - 4, "ID");
	put_line(plane, 5, 2 + MP_COL_MODE, cols - 4, "MODE");
	put_line(plane, 5, 2 + MP_COL_PLAYERS, cols - 4, "PLAY");
	put_line(plane, 5, 2 + MP_COL_STATE, cols - 4, "STATE");
	put_line(plane, 5, 2 + MP_COL_OWNER, cols - 4, "OWNER");
	/* The table stops five rows short so the field, status and legend fit. */
	limit = rows - 8;
	row = draw_room_table(plane, &view->data.lobby, state, 6, cols, limit);
	draw_join_field(plane, state, min_int(row + 1, rows - 5), cols);
	lobby_feedback_text(state, line, sizeof(line));
	if (line[0] != '\0')
	{
		if (state->feedback == LOBBY_FEEDBACK_FULL
			|| state->feedback == LOBBY_FEEDBACK_IN_GAME
			|| state->feedback == LOBBY_FEEDBACK_UNKNOWN_ID)
			set_colour(plane, MP_RED_R, MP_RED_G, MP_RED_B);
		else
			set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		put_centered(plane, rows - 3, cols, line, false);
	}
	set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	put_centered(plane, rows - 2, cols,
		"[UP/DN] SEL [ENTER] JOIN [C] NEW [R] REFRESH [M] MODE [B] BACK",
		false);
}

/**
 * @brief Draws as many room rows as the panel has space for.
 *
 * @return The row after the last one drawn.
 */
static int	draw_room_table(struct ncplane *plane,
	const t_app_lobby_view_model *lobby, const t_lobby_state *state, int row,
	int cols, int limit)
{
	const t_app_room_summary_view_model	*room;
	char								cell[APP_TEXT_MAX];
	int									index;

	index = state->list_offset;
	while (index < state->list_offset + LOBBY_VISIBLE_ROOMS && row < limit)
	{
		room = lobby_visible_room(lobby, state->filter, index);
		if (room == NULL)
			break ;
		if (state->section == LOBBY_SECTION_ROOMS && state->selected == index)
		{
			set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
			put_line(plane, row, 2, 2, ">");
		}
		else
			set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
		put_line(plane, row, 2 + MP_COL_ID, MP_COL_MODE - MP_COL_ID - 1,
			room->id);
		set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
		put_line(plane, row, 2 + MP_COL_MODE, MP_COL_PLAYERS - MP_COL_MODE - 1,
			lobby_mode_tag(room->mode));
		snprintf(cell, sizeof(cell), "%d/%d", room->players, room->capacity);
		if (room->capacity > 0 && room->players >= room->capacity)
			set_colour(plane, MP_RED_R, MP_RED_G, MP_RED_B);
		else
			set_colour(plane, MP_GREEN_R, MP_GREEN_G, MP_GREEN_B);
		put_line(plane, row, 2 + MP_COL_PLAYERS,
			MP_COL_STATE - MP_COL_PLAYERS - 1, cell);
		set_room_state_colour(plane, room);
		put_line(plane, row, 2 + MP_COL_STATE, MP_COL_OWNER - MP_COL_STATE - 1,
			lobby_state_tag(room->state));
		set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
		put_line(plane, row, 2 + MP_COL_OWNER, cols - 4 - MP_COL_OWNER,
			room->owner);
		row++;
		index++;
	}
	if (index == state->list_offset && row < limit)
	{
		set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
		put_line(plane, row, 2 + MP_COL_ID, cols - 6,
			"No rooms here yet - press C to open one.");
		row++;
	}
	return (row);
}

/**
 * @brief Draws the join-by-id field, showing the caret only when focused.
 */
static void	draw_join_field(struct ncplane *plane, const t_lobby_state *state,
	int row, int cols)
{
	char	line[LOBBY_ROOM_ID_MAX + 40];
	bool	focused;

	focused = state->section == LOBBY_SECTION_JOIN;
	if (focused)
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
	else
		set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
	snprintf(line, sizeof(line), "%s JOIN BY ROOM ID [ %s%s ]",
		focused ? ">" : " ",
		state->room_id[0] != '\0' ? state->room_id : "",
		focused ? "_" : "");
	put_line(plane, row, 2, cols - 4, line);
}

/**
 * @brief Draws the create-room panel's two options as a cell list.
 */
static void	draw_create_room(struct ncplane *plane,
	const t_create_room_state *state, int rows, int cols)
{
	char	line[APP_TEXT_MAX * 2];
	int		index;
	int		row;

	set_colour(plane, MP_GREEN_R, MP_GREEN_G, MP_GREEN_B);
	put_centered(plane, 1, cols, ":: CREATE ROOM ::", true);
	set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	put_line(plane, 3, 2, cols - 4, "SELECT MODE");
	row = 5;
	index = 0;
	while (index < MP_MODE_CARD_COUNT && row < rows - 3)
	{
		if (create_room_focused_index(state) == index)
			set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		else
			set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
		snprintf(line, sizeof(line), "%s %-20s %-16s%s",
			create_room_focused_index(state) == index ? ">" : " ",
			mp_mode_card_name(index), mp_mode_card_players(index),
			index == 0 ? "(default)" : "");
		put_line(plane, row, 2, cols - 4, line);
		row += 2;
		index++;
	}
	mp_feedback_text(state->feedback, state->feedback_value, line,
		sizeof(line));
	if (line[0] != '\0')
	{
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		put_centered(plane, rows - 3, cols, line, false);
	}
	set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	put_centered(plane, rows - 2, cols,
		"[1/2] SELECT  [ENTER] CREATE  [ESC] CANCEL", false);
}

/**
 * @brief Draws the waiting room stacked: seats, status, then the transcript.
 */
static void	draw_waiting_room(struct ncplane *plane,
	const t_app_screen_view_model *view, const t_waiting_room_state *state,
	int rows, int cols)
{
	const t_app_room_view_model	*room;
	char						line[APP_TEXT_MAX * 3];
	int							seats;
	int							visible;
	int							start;
	int							slot;
	int							index;
	int							row;

	room = &view->data.room;
	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	snprintf(line, sizeof(line), ":: ROOM %s (%s) ::",
		room->id[0] != '\0' ? room->id : "-",
		room->mode == APP_GAME_MODE_BATTLE_ROYALE ? "BR" : "DOUBLE");
	put_centered(plane, 1, cols, line, true);
	set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	snprintf(line, sizeof(line), "Share this id so a friend can join: %s",
		room->id[0] != '\0' ? room->id : "-");
	put_centered(plane, 2, cols, line, false);
	put_rule(plane, 3, cols);
	seats = waiting_room_slot_count(room);
	visible = waiting_room_visible_slot_count(room);
	start = state->roster_offset;
	snprintf(line, sizeof(line), "SLOTS (%d/%d) - %d-%d", room->player_count,
		seats, visible > 0 ? start + 1 : 0, start + visible);
	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	put_line(plane, 4, 2, cols - 4, line);
	row = 5;
	index = 0;
	while (index < visible && row < rows - 7)
	{
		slot = start + index;
		if (slot == state->roster_cursor)
			set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		else if (slot < room->player_count)
			set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
		else
			set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
		waiting_room_slot_label(room, slot, line, sizeof(line));
		put_line(plane, row, 1, 2, slot == state->roster_cursor ? ">" : " ");
		put_line(plane, row, 3, 30, line);
		if (slot < room->player_count)
		{
			if (waiting_room_seat_ready(room, slot))
				set_colour(plane, MP_GREEN_R, MP_GREEN_G, MP_GREEN_B);
			else
				set_colour(plane, MP_AMBER_R, MP_AMBER_G, MP_AMBER_B);
			put_line(plane, row, 35, 14, waiting_room_badge_text(room, slot));
		}
		row++;
		index++;
	}
	waiting_room_status_text(room, state, line, sizeof(line));
	if (state->counting_down)
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
	else if (waiting_room_can_start(room))
		set_colour(plane, MP_GREEN_R, MP_GREEN_G, MP_GREEN_B);
	else
		set_colour(plane, MP_AMBER_R, MP_AMBER_G, MP_AMBER_B);
	put_line(plane, row + 1, 2, cols - 4, line);
	waiting_room_feedback_text(state, line, sizeof(line));
	if (line[0] != '\0')
	{
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		put_line(plane, row + 2, 2, cols - 4, line);
	}
	(void)draw_chat(plane, room, state, row + 3, cols, rows - 2);
	if (state->character_name[0] != '\0')
	{
		set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
		snprintf(line, sizeof(line), "FIGHTER  <  %s  >",
			state->character_name);
		put_centered(plane, rows - 3, cols, line, false);
	}
	set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
	put_centered(plane, rows - 2, cols, waiting_room_legend(cols), false);
}

/**
 * @brief Draws the transcript tail and the composer line beneath it.
 *
 * @return The row after the composer.
 */
static int	draw_chat(struct ncplane *plane, const t_app_room_view_model *room,
	const t_waiting_room_state *state, int row, int cols, int limit)
{
	char	line[APP_TEXT_MAX + APP_ROOM_CHAT_TEXT_MAX + 8];
	int		visible;
	int		first;
	int		index;

	if (row >= limit)
		return (row);
	set_colour(plane, MP_PINK_R, MP_PINK_G, MP_PINK_B);
	put_line(plane, row, 2, cols - 4, "ROOM CHAT");
	row++;
	/* One row is held back for the composer, whatever the transcript length. */
	visible = limit - row - 1;
	if (visible < 0)
		visible = 0;
	first = room->chat_count > visible ? room->chat_count - visible : 0;
	index = first;
	while (index < room->chat_count && index < APP_ROOM_CHAT_MAX
		&& row < limit - 1)
	{
		if (room->chat[index].system)
		{
			set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
			snprintf(line, sizeof(line), "* %s", room->chat[index].text);
		}
		else
		{
			set_colour(plane, MP_CREAM_R, MP_CREAM_G, MP_CREAM_B);
			snprintf(line, sizeof(line), "%s: %s", room->chat[index].author,
				room->chat[index].text);
		}
		put_line(plane, row, 3, cols - 6, line);
		row++;
		index++;
	}
	if (state->chatting)
	{
		set_colour(plane, MP_GOLD_R, MP_GOLD_G, MP_GOLD_B);
		snprintf(line, sizeof(line), "> %s_", state->compose);
	}
	else
	{
		set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
		snprintf(line, sizeof(line), "[C] type a message");
	}
	put_line(plane, row, 3, cols - 6, line);
	return (row + 1);
}

static void	set_colour(struct ncplane *plane, int red, int green, int blue)
{
	(void)ncplane_set_fg_rgb8(plane, red, green, blue);
}

static void	set_room_state_colour(struct ncplane *plane,
	const t_app_room_summary_view_model *room)
{
	if (room->state == APP_ROOM_STATE_IN_GAME
		|| room->state == APP_ROOM_STATE_FINISHED)
		set_colour(plane, MP_DISABLED_R, MP_DISABLED_G, MP_DISABLED_B);
	else if (room->capacity > 0 && room->players >= room->capacity)
		set_colour(plane, MP_RED_R, MP_RED_G, MP_RED_B);
	else
		set_colour(plane, MP_GREEN_R, MP_GREEN_G, MP_GREEN_B);
}

/**
 * @brief Writes one clipped line, so a long value can never break the frame.
 */
static void	put_line(struct ncplane *plane, int row, int x, int width,
	const char *text)
{
	char	clipped[APP_TEXT_MAX + APP_ROOM_CHAT_TEXT_MAX + 8];
	int		limit;

	if (text == NULL || width <= 0 || row < 0)
		return ;
	limit = min_int(width, (int)sizeof(clipped) - 1);
	snprintf(clipped, sizeof(clipped), "%.*s", limit, text);
	(void)ncplane_putstr_yx(plane, row, x, clipped);
}

static void	put_centered(struct ncplane *plane, int row, int cols,
	const char *text, bool bold)
{
	int	length;
	int	x;

	if (text == NULL || row < 0)
		return ;
	length = (int)strlen(text);
	x = (cols - length) / 2;
	if (x < 1)
		x = 1;
	if (bold)
		(void)ncplane_set_styles(plane, NCSTYLE_BOLD);
	put_line(plane, row, x, cols - x - 1, text);
	if (bold)
		(void)ncplane_set_styles(plane, NCSTYLE_NONE);
}

/**
 * @brief Draws a horizontal rule inside the frame.
 */
static void	put_rule(struct ncplane *plane, int row, int cols)
{
	char	rule[128];
	int		width;
	int		index;

	width = min_int(cols - 4, (int)sizeof(rule) - 1);
	if (width <= 0 || row < 0)
		return ;
	index = 0;
	while (index < width)
	{
		rule[index] = '-';
		index++;
	}
	rule[width] = '\0';
	set_colour(plane, MP_LAVENDER_R, MP_LAVENDER_G, MP_LAVENDER_B);
	put_line(plane, row, 2, width, rule);
}

static int	min_int(int first, int second)
{
	return (first < second ? first : second);
}
