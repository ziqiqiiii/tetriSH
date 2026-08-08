#include "tetrisu.h"

static void	test_filter_selects_the_visible_rooms(void);
static void	test_cursor_clamps_instead_of_wrapping(void);
static void	test_filter_cycles_and_resets_the_cursor(void);
static void	test_sections_read_input_differently(void);
static void	test_typing_a_room_id(void);
static void	test_join_by_id_ignores_the_filter(void);
static void	test_join_blockers(void);
static void	test_feedback_copy(void);
static void	test_input_batch_boundaries(void);
static void	test_all_rooms_scrolls_the_shared_viewport(void);
static void	build_lobby(t_app_lobby_view_model *lobby);
static void	add_room(t_app_lobby_view_model *lobby, const char *id,
				t_app_game_mode mode, t_app_room_state state, int players,
				int capacity);

int	main(void)
{
	test_filter_selects_the_visible_rooms();
	test_cursor_clamps_instead_of_wrapping();
	test_filter_cycles_and_resets_the_cursor();
	test_sections_read_input_differently();
	test_typing_a_room_id();
	test_join_by_id_ignores_the_filter();
	test_join_blockers();
	test_feedback_copy();
	test_input_batch_boundaries();
	test_all_rooms_scrolls_the_shared_viewport();
	return (0);
}

/**
 * @brief All eight rooms stay selectable even though both renderers show six.
 */
static void	test_all_rooms_scrolls_the_shared_viewport(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;
	int						step;

	build_lobby(&lobby);
	add_room(&lobby, "duel-60", APP_GAME_MODE_DOUBLE,
		APP_ROOM_STATE_WAITING, 1, 2);
	add_room(&lobby, "arena-90", APP_GAME_MODE_BATTLE_ROYALE,
		APP_ROOM_STATE_WAITING, 6, APP_ROOM_MAX_PLAYERS);
	add_room(&lobby, "arena-99", APP_GAME_MODE_BATTLE_ROYALE,
		APP_ROOM_STATE_WAITING, 9, APP_ROOM_MAX_PLAYERS);
	lobby_state_init(&state, APP_GAME_MODE_NONE, &lobby);
	step = 0;
	while (step < APP_LOBBY_MAX_ROOMS - 1)
	{
		(void)lobby_handle_key(&state, NCKEY_DOWN);
		step++;
	}
	assert(state.selected == APP_LOBBY_MAX_ROOMS - 1);
	assert(state.list_offset == APP_LOBBY_MAX_ROOMS - LOBBY_VISIBLE_ROOMS);
	assert(strcmp(lobby_selected_room(&lobby, &state)->id, "arena-99") == 0);
	(void)lobby_handle_key(&state, NCKEY_HOME);
	assert(state.selected == 0 && state.list_offset == 0);
	printf("PASS test_all_rooms_scrolls_the_shared_viewport\n");
}

static void	add_room(t_app_lobby_view_model *lobby, const char *id,
	t_app_game_mode mode, t_app_room_state state, int players, int capacity)
{
	t_app_room_summary_view_model	*room;

	assert(lobby->count < APP_LOBBY_MAX_ROOMS);
	room = &lobby->rooms[lobby->count];
	snprintf(room->id, sizeof(room->id), "%s", id);
	snprintf(room->owner, sizeof(room->owner), "owner-%d", lobby->count);
	room->mode = mode;
	room->state = state;
	room->players = players;
	room->capacity = capacity;
	lobby->count++;
}

static void	build_lobby(t_app_lobby_view_model *lobby)
{
	memset(lobby, 0, sizeof(*lobby));
	add_room(lobby, "duel-42", APP_GAME_MODE_DOUBLE, APP_ROOM_STATE_WAITING,
		1, 2);
	add_room(lobby, "duel-47", APP_GAME_MODE_DOUBLE, APP_ROOM_STATE_IN_GAME,
		2, 2);
	add_room(lobby, "duel-51", APP_GAME_MODE_DOUBLE, APP_ROOM_STATE_WAITING,
		2, 2);
	add_room(lobby, "arena-88", APP_GAME_MODE_BATTLE_ROYALE,
		APP_ROOM_STATE_WAITING, 4, APP_ROOM_MAX_PLAYERS);
	add_room(lobby, "arena-89", APP_GAME_MODE_BATTLE_ROYALE,
		APP_ROOM_STATE_IN_GAME, APP_ROOM_MAX_PLAYERS, APP_ROOM_MAX_PLAYERS);
}

/**
 * @brief The mode carried in from the picker decides which rooms are listed.
 */
static void	test_filter_selects_the_visible_rooms(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	assert(state.visible_count == 3);
	assert(lobby_visible_count(&lobby, APP_GAME_MODE_BATTLE_ROYALE) == 2);
	assert(lobby_visible_count(&lobby, APP_GAME_MODE_NONE) == lobby.count);
	assert(lobby_visible_count(NULL, APP_GAME_MODE_NONE) == 0);
	assert(strcmp(lobby_visible_room(&lobby, APP_GAME_MODE_BATTLE_ROYALE,
				0)->id, "arena-88") == 0);
	assert(lobby_visible_room(&lobby, APP_GAME_MODE_BATTLE_ROYALE, 2) == NULL);
	assert(lobby_visible_room(&lobby, APP_GAME_MODE_DOUBLE, -1) == NULL);
	assert(strcmp(lobby_selected_room(&lobby, &state)->id, "duel-42") == 0);
	printf("PASS test_filter_selects_the_visible_rooms\n");
}

/**
 * @brief The cursor stops at the ends rather than cycling under a held key.
 */
static void	test_cursor_clamps_instead_of_wrapping(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;
	int						step;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	step = 0;
	while (step < 10)
	{
		assert(lobby_handle_key(&state, NCKEY_DOWN) == LOBBY_ACTION_NONE);
		step++;
	}
	assert(state.selected == state.visible_count - 1);
	step = 0;
	while (step < 10)
	{
		assert(lobby_handle_key(&state, NCKEY_UP) == LOBBY_ACTION_NONE);
		step++;
	}
	assert(state.selected == 0);
	(void)lobby_handle_key(&state, NCKEY_PGDOWN);
	assert(state.selected == state.visible_count - 1);
	(void)lobby_handle_key(&state, NCKEY_PGUP);
	assert(state.selected == 0);
	printf("PASS test_cursor_clamps_instead_of_wrapping\n");
}

/**
 * @brief Cycling the filter reselects from the top of the new list.
 */
static void	test_filter_cycles_and_resets_the_cursor(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	(void)lobby_handle_key(&state, NCKEY_DOWN);
	(void)lobby_handle_key(&state, NCKEY_DOWN);
	assert(state.selected == 2);
	assert(lobby_handle_key(&state, 'm') == LOBBY_ACTION_NONE);
	assert(state.filter == APP_GAME_MODE_BATTLE_ROYALE);
	assert(state.selected == 0);
	assert(state.feedback == LOBBY_FEEDBACK_FILTER);
	(void)lobby_handle_key(&state, 'M');
	assert(state.filter == APP_GAME_MODE_NONE);
	(void)lobby_handle_key(&state, 'm');
	assert(state.filter == APP_GAME_MODE_DOUBLE);
	/*
	 * The count follows the filter only once the caller syncs it, because the
	 * room list is owned by the view model rather than by this state.
	 */
	lobby_state_sync(&state, &lobby);
	assert(state.visible_count == 3);
	printf("PASS test_filter_cycles_and_resets_the_cursor\n");
}

/**
 * @brief Letters are commands in the table and text in the join field.
 */
static void	test_sections_read_input_differently(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	assert(lobby_handle_key(&state, 'c') == LOBBY_ACTION_CREATE);
	assert(lobby_handle_key(&state, 'r') == LOBBY_ACTION_REFRESH);
	assert(lobby_handle_key(&state, 'b') == LOBBY_ACTION_BACK);
	assert(lobby_handle_key(&state, NCKEY_ESC) == LOBBY_ACTION_BACK);
	assert(lobby_handle_key(&state, NCKEY_ENTER) == LOBBY_ACTION_JOIN);
	assert(lobby_handle_key(&state, NCKEY_RIGHT) == LOBBY_ACTION_NONE);
	assert(state.section == LOBBY_SECTION_JOIN);
	/* Inside the field the same letters are text, and Escape only steps out. */
	assert(lobby_handle_key(&state, 'c') == LOBBY_ACTION_NONE);
	assert(lobby_handle_key(&state, 'r') == LOBBY_ACTION_NONE);
	assert(lobby_handle_key(&state, 'b') == LOBBY_ACTION_NONE);
	assert(strcmp(state.room_id, "crb") == 0);
	assert(lobby_handle_key(&state, NCKEY_ENTER) == LOBBY_ACTION_JOIN_BY_ID);
	assert(lobby_handle_key(&state, NCKEY_ESC) == LOBBY_ACTION_NONE);
	assert(state.section == LOBBY_SECTION_ROOMS);
	printf("PASS test_sections_read_input_differently\n");
}

/**
 * @brief The field takes printable ASCII only, and never overruns its buffer.
 */
static void	test_typing_a_room_id(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;
	int						step;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	(void)lobby_handle_key(&state, NCKEY_RIGHT);
	step = 0;
	while (step < LOBBY_ROOM_ID_MAX * 2)
	{
		(void)lobby_handle_key(&state, 'a');
		step++;
	}
	assert(state.room_id_length == LOBBY_ROOM_ID_MAX - 1);
	assert(state.room_id[state.room_id_length] == '\0');
	assert((int)strlen(state.room_id) == state.room_id_length);
	(void)lobby_handle_key(&state, 21);
	assert(state.room_id_length == 0 && state.room_id[0] == '\0');
	(void)lobby_handle_key(&state, 'd');
	(void)lobby_handle_key(&state, '4');
	assert(strcmp(state.room_id, "d4") == 0);
	/* Non-printable input is dropped rather than stored as a placeholder. */
	(void)lobby_handle_key(&state, NCKEY_F01);
	assert(strcmp(state.room_id, "d4") == 0);
	(void)lobby_handle_key(&state, NCKEY_BACKSPACE);
	assert(strcmp(state.room_id, "d") == 0);
	(void)lobby_handle_key(&state, NCKEY_BACKSPACE);
	(void)lobby_handle_key(&state, NCKEY_BACKSPACE);
	assert(state.room_id_length == 0);
	printf("PASS test_typing_a_room_id\n");
}

/**
 * @brief A room id must resolve whatever filter the browser happens to hold.
 */
static void	test_join_by_id_ignores_the_filter(void)
{
	t_app_lobby_view_model	lobby;

	build_lobby(&lobby);
	assert(lobby_room_by_id(&lobby, "arena-88") != NULL);
	/* Ids are shared by voice and by chat, so matching is case-insensitive. */
	assert(lobby_room_by_id(&lobby, "ARENA-88") != NULL);
	assert(lobby_room_by_id(&lobby, "arena-8") == NULL);
	assert(lobby_room_by_id(&lobby, "arena-888") == NULL);
	assert(lobby_room_by_id(&lobby, "") == NULL);
	assert(lobby_room_by_id(NULL, "arena-88") == NULL);
	printf("PASS test_join_by_id_ignores_the_filter\n");
}

/**
 * @brief Full and in-game rooms are refused with the reason, not silently.
 */
static void	test_join_blockers(void)
{
	t_app_lobby_view_model	lobby;
	t_app_room_summary_view_model	invalid;

	build_lobby(&lobby);
	assert(lobby_join_blocker(lobby_room_by_id(&lobby, "duel-42"))
		== LOBBY_FEEDBACK_NONE);
	assert(lobby_join_blocker(lobby_room_by_id(&lobby, "duel-47"))
		== LOBBY_FEEDBACK_IN_GAME);
	assert(lobby_join_blocker(lobby_room_by_id(&lobby, "duel-51"))
		== LOBBY_FEEDBACK_FULL);
	invalid = *lobby_room_by_id(&lobby, "arena-88");
	invalid.capacity = WAITING_ROOM_ROYALE_MIN_PLAYERS - 1;
	assert(lobby_join_blocker(&invalid) == LOBBY_FEEDBACK_INVALID_ROOM);
	invalid.capacity = APP_ROOM_MAX_PLAYERS;
	invalid.players = APP_ROOM_MAX_PLAYERS + 1;
	assert(lobby_join_blocker(&invalid) == LOBBY_FEEDBACK_INVALID_ROOM);
	invalid.players = 4;
	invalid.state = APP_ROOM_STATE_READY;
	assert(lobby_join_blocker(&invalid) == LOBBY_FEEDBACK_NONE);
	invalid.state = APP_ROOM_STATE_FINISHED;
	assert(lobby_join_blocker(&invalid) == LOBBY_FEEDBACK_FINISHED);
	invalid.state = (t_app_room_state)99;
	assert(lobby_join_blocker(&invalid) == LOBBY_FEEDBACK_INVALID_ROOM);
	assert(lobby_join_blocker(NULL) == LOBBY_FEEDBACK_EMPTY_LIST);
	printf("PASS test_join_blockers\n");
}

/**
 * @brief Every feedback value must produce a line, and NONE must produce none.
 */
static void	test_feedback_copy(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;
	char					line[APP_TEXT_MAX * 2];
	int						feedback;

	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_DOUBLE, &lobby);
	assert(lobby_feedback_text(&state, line, sizeof(line))[0] == '\0');
	feedback = LOBBY_FEEDBACK_REFRESHED;
	while (feedback <= LOBBY_FEEDBACK_FILTER)
	{
		lobby_set_feedback(&state, (t_lobby_feedback)feedback, 55);
		assert(lobby_feedback_text(&state, line, sizeof(line))[0] != '\0');
		feedback++;
	}
	assert(strcmp(lobby_mode_tag(APP_GAME_MODE_DOUBLE), "D") == 0);
	assert(strcmp(lobby_mode_tag(APP_GAME_MODE_BATTLE_ROYALE), "BR") == 0);
	assert(strcmp(lobby_state_tag(APP_ROOM_STATE_IN_GAME), "IN-GAME") == 0);
	assert(strcmp(lobby_state_tag(APP_ROOM_STATE_READY), "READY") == 0);
	assert(strcmp(lobby_state_tag(APP_ROOM_STATE_FINISHED), "FINISHED") == 0);
	assert(lobby_filter_name(APP_GAME_MODE_NONE)[0] != '\0');
	printf("PASS test_feedback_copy\n");
}

/**
 * @brief Typed characters must never coalesce; two 'a' keys are two letters.
 */
static void	test_input_batch_boundaries(void)
{
	t_app_lobby_view_model	lobby;
	t_lobby_state			state;

	assert(lobby_navigation_keys_coalesce(NCKEY_UP, NCKEY_UP));
	assert(lobby_navigation_keys_coalesce(NCKEY_PGDOWN, NCKEY_PGDOWN));
	assert(!lobby_navigation_keys_coalesce(NCKEY_UP, NCKEY_DOWN));
	assert(!lobby_navigation_keys_coalesce('a', 'a'));
	assert(!lobby_navigation_keys_coalesce(NCKEY_ENTER, NCKEY_ENTER));
	assert(lobby_action_leaves_screen(LOBBY_ACTION_JOIN));
	assert(lobby_action_leaves_screen(LOBBY_ACTION_CREATE));
	assert(!lobby_action_leaves_screen(LOBBY_ACTION_REFRESH));
	assert(!lobby_action_leaves_screen(LOBBY_ACTION_VOLUME_UP));
	build_lobby(&lobby);
	lobby_state_init(&state, APP_GAME_MODE_NONE, &lobby);
	lobby_set_feedback(&state, LOBBY_FEEDBACK_REFRESHED, 0);
	assert(lobby_handle_key(&state, '+') == LOBBY_ACTION_VOLUME_UP);
	assert(state.feedback == LOBBY_FEEDBACK_REFRESHED);
	assert(lobby_handle_key(&state, '-') == LOBBY_ACTION_VOLUME_DOWN);
	assert(state.feedback == LOBBY_FEEDBACK_REFRESHED);
	printf("PASS test_input_batch_boundaries\n");
}
