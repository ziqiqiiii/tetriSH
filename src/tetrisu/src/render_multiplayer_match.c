#include "tetrisu.h"

# define MATCH_BG_R	18
# define MATCH_BG_G	9
# define MATCH_BG_B	31
# define MATCH_PANEL_R	31
# define MATCH_PANEL_G	15
# define MATCH_PANEL_B	49
# define MATCH_PINK_R	255
# define MATCH_PINK_G	112
# define MATCH_PINK_B	190
# define MATCH_GOLD_R	255
# define MATCH_GOLD_G	203
# define MATCH_GOLD_B	102
# define MATCH_CREAM_R	250
# define MATCH_CREAM_G	242
# define MATCH_CREAM_B	221
# define MATCH_LAVENDER_R	190
# define MATCH_LAVENDER_G	155
# define MATCH_LAVENDER_B	218
# define MATCH_RED_R	255
# define MATCH_RED_G	92
# define MATCH_RED_B	118
# define MATCH_GREEN_R	112
# define MATCH_GREEN_G	214
# define MATCH_GREEN_B	174

// Static Functions
static bool	create_match_plane(t_render_ctx *ctx);
static void	draw_selection(t_render_ctx *ctx, struct ncplane *plane,
				const t_mp_match_state *state,
				const t_mp_match_layout *layout);
static void	effect_line(const t_solo_effects *effects, char *out,
				size_t size);
static void	draw_double(struct ncplane *plane,
				const t_mp_match_state *state,
				const t_mp_match_layout *layout);
static void	draw_battle_royale(struct ncplane *plane,
				const t_mp_match_state *state,
				const t_mp_match_layout *layout);
static void	draw_board(struct ncplane *plane, const t_mp_rect *rect,
				const t_solo_game *game, const char *title);
static void	draw_targeting(struct ncplane *plane,
				const t_mp_match_state *state, const t_mp_rect *rect);
static void	draw_mini_arena(struct ncplane *plane, const t_mp_rect *rect,
				const t_mp_match_state *state, const int *slots, int count);
static void	draw_abilities(struct ncplane *plane, const t_mp_rect *rect,
				const t_mp_match_state *state, bool compact);
static void	draw_result(struct ncplane *plane,
				const t_mp_match_state *state, const t_mp_rect *rect);
static bool	piece_at(const t_piece *piece, int col, int row);
static void	set_cell_colour(struct ncplane *plane, t_cell cell, bool active,
				int clear_step);
static void	draw_box(struct ncplane *plane, const t_mp_rect *rect,
				int red, int green, int blue);
static void	fill_rect(struct ncplane *plane, const t_mp_rect *rect,
				int red, int green, int blue);
static void	set_fg(struct ncplane *plane, int red, int green, int blue);
static void	put_centered(struct ncplane *plane, int row, int x, int width,
				const char *text, bool bold);
static void	put_clipped(struct ncplane *plane, int row, int x, int width,
				const char *text);
static void	update_portrait(t_render_ctx *ctx,
				const t_mp_match_state *state,
				const t_mp_match_layout *layout);
static void	destroy_portrait(t_render_ctx *ctx);
static void	player_cell_name(const t_mp_match_state *state, char *out,
				size_t size);
static void	arena_grid(int count, const t_mp_rect *rect, int *cols, int *rows);
static t_cell	mini_sample(const t_board *board, int sample_x, int sample_y,
				int sample_width, int sample_height);
static int	minimum(int first, int second);

/**
 * @brief Draws character selection or a live multiplayer match snapshot.
 */
bool	render_multiplayer_match_show(t_render_ctx *ctx,
	const t_mp_match_state *state, bool rebuild_background)
{
	t_mp_match_layout	layout;
	unsigned			rows;
	unsigned			cols;

	if (ctx == NULL || ctx->std == NULL || state == NULL)
		return (false);
	/*
	 * A notification card is a bitmap over bitmaps, and notcurses wipes the
	 * sprixel underneath rather than overlapping it. The panels it covered
	 * are cached by signature, so nothing else would ever consider them
	 * stale - the screen has to be told to rebuild instead of trusting it.
	 */
	if (render_notification_take_repaint(ctx))
		rebuild_background = true;
	if (!render_compatibility_mode(ctx)
		&& render_multiplayer_match_pixel_show(ctx, state, rebuild_background))
		return (true);
	render_multiplayer_match_pixel_destroy(ctx);
	if (rebuild_background)
	{
		render_multiplayer_destroy(ctx);
		if (render_background_replace(ctx,
				ctx->theme_assets.solo_background, false) < 0)
			return (false);
	}
	if (ctx->screen_plane == NULL && !create_match_plane(ctx))
		return (false);
	ncplane_dim_yx(ctx->screen_plane, &rows, &cols);
	mp_match_layout_build(state->mode, (int)rows, (int)cols, &layout);
	ncplane_erase(ctx->screen_plane);
	if (state->phase == MP_MATCH_CHARACTER_SELECT)
		draw_selection(ctx, ctx->screen_plane, state, &layout);
	else if (!layout.valid)
	{
		destroy_portrait(ctx);
		set_fg(ctx->screen_plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
		put_centered(ctx->screen_plane, 2, 0, (int)cols,
			"MULTIPLAYER NEEDS A LARGER TERMINAL", true);
		set_fg(ctx->screen_plane, MATCH_LAVENDER_R,
			MATCH_LAVENDER_G, MATCH_LAVENDER_B);
		put_centered(ctx->screen_plane, 4, 0, (int)cols,
			state->mode == APP_GAME_MODE_DOUBLE
			? "RESIZE TO AT LEAST 100 x 32"
			: "RESIZE TO AT LEAST 132 x 36", false);
	}
	else if (state->mode == APP_GAME_MODE_DOUBLE)
	{
		destroy_portrait(ctx);
		draw_double(ctx->screen_plane, state, &layout);
	}
	else
	{
		destroy_portrait(ctx);
		draw_battle_royale(ctx->screen_plane, state, &layout);
	}
	if (state->phase == MP_MATCH_FINISHED)
		draw_result(ctx->screen_plane, state, &layout.result);
	ncplane_move_top(ctx->screen_plane);
	if (ctx->mp_cards_plane != NULL)
		ncplane_move_top(ctx->mp_cards_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

void	render_multiplayer_match_destroy(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	render_multiplayer_match_pixel_destroy(ctx);
	render_mp_pixel_destroy(ctx);
	destroy_portrait(ctx);
	render_screen_destroy(ctx);
}

static bool	create_match_plane(t_render_ctx *ctx)
{
	ncplane_options	options;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < 12 || cols < 40)
		return (false);
	memset(&options, 0, sizeof(options));
	options.rows = (int)rows;
	options.cols = (int)cols;
	options.y = 0;
	options.x = 0;
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels,
		MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
	(void)ncchannels_set_bg_alpha(&channels, NCALPHA_TRANSPARENT);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	return (true);
}

static void	draw_selection(t_render_ctx *ctx, struct ncplane *plane,
	const t_mp_match_state *state, const t_mp_match_layout *layout)
{
	const t_app_catalogue_item_view_model	*character;
	t_mp_rect	card;
	t_mp_rect	portrait;
	char		line[APP_ABILITY_TEXT_MAX];
	char		countdown[32];
	int		index;
	int		row;

	card = layout->character_card;
	if (card.height < 15)
		card.height = 15;
	fill_rect(plane, &card, MATCH_PANEL_R, MATCH_PANEL_G, MATCH_PANEL_B);
	draw_box(plane, &card, MATCH_PINK_R, MATCH_PINK_G, MATCH_PINK_B);
	set_fg(plane, MATCH_PINK_R, MATCH_PINK_G, MATCH_PINK_B);
	put_centered(plane, card.y + 1, card.x, card.width,
		"CHOOSE YOUR CHARACTER", true);
	snprintf(countdown, sizeof(countdown), state->selection.locked
		? "LOCKED IN  |  %d" : "TIME  %d",
		mp_match_character_seconds(state));
	set_fg(plane, mp_match_character_seconds(state) <= 5
		? MATCH_RED_R : MATCH_GOLD_R,
		mp_match_character_seconds(state) <= 5 ? MATCH_RED_G : MATCH_GOLD_G,
		mp_match_character_seconds(state) <= 5 ? MATCH_RED_B : MATCH_GOLD_B);
	put_centered(plane, card.y + 2, card.x, card.width, countdown, true);
	portrait = (t_mp_rect){card.x + 9, card.y + 4, card.width - 18,
		card.height > 18 ? 9 : 6};
	fill_rect(plane, &portrait, MATCH_BG_R, MATCH_BG_G, MATCH_BG_B);
	draw_box(plane, &portrait, MATCH_LAVENDER_R,
		MATCH_LAVENDER_G, MATCH_LAVENDER_B);
	character = mp_match_selected_character(state);
	if (character != NULL)
	{
		set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
		put_centered(plane, portrait.y + portrait.height, card.x, card.width,
			character->name, true);
		row = portrait.y + portrait.height + 1;
		if (row < card.y + card.height - 3)
		{
			snprintf(line, sizeof(line),
				"POWERS: %.*s / %.*s / %.*s / %.*s",
				MP_MATCH_POWER_NAME_MAX, character->abilities[0].name,
				MP_MATCH_POWER_NAME_MAX, character->abilities[1].name,
				MP_MATCH_POWER_NAME_MAX, character->abilities[2].name,
				MP_MATCH_POWER_NAME_MAX, character->abilities[3].name);
			set_fg(plane, MATCH_LAVENDER_R,
				MATCH_LAVENDER_G, MATCH_LAVENDER_B);
			put_centered(plane, row, card.x + 1, card.width - 2, line, false);
		}
	}
	row = card.y + card.height - 2;
	set_fg(plane, MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
	put_centered(plane, row, card.x, card.width,
		"ARROWS / A D CYCLE   ENTER LOCKS", false);
	index = 0;
	row = card.y + card.height + 1;
	while (index < state->characters.count && index < APP_CATALOGUE_MAX_ITEMS
		&& row < layout->rows - 1)
	{
		if (state->characters.items[index].owned)
		{
			set_fg(plane, index == state->selection.selected
				? MATCH_GOLD_R : MATCH_LAVENDER_R,
				index == state->selection.selected
				? MATCH_GOLD_G : MATCH_LAVENDER_G,
				index == state->selection.selected
				? MATCH_GOLD_B : MATCH_LAVENDER_B);
			snprintf(line, sizeof(line), "%s%s%s",
				index == state->selection.selected ? "[ " : "  ",
				state->characters.items[index].name,
				index == state->selection.selected ? " ]" : "  ");
			put_centered(plane, row, card.x, card.width, line,
				index == state->selection.selected);
			row++;
		}
		index++;
	}
	update_portrait(ctx, state, layout);
}

static void	draw_double(struct ncplane *plane,
	const t_mp_match_state *state, const t_mp_match_layout *layout)
{
	char	line[APP_TEXT_MAX * 2];
	char	player[APP_TEXT_MAX];

	player_cell_name(state, player, sizeof(player));
	set_fg(plane, MATCH_PINK_R, MATCH_PINK_G, MATCH_PINK_B);
	put_centered(plane, 0, 0, layout->cols, ":: DOUBLE PLAYER MODE ::", true);
	snprintf(line, sizeof(line), "HOLD [ ]     NEXT [ I  T  S ]     ROOM %s",
		state->room_id);
	set_fg(plane, MATCH_LAVENDER_R, MATCH_LAVENDER_G, MATCH_LAVENDER_B);
	put_centered(plane, 2, 0, layout->cols, line, false);
	draw_board(plane, &layout->local_board, &state->local_game, "YOUR BOARD");
	draw_board(plane, &layout->opponent_board,
		&state->opponent_game, "OPPONENT BOARD");
	set_fg(plane, MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
	if (state->incoming_garbage > 0)
		snprintf(line, sizeof(line),
			"%s  |  %" PRIu64 " PTS  |  POWER %d/10  |  +%d INCOMING",
			player, state->local_game.scoring.total,
			state->local_game.crystal_charge, state->incoming_garbage);
	else
		snprintf(line, sizeof(line), "%s  |  %" PRIu64 " PTS  |  POWER %d/10",
			player, state->local_game.scoring.total,
			state->local_game.crystal_charge);
	put_centered(plane, 27, layout->local_board.x - 8,
		layout->local_board.width + 16, line, false);
	snprintf(line, sizeof(line), "%s  |  %" PRIu64 " PTS  |  POWER %d/10",
		state->opponent_name, state->opponent_game.scoring.total,
		state->opponent_charge);
	put_centered(plane, 27, layout->opponent_board.x - 14,
		layout->opponent_board.width + 28, line, false);
	draw_abilities(plane, &layout->abilities, state, false);
	effect_line(&state->local_game.effects, line, sizeof(line));
	if (line[0] != '\0')
	{
		set_fg(plane, MATCH_RED_R, MATCH_RED_G, MATCH_RED_B);
		put_centered(plane, 29, 0, layout->cols, line, true);
	}
	set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	put_centered(plane, 31, 0, layout->cols, state->status, false);
	set_fg(plane, MATCH_LAVENDER_R, MATCH_LAVENDER_G, MATCH_LAVENDER_B);
	put_centered(plane, layout->rows - 2, 0, layout->cols,
		"ARROWS MOVE  Z/X ROTATE  SPACE DROP  C HOLD  1-4 POWERS  ESC LEAVE",
		false);
}

static void	draw_battle_royale(struct ncplane *plane,
	const t_mp_match_state *state, const t_mp_match_layout *layout)
{
	char	line[APP_TEXT_MAX * 2];
	int		cards[APP_ROOM_MAX_PLAYERS];
	int		opponents;
	int		left_count;

	set_fg(plane, MATCH_PINK_R, MATCH_PINK_G, MATCH_PINK_B);
	put_centered(plane, 0, 0, layout->cols, ":: BATTLE ROYALE ::", true);
	snprintf(line, sizeof(line), "SCORE %" PRIu64 "   ALIVE %d/%d   K.O. %02d",
		state->local_game.scoring.total, state->players_alive,
		state->players_total, state->ko_count);
	set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	put_centered(plane, 1, 0, layout->cols, line, true);
	draw_targeting(plane, state, &layout->targeting);
	draw_board(plane, &layout->local_board, &state->local_game, "YOUR BOARD");
	/*
	 * The cards are filed by seat, so the array is sparse: a seat nobody is in
	 * is a hole in it, not a card at the end. Walking it by position would
	 * draw those holes as empty boxes and stop before the last rival in a room
	 * that has ever had somebody leave, so the occupied seats are gathered
	 * first and the grid is laid out over that.
	 */
	opponents = mp_match_collect_cards(state, cards, APP_ROOM_MAX_PLAYERS);
	left_count = (opponents + 1) / 2;
	draw_mini_arena(plane, &layout->left_opponents, state, cards, left_count);
	draw_mini_arena(plane, &layout->right_opponents, state,
		cards + left_count, opponents - left_count);
	if (state->incoming_attackers > 0)
	{
		set_fg(plane, MATCH_RED_R, MATCH_RED_G, MATCH_RED_B);
		snprintf(line, sizeof(line), "WATCH OUT!  %d ATTACKER%s",
			state->incoming_attackers,
			state->incoming_attackers == 1 ? "" : "S");
		put_centered(plane, layout->local_board.y + layout->local_board.height,
			layout->local_board.x - 6, layout->local_board.width + 12,
			line, true);
	}
	draw_abilities(plane, &layout->abilities, state, false);
	set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	put_centered(plane, layout->rows - 3, 0, layout->cols,
		state->status, false);
	set_fg(plane, MATCH_LAVENDER_R, MATCH_LAVENDER_G, MATCH_LAVENDER_B);
	put_centered(plane, layout->rows - 1, 0, layout->cols,
		"W KOs  A RANDOMS  S ATTACKERS  D BADGES   |   ARROWS/Z/X/SPACE/C PLAY",
		false);
}

static void	draw_board(struct ncplane *plane, const t_mp_rect *rect,
	const t_solo_game *game, const char *title)
{
	t_cell	cell;
	int		row;
	int		col;
	int		cell_width;
	int		clear_step;
	int		danger;
	int		x;

	/*
	 * The same danger signal the bitmap tier reddens the board with, so a
	 * player on the compatibility tier gets the same warning.
	 */
	danger = (int)solo_game_danger_dim(game);
	fill_rect(plane, rect, MATCH_BG_R + (74 - MATCH_BG_R) * danger
		/ SOLO_DANGER_DIM_MAX, MATCH_BG_G + (10 - MATCH_BG_G) * danger
		/ SOLO_DANGER_DIM_MAX, MATCH_BG_B + (20 - MATCH_BG_B) * danger
		/ SOLO_DANGER_DIM_MAX);
	draw_box(plane, rect, MATCH_PINK_R, MATCH_PINK_G
		+ (MATCH_RED_G - MATCH_PINK_G) * danger / SOLO_DANGER_DIM_MAX,
		MATCH_PINK_B + (MATCH_RED_B - MATCH_PINK_B) * danger
		/ SOLO_DANGER_DIM_MAX);
	set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	if ((int)strlen(title) + 2 <= rect->width)
		put_centered(plane, rect->y, rect->x, rect->width, title, true);
	else
		put_centered(plane, rect->y - 1, rect->x - 6,
			rect->width + 12, title, true);
	cell_width = rect->width >= BOARD_WIDTH * 2 + 2 ? 2 : 1;
	clear_step = 1;
	if (game->clear_elapsed_ms >= solo_clear_duration_ms(game->level) / 2)
		clear_step = 2;
	row = 0;
	while (row < BOARD_HEIGHT && row + 1 < rect->height)
	{
		col = 0;
		while (col < BOARD_WIDTH && cell_width * col + 1 < rect->width)
		{
			cell = board_get(&game->board, col, row);
			set_cell_colour(plane, cell,
				game->phase != SOLO_GAME_OVER
				&& piece_at(&game->active, col, row),
				solo_game_row_is_clearing(game, row) ? clear_step : 0);
			x = rect->x + 1 + col * cell_width;
			(void)ncplane_putchar_yx(plane, rect->y + 1 + row, x, ' ');
			if (cell_width == 2)
				(void)ncplane_putchar_yx(plane, rect->y + 1 + row, x + 1, ' ');
			col++;
		}
		row++;
	}
	(void)ncplane_set_bg_rgb8(plane, MATCH_BG_R, MATCH_BG_G, MATCH_BG_B);
}

static void	draw_targeting(struct ncplane *plane,
	const t_mp_match_state *state, const t_mp_rect *rect)
{
	const char	*labels[4] = {"W KOs", "A RANDOMS", "S ATTACKERS", "D BADGES"};
	t_target_mode	modes[4] = {TARGET_KO, TARGET_RANDOM,
		TARGET_ATTACKERS, TARGET_TOP_SCORE};
	int	positions[4][2];
	int	index;

	positions[0][0] = rect->y;
	positions[0][1] = rect->x + rect->width / 2 - 4;
	positions[1][0] = rect->y + 2;
	positions[1][1] = rect->x + 1;
	positions[2][0] = rect->y + 2;
	positions[2][1] = rect->x + rect->width / 2 - 6;
	positions[3][0] = rect->y + 2;
	positions[3][1] = rect->x + rect->width - 10;
	index = 0;
	while (index < 4)
	{
		set_fg(plane, state->target_mode == modes[index]
			? MATCH_GOLD_R : MATCH_LAVENDER_R,
			state->target_mode == modes[index]
			? MATCH_GOLD_G : MATCH_LAVENDER_G,
			state->target_mode == modes[index]
			? MATCH_GOLD_B : MATCH_LAVENDER_B);
		put_clipped(plane, positions[index][0], positions[index][1],
			12, labels[index]);
		index++;
	}
}

static void	draw_mini_arena(struct ncplane *plane, const t_mp_rect *rect,
	const t_mp_match_state *state, const int *slots, int count)
{
	const struct s_mp_opponent_snapshot	*opponent;
	t_cell	board_cell;
	int	grid_cols;
	int	grid_rows;
	int	cell_width;
	int	cell_height;
	int	index;
	int	player;
	int	x;
	int	y;
	int	board_y;
	int	board_height;
	int	draw_x;
	int	draw_y;
	char	label[12];
	t_mp_rect	cell;

	if (count <= 0)
		return ;
	arena_grid(count, rect, &grid_cols, &grid_rows);
	cell_width = rect->width / grid_cols;
	cell_height = rect->height / grid_rows;
	if (cell_width < 5 || cell_height < 5)
		return ;
	index = 0;
	while (index < count)
	{
		opponent = &state->opponents[slots[index]];
		player = slots[index];
		x = rect->x + (index % grid_cols) * cell_width;
		y = rect->y + (index / grid_cols) * cell_height;
		cell = (t_mp_rect){x, y, cell_width - 1, cell_height - 1};
		fill_rect(plane, &cell, MATCH_PANEL_R, MATCH_PANEL_G, MATCH_PANEL_B);
		draw_box(plane, &cell, opponent->targeting_local
			? MATCH_RED_R : MATCH_LAVENDER_R,
			opponent->targeting_local ? MATCH_RED_G : MATCH_LAVENDER_G,
			opponent->targeting_local ? MATCH_RED_B : MATCH_LAVENDER_B);
		if (!opponent->alive)
		{
			set_fg(plane, MATCH_RED_R, MATCH_RED_G, MATCH_RED_B);
			put_centered(plane, y + cell_height / 2, x,
				cell.width, "KO", true);
		}
		else
		{
			set_fg(plane, opponent->targeting_local
				? MATCH_RED_R : MATCH_GREEN_R,
				opponent->targeting_local ? MATCH_RED_G : MATCH_GREEN_G,
				opponent->targeting_local ? MATCH_RED_B : MATCH_GREEN_B);
			snprintf(label, sizeof(label), opponent->targeting_local
				? "!%02d!" : "#%02d", player);
			put_centered(plane, y + 1, x, cell.width, label,
				opponent->targeting_local);
			board_y = y + 2;
			board_height = cell.height - 3;
			draw_y = 0;
			while (draw_y < board_height)
			{
				draw_x = 0;
				while (draw_x < cell.width - 2)
				{
					board_cell = mini_sample(&opponent->board, draw_x, draw_y,
						cell.width - 2, board_height);
					set_cell_colour(plane, board_cell, false, 0);
					(void)ncplane_putchar_yx(plane, board_y + draw_y,
						x + 1 + draw_x, ' ');
					draw_x++;
				}
				draw_y++;
			}
		}
		index++;
	}
}

static void	draw_abilities(struct ncplane *plane, const t_mp_rect *rect,
	const t_mp_match_state *state, bool compact)
{
	static const char	piece_letters[] = "IOTSZJL";
	const t_app_catalogue_item_view_model	*character;
	char	line[APP_ABILITY_TEXT_MAX];
	int		index;
	int		row;

	character = mp_match_selected_character(state);
	if (character == NULL || rect->width <= 0 || rect->height <= 0)
		return ;
	fill_rect(plane, rect, MATCH_PANEL_R, MATCH_PANEL_G, MATCH_PANEL_B);
	draw_box(plane, rect, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	set_fg(plane, MATCH_GOLD_R, MATCH_GOLD_G, MATCH_GOLD_B);
	snprintf(line, sizeof(line), "%s LOADOUT", character->name);
	put_centered(plane, rect->y + 1, rect->x, rect->width, line, true);
	snprintf(line, sizeof(line), "POWER %d/10",
		state->local_game.crystal_charge);
	put_centered(plane, rect->y + 2, rect->x, rect->width, line, true);
	snprintf(line, sizeof(line), "HOLD %c   NEXT %c %c %c",
		state->local_game.has_hold
			? piece_letters[state->local_game.hold] : '-',
		piece_letters[state->local_game.next[0]],
		piece_letters[state->local_game.next[1]],
		piece_letters[state->local_game.next[2]]);
	set_fg(plane, MATCH_LAVENDER_R, MATCH_LAVENDER_G, MATCH_LAVENDER_B);
	put_centered(plane, rect->y + 3, rect->x, rect->width, line, false);
	if (compact)
	{
		snprintf(line, sizeof(line),
			"[1] %.*s  [2] %.*s  [3] %.*s  [4] %.*s",
			MP_MATCH_POWER_NAME_MAX, character->abilities[0].name,
			MP_MATCH_POWER_NAME_MAX, character->abilities[1].name,
			MP_MATCH_POWER_NAME_MAX, character->abilities[2].name,
			MP_MATCH_POWER_NAME_MAX, character->abilities[3].name);
		set_fg(plane, MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
		put_centered(plane, rect->y + 1, rect->x, rect->width, line, false);
		return ;
	}
	index = 0;
	row = rect->y + 5;
	while (index < APP_CHARACTER_ABILITY_COUNT && row < rect->y + rect->height)
	{
		snprintf(line, sizeof(line), "[%d] %s", index + 1,
			character->abilities[index].name);
		set_fg(plane, MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
		put_clipped(plane, row, rect->x, rect->width, line);
		row += 3;
		index++;
	}
}

static void	draw_result(struct ncplane *plane,
	const t_mp_match_state *state, const t_mp_rect *rect)
{
	char	line[MP_MATCH_STATUS_MAX];

	fill_rect(plane, rect, MATCH_PANEL_R, MATCH_PANEL_G, MATCH_PANEL_B);
	draw_box(plane, rect, state->result == MP_MATCH_RESULT_WON
		? MATCH_GOLD_R : MATCH_RED_R,
		state->result == MP_MATCH_RESULT_WON ? MATCH_GOLD_G : MATCH_RED_G,
		state->result == MP_MATCH_RESULT_WON ? MATCH_GOLD_B : MATCH_RED_B);
	set_fg(plane, state->result == MP_MATCH_RESULT_WON
		? MATCH_GOLD_R : MATCH_RED_R,
		state->result == MP_MATCH_RESULT_WON ? MATCH_GOLD_G : MATCH_RED_G,
		state->result == MP_MATCH_RESULT_WON ? MATCH_GOLD_B : MATCH_RED_B);
	put_centered(plane, rect->y + 2, rect->x, rect->width,
		mp_match_result_text(state, line, sizeof(line)), true);
	set_fg(plane, MATCH_CREAM_R, MATCH_CREAM_G, MATCH_CREAM_B);
	put_centered(plane, rect->y + 4, rect->x, rect->width,
		"ENTER OR ESC TO RETURN TO THE ROOM", false);
}

static bool	piece_at(const t_piece *piece, int col, int row)
{
	int	cols[4];
	int	rows[4];
	int	index;

	if (piece == NULL || !piece_cells(piece, cols, rows))
		return (false);
	index = 0;
	while (index < 4)
	{
		if (cols[index] == col && rows[index] == row)
			return (true);
		index++;
	}
	return (false);
}

/*
 * clear_step carries the same two-frame flash the bitmap tier draws with
 * TILE_CLEAR_FIRST / TILE_CLEAR_SECOND, so a player on the compatibility tier
 * sees a line clear rather than rows that vanish between two frames.
 */
static void	set_cell_colour(struct ncplane *plane, t_cell cell, bool active,
	int clear_step)
{
	static const int	colours[8][3] = {
		{MATCH_BG_R, MATCH_BG_G, MATCH_BG_B}, {55, 207, 225},
		{255, 210, 76}, {188, 105, 255}, {86, 222, 126},
		{255, 92, 118}, {85, 124, 255}, {255, 158, 67}
	};
	int	index;

	if (clear_step == 1 || clear_step == 2)
	{
		if (clear_step == 1)
			(void)ncplane_set_bg_rgb8(plane, 250, 242, 221);
		else
			(void)ncplane_set_bg_rgb8(plane, 148, 140, 128);
		(void)ncplane_set_bg_alpha(plane, NCALPHA_OPAQUE);
		return ;
	}
	if (active)
		index = 3;
	else if (cell.type == CELL_GARBAGE)
		index = 0;
	else if (cell.type == CELL_FILLED)
		index = cell.color >= 1 && cell.color <= 7 ? cell.color : 1;
	else
		index = 0;
	if (cell.type == CELL_GARBAGE && !active)
		(void)ncplane_set_bg_rgb8(plane, 112, 112, 126);
	else
		(void)ncplane_set_bg_rgb8(plane, colours[index][0],
			colours[index][1], colours[index][2]);
	(void)ncplane_set_bg_alpha(plane, NCALPHA_OPAQUE);
}

static void	draw_box(struct ncplane *plane, const t_mp_rect *rect,
	int red, int green, int blue)
{
	int	x;
	int	y;

	if (rect == NULL || rect->width < 2 || rect->height < 2)
		return ;
	set_fg(plane, red, green, blue);
	x = 0;
	while (x < rect->width)
	{
		(void)ncplane_putchar_yx(plane, rect->y, rect->x + x,
			x == 0 || x == rect->width - 1 ? '+' : '-');
		(void)ncplane_putchar_yx(plane, rect->y + rect->height - 1,
			rect->x + x, x == 0 || x == rect->width - 1 ? '+' : '-');
		x++;
	}
	y = 1;
	while (y < rect->height - 1)
	{
		(void)ncplane_putchar_yx(plane, rect->y + y, rect->x, '|');
		(void)ncplane_putchar_yx(plane, rect->y + y,
			rect->x + rect->width - 1, '|');
		y++;
	}
}

static void	fill_rect(struct ncplane *plane, const t_mp_rect *rect,
	int red, int green, int blue)
{
	int	x;
	int	y;

	if (rect == NULL || rect->width <= 0 || rect->height <= 0)
		return ;
	(void)ncplane_set_bg_rgb8(plane, red, green, blue);
	(void)ncplane_set_bg_alpha(plane, NCALPHA_OPAQUE);
	y = 0;
	while (y < rect->height)
	{
		x = 0;
		while (x < rect->width)
		{
			(void)ncplane_putchar_yx(plane, rect->y + y, rect->x + x, ' ');
			x++;
		}
		y++;
	}
	(void)ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
}

static void	set_fg(struct ncplane *plane, int red, int green, int blue)
{
	(void)ncplane_set_fg_rgb8(plane, red, green, blue);
	(void)ncplane_set_bg_alpha(plane, NCALPHA_TRANSPARENT);
}

static void	put_centered(struct ncplane *plane, int row, int x, int width,
	const char *text, bool bold)
{
	int	length;
	int	start;

	if (text == NULL || width <= 0)
		return ;
	length = (int)strlen(text);
	if (length > width)
		length = width;
	start = x + (width - length) / 2;
	if (bold)
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	put_clipped(plane, row, start, length, text);
	if (bold)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	put_clipped(struct ncplane *plane, int row, int x, int width,
	const char *text)
{
	char	buffer[APP_ABILITY_TEXT_MAX + 1];
	size_t	length;

	if (plane == NULL || text == NULL || width <= 0 || row < 0 || x < 0)
		return ;
	length = strlen(text);
	if (length > (size_t)width)
		length = (size_t)width;
	if (length >= sizeof(buffer))
		length = sizeof(buffer) - 1;
	memcpy(buffer, text, length);
	buffer[length] = '\0';
	(void)ncplane_putstr_yx(plane, row, x, buffer);
}

static void	update_portrait(t_render_ctx *ctx,
	const t_mp_match_state *state, const t_mp_match_layout *layout)
{
	const t_app_catalogue_item_view_model	*character;
	struct ncvisual_options	options;
	struct ncvisual			*visual;
	int						rows;
	int						cols;
	int						origin_y;
	int						origin_x;
	uint64_t				signature;

	character = mp_match_selected_character(state);
	if (character == NULL || character->portrait_asset[0] == '\0'
		|| !render_pixel_planes_reliable(ctx)
		|| ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0)
	{
		destroy_portrait(ctx);
		return ;
	}
	signature = (uint64_t)(state->selection.selected + 1);
	if (ctx->mp_cards_plane != NULL && ctx->mp_cards_signature == signature)
		return ;
	destroy_portrait(ctx);
	visual = ncvisual_from_file(character->portrait_asset);
	if (visual == NULL)
		return ;
	rows = layout->character_card.height > 18 ? 9 : 6;
	cols = layout->character_card.width - 20;
	if (ncvisual_resize_noninterpolative(visual, rows * ctx->cell_px_y,
			cols * ctx->cell_px_x) != 0)
	{
		ncvisual_destroy(visual);
		return ;
	}
	memset(&options, 0, sizeof(options));
	options.n = ctx->std;
	ncplane_abs_yx(ctx->screen_plane, &origin_y, &origin_x);
	options.y = origin_y + layout->character_card.y + 4;
	options.x = origin_x + layout->character_card.x + 10;
	options.scaling = NCSCALE_NONE;
	options.blitter = NCBLIT_PIXEL;
	options.flags = NCVISUAL_OPTION_CHILDPLANE
		| NCVISUAL_OPTION_NOINTERPOLATE | NCVISUAL_OPTION_NODEGRADE;
	ctx->mp_cards_plane = ncvisual_blit(ctx->nc, visual, &options);
	ncvisual_destroy(visual);
	if (ctx->mp_cards_plane != NULL)
		ctx->mp_cards_signature = signature;
}

static void	destroy_portrait(t_render_ctx *ctx)
{
	if (ctx != NULL && ctx->mp_cards_plane != NULL)
	{
		ncplane_destroy(ctx->mp_cards_plane);
		ctx->mp_cards_plane = NULL;
	}
	if (ctx != NULL)
		ctx->mp_cards_signature = 0;
}

static void	player_cell_name(const t_mp_match_state *state, char *out,
	size_t size)
{
	if (state->profile.username[0] != '\0')
		snprintf(out, size, "%s", state->profile.username);
	else
		snprintf(out, size, "You");
}

static void	arena_grid(int count, const t_mp_rect *rect, int *cols, int *rows)
{
	int	candidate;
	int	candidate_rows;
	int	score;
	int	best_score;

	*cols = 1;
	*rows = count;
	best_score = 0;
	candidate = 1;
	while (candidate <= count)
	{
		candidate_rows = (count + candidate - 1) / candidate;
		score = minimum(rect->width / candidate,
			(rect->height / candidate_rows) / 2);
		if (score > best_score)
		{
			best_score = score;
			*cols = candidate;
			*rows = candidate_rows;
		}
		candidate++;
	}
}

static t_cell	mini_sample(const t_board *board, int sample_x, int sample_y,
	int sample_width, int sample_height)
{
	t_cell	result;
	t_cell	cell;
	int		first_col;
	int		last_col;
	int		first_row;
	int		last_row;
	int		row;
	int		col;

	result.type = CELL_EMPTY;
	result.color = 0;
	first_col = sample_x * BOARD_WIDTH / sample_width;
	last_col = (sample_x + 1) * BOARD_WIDTH / sample_width;
	first_row = sample_y * BOARD_HEIGHT / sample_height;
	last_row = (sample_y + 1) * BOARD_HEIGHT / sample_height;
	if (last_col <= first_col)
		last_col = first_col + 1;
	if (last_row <= first_row)
		last_row = first_row + 1;
	row = first_row;
	while (row < last_row && row < BOARD_HEIGHT)
	{
		col = first_col;
		while (col < last_col && col < BOARD_WIDTH)
		{
			cell = board_get(board, col, row);
			if (cell.type == CELL_GARBAGE)
				return (cell);
			if (cell.type == CELL_FILLED)
				result = cell;
			col++;
		}
		row++;
	}
	return (result);
}

static int	minimum(int first, int second)
{
	return (first < second ? first : second);
}

/**
 * @brief Names every effect currently riding on the player, or nothing.
 *
 * The card that announces an arrival fades; this does not. An effect lasting
 * three pieces outlives its own notification, so without a standing line a
 * player who looked away would be back to guessing why their piece will not
 * turn - which is the whole problem the wire field was added to solve.
 *
 * @param effects The counts the server sent.
 * @param out Destination buffer, emptied when nothing is active.
 * @param size Capacity of out.
 */
static void	effect_line(const t_solo_effects *effects, char *out, size_t size)
{
	out[0] = '\0';
	if (effects->paralysis > 0)
		snprintf(out, size, "NO ROTATION (%d)", effects->paralysis);
	else if (effects->inversion > 0)
		snprintf(out, size, "CONTROLS INVERTED (%d)", effects->inversion);
	else if (effects->nue > 0)
		snprintf(out, size, "NO FAST DROP (%d)", effects->nue);
	else if (effects->dark > 0)
		snprintf(out, size, "BLACKOUT (%d)", effects->dark);
	else if (effects->fry > 0)
		snprintf(out, size, "%d ROWS BURN AT THE NEXT LOCK", effects->fry);
	else if (effects->thwack > 0)
		snprintf(out, size, "THWACK (%d)", effects->thwack);
	else if (effects->pals > 0)
		snprintf(out, size, "PALS (%d)", effects->pals);
	else if (effects->mirror > 0)
		snprintf(out, size, "MIRROR READY");
}

