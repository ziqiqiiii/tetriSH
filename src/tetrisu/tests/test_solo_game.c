#include "tetrisu.h"
#include <assert.h>
#include <stdio.h>

static int	filled_cells(const t_board *board)
{
	int	count;

	count = 0;
	for (int row = 0; row < BOARD_HEIGHT; row++)
		for (int col = 0; col < BOARD_WIDTH; col++)
			if (board_get(board, col, row).type != CELL_EMPTY)
				count++;
	return (count);
}

static void	prepare_single_line_clear(solo_game_t *game, uint32_t seed)
{
	solo_game_init(game, seed);
	for (int col = 0; col < BOARD_WIDTH; col++)
		board_set(&game->board, col, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_I});
	assert(solo_game_apply_action(game, SOLO_HARD_DROP));
	assert(game->phase == SOLO_CLEARING);
}

void	test_init_has_active_and_three_previews(void)
{
	solo_game_t	game;
	bool			seen[BRAIN_BAG_SIZE] = {false};

	solo_game_init(&game, 123u);
	assert(game.level == 1 && game.total_lines == 0);
	assert(game.scoring.total == 0 && game.phase == SOLO_ACTIVE);
	assert(piece_is_valid(&game.board, &game.active));
	seen[game.active.type] = true;
	for (int index = 0; index < SOLO_NEXT_COUNT; index++)
	{
		assert(!seen[game.next[index]]);
		seen[game.next[index]] = true;
	}
	printf("PASS test_init_has_active_and_three_previews\n");
}

void	test_ghost_and_hard_drop(void)
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

void	test_ghost_stops_above_stack(void)
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

void	test_locking_piece_in_top_row_reveals_before_game_over(void)
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

void	test_locking_piece_below_top_row_continues(void)
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

void	test_blocked_spawn_with_empty_top_row_reveals_before_game_over(void)
{
	solo_game_t	game;

	solo_game_init(&game, 460u);
	game.active = piece_spawn(PIECE_O);
	game.active.col = 0;
	game.active.row = BOARD_HEIGHT - 2;
	game.next[0] = PIECE_O;
	board_set(&game.board, 4, 1, (t_cell){CELL_GARBAGE, 0});
	assert(piece_drop_distance(&game.board, &game.active) == 0);
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	for (int col = 0; col < BOARD_WIDTH; col++)
		assert(board_get(&game.board, col, 0).type == CELL_EMPTY);
	assert(filled_cells(&game.board) == 5);
	assert(!piece_is_valid(&game.board, &game.active));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(game.top_out_elapsed_ms == 0);
	assert(!solo_game_update(&game, SOLO_TOP_OUT_REVEAL_MS - 1));
	assert(game.phase == SOLO_TOP_OUT_REVEAL);
	assert(filled_cells(&game.board) == 5);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_GAME_OVER);
	printf("PASS test_blocked_spawn_with_empty_top_row_reveals_before_game_over\n");
}

void	test_catchup_cannot_skip_new_top_out_reveal(void)
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

void	test_lock_delay_waits_half_second(void)
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

void	test_landing_starts_a_fresh_lock_delay(void)
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

void	test_next_wake_tracks_gravity_and_lock_deadlines(void)
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

void	test_zero_elapsed_consumes_due_deadlines(void)
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
	game.clear_elapsed_ms = SOLO_CLEAR_ANIMATION_MS;
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

void	test_grounded_piece_sleeps_until_lock_deadline(void)
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

void	test_clear_animation_wakes_at_visual_boundaries(void)
{
	solo_game_t	game;
	int				halfway;

	prepare_single_line_clear(&game, 322u);
	halfway = SOLO_CLEAR_ANIMATION_MS / 2;
	assert(solo_game_next_wake_ms(&game) == halfway);
	assert(!solo_game_update(&game, halfway - 1));
	assert(game.clear_elapsed_ms == halfway - 1);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_CLEARING);
	assert(game.clear_elapsed_ms == halfway);
	assert(solo_game_next_wake_ms(&game) == halfway);
	assert(!solo_game_update(&game, halfway - 1));
	assert(game.clear_elapsed_ms == SOLO_CLEAR_ANIMATION_MS - 1);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.phase == SOLO_ACTIVE);
	assert(game.total_lines == 1);
	printf("PASS test_clear_animation_wakes_at_visual_boundaries\n");
}

void	test_pause_freezes_active_and_clear_timers(void)
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
		== SOLO_CLEAR_ANIMATION_MS / 2 - 40);
	printf("PASS test_pause_freezes_active_and_clear_timers\n");
}

void	test_clear_animation_then_level_and_meter_update(void)
{
	solo_game_t	game;

	solo_game_init(&game, 321u);
	game.total_lines = 9;
	game.level = 1;
	for (int col = 0; col < BOARD_WIDTH; col++)
		board_set(&game.board, col, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_I});
	assert(solo_game_apply_action(&game, SOLO_HARD_DROP));
	assert(game.phase == SOLO_CLEARING);
	assert(solo_game_row_is_clearing(&game, BOARD_HEIGHT - 1));
	solo_game_update(&game, SOLO_CLEAR_ANIMATION_MS - 1);
	assert(game.phase == SOLO_CLEARING);
	solo_game_update(&game, 1);
	assert(game.phase == SOLO_ACTIVE);
	assert(game.total_lines == 10 && game.level == 2);
	assert(game.crystal_charge == 1);
	printf("PASS test_clear_animation_then_level_and_meter_update\n");
}

int	main(void)
{
	test_init_has_active_and_three_previews();
	test_ghost_and_hard_drop();
	test_ghost_stops_above_stack();
	test_locking_piece_in_top_row_reveals_before_game_over();
	test_locking_piece_below_top_row_continues();
	test_blocked_spawn_with_empty_top_row_reveals_before_game_over();
	test_catchup_cannot_skip_new_top_out_reveal();
	test_lock_delay_waits_half_second();
	test_landing_starts_a_fresh_lock_delay();
	test_next_wake_tracks_gravity_and_lock_deadlines();
	test_zero_elapsed_consumes_due_deadlines();
	test_grounded_piece_sleeps_until_lock_deadline();
	test_clear_animation_wakes_at_visual_boundaries();
	test_pause_freezes_active_and_clear_timers();
	test_clear_animation_then_level_and_meter_update();
	return (0);
}
