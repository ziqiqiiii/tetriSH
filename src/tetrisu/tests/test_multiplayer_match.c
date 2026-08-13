#include "tetrisu.h"

static void	load_fixture(t_app_profile_view_model *profile,
				t_app_catalogue_view_model *characters);
static void	test_character_selection_locks_and_times_out(void);
static void	test_targeting_uses_wasd_diamond(void);
static void	test_result_copy_includes_battle_rank(void);
static void	test_match_layouts_follow_wireframes(void);
static void	test_room_roster_controls_opponent_count(void);
static void	test_pixel_ability_targets_follow_layout(void);
static void	test_the_arena_puts_what_matters_in_the_near_columns(void);
static void	test_a_mode_that_matches_nobody_says_ANY(void);
static void	test_multiplayer_movement_matches_solo_at_wall(void);

int	main(void)
{
	test_character_selection_locks_and_times_out();
	test_targeting_uses_wasd_diamond();
	test_result_copy_includes_battle_rank();
	test_match_layouts_follow_wireframes();
	test_room_roster_controls_opponent_count();
	test_pixel_ability_targets_follow_layout();
	test_multiplayer_movement_matches_solo_at_wall();
	test_the_arena_puts_what_matters_in_the_near_columns();
	test_a_mode_that_matches_nobody_says_ANY();
	return (0);
}

/**
 * @brief A match routes movement through Solo's handling without altering it.
 *
 * Every assertion here is a property of solo_handling_event() itself, which is
 * the point: the match is required to hand the terminal's event type over
 * unchanged, so holding a key charges DAS and repeats on the application's
 * clock exactly as it does in single player.
 */
static void	test_multiplayer_movement_matches_solo_at_wall(void)
{
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_mp_match_state			state;
	t_solo_handling_config		config;
	t_solo_handling_state		handling;
	t_solo_action				action;
	t_solo_action				repeats[SOLO_HANDLING_ACTION_CAP];
	int						wall_col;

	load_fixture(&profile, &characters);
	mp_match_state_init(&state, APP_GAME_MODE_BATTLE_ROYALE, "tap-test",
		&profile, &characters, 19u);
	state.phase = MP_MATCH_PLAYING;
	state.local_game.countdown_active = false;
	config = solo_handling_default_config();
	solo_handling_reset(&handling);
	/* A press moves once and takes the key held, charging DAS. */
	assert(mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
		NCTYPE_PRESS, &action));
	assert(action == SOLO_MOVE_LEFT);
	assert(handling.horizontal_direction == -1);
	assert(solo_game_apply_action(&state.local_game, action));
	/* Nothing repeats before DAS elapses; afterwards ARR owns the rate. */
	assert(solo_handling_update(&handling, &config,
			gravity_interval_ms(state.local_game.level), config.das_ms - 1,
			repeats, SOLO_HANDLING_ACTION_CAP) == 0);
	assert(solo_handling_update(&handling, &config,
			gravity_interval_ms(state.local_game.level), 1, repeats,
			SOLO_HANDLING_ACTION_CAP) == 1);
	assert(repeats[0] == SOLO_MOVE_LEFT);
	/* The host's own key repeat is ignored while the key is already held. */
	assert(!mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
			NCTYPE_REPEAT, &action));
	assert(handling.horizontal_direction == -1);
	while (solo_game_apply_action(&state.local_game, SOLO_MOVE_LEFT))
		;
	wall_col = state.local_game.active.col;
	/* Pressing the opposite direction at the wall takes the axis over. */
	assert(mp_match_movement_event(&state, &handling, &config, NCKEY_RIGHT,
		NCTYPE_PRESS, &action));
	assert(action == SOLO_MOVE_RIGHT);
	assert(handling.horizontal_direction == 1);
	assert(solo_game_apply_action(&state.local_game, action));
	assert(state.local_game.active.col == wall_col + 1);
	/* Releasing it hands the axis back to the key still held. */
	assert(mp_match_movement_event(&state, &handling, &config, NCKEY_RIGHT,
		NCTYPE_RELEASE, &action));
	assert(action == SOLO_MOVE_LEFT);
	assert(handling.horizontal_direction == -1);
	assert(!mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
		NCTYPE_RELEASE, &action));
	assert(handling.horizontal_direction == 0);
	/* Soft drop is held the same way, and a release ends it. */
	assert(mp_match_movement_event(&state, &handling, &config, NCKEY_DOWN,
		NCTYPE_PRESS, &action));
	assert(action == SOLO_SOFT_DROP && handling.down_held);
	assert(!mp_match_movement_event(&state, &handling, &config, NCKEY_DOWN,
		NCTYPE_RELEASE, &action));
	assert(!handling.down_held);
	/* A board that is not the player's to move still clears held keys. */
	solo_handling_reset(&handling);
	assert(mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
		NCTYPE_PRESS, &action));
	state.local_game.paused = true;
	assert(!mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
		NCTYPE_PRESS, &action));
	assert(!mp_match_movement_event(&state, &handling, &config, NCKEY_LEFT,
		NCTYPE_RELEASE, &action));
	assert(handling.horizontal_direction == 0);
	state.local_game.paused = false;
	printf("PASS test_multiplayer_movement_matches_solo_at_wall\n");
}

static void	test_pixel_ability_targets_follow_layout(void)
{
	t_mp_match_pixel_layout layout;
	int index;

	mp_match_pixel_layout_build(APP_GAME_MODE_DOUBLE, 2400, 1350, &layout);
	assert(layout.valid);
	assert(layout.loadout.x < layout.local_board.x);
	assert(layout.ability_bar.x + layout.ability_bar.width
		< layout.local_board.x);
	assert(layout.local_board.x - layout.ability_bar.x
		< layout.ability_bar.width * 2);
	assert(layout.local_board.x + layout.local_board.width
		< layout.opponent_board.x);
	/*
	 * The rival's column mirrors the local one past their board, and the
	 * arena has to have made room for it: a layout that sized the tiles from
	 * one pair of chrome columns and then drew two put the far one off the
	 * right edge of the screen.
	 */
	assert(layout.opponent_board.x + layout.opponent_board.width
		< layout.opponent_ability_bar.x);
	assert(layout.opponent_ability_bar.x + layout.opponent_ability_bar.width
		< layout.opponent_loadout.x);
	assert(layout.opponent_loadout.x + layout.opponent_loadout.width
		<= layout.width);
	assert(layout.opponent_ability_center_x
		> layout.opponent_ability_bar.x);
	assert(layout.opponent_loadout.width == layout.loadout.width);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		assert(mp_match_ability_at_pixel(&layout,
			layout.ability_center_x, layout.ability_center_y[index])
			== index + 1);
		index++;
	}
	assert(mp_match_ability_at_pixel(&layout, layout.local_board.x,
		layout.local_board.y) == 0);
	mp_match_pixel_layout_build(APP_GAME_MODE_BATTLE_ROYALE,
		2400, 1350, &layout);
	assert(layout.valid);
	assert(layout.ability_bar.x + layout.ability_bar.width
		< layout.local_board.x);
	assert(layout.local_board.x - layout.ability_bar.x
		< layout.ability_bar.width * 2);
	assert(layout.left_opponents.x + layout.left_opponents.width
		< layout.ability_bar.x);
	assert(layout.local_board.x + layout.local_board.width
		< layout.right_opponents.x);
	/* Battle Royale has no single rival, so it is given no column. */
	assert(layout.opponent_loadout.width == 0);
	assert(layout.opponent_ability_bar.width == 0);
	printf("PASS test_pixel_ability_targets_follow_layout\n");
}

static void	load_fixture(t_app_profile_view_model *profile,
	t_app_catalogue_view_model *characters)
{
	t_app_data_provider	provider;

	app_fixture_provider_init(&provider);
	assert(provider.load_profile(provider.userdata, profile) == APP_PROVIDER_OK);
	assert(provider.load_catalogue(provider.userdata, APP_CATALOGUE_CHARACTERS,
			characters) == APP_PROVIDER_OK);
}

/**
 * Only owned characters cycle, Enter freezes the cursor, and timeout preserves
 * that exact character and its four ability records for the match renderer.
 */
static void	test_character_selection_locks_and_times_out(void)
{
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_mp_match_state		state;
	const t_app_catalogue_item_view_model	*selected;
	uint32_t				events;

	load_fixture(&profile, &characters);
	mp_match_state_init(&state, APP_GAME_MODE_DOUBLE, "duel-42", &profile,
		&characters, 42u);
	assert(state.phase == MP_MATCH_CHARACTER_SELECT);
	assert(mp_match_character_seconds(&state) == 15);
	assert(strcmp(mp_match_selected_character(&state)->name, "Mirurun") == 0);
	assert(mp_match_character_handle_key(&state, NCKEY_RIGHT));
	assert(strcmp(mp_match_selected_character(&state)->name, "Halloween") == 0);
	/* Princess and Wolf-man are locked in the fixture and are skipped. */
	assert(mp_match_character_handle_key(&state, NCKEY_RIGHT));
	assert(strcmp(mp_match_selected_character(&state)->name, "Mirurun") == 0);
	assert(mp_match_character_handle_key(&state, NCKEY_LEFT));
	assert(strcmp(mp_match_selected_character(&state)->name, "Halloween") == 0);
	assert(mp_match_character_handle_key(&state, NCKEY_ENTER));
	assert(state.selection.locked);
	assert(!mp_match_character_handle_key(&state, NCKEY_LEFT));
	events = mp_match_character_update(&state, 9999);
	assert(events == MP_SELECTION_EVENT_NONE);
	assert(mp_match_character_seconds(&state) == 6);
	events = mp_match_character_update(&state, 1);
	assert((events & MP_SELECTION_EVENT_SECOND) != 0);
	assert(mp_match_character_seconds(&state) == 5);
	events = mp_match_character_update(&state, 5000);
	assert((events & MP_SELECTION_EVENT_FINISHED) != 0);
	assert(state.phase == MP_MATCH_PLAYING);
	assert(state.local_game.countdown_active);
	assert(state.opponent_game.countdown_active);
	selected = mp_match_selected_character(&state);
	assert(selected != NULL && strcmp(selected->name, "Halloween") == 0);
	assert(strcmp(selected->abilities[2].name, "Vampire") == 0);
	printf("PASS test_character_selection_locks_and_times_out\n");
}

static void	test_targeting_uses_wasd_diamond(void)
{
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_mp_match_state		state;

	load_fixture(&profile, &characters);
	mp_match_state_init(&state, APP_GAME_MODE_BATTLE_ROYALE, "arena-88",
		&profile, &characters, 7u);
	state.phase = MP_MATCH_PLAYING;
	assert(state.target_mode == TARGET_RANDOM);
	assert(mp_match_target_handle_key(&state, 'w'));
	assert(state.target_mode == TARGET_KO);
	assert(strcmp(mp_match_target_name(state.target_mode), "KOs") == 0);
	assert(mp_match_target_handle_key(&state, 'a'));
	assert(state.target_mode == TARGET_RANDOM);
	assert(mp_match_target_handle_key(&state, 's'));
	assert(state.target_mode == TARGET_ATTACKERS);
	assert(mp_match_target_handle_key(&state, 'd'));
	assert(state.target_mode == TARGET_BADGES);
	assert(strcmp(mp_match_target_name(state.target_mode), "Badges") == 0);
	state.mode = APP_GAME_MODE_DOUBLE;
	assert(!mp_match_target_handle_key(&state, 'w'));
	printf("PASS test_targeting_uses_wasd_diamond\n");
}

static void	test_result_copy_includes_battle_rank(void)
{
	t_mp_match_state	state;
	char				text[MP_MATCH_STATUS_MAX];

	memset(&state, 0, sizeof(state));
	state.mode = APP_GAME_MODE_BATTLE_ROYALE;
	mp_match_finish(&state, false, 17);
	assert(state.phase == MP_MATCH_FINISHED);
	assert(state.final_rank == 17);
	assert(strstr(mp_match_result_text(&state, text, sizeof(text)),
			"RANK #17") != NULL);
	mp_match_finish(&state, true, 99);
	assert(state.final_rank == 1);
	assert(strcmp(mp_match_result_text(&state, text, sizeof(text)),
			"WOW, YOU WON!") == 0);
	state.mode = APP_GAME_MODE_DOUBLE;
	mp_match_finish(&state, false, 2);
	assert(strcmp(mp_match_result_text(&state, text, sizeof(text)),
			"SO SAD, YOU LOST") == 0);
	printf("PASS test_result_copy_includes_battle_rank\n");
}

static void	test_match_layouts_follow_wireframes(void)
{
	t_mp_match_layout	layout;

	mp_match_layout_build(APP_GAME_MODE_DOUBLE, 40, 120, &layout);
	assert(layout.valid);
	assert(layout.local_board.x < layout.opponent_board.x);
	assert(layout.local_board.width == 22);
	assert(layout.opponent_board.width == 22);
	assert(layout.local_board.height == BOARD_HEIGHT + 2);
	assert(layout.abilities.x < layout.local_board.x);
	mp_match_layout_build(APP_GAME_MODE_BATTLE_ROYALE, 60, 210, &layout);
	assert(layout.valid);
	assert(layout.abilities.x < layout.left_opponents.x);
	assert(layout.left_opponents.x + layout.left_opponents.width
		< layout.local_board.x);
	assert(layout.local_board.x + layout.local_board.width
		< layout.right_opponents.x);
	assert(layout.targeting.y < layout.local_board.y);
	/*
	 * The arena gives way before the screen does. 132x36 is comfortable and
	 * was the minimum; a terminal one row short got a refusal and a blank
	 * screen. Now both side columns go, then one, then the board and the head
	 * count stand alone - and only a terminal too small for the board itself
	 * is refused.
	 */
	mp_match_layout_build(APP_GAME_MODE_BATTLE_ROYALE, 30, 100, &layout);
	assert(layout.valid);
	assert(layout.left_opponents.width > 0);
	assert(layout.right_opponents.width == 0);
	mp_match_layout_build(APP_GAME_MODE_BATTLE_ROYALE, 24, 80, &layout);
	assert(layout.valid);
	assert(layout.left_opponents.width == 0);
	assert(layout.right_opponents.width == 0);
	assert(layout.local_board.x + layout.local_board.width <= 80);
	mp_match_layout_build(APP_GAME_MODE_BATTLE_ROYALE, 23, 79, &layout);
	assert(!layout.valid);
	printf("PASS test_match_layouts_follow_wireframes\n");
}

static void	test_room_roster_controls_opponent_count(void)
{
	t_app_profile_view_model	profile;
	t_app_catalogue_view_model	characters;
	t_app_room_view_model		room;
	t_mp_match_state		state;
	int					index;
	int					present;

	load_fixture(&profile, &characters);
	memset(&room, 0, sizeof(room));
	room.mode = APP_GAME_MODE_BATTLE_ROYALE;
	room.player_count = 4;
	room.local_slot = 1;
	index = 0;
	while (index < room.player_count)
	{
		snprintf(room.players[index].username,
			sizeof(room.players[index].username), "roster-%d", index);
		index++;
	}
	snprintf(room.players[room.local_slot].username,
		sizeof(room.players[room.local_slot].username), "%s", profile.username);
	mp_match_state_init(&state, APP_GAME_MODE_BATTLE_ROYALE, "arena",
		&profile, &characters, 9u);
	mp_match_apply_room(&state, &room, 99, true);
	assert(state.players_total == 4);
	present = 0;
	index = 0;
	while (index < MP_ARENA_SEATS)
	{
		present += state.opponents[index].present;
		index++;
	}
	assert(present == 3);
	/*
	 * From seat 1, because a Battle Royale's cards are filed by seat and a
	 * room numbers its seats from 1. The fixture lays itself out the way the
	 * server's arena does, or the preview labels every card as the seat next
	 * door to the one it draws.
	 */
	assert(strcmp(state.opponents[1].name, "roster-0") == 0);
	assert(strcmp(state.opponents[2].name, "roster-2") == 0);
	assert(!state.opponents[0].present);
	mp_match_apply_room(&state, NULL, 99, true);
	assert(state.players_total == 99);
	assert(state.opponents[98].present);
	printf("PASS test_room_roster_controls_opponent_count\n");
}

/*
** Ninety-eight thumbnails is a crowd, not a list, and a player cannot search
** it while a piece is falling. So the order is what they need in the order
** they need it: whoever is attacking them, then whoever their mode has
** singled out, then everybody still playing, then the dead.
**
** It is an order over an index list and never over the cards. A card keeps
** the seat it was filed under whatever tier it is drawn in - which is what
** stops a rival changing tier from losing the board being held for them - and
** the sort is stable on the seat, so a card only moves when its tier does.
*/
static void	test_the_arena_puts_what_matters_in_the_near_columns(void)
{
	t_mp_match_state	state;
	int					cards[MP_ARENA_SEATS];
	int					count;

	memset(&state, 0, sizeof(state));
	state.mode = APP_GAME_MODE_BATTLE_ROYALE;
	state.opponents[1].present = true;
	state.opponents[1].alive = true;
	state.opponents[2].present = true;
	state.opponents[2].alive = false;
	state.opponents[3].present = true;
	state.opponents[3].alive = true;
	state.opponents[3].targeting_local = true;
	state.opponents[4].present = true;
	state.opponents[4].alive = true;
	state.opponents[4].targeted_by_local = true;
	state.opponents[5].present = true;
	state.opponents[5].local = true;
	count = mp_match_collect_cards(&state, cards, MP_ARENA_SEATS);
	/* four rivals; the local card is drawn full size elsewhere */
	assert(count == 4);
	assert(cards[0] == 3);
	assert(cards[1] == 4);
	assert(cards[2] == 1);
	assert(cards[3] == 2);
	/* and the seats themselves have not moved */
	assert(state.opponents[3].targeting_local);
	assert(state.opponents[5].local);
	printf("PASS test_the_arena_puts_what_matters_in_the_near_columns\n");
}

/*
** A declared mode with nothing matching it reads ANY, not (0).
**
** The server does not drop an attack whose mode matched nobody - it falls back
** to one drawn live rival, which is what Randoms does. (0) says the opposite,
** and it was read that way: a player declared Attackers before anybody had
** attacked, saw a zero and no marked cards, and reported that targeting had
** stopped working. Nothing had; the legend was describing an empty match set
** as an empty outcome.
**
** Randoms never carries a number at all, matched or not: its set is everybody,
** so a count beside it would be the head count written twice.
*/
static void	test_a_mode_that_matches_nobody_says_ANY(void)
{
	t_mp_match_state	state;
	char				label[APP_TEXT_MAX];

	memset(&state, 0, sizeof(state));
	state.mode = APP_GAME_MODE_BATTLE_ROYALE;
	state.target_mode = TARGET_ATTACKERS;
	mp_match_target_label(&state, TARGET_ATTACKERS, "S ATTACKERS", label,
		sizeof(label));
	assert(strcmp(label, "S ATTACKERS (ANY)") == 0);
	/* an unselected mode carries nothing, whatever is on the board */
	mp_match_target_label(&state, TARGET_KO, "W KOs", label, sizeof(label));
	assert(strcmp(label, "W KOs") == 0);
	/* and once somebody matches, the count is the count */
	state.opponents[2].present = true;
	state.opponents[2].targeted_by_local = true;
	state.opponents[6].present = true;
	state.opponents[6].targeted_by_local = true;
	mp_match_target_label(&state, TARGET_ATTACKERS, "S ATTACKERS", label,
		sizeof(label));
	assert(strcmp(label, "S ATTACKERS (2)") == 0);
	/* Randoms is selected and still bare */
	state.target_mode = TARGET_RANDOM;
	mp_match_target_label(&state, TARGET_RANDOM, "A RANDOMS", label,
		sizeof(label));
	assert(strcmp(label, "A RANDOMS") == 0);
	printf("PASS test_a_mode_that_matches_nobody_says_ANY\n");
}
