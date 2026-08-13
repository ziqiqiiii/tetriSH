// tests/test_garbage.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

// ---- garbage_lines_from_clear ----

// Single-line clears don't trigger Battle Royale (N-1 = 0 for N=1).
void test_single_clear_sends_no_garbage(void)
{
	assert(garbage_lines_from_clear(1) == 0);
	printf("PASS test_single_clear_sends_no_garbage\n");
}

// The canonical N-1 values for each multi-line clear.
void test_table_values_match_spec(void)
{
	assert(garbage_lines_from_clear(2) == 1); // double
	assert(garbage_lines_from_clear(3) == 2); // triple
	assert(garbage_lines_from_clear(4) == 3); // tetris
	printf("PASS test_table_values_match_spec\n");
}

// No garbage for zero or negative inputs.
void test_zero_and_negative_send_no_garbage(void)
{
	assert(garbage_lines_from_clear(0) == 0);
	assert(garbage_lines_from_clear(-1) == 0);
	assert(garbage_lines_from_clear(-100) == 0);
	printf("PASS test_zero_and_negative_send_no_garbage\n");
}

// Inputs above 4 clamp to the Tetris value (3 garbage lines); the board can
// only clear 4 lines per tick, but a corrupted or extended caller should not
// index past the table.
void test_over_four_lines_clamps_to_tetris(void)
{
	assert(garbage_lines_from_clear(5) == 3);
	assert(garbage_lines_from_clear(20) == 3);
	printf("PASS test_over_four_lines_clamps_to_tetris\n");
}

// Verify the garbage amount is always strictly less than lines_cleared for
// valid inputs 2-4; the attacker keeps 1 row as a reward.
void test_garbage_always_less_than_lines_cleared(void)
{
	for (int n = 2; n <= 4; n++)
		assert(garbage_lines_from_clear(n) < n);
	printf("PASS test_garbage_always_less_than_lines_cleared\n");
}

// ---- t_target_mode enum values ----
// Sanity-check that the enum constants have the expected integer values so
// the wire representation in HTTTP bodies remains stable.

void test_target_mode_enum_values(void)
{
	assert(TARGET_RANDOM    == 0);
	assert(TARGET_ATTACKERS == 1);
	assert(TARGET_KO        == 2);
	assert(TARGET_BADGES == 3);
	printf("PASS test_target_mode_enum_values\n");
}

// ---- integration: garbage_lines_from_clear feeds board_inject_garbage ----
// Verify the full pipeline: clear N lines -> compute garbage -> inject into
// a receiver board. This mirrors what tetrisd's ticker does after a lock.

void test_double_clear_injects_one_garbage_row(void)
{
	t_board receiver;
	board_init(&receiver);

	int lines_to_send = garbage_lines_from_clear(2); // expect 1
	assert(lines_to_send == 1);

	board_inject_garbage(&receiver, lines_to_send, 4); // hole at col 4

	// bottom row: garbage everywhere except the hole column
	for (int col = 0; col < BOARD_WIDTH; col++)
	{
		if (col == 4)
			assert(board_get(&receiver, col, BOARD_HEIGHT - 1).type == CELL_EMPTY);
		else
			assert(board_get(&receiver, col, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
	}
	// row above the garbage is still empty (only 1 garbage row injected)
	for (int col = 0; col < BOARD_WIDTH; col++)
		assert(board_get(&receiver, col, BOARD_HEIGHT - 2).type == CELL_EMPTY);

	printf("PASS test_double_clear_injects_one_garbage_row\n");
}

void test_tetris_clear_injects_three_garbage_rows(void)
{
	t_board receiver;
	board_init(&receiver);

	int lines_to_send = garbage_lines_from_clear(4); // expect 3
	assert(lines_to_send == 3);

	board_inject_garbage(&receiver, lines_to_send, 0); // hole at col 0

	// bottom 3 rows should be garbage (except hole column)
	for (int row = BOARD_HEIGHT - 3; row < BOARD_HEIGHT; row++)
	{
		for (int col = 0; col < BOARD_WIDTH; col++)
		{
			if (col == 0)
				assert(board_get(&receiver, col, row).type == CELL_EMPTY);
			else
				assert(board_get(&receiver, col, row).type == CELL_GARBAGE);
		}
	}
	// row above the garbage block is still empty
	assert(board_get(&receiver, 1, BOARD_HEIGHT - 4).type == CELL_EMPTY);

	printf("PASS test_tetris_clear_injects_three_garbage_rows\n");
}

// A single-line clear produces 0 garbage, so inject is a no-op: the
// receiver's board must be byte-identical before and after.
void test_single_clear_does_not_modify_receiver(void)
{
	t_board receiver, before;
	board_init(&receiver);
	board_set(&receiver, 3, 15, (t_cell){CELL_FILLED, 5}); // some existing content
	board_copy(&before, &receiver);

	int lines_to_send = garbage_lines_from_clear(1); // 0
	assert(lines_to_send == 0);
	board_inject_garbage(&receiver, lines_to_send, 2);

	for (int r = 0; r < BOARD_HEIGHT; r++)
		for (int c = 0; c < BOARD_WIDTH; c++)
		{
			assert(board_get(&receiver, c, r).type  == board_get(&before, c, r).type);
			assert(board_get(&receiver, c, r).color == board_get(&before, c, r).color);
		}

	printf("PASS test_single_clear_does_not_modify_receiver\n");
}

int main(void)
{
	test_single_clear_sends_no_garbage();
	test_table_values_match_spec();
	test_zero_and_negative_send_no_garbage();
	test_over_four_lines_clamps_to_tetris();
	test_garbage_always_less_than_lines_cleared();

	test_target_mode_enum_values();

	test_double_clear_injects_one_garbage_row();
	test_tetris_clear_injects_three_garbage_rows();
	test_single_clear_does_not_modify_receiver();

	return 0;
}
