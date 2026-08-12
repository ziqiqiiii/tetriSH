#include "tetrisu.h"

static void	test_double_needs_both_players_ready(void);
static void	test_battle_royale_needs_four_and_everyone_ready(void);
static void	test_room_status_and_auto_start_policy(void);
static void	test_battle_royale_capacity_and_roster_bounds(void);
static void	test_roster_pagination(void);
static void	test_start_blockers_explain_themselves(void);
static void	test_countdown_runs_down_and_cancels(void);
static void	test_chat_modes_read_input_differently(void);
static void	test_chat_ring_drops_the_oldest(void);
static void	test_slot_labels_and_badges(void);
static void	test_status_and_feedback_copy(void);
static void	test_launch_action_matches_the_mode(void);
static void	build_room(t_app_room_view_model *room, t_app_game_mode mode,
				int players, int ready);

int	main(void)
{
	test_double_needs_both_players_ready();
	test_battle_royale_needs_four_and_everyone_ready();
	test_room_status_and_auto_start_policy();
	test_battle_royale_capacity_and_roster_bounds();
	test_roster_pagination();
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
 * @brief Builds a room owned from seat zero with a chosen ready-seat prefix.
 */
static void	build_room(t_app_room_view_model *room, t_app_game_mode mode,
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
	t_app_room_view_model	room;

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
 * @brief Battle Royale needs four seats and every occupied player ready.
 */
static void	test_battle_royale_needs_four_and_everyone_ready(void)
{
	t_app_room_view_model	room;

	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 3, 3);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 3);
	assert(waiting_room_required_ready(&room) == 4);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 4);
	assert(waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 99, 98);
	assert(waiting_room_required_ready(&room) == 99);
	assert(!waiting_room_can_start(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 99, 99);
	assert(waiting_room_can_start(&room));
	printf("PASS test_battle_royale_needs_four_and_everyone_ready\n");
}

/**
 * @brief Only a ready Double auto-starts; room statuses remain reconciled.
 */
static void	test_room_status_and_auto_start_policy(void)
{
	t_app_room_view_model	room;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(waiting_room_auto_start_allowed(&room));
	assert(waiting_room_sync_state(&room));
	assert(room.state == APP_ROOM_STATE_READY);
	assert(waiting_room_auto_start_allowed(&room));
	room.players[1].ready = false;
	assert(waiting_room_sync_state(&room));
	assert(room.state == APP_ROOM_STATE_WAITING);
	assert(!waiting_room_auto_start_allowed(&room));
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 4);
	assert(waiting_room_sync_state(&room));
	assert(room.state == APP_ROOM_STATE_READY);
	assert(waiting_room_can_start(&room));
	assert(!waiting_room_auto_start_allowed(&room));
	room.state = APP_ROOM_STATE_IN_GAME;
	assert(!waiting_room_can_start(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_UNAVAILABLE);
	room.state = APP_ROOM_STATE_FINISHED;
	assert(!waiting_room_can_start(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_UNAVAILABLE);
	/*
	 * SELECTING is the server's alone. Recomputing it away answered "is this
	 * room under way" with no, so the screen sat still while the room it was
	 * showing was already committing to a match.
	 */
	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	room.state = APP_ROOM_STATE_SELECTING;
	assert(!waiting_room_sync_state(&room));
	assert(room.state == APP_ROOM_STATE_SELECTING);
	assert(waiting_room_is_under_way(&room));
	printf("PASS test_room_status_and_auto_start_policy\n");
}

/**
 * @brief Invalid capacities never start and the fixed roster stays in bounds.
 */
static void	test_battle_royale_capacity_and_roster_bounds(void)
{
	t_app_room_view_model	room;

	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 4);
	assert(waiting_room_slot_count(&room) == 99);
	assert(waiting_room_visible_slot_count(&room)
		== WAITING_ROOM_VISIBLE_PLAYERS);
	room.capacity = 3;
	assert(!waiting_room_can_start(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_INVALID_ROOM);
	room.capacity = 100;
	assert(waiting_room_slot_count(&room) == APP_ROOM_MAX_PLAYERS);
	assert(!waiting_room_can_start(&room));
	room.capacity = APP_ROOM_MAX_PLAYERS;
	room.player_count = APP_ROOM_MAX_PLAYERS + 1;
	assert(!waiting_room_can_start(&room));
	assert(waiting_room_start_blocker(&room) == ROOM_FEEDBACK_INVALID_ROOM);
	printf("PASS test_battle_royale_capacity_and_roster_bounds\n");
}

/**
 * @brief The eight-row window can reach every seat in a 99-player room.
 */
static void	test_roster_pagination(void)
{
	t_app_room_view_model	room;
	t_waiting_room_state	state;
	t_waiting_room_state	before;
	int					step;

	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 99, 50);
	waiting_room_state_init(&state);
	before = state;
	assert(waiting_room_handle_key(&state, &room, NCKEY_DOWN)
		== ROOM_ACTION_NONE);
	assert(state.roster_offset == 1);
	assert(waiting_room_state_view_changed(&before, &state));
	(void)waiting_room_handle_key(&state, &room, NCKEY_PGDOWN);
	assert(state.roster_offset == 9);
	step = 0;
	while (step++ < 20)
		(void)waiting_room_handle_key(&state, &room, NCKEY_PGDOWN);
	assert(state.roster_offset == APP_ROOM_MAX_PLAYERS
		- WAITING_ROOM_VISIBLE_PLAYERS);
	(void)waiting_room_handle_key(&state, &room, NCKEY_UP);
	assert(state.roster_offset == APP_ROOM_MAX_PLAYERS
		- WAITING_ROOM_VISIBLE_PLAYERS - 1);
	(void)waiting_room_handle_key(&state, &room, NCKEY_PGUP);
	assert(state.roster_offset == APP_ROOM_MAX_PLAYERS
		- WAITING_ROOM_VISIBLE_PLAYERS * 2 - 1);
	(void)waiting_room_handle_key(&state, &room, 'c');
	before = state;
	(void)waiting_room_handle_key(&state, &room, NCKEY_DOWN);
	assert(state.roster_offset == before.roster_offset);
	printf("PASS test_roster_pagination\n");
}

/**
 * @brief A refused start says why, and only the owner may start at all.
 */
static void	test_start_blockers_explain_themselves(void)
{
	t_app_room_view_model	room;

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
	t_waiting_room_state	state;
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
	t_app_room_view_model	room;
	t_waiting_room_state	state;
	int						step;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 1);
	waiting_room_state_init(&state);
	assert(waiting_room_handle_key(&state, &room, 'r')
		== ROOM_ACTION_TOGGLE_READY);
	assert(waiting_room_handle_key(&state, &room, 's') == ROOM_ACTION_START);
	assert(waiting_room_handle_key(&state, &room, 'l') == ROOM_ACTION_LEAVE);
	assert(waiting_room_handle_key(&state, &room, NCKEY_ESC)
		== ROOM_ACTION_LEAVE);
	assert(waiting_room_handle_key(&state, &room, 'q') == ROOM_ACTION_QUIT);
	assert(waiting_room_handle_key(&state, &room, 'c') == ROOM_ACTION_NONE);
	assert(state.chatting);
	/* Inside the composer the same letters are text, so "s" cannot start. */
	assert(waiting_room_handle_key(&state, &room, 's') == ROOM_ACTION_NONE);
	assert(waiting_room_handle_key(&state, &room, 'l') == ROOM_ACTION_NONE);
	assert(strcmp(state.compose, "sl") == 0);
	assert(waiting_room_handle_key(&state, &room, NCKEY_BACKSPACE)
		== ROOM_ACTION_NONE);
	assert(strcmp(state.compose, "s") == 0);
	assert(waiting_room_handle_key(&state, &room, NCKEY_ENTER)
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
		(void)waiting_room_handle_key(&state, &room, 'x');
		step++;
	}
	assert(state.compose_length == APP_ROOM_CHAT_TEXT_MAX - 1);
	(void)waiting_room_handle_key(&state, &room, 21);
	assert(state.compose_length == 0);
	assert(waiting_room_handle_key(&state, &room, NCKEY_ESC)
		== ROOM_ACTION_NONE);
	assert(!state.chatting);
	printf("PASS test_chat_modes_read_input_differently\n");
}

/**
 * @brief The transcript is a fixed ring: the oldest line leaves, not the newest.
 */
static void	test_chat_ring_drops_the_oldest(void)
{
	t_app_room_view_model	room;
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
	t_app_room_view_model	room;
	char					line[APP_TEXT_MAX * 2];

	build_room(&room, APP_GAME_MODE_DOUBLE, 1, 1);
	assert(strstr(waiting_room_slot_label(&room, 0, line, sizeof(line)),
			"(owner)") != NULL);
	assert(strstr(waiting_room_slot_label(&room, 1, line, sizeof(line)),
			"(empty)") != NULL);
	assert(waiting_room_slot_label(&room, 99, line, sizeof(line))[0] == '\0');
	assert(waiting_room_slot_count(&room) == WAITING_ROOM_DOUBLE_PLAYERS);
	assert(waiting_room_visible_slot_count(&room)
		== WAITING_ROOM_DOUBLE_PLAYERS);
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
	t_app_room_view_model	room;
	t_waiting_room_state	state;
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
	room.state = APP_ROOM_STATE_IN_GAME;
	assert(strcmp(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Game in progress") == 0);
	room.state = APP_ROOM_STATE_FINISHED;
	assert(strstr(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Game over") != NULL);
	room.state = APP_ROOM_STATE_READY;
	(void)waiting_room_begin_countdown(&state);
	assert(strstr(waiting_room_status_text(&room, &state, line, sizeof(line)),
			"Starting in") != NULL);
	waiting_room_state_init(&state);
	assert(waiting_room_feedback_text(&state, line, sizeof(line))[0] == '\0');
	feedback = ROOM_FEEDBACK_READY;
	while (feedback <= ROOM_FEEDBACK_VOLUME)
	{
		state.feedback = (t_room_feedback)feedback;
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
	t_app_room_view_model	room;

	build_room(&room, APP_GAME_MODE_DOUBLE, 2, 2);
	assert(waiting_room_launch_action(&room) == APP_NAV_START_DOUBLE);
	build_room(&room, APP_GAME_MODE_BATTLE_ROYALE, 4, 4);
	assert(waiting_room_launch_action(&room) == APP_NAV_START_BATTLE_ROYALE);
	assert(waiting_room_launch_action(NULL) == APP_NAV_START_DOUBLE);
	printf("PASS test_launch_action_matches_the_mode\n");
}
