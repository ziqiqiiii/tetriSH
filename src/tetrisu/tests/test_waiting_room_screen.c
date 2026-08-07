#include "tetrisu.h"

static void	test_double_needs_both_players_ready(void);
static void	test_battle_royale_needs_four_and_a_majority(void);
static void	test_start_blockers_explain_themselves(void);
static void	test_countdown_runs_down_and_cancels(void);
static void	test_chat_modes_read_input_differently(void);
static void	test_chat_ring_drops_the_oldest(void);
static void	test_slot_labels_and_badges(void);
static void	test_status_and_feedback_copy(void);
static void	test_launch_action_matches_the_mode(void);
static void	build_room(app_room_view_model_t *room, app_game_mode_t mode,
				int players, int ready);

int	main(void)
{
	test_double_needs_both_players_ready();
	test_battle_royale_needs_four_and_a_majority();
	test_start_blockers_explain_themselves();
	test_countdown_runs_down_and_cancels();
	test_chat_modes_read_input_differently();
	test_chat_ring_drops_the_oldest();
	test_slot_labels_and_badges();
	test_status_and_feedback_copy();
	test_launch_action_matches_the_mode();
	return (0);
}

/**
 * @brief Builds a room whose local player owns seat zero and is never ready.
 */
static void	build_room(app_room_view_model_t *room, app_game_mode_t mode,
	int players, int ready)
{
	int	index;

	memset(room, 0, sizeof(*room));
	room->mode = mode;
	room->state = APP_ROOM_STATE_WAITING;
	snprintf(room->id, sizeof(room->id), "%s",
		mode == APP_GAME_MODE_DOUBLE ? "duel-22" : "arena-31");
	room->capacity = mode == APP_GAME_MODE_DOUBLE
		? WAITING_ROOM_DOUBLE_PLAYERS : APP_ROOM_MAX_PLAYERS;
	room->required_players = mode == APP_GAME_MODE_DOUBLE
		? WAITING_ROOM_DOUBLE_PLAYERS : WAITING_ROOM_ROYALE_MIN_PLAYERS;
	room->player_count = players;
	room->local_slot = 0;
	index = 0;
	while (index < players && index < APP_ROOM_MAX_PLAYERS)
	{
		snprintf(room->players[index].username,
			sizeof(room->players[index].username), "player%d", index);
		room->players[index].owner = index == 0;
		room->players[index].ready = index < ready;
		index++;
	}
}

/**
 * @brief A duel starts only with both seats filled and both players ready.
 */
static void	test_double_needs_both_players_ready(void)
{
	app_room_view_model_t	room;

	build_room(&room, APP_GAME_MODE_DOUBLE, 1, 1);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	assert(waiting_room_ready_count(&room) == 1);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(waiting_room_can_start(&room));
	/* A room already playing cannot be started again. */
	room.state = APP_ROOM_STATE_IN_GAME;
	assert(!waiting_room_can_start(&room));
	assert(!waiting_room_can_start(NULL));
	printf("PASS test_double_needs_both_players_ready\n");
}

/**
 * @brief Battle Royale needs four seats filled and a strict majority ready.
 */
static void	test_battle_royale_needs_four_and_a_majority(void)
{
	app_room_view_model_t	room;

	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 3, 3);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 2);
	assert(waiting_room_required_ready(&room) == 3);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 3);
	assert(waiting_room_can_start(&room));
	/* Eight players start on five, not on eight. */
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 8, 5);
	assert(waiting_room_required_ready(&room) == 5);
	assert(waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 8, 4);
	assert(!waiting_room_can_start(&room));
	printf("PASS test_battle_royale_needs_four_and_a_majority\n");
}

/**
 * @brief A refused start says why, and only the owner may start at all.
 */
static void	test_start_blockers_explain_themselves(void)
{
	app_room_view_model_t	room;

	build_room(&room, APP_GAME_MODE_DOUBLE, 1, 1);
	assert(waiting_room_local_is_owner(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_NEED_PLAYERS);
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_NEED_READY);
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_NONE);
	room.players[0].owner = false;
	room.players[1].owner = true;
	assert(!waiting_room_local_is_owner(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_NOT_OWNER);
	assert(waiting_room_start_blocker(NULL) == ROOM_FEEDBACK_NEED_PLAYERS);
	/* A local slot outside the seated players must not be read. */
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	room.local_slot = 5;
	assert(!waiting_room_local_is_owner(&room));
	assert(!waiting_room_local_ready(&room));
	assert(!waiting_room_toggle_ready(&room));
	printf("PASS test_start_blockers_explain_themselves\n");
}

/**
 * @brief The countdown ticks to zero exactly once and can be cancelled.
 */
static void	test_countdown_runs_down_and_cancels(void)
{
	waiting_room_state_t	state;
	int						step;

	waiting_room_state_init(&state);
	assert(!waiting_room_tick(&state));
	assert(waiting_room_begin_countdown(&state));
	assert(!waiting_room_begin_countdown(&state));
	assert(state.countdown == WAITING_ROOM_COUNTDOWN_START);
	step = 1;
	while (step < WAITING_ROOM_COUNTDOWN_START)
	{
		assert(!waiting_room_tick(&state));
		assert(state.countdown == WAITING_ROOM_COUNTDOWN_START - step);
		step++;
	}
	assert(waiting_room_tick(&state));
	assert(!state.counting_down);
	/* A finished countdown must not launch a second time. */
	assert(!waiting_room_tick(&state));
	assert(waiting_room_begin_countdown(&state));
	assert(waiting_room_cancel_countdown(&state));
	assert(!state.counting_down && state.countdown == 0);
	assert(state.feedback == ROOM_FEEDBACK_CANCELLED);
	assert(!waiting_room_cancel_countdown(&state));
	printf("PASS test_countdown_runs_down_and_cancels\n");
}

/**
 * @brief Letters are commands outside the composer and text inside it.
 */
static void	test_chat_modes_read_input_differently(void)
{
	app_room_view_model_t	room;
	waiting_room_state_t	state;
	int						step;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	waiting_room_state_init(&state);
	assert(waiting_room_handle_key(&state, 'r')
		== ROOM_ACTION_TOGGLE_READY);
	assert(waiting_room_handle_key(&state, 's') == ROOM_ACTION_START);
	assert(waiting_room_handle_key(&state, 'l') == ROOM_ACTION_LEAVE);
	assert(waiting_room_handle_key(&state, NCKEY_ESC) == ROOM_ACTION_LEAVE);
	assert(waiting_room_handle_key(&state, 'q') == ROOM_ACTION_QUIT);
	assert(waiting_room_handle_key(&state, 'c') == ROOM_ACTION_NONE);
	assert(state.chatting);
	/* Inside the composer the same letters are text, so "s" cannot start. */
	assert(waiting_room_handle_key(&state, 's') == ROOM_ACTION_NONE);
	assert(waiting_room_handle_key(&state, 'l') == ROOM_ACTION_NONE);
	assert(strcmp(state.compose, "sl") == 0);
	assert(waiting_room_handle_key(&state, NCKEY_BACKSPACE)
		== ROOM_ACTION_NONE);
	assert(strcmp(state.compose, "s") == 0);
	assert(waiting_room_handle_key(&state, NCKEY_ENTER)
		== ROOM_ACTION_SEND_CHAT);
	assert(waiting_room_send_chat(&room, &state));
	assert(state.compose_length == 0);
	assert(room.chat_count == 1 && !room.chat[0].system);
	assert(strcmp(room.chat[0].author, "player0") == 0);
	/* Sending nothing says so rather than posting a blank line. */
	assert(!waiting_room_send_chat(&room, &state));
	assert(state.feedback == ROOM_FEEDBACK_CHAT_EMPTY);
	assert(room.chat_count == 1);
	step = 0;
	while (step < APP_ROOM_CHAT_TEXT_MAX * 2)
	{
		(void)waiting_room_handle_key(&state, 'x');
		step++;
	}
	assert(state.compose_length == APP_ROOM_CHAT_TEXT_MAX - 1);
	(void)waiting_room_handle_key(&state, 21);
	assert(state.compose_length == 0);
	assert(waiting_room_handle_key(&state, NCKEY_ESC) == ROOM_ACTION_NONE);
	assert(!state.chatting);
	printf("PASS test_chat_modes_read_input_differently\n");
}

/**
 * @brief The transcript is a fixed ring: the oldest line leaves, not the newest.
 */
static void	test_chat_ring_drops_the_oldest(void)
{
	app_room_view_model_t	room;
	char					line[APP_ROOM_CHAT_TEXT_MAX];
	int						step;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 0);
	step = 0;
	while (step < APP_ROOM_CHAT_MAX + 5)
	{
		snprintf(line, sizeof(line), "line-%d", step);
		assert(waiting_room_append_chat(&room, "author", line, false));
		step++;
	}
	assert(room.chat_count == APP_ROOM_CHAT_MAX);
	snprintf(line, sizeof(line), "line-%d", APP_ROOM_CHAT_MAX + 4);
	assert(strcmp(room.chat[APP_ROOM_CHAT_MAX - 1].text, line) == 0);
	snprintf(line, sizeof(line), "line-5");
	assert(strcmp(room.chat[0].text, line) == 0);
	assert(!waiting_room_append_chat(&room, "author", "", false));
	assert(!waiting_room_append_chat(NULL, "author", "hello", false));
	assert(waiting_room_append_chat(&room, "", "system line", true));
	assert(room.chat[APP_ROOM_CHAT_MAX - 1].system);
	printf("PASS test_chat_ring_drops_the_oldest\n");
}

/**
 * @brief Empty seats read as empty, and only seated players carry a badge.
 */
static void	test_slot_labels_and_badges(void)
{
	app_room_view_model_t	room;
	char					line[APP_TEXT_MAX * 2];

	build_room(&room, APP_GAME_MODE_DOUBLE, 1, 1);
	assert(strstr(waiting_room_slot_label(&room, 0, line, sizeof(line)),
			"(owner)") != NULL);
	assert(strstr(waiting_room_slot_label(&room, 1, line, sizeof(line)),
			"(empty)") != NULL);
	assert(waiting_room_slot_label(&room, 99, line, sizeof(line))[0] == '\0');
	assert(strcmp(waiting_room_badge_text(&room, 0), "ready") == 0);
	assert(waiting_room_badge_text(&room, 1)[0] == '\0');
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	assert(strcmp(waiting_room_badge_text(&room, 1), "not ready") == 0);
	assert(waiting_room_badge_text(NULL, 0)[0] == '\0');
	assert(waiting_room_toggle_ready(&room));
	assert(!waiting_room_local_ready(&room));
	assert(waiting_room_toggle_ready(&room));
	assert(waiting_room_local_ready(&room));
	printf("PASS test_slot_labels_and_badges\n");
}

/**
 * @brief The status line names the actual blocker, and the countdown wins.
 */
static void	test_status_and_feedback_copy(void)
{
	app_room_view_model_t	room;
	waiting_room_state_t	state;
	char					line[APP_TEXT_MAX * 2];
	int						feedback;

	waiting_room_state_init(&state);
	build_room(&room, APP_GAME_MODE_DOUBLE, 1, 1);
	assert(strstr(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Waiting for opponents") != NULL);
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	assert(strstr(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"ready") != NULL);
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(strcmp(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Ready to start") == 0);
	(void)waiting_room_begin_countdown(&state);
	assert(strstr(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Starting in") != NULL);
	waiting_room_state_init(&state);
	assert(waiting_room_feedback_text(&state, line, sizeof(line))[0] == '\0');
	feedback = ROOM_FEEDBACK_READY;
	while (feedback <= ROOM_FEEDBACK_VOLUME)
	{
		state.feedback = (room_feedback_t)feedback;
		state.feedback_value = 70;
		assert(waiting_room_feedback_text(&state, line, sizeof(line))[0]
			!= '\0');
		feedback++;
	}
	state.feedback = ROOM_FEEDBACK_VOLUME;
	assert(strstr(waiting_room_feedback_text(&state, line, sizeof(line)),
			"70%") != NULL);
	assert(!waiting_room_navigation_keys_coalesce(NCKEY_UP, NCKEY_UP));
	assert(waiting_room_action_leaves_screen(ROOM_ACTION_LAUNCH));
	assert(!waiting_room_action_leaves_screen(ROOM_ACTION_SEND_CHAT));
	printf("PASS test_status_and_feedback_copy\n");
}

/**
 * @brief The room's mode decides which match screen the countdown hands off to.
 */
static void	test_launch_action_matches_the_mode(void)
{
	app_room_view_model_t	room;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(waiting_room_launch_action(&room) == APP_NAV_START_DOUBLE);
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 4);
	assert(waiting_room_launch_action(&room) == APP_NAV_START_BATTLE_ROYALE);
	assert(waiting_room_launch_action(NULL) == APP_NAV_START_DOUBLE);
	printf("PASS test_launch_action_matches_the_mode\n");
}
