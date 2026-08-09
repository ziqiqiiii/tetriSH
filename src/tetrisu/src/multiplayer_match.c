#include "tetrisu.h"

// Static Functions
static int	first_selectable(const t_app_catalogue_view_model *characters);
static int	step_selectable(const t_app_catalogue_view_model *characters,
				int current, int direction);
static void	seed_preview_stack(t_solo_game *game);
static void	seed_opponent_board(t_board *board, int player);
static t_mp_rect	match_rect(int x, int y, int width, int height);
static int	max_int(int first, int second);
static int	min_int(int first, int second);

/**
 * @brief Creates a presentation-ready multiplayer snapshot.
 *
 * The local Solo rules are only a fixture authority until tetrisd supplies
 * Double/Battle Royale snapshots. Keeping them inside the same view model the
 * renderer will later consume makes that replacement mechanical.
 */
void	mp_match_state_init(t_mp_match_state *state, t_app_game_mode mode,
	const char *room_id, const t_app_profile_view_model *profile,
	const t_app_catalogue_view_model *characters, uint32_t seed)
{
	int	index;

	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	state->mode = mode == APP_GAME_MODE_BATTLE_ROYALE
		? APP_GAME_MODE_BATTLE_ROYALE : APP_GAME_MODE_DOUBLE;
	state->phase = MP_MATCH_CHARACTER_SELECT;
	state->selection.remaining_ms = MP_CHARACTER_SELECT_MS;
	state->selection.last_second = MP_CHARACTER_SELECT_MS / 1000;
	if (profile != NULL)
		state->profile = *profile;
	if (characters != NULL)
		state->characters = *characters;
	state->selection.selected = first_selectable(&state->characters);
	index = 0;
	while (index < state->characters.count)
	{
		if (state->characters.items[index].owned
			&& state->characters.items[index].equipped)
		{
			state->selection.selected = index;
			break ;
		}
		index++;
	}
	state->target_mode = TARGET_RANDOM;
	state->players_total = state->mode == APP_GAME_MODE_BATTLE_ROYALE ? 99 : 2;
	state->players_alive = state->players_total;
	state->incoming_attackers = state->mode == APP_GAME_MODE_BATTLE_ROYALE ? 2 : 1;
	state->opponent_charge = 6;
	snprintf(state->room_id, sizeof(state->room_id), "%s",
		room_id != NULL && room_id[0] != '\0' ? room_id : "LOCAL-PREVIEW");
	snprintf(state->opponent_name, sizeof(state->opponent_name), "%s",
		state->mode == APP_GAME_MODE_DOUBLE ? "Opponent" : "Arena");
	snprintf(state->status, sizeof(state->status),
		"LOCAL VISUAL PREVIEW - SERVER SNAPSHOTS PENDING");
	solo_game_init(&state->local_game, seed);
	solo_game_init(&state->opponent_game, seed ^ 0x9e3779b9u);
	seed_preview_stack(&state->opponent_game);
	mp_match_apply_room(state, NULL,
		state->mode == APP_GAME_MODE_BATTLE_ROYALE ? 12 : 2);
}

/**
 * @brief Applies the active waiting-room roster to the match snapshot.
 *
 * Battle Royale renders exactly one local board plus one board per other
 * player. Preview counts are only used by the direct visual QA entry point;
 * live rooms always use their authoritative player_count.
 */
void	mp_match_apply_room(t_mp_match_state *state,
	const t_app_room_view_model *room, int preview_players)
{
	int	player_count;
	int	room_index;
	int	opponent_index;
	bool	local;

	if (state == NULL)
		return ;
	player_count = state->mode == APP_GAME_MODE_DOUBLE ? 2 : preview_players;
	if (room != NULL && room->player_count > 0)
		player_count = room->player_count;
	if (state->mode == APP_GAME_MODE_BATTLE_ROYALE)
		player_count = max_int(WAITING_ROOM_ROYALE_MIN_PLAYERS,
			min_int(APP_ROOM_MAX_PLAYERS, player_count));
	else
		player_count = WAITING_ROOM_DOUBLE_PLAYERS;
	memset(state->opponents, 0, sizeof(state->opponents));
	state->players_total = player_count;
	state->players_alive = player_count;
	room_index = 0;
	opponent_index = 0;
	while (opponent_index < player_count - 1)
	{
		local = false;
		if (room != NULL && room_index < room->player_count)
		{
			local = room_index == room->local_slot
				|| (state->profile.username[0] != '\0'
					&& strcmp(room->players[room_index].username,
						state->profile.username) == 0);
			if (local)
			{
				room_index++;
				continue ;
			}
		}
		state->opponents[opponent_index].present = true;
		state->opponents[opponent_index].alive = true;
		state->opponents[opponent_index].targeting_local
			= opponent_index < state->incoming_attackers;
		state->opponents[opponent_index].garbage_pending
			= (opponent_index * 3 + 1) % 5;
		if (room != NULL && room_index < room->player_count
			&& room->players[room_index].username[0] != '\0')
			snprintf(state->opponents[opponent_index].name,
				sizeof(state->opponents[opponent_index].name), "%s",
				room->players[room_index].username);
		else
			snprintf(state->opponents[opponent_index].name,
				sizeof(state->opponents[opponent_index].name), "PLAYER %02d",
				opponent_index + 2);
		seed_opponent_board(&state->opponents[opponent_index].board,
			opponent_index + 2);
		room_index++;
		opponent_index++;
	}
	if (state->mode == APP_GAME_MODE_DOUBLE && state->opponents[0].present)
		snprintf(state->opponent_name, sizeof(state->opponent_name), "%s",
			state->opponents[0].name);
}

/**
 * @brief Cycles owned characters or locks the current one with Enter.
 */
bool	mp_match_character_handle_key(t_mp_match_state *state, uint32_t key)
{
	int	before;

	if (state == NULL || state->phase != MP_MATCH_CHARACTER_SELECT
		|| state->selection.locked)
		return (false);
	if (key == NCKEY_ENTER || key == '\n' || key == '\r')
	{
		state->selection.locked = true;
		return (true);
	}
	before = state->selection.selected;
	if (key == NCKEY_LEFT || key == NCKEY_UP || key == 'a' || key == 'A')
		state->selection.selected = step_selectable(&state->characters,
			state->selection.selected, -1);
	else if (key == NCKEY_RIGHT || key == NCKEY_DOWN
		|| key == 'd' || key == 'D')
		state->selection.selected = step_selectable(&state->characters,
			state->selection.selected, 1);
	return (before != state->selection.selected);
}

/**
 * @brief Advances the 15-second selector and reports sound/transition edges.
 */
uint32_t	mp_match_character_update(t_mp_match_state *state, int elapsed_ms)
{
	uint32_t	events;
	int		seconds;

	if (state == NULL || state->phase != MP_MATCH_CHARACTER_SELECT
		|| elapsed_ms <= 0)
		return (MP_SELECTION_EVENT_NONE);
	events = MP_SELECTION_EVENT_NONE;
	if (elapsed_ms >= state->selection.remaining_ms)
		state->selection.remaining_ms = 0;
	else
		state->selection.remaining_ms -= elapsed_ms;
	seconds = mp_match_character_seconds(state);
	if (seconds != state->selection.last_second)
	{
		state->selection.last_second = seconds;
		if (seconds > 0 && seconds <= MP_CHARACTER_TICK_AUDIO_SECONDS)
			events |= MP_SELECTION_EVENT_SECOND;
	}
	if (state->selection.remaining_ms == 0)
	{
		state->selection.locked = true;
		state->phase = MP_MATCH_PLAYING;
		solo_game_start_countdown(&state->local_game);
		solo_game_start_countdown(&state->opponent_game);
		events |= MP_SELECTION_EVENT_FINISHED;
	}
	return (events);
}

int	mp_match_character_seconds(const t_mp_match_state *state)
{
	if (state == NULL || state->selection.remaining_ms <= 0)
		return (0);
	return ((state->selection.remaining_ms + 999) / 1000);
}

const t_app_catalogue_item_view_model	*mp_match_selected_character(
	const t_mp_match_state *state)
{
	int	selected;

	if (state == NULL)
		return (NULL);
	selected = state->selection.selected;
	if (selected < 0 || selected >= state->characters.count
		|| selected >= APP_CATALOGUE_MAX_ITEMS)
		return (NULL);
	return (&state->characters.items[selected]);
}

/**
 * @brief Applies the Tetris 99 diamond mapping requested for WASD.
 *
 * W is KOs, A Randoms, S Attackers, and D Badges/top score, matching the
 * positions in the reference screenshot.
 */
bool	mp_match_target_handle_key(t_mp_match_state *state, uint32_t key)
{
	t_target_mode	before;

	if (state == NULL || state->mode != APP_GAME_MODE_BATTLE_ROYALE
		|| state->phase != MP_MATCH_PLAYING)
		return (false);
	before = state->target_mode;
	if (key == 'w' || key == 'W')
		state->target_mode = TARGET_KO;
	else if (key == 'a' || key == 'A')
		state->target_mode = TARGET_RANDOM;
	else if (key == 's' || key == 'S')
		state->target_mode = TARGET_ATTACKERS;
	else if (key == 'd' || key == 'D')
		state->target_mode = TARGET_TOP_SCORE;
	return (before != state->target_mode);
}

const char	*mp_match_target_name(t_target_mode mode)
{
	if (mode == TARGET_KO)
		return ("KOs");
	if (mode == TARGET_ATTACKERS)
		return ("Attackers");
	if (mode == TARGET_TOP_SCORE)
		return ("Badges");
	return ("Randoms");
}

void	mp_match_finish(t_mp_match_state *state, bool won, int rank)
{
	if (state == NULL)
		return ;
	state->phase = MP_MATCH_FINISHED;
	state->result = won ? MP_MATCH_RESULT_WON : MP_MATCH_RESULT_LOST;
	if (won)
		state->final_rank = 1;
	else
		state->final_rank = rank > 1 ? rank : 2;
}

const char	*mp_match_result_text(const t_mp_match_state *state,
	char *out, size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (state == NULL || state->result == MP_MATCH_RESULT_NONE)
		return (out);
	if (state->result == MP_MATCH_RESULT_WON)
		snprintf(out, size, "WOW, YOU WON!");
	else if (state->mode == APP_GAME_MODE_BATTLE_ROYALE)
		snprintf(out, size, "SO SAD, YOU LOST - RANK #%d", state->final_rank);
	else
		snprintf(out, size, "SO SAD, YOU LOST");
	return (out);
}

/**
 * @brief Calculates responsive cell geometry for both requested wireframes.
 */
void	mp_match_layout_build(t_app_game_mode mode, int rows, int cols,
	t_mp_match_layout *layout)
{
	int	board_x;
	int	card_height;
	int	local_x;
	int	opponent_x;

	if (layout == NULL)
		return ;
	memset(layout, 0, sizeof(*layout));
	layout->rows = rows;
	layout->cols = cols;
	card_height = min_int(20, max_int(15, rows - 10));
	layout->character_card = match_rect(max_int(2, (cols - 50) / 2),
		max_int(3, (rows - card_height) / 2 - 2),
		cols < 54 ? cols - 4 : 50, card_height);
	layout->result = match_rect(max_int(2, (cols - 54) / 2),
		max_int(2, (rows - 7) / 2), cols < 58 ? cols - 4 : 54, 7);
	if (mode == APP_GAME_MODE_DOUBLE)
	{
		layout->valid = rows >= 30 && cols >= 82;
		layout->valid = rows >= 32 && cols >= 100;
		local_x = max_int(34, (cols - 58) / 2);
		opponent_x = local_x + 32;
		layout->local_board = match_rect(local_x, 5, 22, 22);
		layout->opponent_board = match_rect(opponent_x, 5, 22, 22);
		layout->abilities = match_rect(1, 4, 29,
			min_int(24, rows - 8));
	}
	else
	{
		layout->valid = rows >= 36 && cols >= 132;
		board_x = max_int(56, (cols + 26 - 22) / 2);
		layout->local_board = match_rect(board_x, 8, 22, 22);
		layout->abilities = match_rect(1, 5, 24,
			min_int(27, rows - 9));
		layout->left_opponents = match_rect(27, 7, board_x - 29,
			max_int(24, rows - 12));
		layout->right_opponents = match_rect(board_x + 23, 7,
			cols - board_x - 24, layout->left_opponents.height);
		layout->targeting = match_rect(board_x - 10, 3, 42, 4);
	}
}

/**
 * @brief Calculates the shared authored-pixel match geometry.
 *
 * Rendering and pointer hit-testing consume this same layout so an ability
 * marker never drifts away from its mouse target after a resize.
 */
void	mp_match_pixel_layout_build(t_app_game_mode mode, int width, int height,
	t_mp_match_pixel_layout *layout)
{
	int	margin;
	int	gap;
	int	top;
	int	footer;
	int	info_width;
	int	meter_width;
	int	tile;
	int	arena_left;
	int	arena_width;
	int	index;

	if (layout == NULL)
		return ;
	memset(layout, 0, sizeof(*layout));
	layout->width = width;
	layout->height = height;
	if (width < 640 || height < 480)
		return ;
	if (mode == APP_GAME_MODE_DOUBLE)
	{
		margin = max_int(22, min_int(56, width / 45));
		gap = max_int(24, min_int(62, width / 45));
		top = max_int(96, min_int(160, height / 10));
		footer = max_int(90, min_int(150, height / 11));
		info_width = max_int(150, min_int(250, width * 10 / 100));
		meter_width = max_int(54, min_int(84, width / 30));
		tile = min_int((height - top - footer) / BOARD_HEIGHT,
			(width - margin * 2 - info_width - meter_width - gap * 4)
			/ (BOARD_WIDTH * 2));
		tile = max_int(8, tile);
		arena_left = max_int(margin, (width - (info_width + meter_width
				+ gap * 3 + tile * BOARD_WIDTH * 2)) / 2);
		layout->loadout = match_rect(arena_left, top, info_width,
			tile * BOARD_HEIGHT);
		layout->portrait = match_rect(layout->loadout.x + 12,
			layout->loadout.y + 52, layout->loadout.width - 24,
			min_int(layout->loadout.height / 3, 230));
		layout->ability_bar = match_rect(layout->loadout.x
			+ layout->loadout.width + gap, top, meter_width,
			tile * BOARD_HEIGHT);
		layout->local_board = match_rect(layout->ability_bar.x
			+ layout->ability_bar.width + max_int(10, gap / 3),
			top, tile * BOARD_WIDTH, tile * BOARD_HEIGHT);
		layout->opponent_board = match_rect(layout->local_board.x
			+ layout->local_board.width + gap, top,
			layout->local_board.width, layout->local_board.height);
	}
	else
	{
		margin = max_int(18, min_int(42, width / 60));
		gap = max_int(14, min_int(34, width / 80));
		top = max_int(190, min_int(230, height / 7));
		footer = max_int(62, min_int(96, height / 14));
		info_width = max_int(130, min_int(220, width * 9 / 100));
		meter_width = max_int(48, min_int(76, width / 34));
		tile = max_int(7, min_int((height - top - footer) / BOARD_HEIGHT, 54));
		arena_left = margin;
		arena_width = width - margin * 2;
		layout->local_board = match_rect(arena_left
			+ (arena_width - tile * BOARD_WIDTH) / 2, top,
			tile * BOARD_WIDTH, tile * BOARD_HEIGHT);
		layout->ability_bar = match_rect(layout->local_board.x
			- meter_width - max_int(10, gap / 2), top, meter_width,
			layout->local_board.height);
		layout->loadout = match_rect(layout->ability_bar.x - info_width
			- max_int(10, gap / 2), top, info_width,
			layout->local_board.height);
		layout->portrait = match_rect(layout->loadout.x + 10,
			layout->loadout.y + 54, layout->loadout.width - 20,
			min_int(layout->loadout.height / 3, 210));
		layout->left_opponents = match_rect(margin, top,
			layout->loadout.x - margin - gap,
			layout->local_board.height);
		layout->right_opponents = match_rect(layout->local_board.x
			+ layout->local_board.width + gap, top,
			width - margin - layout->local_board.x
			- layout->local_board.width - gap, layout->local_board.height);
		layout->targeting = match_rect(layout->local_board.x
			- min_int(90, layout->left_opponents.width / 2),
			max_int(84, top - 102), layout->local_board.width
			+ min_int(180, layout->left_opponents.width), 92);
	}
	layout->hud = match_rect(margin, 20, width - margin * 2,
		max_int(76, top - 32));
	if (mode == APP_GAME_MODE_BATTLE_ROYALE
		&& layout->targeting.y + layout->targeting.height + 6
			> layout->hud.y + layout->hud.height)
		layout->hud.height = layout->targeting.y + layout->targeting.height
			+ 6 - layout->hud.y;
	layout->controls = match_rect(margin, height - 62,
		width - margin * 2, 46);
	/*
	 * Hold and Next take every row the loadout column has left under the
	 * portrait and the fighter's name, rather than a fixed bottom fraction of
	 * it. The four preview slots have to share whatever this is, so handing
	 * them the leftover instead of 40% is the difference between a piece the
	 * player can read at a glance and four thumbnails. The hovered ability
	 * description borrows the same rectangle.
	 */
	top = layout->portrait.y + layout->portrait.height + 62;
	layout->ability_popover = match_rect(layout->loadout.x + 10, top,
		layout->loadout.width - 20,
		max_int(120, layout->loadout.y + layout->loadout.height - top - 12));
	layout->ability_center_x = layout->ability_bar.x
		+ layout->ability_bar.width / 2;
	layout->ability_hit_radius = max_int(18, min_int(32,
		layout->ability_bar.width / 2 - 3));
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		layout->ability_center_y[index] = layout->ability_bar.y
			+ layout->ability_bar.height
			- (index + 1) * layout->ability_bar.height / 5;
		index++;
	}
	layout->valid = layout->local_board.width >= BOARD_WIDTH
		&& layout->local_board.height >= BOARD_HEIGHT;
}

int	mp_match_ability_at_pixel(const t_mp_match_pixel_layout *layout,
	int x, int y)
{
	int	index;

	if (layout == NULL || !layout->valid)
		return (0);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		if (abs(x - layout->ability_center_x) <= layout->ability_hit_radius
			&& abs(y - layout->ability_center_y[index])
				<= layout->ability_hit_radius)
			return (index + 1);
		index++;
	}
	return (0);
}

/**
 * @brief Routes multiplayer movement through Solo's handling, unaltered.
 *
 * The event type is handed to solo_handling_event() exactly as the terminal
 * reported it, because that function is the whole DAS/ARR state machine and
 * every event type already means something specific to it: Press takes the key
 * held and charges DAS, Repeat is deliberately ignored while the key is held
 * so that repeat timing belongs to the application rather than to the host's
 * keyboard settings, Release hands the axis back to the opposite key, and
 * Unknown is a legacy tap that never becomes held.
 *
 * Translating those types here is what made a match feel unlike Solo: a Press
 * forwarded as Unknown returns an action without ever marking the key held, so
 * horizontal_direction stayed zero, solo_handling_update() had nothing to
 * repeat, and the piece could only move again when the host's own key repeat
 * fired - one cell, then the OS repeat delay, then DAS on top of it.
 *
 * All this adds is the guard for when the board is not the player's to move:
 * a Release must still be delivered, or the key stays held across the pause.
 */
bool	mp_match_movement_event(const t_mp_match_state *state,
	t_solo_handling_state *handling, const t_solo_handling_config *config,
	uint32_t key, ncintype_e event_type, t_solo_action *action)
{
	bool	movement_key;

	movement_key = key == NCKEY_LEFT || key == NCKEY_RIGHT
		|| key == NCKEY_DOWN;
	if (!movement_key || state == NULL || handling == NULL || config == NULL
		|| action == NULL)
		return (false);
	if (state->phase != MP_MATCH_PLAYING || state->local_game.paused
		|| state->local_game.countdown_active
		|| state->local_game.phase != SOLO_ACTIVE)
	{
		if (event_type == NCTYPE_RELEASE)
			(void)solo_handling_event(handling, config, key, event_type,
				action);
		return (false);
	}
	return (solo_handling_event(handling, config, key, event_type, action));
}

static int	first_selectable(const t_app_catalogue_view_model *characters)
{
	int	index;

	if (characters == NULL)
		return (-1);
	index = 0;
	while (index < characters->count && index < APP_CATALOGUE_MAX_ITEMS)
	{
		if (characters->items[index].owned)
			return (index);
		index++;
	}
	return (characters->count > 0 ? 0 : -1);
}

static int	step_selectable(const t_app_catalogue_view_model *characters,
	int current, int direction)
{
	int	candidate;
	int	steps;
	int	count;

	if (characters == NULL || characters->count <= 0)
		return (-1);
	count = characters->count;
	if (count > APP_CATALOGUE_MAX_ITEMS)
		count = APP_CATALOGUE_MAX_ITEMS;
	candidate = current < 0 || current >= count ? 0 : current;
	steps = 0;
	while (steps < count)
	{
		candidate = (candidate + direction + count) % count;
		if (characters->items[candidate].owned)
			return (candidate);
		steps++;
	}
	return (current);
}

static void	seed_preview_stack(t_solo_game *game)
{
	t_cell	cell;
	int		row;
	int		col;

	cell.type = CELL_FILLED;
	row = BOARD_HEIGHT - 1;
	while (row >= BOARD_HEIGHT - 6)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if ((col + row) % 5 != 0 && col + row > BOARD_HEIGHT - 3)
			{
				cell.color = (uint8_t)((col + row) % 7);
				board_set(&game->board, col, row, cell);
			}
			col++;
		}
		row--;
	}
}

static void	seed_opponent_board(t_board *board, int player)
{
	t_cell	cell;
	int		height;
	int		row;
	int		col;

	board_init(board);
	height = 2 + player * 7 % 16;
	row = BOARD_HEIGHT - 1;
	while (row >= BOARD_HEIGHT - height)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if ((col * 3 + row + player) % 7 != 0)
			{
				cell.type = row >= BOARD_HEIGHT
					- ((player * 3 + 1) % 5) ? CELL_GARBAGE : CELL_FILLED;
				cell.color = (uint8_t)((col + row + player) % 7);
				board_set(board, col, row, cell);
			}
			col++;
		}
		row--;
	}
}

static t_mp_rect	match_rect(int x, int y, int width, int height)
{
	t_mp_rect	rect;

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	return (rect);
}

static int	max_int(int first, int second)
{
	return (first > second ? first : second);
}

static int	min_int(int first, int second)
{
	return (first < second ? first : second);
}
