#include "tetrisbrain.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void	fill_row(t_board *b, int row, t_cell_type type, uint8_t color)
{
	int	c;

	for (c = 0; c < BOARD_WIDTH; c++)
		board_set(b, c, row, (t_cell){type, color});
}

static void	fill_board(t_board *b, t_cell_type type, uint8_t color)
{
	int	r;

	for (r = 0; r < BOARD_HEIGHT; r++)
		fill_row(b, r, type, color);
}

// ---- board_cut_top (Wolfman: cut off the top N rows of the stack) ----

void	test_cut_top_shifts_rows_up_and_clears_bottom(void)
{
	t_board	b;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 2, 3, (t_cell){CELL_FILLED, 5});  // survives the cut
	board_set(&b, 0, 19, (t_cell){CELL_FILLED, 9}); // bottom of the stack

	board_cut_top(&b, 3);

	// row 3 -> row 0 (top 3 rows discarded)
	assert(board_get(&b, 2, 0).type == CELL_FILLED);
	assert(board_get(&b, 2, 0).color == 5);
	// row 19 -> row 16
	assert(board_get(&b, 0, 16).type == CELL_FILLED);
	assert(board_get(&b, 0, 16).color == 9);

	// new bottom 3 rows are empty
	for (r = BOARD_HEIGHT - 3; r < BOARD_HEIGHT; r++)
		for (c = 0; c < BOARD_WIDTH; c++)
			assert(board_get(&b, c, r).type == CELL_EMPTY);

	printf("PASS test_cut_top_shifts_rows_up_and_clears_bottom\n");
}

void	test_cut_top_zero_is_noop(void)
{
	t_board	before;
	t_board	after;

	board_init(&before);
	board_set(&before, 4, 10, (t_cell){CELL_FILLED, 3});
	board_copy(&after, &before);

	board_cut_top(&after, 0);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_cut_top_zero_is_noop\n");
}

void	test_cut_top_clamped_to_board_height(void)
{
	t_board	b;
	t_board	empty;

	fill_board(&b, CELL_FILLED, 1);
	board_init(&empty);

	board_cut_top(&b, BOARD_HEIGHT + 5); // n > height must not read/write OOB
	assert(memcmp(&b, &empty, sizeof(t_board)) == 0);

	printf("PASS test_cut_top_clamped_to_board_height\n");
}

// ---- board_cut_bottom (Mirurun: smash down + remove the bottom N lines) ----

void	test_cut_bottom_shifts_rows_down_and_clears_top(void)
{
	t_board	b;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 3, 0, (t_cell){CELL_FILLED, 7});  // top of the stack
	board_set(&b, 4, 15, (t_cell){CELL_FILLED, 2}); // survives the smash

	board_cut_bottom(&b, 4);

	// row 0 -> row 4
	assert(board_get(&b, 3, 4).type == CELL_FILLED);
	assert(board_get(&b, 3, 4).color == 7);
	// row 15 -> row 19 (bottom 4 rows discarded)
	assert(board_get(&b, 4, 19).type == CELL_FILLED);
	assert(board_get(&b, 4, 19).color == 2);

	// new top 4 rows are empty
	for (r = 0; r < 4; r++)
		for (c = 0; c < BOARD_WIDTH; c++)
			assert(board_get(&b, c, r).type == CELL_EMPTY);

	printf("PASS test_cut_bottom_shifts_rows_down_and_clears_top\n");
}

void	test_cut_bottom_zero_is_noop(void)
{
	t_board	before;
	t_board	after;

	board_init(&before);
	board_set(&before, 1, 5, (t_cell){CELL_FILLED, 6});
	board_copy(&after, &before);

	board_cut_bottom(&after, 0);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_cut_bottom_zero_is_noop\n");
}

void	test_cut_bottom_clamped_to_board_height(void)
{
	t_board	b;
	t_board	empty;

	fill_board(&b, CELL_FILLED, 1);
	board_init(&empty);

	board_cut_bottom(&b, BOARD_HEIGHT + 5);
	assert(memcmp(&b, &empty, sizeof(t_board)) == 0);

	printf("PASS test_cut_bottom_clamped_to_board_height\n");
}

// ---- board_apply_gravity (Wolfman lvl4: suspended blocks fall) ----

void	test_apply_gravity_drops_floating_block_to_floor(void)
{
	t_board	b;

	board_init(&b);
	board_set(&b, 4, 10, (t_cell){CELL_FILLED, 5}); // floating, nothing below

	board_apply_gravity(&b);

	assert(board_get(&b, 4, 19).type == CELL_FILLED);
	assert(board_get(&b, 4, 19).color == 5);
	assert(board_get(&b, 4, 10).type == CELL_EMPTY);

	printf("PASS test_apply_gravity_drops_floating_block_to_floor\n");
}

void	test_apply_gravity_preserves_relative_order_within_column(void)
{
	t_board	b;

	board_init(&b);
	board_set(&b, 0, 5, (t_cell){CELL_FILLED, 1});  // higher block
	board_set(&b, 0, 10, (t_cell){CELL_FILLED, 2}); // lower block

	board_apply_gravity(&b);

	// both fall to the floor; the higher block stays above the lower one
	assert(board_get(&b, 0, 18).color == 1);
	assert(board_get(&b, 0, 19).color == 2);

	printf("PASS test_apply_gravity_preserves_relative_order_within_column\n");
}

void	test_apply_gravity_column_without_gaps_unchanged(void)
{
	t_board	before;
	t_board	after;
	int		r;

	board_init(&before);
	for (r = 15; r < BOARD_HEIGHT; r++)
		board_set(&before, 0, r, (t_cell){CELL_FILLED, 4}); // solid, no gaps
	board_copy(&after, &before);

	board_apply_gravity(&after);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_apply_gravity_column_without_gaps_unchanged\n");
}

// ---- board_invert (Halloween "Dark": swap empty <-> filled) ----

void	test_invert_empty_cell_becomes_garbage(void)
{
	t_board	b;

	board_init(&b);

	board_invert(&b);

	assert(board_get(&b, 0, 0).type == CELL_GARBAGE);
	assert(board_get(&b, 0, 0).color == 0);

	printf("PASS test_invert_empty_cell_becomes_garbage\n");
}

void	test_invert_filled_cell_becomes_empty(void)
{
	t_board	b;

	board_init(&b);
	board_set(&b, 5, 10, (t_cell){CELL_FILLED, 3});

	board_invert(&b);

	assert(board_get(&b, 5, 10).type == CELL_EMPTY);
	assert(board_get(&b, 5, 10).color == 0);

	printf("PASS test_invert_filled_cell_becomes_empty\n");
}

void	test_invert_garbage_cell_becomes_empty(void)
{
	t_board	b;

	board_init(&b);
	board_set(&b, 6, 11, (t_cell){CELL_GARBAGE, 0});

	board_invert(&b);

	assert(board_get(&b, 6, 11).type == CELL_EMPTY);

	printf("PASS test_invert_garbage_cell_becomes_empty\n");
}

// ---- board_fill_rows (Halloween "Burn": fill N rows so they clear) ----

void	test_fill_rows_overwrites_bottom_rows_with_hole(void)
{
	t_board	b;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 0, BOARD_HEIGHT - 4, (t_cell){CELL_FILLED, 9}); // above the burn

	board_fill_rows(&b, 3, 5); // burn bottom 3 rows, hole at col 5

	for (r = BOARD_HEIGHT - 3; r < BOARD_HEIGHT; r++)
	{
		for (c = 0; c < BOARD_WIDTH; c++)
		{
			if (c == 5)
				assert(board_get(&b, c, r).type == CELL_EMPTY);
			else
				assert(board_get(&b, c, r).type == CELL_GARBAGE);
		}
	}

	// row above the burn is untouched - no shift
	assert(board_get(&b, 0, BOARD_HEIGHT - 4).type == CELL_FILLED);
	assert(board_get(&b, 0, BOARD_HEIGHT - 4).color == 9);

	printf("PASS test_fill_rows_overwrites_bottom_rows_with_hole\n");
}

void	test_fill_rows_hole_out_of_range_makes_rows_clearable(void)
{
	t_board	b;

	board_init(&b);

	board_fill_rows(&b, 2, -1); // no hole -> rows come out fully solid

	assert(board_clear_lines(&b) == 2);

	printf("PASS test_fill_rows_hole_out_of_range_makes_rows_clearable\n");
}

void	test_fill_rows_zero_is_noop(void)
{
	t_board	before;
	t_board	after;

	board_init(&before);
	board_set(&before, 2, 18, (t_cell){CELL_FILLED, 1});
	board_copy(&after, &before);

	board_fill_rows(&after, 0, 5);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_fill_rows_zero_is_noop\n");
}

void	test_fill_rows_clamped_to_board_height(void)
{
	t_board	b;

	board_init(&b);

	board_fill_rows(&b, BOARD_HEIGHT + 5, 3); // must not write OOB

	assert(board_get(&b, 0, 0).type == CELL_GARBAGE);
	assert(board_get(&b, 3, 0).type == CELL_EMPTY);
	assert(board_get(&b, 0, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
	assert(board_get(&b, 3, BOARD_HEIGHT - 1).type == CELL_EMPTY);

	printf("PASS test_fill_rows_clamped_to_board_height\n");
}

// ---- board_clear_cells (Halloween "Bomb": clear a scattered set of cells) ----

void	test_clear_cells_clears_only_listed_cells(void)
{
	t_board	b;
	int		cols[] = {1, 2, 3};
	int		rows[] = {5, 5, 5};

	board_init(&b);
	fill_row(&b, 5, CELL_FILLED, 1);

	board_clear_cells(&b, cols, rows, 3);

	assert(board_get(&b, 1, 5).type == CELL_EMPTY);
	assert(board_get(&b, 2, 5).type == CELL_EMPTY);
	assert(board_get(&b, 3, 5).type == CELL_EMPTY);
	assert(board_get(&b, 0, 5).type == CELL_FILLED); // untouched
	assert(board_get(&b, 4, 5).type == CELL_FILLED); // untouched

	printf("PASS test_clear_cells_clears_only_listed_cells\n");
}

void	test_clear_cells_count_zero_is_noop(void)
{
	t_board	before;
	t_board	after;
	int		cols[] = {7};
	int		rows[] = {7};

	board_init(&before);
	board_set(&before, 7, 7, (t_cell){CELL_FILLED, 2});
	board_copy(&after, &before);

	board_clear_cells(&after, cols, rows, 0);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_clear_cells_count_zero_is_noop\n");
}

void	test_clear_cells_ignores_out_of_bounds_indices(void)
{
	t_board	before;
	t_board	after;
	int		cols[] = {-1, BOARD_WIDTH};
	int		rows[] = {0, BOARD_HEIGHT};

	board_init(&before);
	board_set(&before, 7, 7, (t_cell){CELL_FILLED, 2});
	board_copy(&after, &before);

	// a malformed ABILITY frame could carry out-of-range targets; must not
	// crash or corrupt neighbouring cells (board_set already bounds-checks)
	board_clear_cells(&after, cols, rows, 2);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_clear_cells_ignores_out_of_bounds_indices\n");
}

// ---- board_delete_columns (Princess: laser destroys a column range) ----

void	test_delete_columns_clears_inclusive_range(void)
{
	t_board	b;
	int		r;

	fill_board(&b, CELL_FILLED, 3);

	board_delete_columns(&b, 2, 4);

	for (r = 0; r < BOARD_HEIGHT; r++)
	{
		assert(board_get(&b, 1, r).type == CELL_FILLED); // untouched
		assert(board_get(&b, 2, r).type == CELL_EMPTY);
		assert(board_get(&b, 3, r).type == CELL_EMPTY);
		assert(board_get(&b, 4, r).type == CELL_EMPTY);
		assert(board_get(&b, 5, r).type == CELL_FILLED); // untouched
	}

	printf("PASS test_delete_columns_clears_inclusive_range\n");
}

void	test_delete_columns_single_column(void)
{
	t_board	b;
	int		r;

	fill_board(&b, CELL_FILLED, 3);

	board_delete_columns(&b, 0, 0); // single-column laser shot

	for (r = 0; r < BOARD_HEIGHT; r++)
	{
		assert(board_get(&b, 0, r).type == CELL_EMPTY);
		assert(board_get(&b, 1, r).type == CELL_FILLED);
	}

	printf("PASS test_delete_columns_single_column\n");
}

void	test_delete_columns_out_of_range_clamped(void)
{
	t_board	b;
	t_board	empty;

	fill_board(&b, CELL_FILLED, 3);
	board_init(&empty);

	board_delete_columns(&b, -5, BOARD_WIDTH + 5); // must not write OOB
	assert(memcmp(&b, &empty, sizeof(t_board)) == 0);

	printf("PASS test_delete_columns_out_of_range_clamped\n");
}

// ---- board_cascade_clear (Wolfman "Thwack": clears cascade via gravity) ----

void	test_cascade_clear_no_full_lines_is_noop(void)
{
	t_board	before;
	t_board	after;

	board_init(&before);
	board_set(&before, 4, 10, (t_cell){CELL_FILLED, 5}); // floating block
	board_copy(&after, &before);

	// nothing cleared -> no gravity either; the floating block must not move
	assert(board_cascade_clear(&after) == 0);
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);

	printf("PASS test_cascade_clear_no_full_lines_is_noop\n");
}

void	test_cascade_clear_single_clear_applies_gravity(void)
{
	t_board	b;

	board_init(&b);
	fill_row(&b, 19, CELL_FILLED, 1);
	board_set(&b, 4, 10, (t_cell){CELL_FILLED, 5}); // suspended above the clear

	assert(board_cascade_clear(&b) == 1);

	// the suspended block falls to the floor instead of shifting down one row
	assert(board_get(&b, 4, 19).type == CELL_FILLED);
	assert(board_get(&b, 4, 19).color == 5);
	assert(board_get(&b, 4, 11).type == CELL_EMPTY);

	printf("PASS test_cascade_clear_single_clear_applies_gravity\n");
}

void	test_cascade_clear_chains_falling_blocks_into_new_clears(void)
{
	t_board	b;
	int		c;

	board_init(&b);
	fill_row(&b, 19, CELL_FILLED, 1); // clears first
	for (c = 0; c < BOARD_WIDTH - 1; c++)
		board_set(&b, c, 18, (t_cell){CELL_FILLED, 2}); // missing only col 9
	board_set(&b, 9, 10, (t_cell){CELL_FILLED, 7});   // falls into the gap

	// row 19 clears, the col-9 block falls into the near-full row, which then
	// clears too - two lines total from one call
	assert(board_cascade_clear(&b) == 2);
	assert(board_is_empty(&b));

	printf("PASS test_cascade_clear_chains_falling_blocks_into_new_clears\n");
}

int	main(void)
{
	test_cut_top_shifts_rows_up_and_clears_bottom();
	test_cut_top_zero_is_noop();
	test_cut_top_clamped_to_board_height();

	test_cut_bottom_shifts_rows_down_and_clears_top();
	test_cut_bottom_zero_is_noop();
	test_cut_bottom_clamped_to_board_height();

	test_apply_gravity_drops_floating_block_to_floor();
	test_apply_gravity_preserves_relative_order_within_column();
	test_apply_gravity_column_without_gaps_unchanged();

	test_invert_empty_cell_becomes_garbage();
	test_invert_filled_cell_becomes_empty();
	test_invert_garbage_cell_becomes_empty();

	test_fill_rows_overwrites_bottom_rows_with_hole();
	test_fill_rows_hole_out_of_range_makes_rows_clearable();
	test_fill_rows_zero_is_noop();
	test_fill_rows_clamped_to_board_height();

	test_clear_cells_clears_only_listed_cells();
	test_clear_cells_count_zero_is_noop();
	test_clear_cells_ignores_out_of_bounds_indices();

	test_delete_columns_clears_inclusive_range();
	test_delete_columns_single_column();
	test_delete_columns_out_of_range_clamped();

	test_cascade_clear_no_full_lines_is_noop();
	test_cascade_clear_single_clear_applies_gravity();
	test_cascade_clear_chains_falling_blocks_into_new_clears();

	return (0);
}
