#include "tetrisu.h"

// Static Functions
static void	test_init_has_active_and_three_previews(void);
static void	test_ghost_and_hard_drop(void);
static void	test_ghost_stops_above_stack(void);
static void	test_soft_drop_overrides_gravity_timer(void);
static void	test_first_hold_consumes_queue_once(void);
static void	test_hold_rearms_after_lock(void);
static void	test_locking_piece_in_top_row_reveals_before_game_over(void);
static void	test_locking_piece_below_top_row_continues(void);
static void	test_blocked_spawn_with_empty_top_row_reveals_before_game_over(
	void);
static void	test_catchup_cannot_skip_new_top_out_reveal(void);
static void	test_lock_delay_waits_half_second(void);
static void	test_landing_starts_a_fresh_lock_delay(void);
static void	test_next_wake_tracks_gravity_and_lock_deadlines(void);
static void	test_zero_elapsed_consumes_due_deadlines(void);
static void	test_grounded_piece_sleeps_until_lock_deadline(void);
static void	test_clear_duration_progresses_by_level(void);
static void	test_level_seven_clear_splits_at_midpoint(void);
static void	test_clear_animation_wakes_at_visual_boundaries(void);
static void	test_pause_freezes_active_and_clear_timers(void);
static void	test_clear_animation_then_level_and_meter_update(void);
static void	test_danger_state_has_stable_exit_hysteresis(void);
static void	test_gameplay_events_are_one_shot(void);
static void	test_personal_best_only_finishes_once_at_game_over(void);
static void	test_rotation_stress_preserves_valid_state(void);
static int	filled_cells(const t_board *board);
static void	prepare_single_line_clear(solo_game_t *game, uint32_t seed);

/**
 * @brief Runs the complete local Solo state regression suite.
 *
 * Each test aborts on the first failed invariant and prints its PASS marker
 *   only after success.
 *
 * @return 0 after every Solo regression passes.
 */
int	main(void)
{
	test_init_has_active_and_three_previews();
	test_ghost_and_hard_drop();
	test_ghost_stops_above_stack();
	test_soft_drop_overrides_gravity_timer();
	test_first_hold_consumes_queue_once();
	test_hold_rearms_after_lock();
	test_locking_piece_in_top_row_reveals_before_game_over();
	test_locking_piece_below_top_row_continues();
	test_blocked_spawn_with_empty_top_row_reveals_before_game_over();
	test_catchup_cannot_skip_new_top_out_reveal();
	test_lock_delay_waits_half_second();
	test_landing_starts_a_fresh_lock_delay();
	test_next_wake_tracks_gravity_and_lock_deadlines();
	test_zero_elapsed_consumes_due_deadlines();
	test_grounded_piece_sleeps_until_lock_deadline();
	test_clear_duration_progresses_by_level();
	test_level_seven_clear_splits_at_midpoint();
	test_clear_animation_wakes_at_visual_boundaries();
	test_pause_freezes_active_and_clear_timers();
	test_clear_animation_then_level_and_meter_update();
	test_danger_state_has_stable_exit_hysteresis();
	test_gameplay_events_are_one_shot();
	test_personal_best_only_finishes_once_at_game_over();
	test_rotation_stress_preserves_valid_state();
	return (0);
}

/**
 * @brief Exercises odd clear duration splits into 87 ms and 88 ms frames.
 */
static void	test_level_seven_clear_splits_at_midpoint(void)
{
	solo_game_t	game;

	prepare_single_line_clear(&game, 323u);
	game.level = 7;
	assert(solo_game_next_wake_ms(&game) == 87);
	assert(solo_game_update(&game, 87));
	assert(game.phase == SOLO_CLEARING);
	assert(game.clear_elapsed_ms == 87);
	assert(solo_game_next_wake_ms(&game) == 88);
	assert(solo_game_update(&game, 88));
	assert(game.phase == SOLO_ACTIVE);
	printf("PASS test_level_seven_clear_splits_at_midpoint\n");
}

/**
 * @brief Exercises soft drop overrides accumulated natural gravity.
 */
static void	test_soft_drop_overrides_gravity_timer(void)
{
	solo_game_t	game;
	int				row;

	solo_game_init(&game, 125u);
	game.gravity_elapsed_ms = gravity_interval_ms(game.level) - 1;
	row = game.active.row;
	assert(solo_game_apply_action(&game, SOLO_SOFT_DROP));
	assert(game.active.row == row + 1);
	assert(game.gravity_elapsed_ms == 0);
	assert(game.scoring.total == 1);
	printf("PASS test_soft_drop_overrides_gravity_timer\n");
}

/**
 * @brief Exercises init has active and three previews.
 *
 * A fresh deterministic bag must provide one valid active piece and three
 *   distinct preview entries.
 */
static void	test_init_has_active_and_three_previews(void)
{
	solo_game_t	game;
	bool			seen[BRAIN_BAG_SIZE] = {false};
	int				index;

	solo_game_init(&game, 123u);
	assert(game.level == 1 && game.total_lines == 0);
	assert(game.scoring.total == 0 && game.phase == SOLO_ACTIVE);
	assert(piece_is_valid(&game.board, &game.active));
	seen[game.active.type] = true;
	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		assert(!seen[game.next[index]]);
		seen[game.next[index]] = true;
		index++;
	}
	printf("PASS test_init_has_active_and_three_previews\n");
}

/**
 * @brief Exercises ghost and hard drop.
 *
 * Ghost distance, hard-drop score, stamping, and next spawn must agree on the
 *   same landing.
 */
static void	test_ghost_and_hard_drop(void)
{
	solo_game_t	game;
	t_piece		before;
	t_piece		ghost;
	int			distance;

	solo_game_init(&game, 456u);
	before = game.active;
	distance = piece_drop_distance(&game.board, &game.active);
	ghost = solo_game_ghost(&game);
	assert(game.active.row == before.row && ghost.row == before.row + distance);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.scoring.total == (uint64_t)(distance * 2));
	assert(filled_cells(&game.board) == 4);
	assert(game.phase == SOLO_ACTIVE);
	printf("PASS test_ghost_and_hard_drop\n");
}

/**
 * @brief Exercises ghost stops above stack.
 *
 * A settled obstruction must bound the ghost without mutating the active spawn
 *   position.
 */
static void	test_ghost_stops_above_stack(void)
{
	solo_game_t	game;
	t_piece		ghost;

	solo_game_init(&game, 457u);
	game.active = piece_spawn(PIECE_O);
	board_set(&game.board, 4, BOARD_HEIGHT - 1,
		(t_cell){CELL_GARBAGE, 0});
	board_set(&game.board, 5, BOARD_HEIGHT - 1,
		(t_cell){CELL_GARBAGE, 0});
	ghost = solo_game_ghost(&game);
	assert(ghost.row == BOARD_HEIGHT - 3);
	assert(game.active.row == 0);
	printf("PASS test_ghost_stops_above_stack\n");
}

/**
 * @brief Exercises first HOLD consumes the preview head exactly once.
 */
static void	test_first_hold_consumes_queue_once(void)
{
	solo_game_t	game;
	t_piece_type	initial;
	t_piece_type	first_next;
	t_piece_type	second_next;
	t_piece		active_after_hold;

	solo_game_init(&game, 470u);
	initial = game.active.type;
	first_next = game.next[0];
	second_next = game.next[1];
	assert(!game.has_hold && !game.hold_used);
	assert(solo_game_apply_action(&game, SOLO_HOLD));
	assert(game.has_hold && game.hold_used);
	assert(game.hold == initial);
	assert(game.active.type == first_next);
	assert(game.next[0] == second_next);
	active_after_hold = game.active;
	assert(!solo_game_apply_action(&game, SOLO_HOLD));
	assert(game.active.type == active_after_hold.type);
	assert(game.active.rotation == active_after_hold.rotation);
	assert(game.active.col == active_after_hold.col);
	assert(game.active.row == active_after_hold.row);
	assert(game.hold == initial && game.hold_used);
	printf("PASS test_first_hold_consumes_queue_once\n");
}

/**
 * @brief Exercises locking rearms HOLD and a swap uses canonical spawn state.
 */
static void	test_hold_rearms_after_lock(void)
{
	solo_game_t	game;
	t_piece_type	first_held;
	t_piece_type	outgoing;
	t_piece		expected;

	solo_game_init(&game, 471u);
	first_held = game.active.type;
	assert(solo_game_apply_action(&game, SOLO_HOLD));
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_ACTIVE && !game.hold_used);
	outgoing = game.active.type;
	expected = piece_spawn(first_held);
	assert(solo_game_apply_action(&game, SOLO_HOLD));
	assert(game.hold == outgoing && game.hold_used);
	assert(game.active.type == expected.type);
	assert(game.active.rotation == expected.rotation);
	assert(game.active.col == expected.col && game.active.row == expected.row);
	printf("PASS test_hold_rearms_after_lock\n");
}

/**
 * @brief Exercises locking piece in top row reveals before game over.
 *
 * A visible row-zero lock must stamp first, reveal for the full deadline, then
 *   enter game over.
 */
static void	test_locking_piece_in_top_row_reveals_before_game_over(void)
{
	solo_game_t	game;

	solo_game_init(&game, 458u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	board_set(&game.board, 0, 2, (t_cell){CELL_GARBAGE, 0});
	board_set(&game.board, 1, 2, (t_cell){CELL_GARBAGE, 0});
	assert(piece_drop_distance(&game.board, &game.active) == 0);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == 0);
	assert(board_get(&game.board, 0, 0).type == CELL_FILLED);
	assert(board_get(&game.board, 1, 0).type == CELL_FILLED);
	assert(!solo_game_apply_action(&game, SOLO_MOVE_RIGHT));
	solo_game_toggle_pause(&game);
	assert(!game.paused);
	assert(solo_game_next_wake_ms(&game) == SOLO_TOP_OUT_REVEAL_MS);
	assert(!solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS - 1));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == SOLO_TOP_OUT_REVEAL_MS - 1);
	assert(board_get(&game.board, 0, 0).type == CELL_FILLED);
	assert(board_get(&game.board, 1, 0).type == CELL_FILLED);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_GAME_OVER);
	assert(solo_game_next_wake_ms(&game) == -1);
	printf("PASS test_locking_piece_in_top_row_reveals_before_game_over\n");
}

/**
 * @brief Exercises locking piece below top row continues.
 *
 * A lock starting below row zero must continue the endless session.
 */
static void	test_locking_piece_below_top_row_continues(void)
{
	solo_game_t	game;

	solo_game_init(&game, 459u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	game.active.row = 1;
	board_set(&game.board, 0, 3, (t_cell){CELL_GARBAGE, 0});
	board_set(&game.board, 1, 3, (t_cell){CELL_GARBAGE, 0});
	assert(piece_drop_distance(&game.board, &game.active) == 0);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_ACTIVE);
	printf("PASS test_locking_piece_below_top_row_continues\n");
}

/**
 * @brief Exercises blocked spawn with empty top row reveals before game over.
 *
 * Spawn collision must reveal the final board even when visible row zero is
 *   empty.
 */
static void	test_blocked_spawn_with_empty_top_row_reveals_before_game_over(
	void)
{
	solo_game_t	game;
	int				col;

	solo_game_init(&game, 460u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	game.active.row = BOARD_HEIGHT - 2;
	game.next[0] = PIECE_O;
	board_set(&game.board, 4, 1, (t_cell){CELL_GARBAGE, 0});
	assert(piece_drop_distance(&game.board, &game.active) == 0);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	col = 0;
	while (col < BOARD_WIDTH)
	{
		assert(board_get(&game.board, col, 0).type == CELL_EMPTY);
		col++;
	}
	assert(filled_cells(&game.board) == 5);
	assert(!piece_is_valid(&game.board, &game.active));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == 0);
	assert(!solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS - 1));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(filled_cells(&game.board) == 5);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_GAME_OVER);
	printf("PASS test_blocked_spawn_with_empty_top_row_reveals_"
		"before_game_over\n");
}

/**
 * @brief Exercises catchup cannot skip new top out reveal.
 *
 * Large or already-due timer updates must never consume a reveal created
 *   inside the same update.
 */
static void	test_catchup_cannot_skip_new_top_out_reveal(void)
{
	solo_game_t	game;

	solo_game_init(&game, 461u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	board_set(&game.board, 0, 2, (t_cell){CELL_GARBAGE, 0});
	board_set(&game.board, 1, 2, (t_cell){CELL_GARBAGE, 0});
	game.lock_elapsed_ms = SOLO_LOCK_DELAY_MS - 1;
	assert(solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS + 1));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == 0);
	assert(board_get(&game.board, 0, 0).type == CELL_FILLED);
	assert(board_get(&game.board, 1, 0).type == CELL_FILLED);
	assert(solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS));
	assert(game.phase == SOLO_GAME_OVER);
	solo_game_init(&game, 462u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	board_set(&game.board, 0, 2, (t_cell){CELL_GARBAGE, 0});
	board_set(&game.board, 1, 2, (t_cell){CELL_GARBAGE, 0});
	game.lock_elapsed_ms = SOLO_LOCK_DELAY_MS;
	assert(solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == 0);
	assert(solo_game_next_wake_ms(&game) == SOLO_TOP_OUT_REVEAL_MS);
	printf("PASS test_catchup_cannot_skip_new_top_out_reveal\n");
}

/**
 * @brief Exercises lock delay waits half second.
 *
 * A grounded piece must remain movable until the full Guideline lock delay
 *   expires.
 */
static void	test_lock_delay_waits_half_second(void)
{
	solo_game_t	game;

	solo_game_init(&game, 789u);
	piece_hard_drop(&game.board, &game.active);
	solo_game_update(&game, SOLO_LOCK_DELAY_MS - 1);
	assert(filled_cells(&game.board) == 0);
	solo_game_update(&game, 1);
	assert(filled_cells(&game.board) == 4);
	printf("PASS test_lock_delay_waits_half_second\n");
}

/**
 * @brief Exercises landing starts a fresh lock delay.
 *
 * Gravity time accumulated before landing must not be charged to grounded lock
 *   time.
 */
static void	test_landing_starts_a_fresh_lock_delay(void)
{
	solo_game_t	game;
	int				gravity_ms;

	solo_game_init(&game, 790u);
	game.active = piece_spawn(PIECE_O);
	game.active.row = BOARD_HEIGHT - 3;
	gravity_ms = gravity_interval_ms(game.level);
	game.gravity_elapsed_ms = gravity_ms - 1;
	assert(piece_drop_distance(&game.board, &game.active) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.active.row == BOARD_HEIGHT - 2);
	assert(game.lock_elapsed_ms == 0);
	assert(filled_cells(&game.board) == 0);
	assert(solo_game_next_wake_ms(&game) == SOLO_LOCK_DELAY_MS);
	assert(!solo_game_update(&game, SOLO_LOCK_DELAY_MS - 1));
	assert(game.lock_elapsed_ms == SOLO_LOCK_DELAY_MS - 1);
	assert(filled_cells(&game.board) == 0);
	assert(solo_game_update(&game, 1));
	assert(filled_cells(&game.board) == 4);
	printf("PASS test_landing_starts_a_fresh_lock_delay\n");
}

/**
 * @brief Exercises next wake tracks gravity and lock deadlines.
 *
 * Wake calculations must follow the active gravity and grounded lock
 *   boundaries exactly.
 */
static void	test_next_wake_tracks_gravity_and_lock_deadlines(void)
{
	solo_game_t	game;
	int				gravity_ms;

	solo_game_init(&game, 791u);
	gravity_ms = gravity_interval_ms(game.level);
	assert(solo_game_next_wake_ms(&game) == gravity_ms);
	game.gravity_elapsed_ms = 250;
	assert(solo_game_next_wake_ms(&game) == gravity_ms - 250);
	piece_hard_drop(&game.board, &game.active);
	assert(solo_game_next_wake_ms(&game) == SOLO_LOCK_DELAY_MS);
	game.lock_elapsed_ms = SOLO_LOCK_DELAY_MS - 1;
	assert(solo_game_next_wake_ms(&game) == 1);
	printf("PASS test_next_wake_tracks_gravity_and_lock_deadlines\n");
}

/**
 * @brief Exercises zero elapsed consumes due deadlines.
 *
 * Already-due gravity, lock, clear, and reveal events must progress without
 *   elapsed time.
 */
static void	test_zero_elapsed_consumes_due_deadlines(void)
{
	solo_game_t	game;
	int				gravity_ms;
	int				row_before;

	solo_game_init(&game, 794u);
	gravity_ms = gravity_interval_ms(game.level);
	game.gravity_elapsed_ms = gravity_ms;
	row_before = game.active.row;
	assert(solo_game_next_wake_ms(&game) == 0);
	assert(solo_game_update(&game, 0));
	assert(game.active.row == row_before + 1);
	assert(game.gravity_elapsed_ms == 0);
	assert(solo_game_next_wake_ms(&game) == gravity_ms);
	piece_hard_drop(&game.board, &game.active);
	game.lock_elapsed_ms = SOLO_LOCK_DELAY_MS;
	assert(solo_game_next_wake_ms(&game) == 0);
	assert(solo_game_update(&game, 0));
	assert(filled_cells(&game.board) == 4);
	assert(solo_game_next_wake_ms(&game) > 0);
	prepare_single_line_clear(&game, 795u);
	game.clear_elapsed_ms = solo_clear_duration_ms(game.level);
	assert(solo_game_next_wake_ms(&game) == 0);
	assert(solo_game_update(&game, 0));
	assert(game.phase == SOLO_ACTIVE);
	assert(game.total_lines == 1);
	assert(solo_game_next_wake_ms(&game) > 0);
	solo_game_init(&game, 797u);
	game.phase = SOLO_TOP_OUT_REVEAL;
	game.top_out_elapsed_ms = SOLO_TOP_OUT_REVEAL_MS;
	assert(solo_game_next_wake_ms(&game) == 0);
	assert(solo_game_update(&game, 0));
	assert(game.phase == SOLO_GAME_OVER);
	printf("PASS test_zero_elapsed_consumes_due_deadlines\n");
}

/**
 * @brief Exercises progressive line-clear durations.
 */
static void	test_clear_duration_progresses_by_level(void)
{
	assert(solo_clear_duration_ms(1) == 200);
	assert(solo_clear_duration_ms(6) == 200);
	assert(solo_clear_duration_ms(7) == 175);
	assert(solo_clear_duration_ms(8) == 150);
	assert(solo_clear_duration_ms(9) == 125);
	assert(solo_clear_duration_ms(10) == 100);
	assert(solo_clear_duration_ms(19) == 100);
	printf("PASS test_clear_duration_progresses_by_level\n");
}

/**
 * @brief Exercises grounded piece sleeps until lock deadline.
 *
 * High-level gravity must not cause polling while a grounded piece waits for
 *   lock.
 */
static void	test_grounded_piece_sleeps_until_lock_deadline(void)
{
	solo_game_t	game;

	solo_game_init(&game, 796u);
	game.level = 18;
	assert(gravity_interval_ms(game.level) == 1);
	piece_hard_drop(&game.board, &game.active);
	assert(solo_game_next_wake_ms(&game) == SOLO_LOCK_DELAY_MS);
	assert(!solo_game_update(&game, SOLO_LOCK_DELAY_MS - 1));
	assert(game.lock_elapsed_ms == SOLO_LOCK_DELAY_MS - 1);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(filled_cells(&game.board) == 4);
	printf("PASS test_grounded_piece_sleeps_until_lock_deadline\n");
}

/**
 * @brief Exercises clear animation wakes at visual boundaries.
 *
 * The line-clear timer must wake at its sprite midpoint and compaction
 *   endpoint.
 */
static void	test_clear_animation_wakes_at_visual_boundaries(void)
{
	solo_game_t	game;
	int				duration_ms;
	int				halfway;

	prepare_single_line_clear(&game, 322u);
	duration_ms = solo_clear_duration_ms(game.level);
	halfway = duration_ms / 2;
	assert(solo_game_next_wake_ms(&game) == halfway);
	assert(!solo_game_update(&game, halfway - 1));
	assert(game.clear_elapsed_ms == halfway - 1);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_CLEARING);
	assert(game.clear_elapsed_ms == halfway);
	assert(solo_game_next_wake_ms(&game) == halfway);
	assert(!solo_game_update(&game, halfway - 1));
	assert(game.clear_elapsed_ms == duration_ms - 1);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_ACTIVE);
	assert(game.total_lines == 1);
	printf("PASS test_clear_animation_wakes_at_visual_boundaries\n");
}

/**
 * @brief Exercises pause freezes active and clear timers.
 *
 * Pause must preserve gravity, lock, piece position, and clear-animation
 *   elapsed time.
 */
static void	test_pause_freezes_active_and_clear_timers(void)
{
	solo_game_t	active;
	solo_game_t	clearing;
	int				active_row;

	solo_game_init(&active, 792u);
	active.gravity_elapsed_ms = 250;
	active_row = active.active.row;
	solo_game_toggle_pause(&active);
	assert(active.paused);
	assert(solo_game_next_wake_ms(&active) == -1);
	assert(!solo_game_update(&active, 10000));
	assert(active.gravity_elapsed_ms == 250);
	assert(active.lock_elapsed_ms == 0);
	assert(active.active.row == active_row);
	assert(!solo_game_apply_action(&active, SOLO_SOFT_DROP));
	solo_game_toggle_pause(&active);
	assert(!active.paused);
	assert(solo_game_next_wake_ms(&active)
		== gravity_interval_ms(active.level) - 250);
	prepare_single_line_clear(&clearing, 793u);
	assert(!solo_game_update(&clearing, 40));
	solo_game_toggle_pause(&clearing);
	assert(!solo_game_update(&clearing, 10000));
	assert(clearing.clear_elapsed_ms == 40);
	assert(solo_game_next_wake_ms(&clearing) == -1);
	solo_game_toggle_pause(&clearing);
	assert(solo_game_next_wake_ms(&clearing)
		== solo_clear_duration_ms(clearing.level) / 2 - 40);
	printf("PASS test_pause_freezes_active_and_clear_timers\n");
}

/**
 * @brief Exercises clear animation then level and meter update.
 *
 * Completing the tenth line must advance level while two cumulative cleared
 * lines are required for one crystal segment.
 */
static void	test_clear_animation_then_level_and_meter_update(void)
{
	solo_game_t	game;
	int				col;

	solo_game_init(&game, 321u);
	game.total_lines = 9;
	game.level = 1;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		board_set(&game.board, col, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_I});
		col++;
	}
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_CLEARING);
	assert(solo_game_row_is_clearing(&game, BOARD_HEIGHT - 1));
	solo_game_update(&game, solo_clear_duration_ms(game.level) - 1);
	assert(game.phase == SOLO_CLEARING);
	solo_game_update(&game, 1);
	assert(game.phase == SOLO_ACTIVE);
	assert(game.total_lines == 10 && game.level == 2);
	assert(game.crystal_charge == 0 && game.crystal_line_progress == 1);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		board_set(&game.board, col, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_I});
		col++;
	}
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_CLEARING);
	assert(solo_game_update(&game, solo_clear_duration_ms(game.level)));
	assert(game.total_lines == 11 && game.level == 2);
	assert(game.crystal_charge == 1 && game.crystal_line_progress == 0);
	printf("PASS test_clear_animation_then_level_and_meter_update\n");
}

/**
 * @brief Exercises rotation stress preserves valid state.
 *
 * One hundred thousand alternating rotations must preserve a valid empty-board
 *   piece without locking or corruption.
 */
static void	test_rotation_stress_preserves_valid_state(void)
{
	solo_game_t	game;
	int				index;

	solo_game_init(&game, 798u);
	game.active = piece_spawn(PIECE_T);
	index = 0;
	while (index < 100000)
	{
		if (index % 2 == 0)
			assert(solo_game_apply_action(&game, SOLO_ROTATE_CW));
		else
			assert(solo_game_apply_action(&game, SOLO_ROTATE_CCW));
		assert(piece_is_valid(&game.board, &game.active));
		assert(filled_cells(&game.board) == 0);
		index++;
	}
	printf("PASS test_rotation_stress_preserves_valid_state\n");
}

/**
 * @brief Exercises immediate danger entry and the delayed safe-stack exit.
 */
static void	test_danger_state_has_stable_exit_hysteresis(void)
{
	solo_game_t	game;

	solo_game_init(&game, 799u);
	board_set(&game.board, 0, SOLO_DANGER_ENTER_ROW,
		(t_cell){CELL_FILLED, PIECE_T});
	assert(solo_game_update_danger(&game, 0));
	assert(game.danger_active);
	board_set(&game.board, 0, SOLO_DANGER_ENTER_ROW,
		(t_cell){CELL_EMPTY, 0});
	board_set(&game.board, 0, SOLO_DANGER_EXIT_ROW - 1,
		(t_cell){CELL_FILLED, PIECE_T});
	assert(!solo_game_update_danger(&game, SOLO_DANGER_EXIT_HOLD_MS));
	assert(game.danger_active);
	board_set(&game.board, 0, SOLO_DANGER_EXIT_ROW - 1,
		(t_cell){CELL_EMPTY, 0});
	board_set(&game.board, 0, SOLO_DANGER_EXIT_ROW,
		(t_cell){CELL_FILLED, PIECE_T});
	assert(!solo_game_update_danger(&game,
			SOLO_DANGER_EXIT_HOLD_MS - 1));
	assert(game.danger_active);
	game.paused = true;
	assert(!solo_game_update_danger(&game, 1));
	assert(game.danger_active
		&& game.danger_safe_elapsed_ms == SOLO_DANGER_EXIT_HOLD_MS - 1);
	game.paused = false;
	assert(solo_game_update_danger(&game, 1));
	assert(!game.danger_active);
	printf("PASS test_danger_state_has_stable_exit_hysteresis\n");
}

/**
 * @brief Exercises one-shot action, clear, pause, and ability event delivery.
 */
static void	test_gameplay_events_are_one_shot(void)
{
	solo_game_t	game;
	uint32_t	events;

	solo_game_init(&game, 800u);
	assert(solo_game_take_events(&game) == 0);
	assert(solo_game_apply_action(&game, SOLO_HOLD));
	assert(solo_game_take_events(&game) == SOLO_EVENT_HOLD);
	assert(solo_game_take_events(&game) == 0);
	assert(solo_game_apply_action(&game, SOLO_ROTATE_CW));
	assert(solo_game_take_events(&game) == SOLO_EVENT_ROTATE);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	events = solo_game_take_events(&game);
	assert((events & SOLO_EVENT_HARD_DROP) != 0);
	assert((events & SOLO_EVENT_LOCK) != 0);
	prepare_single_line_clear(&game, 801u);
	(void)solo_game_take_events(&game);
	assert(solo_game_update(&game, solo_clear_duration_ms(game.level)));
	assert((solo_game_take_events(&game) & SOLO_EVENT_SINGLE) != 0);
	solo_game_toggle_pause(&game);
	assert((solo_game_take_events(&game) & SOLO_EVENT_PAUSE) != 0);
	solo_game_toggle_pause(&game);
	(void)solo_game_take_events(&game);
	game.crystal_charge = solo_ability_cost(SOLO_ABILITY_MIRURUN);
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_ACTIVATED);
	assert((solo_game_take_events(&game)
			& SOLO_EVENT_ABILITY_ACTIVATED) != 0);
	game.crystal_charge = 0;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_NO_CHARGE);
	assert((solo_game_take_events(&game)
			& SOLO_EVENT_ABILITY_REJECTED) != 0);
	printf("PASS test_gameplay_events_are_one_shot\n");
}

/**
 * @brief Exercises completed-game best comparison, one-shot cue, and fade.
 */
static void	test_personal_best_only_finishes_once_at_game_over(void)
{
	solo_game_t	game;
	unsigned	opacity;

	solo_game_init(&game, 802u);
	solo_game_set_personal_best(&game, UINT64_C(900));
	game.scoring.total = UINT64_C(1000);
	assert(!solo_game_finish_personal_best(&game));
	game.phase = SOLO_GAME_OVER;
	assert(solo_game_finish_personal_best(&game));
	assert(game.personal_best == UINT64_C(1000));
	assert((solo_game_take_events(&game)
			& SOLO_EVENT_PERSONAL_BEST) != 0);
	assert(!solo_game_finish_personal_best(&game));
	opacity = solo_game_personal_best_opacity(&game);
	assert(opacity == 255);
	assert(solo_game_next_wake_ms(&game) == SOLO_PERSONAL_BEST_FRAME_MS);
	assert(solo_game_update(&game, SOLO_PERSONAL_BEST_PULSE_MS));
	assert(solo_game_personal_best_opacity(&game) == 255);
	assert(solo_game_update(&game, SOLO_PERSONAL_BEST_FADE_MS));
	assert(solo_game_personal_best_opacity(&game) == 0);
	solo_game_init(&game, 803u);
	solo_game_set_personal_best(&game, UINT64_C(1000));
	game.phase = SOLO_GAME_OVER;
	game.scoring.total = UINT64_C(999);
	assert(!solo_game_finish_personal_best(&game));
	assert(game.personal_best_checked);
	assert(!game.new_personal_best);
	printf("PASS test_personal_best_only_finishes_once_at_game_over\n");
}

/**
 * @brief Counts non-empty cells in a board snapshot.
 *
 * Tests use this helper to distinguish active-piece movement from committed
 *   settled state.
 *
 * @param board Pointer to the board to inspect.
 * @return Number of filled or garbage cells.
 */
static int	filled_cells(const t_board *board)
{
	int	count;
	int	row;
	int	col;

	count = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (board_get(board, col, row).type != CELL_EMPTY)
				count++;
			col++;
		}
		row++;
	}
	return (count);
}

/**
 * @brief Places a game directly into a deterministic one-line clear phase.
 *
 * The helper fills the bottom row and hard-drops the active piece so timing
 *   tests share one consistent animation setup.
 *
 * @param game Pointer to the Solo state to prepare.
 * @param seed Deterministic seven-bag seed.
 */
static void	prepare_single_line_clear(solo_game_t *game, uint32_t seed)
{
	int	col;

	solo_game_init(game, seed);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		board_set(&game->board, col, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_I});
		col++;
	}
	assert(solo_game_apply_action(game, SOLO_HARD_DROP));
	assert(game->phase == SOLO_CLEARING);
}
