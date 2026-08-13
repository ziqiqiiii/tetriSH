#include "tetrisbrain.h"

#include <assert.h>
#include <stdio.h>

void	test_init_empty(void)
{
	t_board	b;
	int		r;
	int		c;

	board_init(&b);
	for (r = 0; r < BOARD_HEIGHT; r++)
		for (c = 0; c < BOARD_WIDTH; c++)
			assert(board_get(&b, c, r).type == CELL_EMPTY);
	printf("PASS test_init_empty\n");
}

void	test_out_of_bounds_is_solid(void)
{
	t_board	b;

	board_init(&b);
	assert(board_get(&b, -1, 0).type == CELL_FILLED);
	assert(board_get(&b, BOARD_WIDTH, 0).type == CELL_FILLED);
	assert(board_get(&b, 0, -1).type == CELL_FILLED);
	assert(board_get(&b, 0, BOARD_HEIGHT).type == CELL_FILLED);
	printf("PASS test_out_of_bounds_is_solid\n");
}

void	test_inject_garbage_bottom_rows(void)
{
	t_board	b;
	int		col;

	board_init(&b);
	board_inject_garbage(&b, 2, 3);
	for (col = 0; col < BOARD_WIDTH; col++)
	{
		if (col == 3)
		{
			assert(board_get(&b, col, BOARD_HEIGHT - 1).type == CELL_EMPTY);
			assert(board_get(&b, col, BOARD_HEIGHT - 2).type == CELL_EMPTY);
		}
		else
		{
			assert(board_get(&b, col, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
			assert(board_get(&b, col, BOARD_HEIGHT - 2).type == CELL_GARBAGE);
		}
	}
	printf("PASS test_inject_garbage_bottom_rows\n");
}

void	test_inject_garbage_shifts_existing_blocks(void)
{
	t_board	b;

	board_init(&b);
	board_set(&b, 0, BOARD_HEIGHT - 1, (t_cell){CELL_FILLED, 1});
	board_inject_garbage(&b, 1, 5);
	assert(board_get(&b, 0, BOARD_HEIGHT - 2).type == CELL_FILLED);
	assert(board_get(&b, 0, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
	printf("PASS test_inject_garbage_shifts_existing_blocks\n");
}

void	test_inject_garbage_hole_is_empty(void)
{
	t_board	b;
	int		i;

	board_init(&b);
	board_inject_garbage(&b, 3, 7);
	for (i = 1; i <= 3; i++)
		assert(board_get(&b, 7, BOARD_HEIGHT - i).type == CELL_EMPTY);
	printf("PASS test_inject_garbage_hole_is_empty\n");
}

void	test_inject_garbage_zero_lines_is_noop(void)
{
	t_board	b;
	t_board	before;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 4, 5, (t_cell){CELL_FILLED, 2});
	board_copy(&before, &b);
	board_inject_garbage(&b, 0, 3);
	for (r = 0; r < BOARD_HEIGHT; r++)
	{
		for (c = 0; c < BOARD_WIDTH; c++)
		{
			assert(board_get(&b, c, r).type == board_get(&before, c, r).type);
			assert(board_get(&b, c, r).color
				== board_get(&before, c, r).color);
		}
	}
	printf("PASS test_inject_garbage_zero_lines_is_noop\n");
}

void	test_inject_garbage_negative_lines_is_noop(void)
{
	t_board	b;
	t_board	before;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 4, 5, (t_cell){CELL_FILLED, 2});
	board_copy(&before, &b);
	board_inject_garbage(&b, -3, 3);
	for (r = 0; r < BOARD_HEIGHT; r++)
	{
		for (c = 0; c < BOARD_WIDTH; c++)
		{
			assert(board_get(&b, c, r).type == board_get(&before, c, r).type);
			assert(board_get(&b, c, r).color
				== board_get(&before, c, r).color);
		}
	}
	printf("PASS test_inject_garbage_negative_lines_is_noop\n");
}

void	test_inject_garbage_more_than_height_caps(void)
{
	t_board	b;
	int		r;
	int		c;

	board_init(&b);
	board_set(&b, 4, 5, (t_cell){CELL_FILLED, 2});
	board_inject_garbage(&b, BOARD_HEIGHT + 5, 3);
	for (r = 0; r < BOARD_HEIGHT; r++)
		for (c = 0; c < BOARD_WIDTH; c++)
			if (c == 3)
				assert(board_get(&b, c, r).type == CELL_EMPTY);
			else
				assert(board_get(&b, c, r).type == CELL_GARBAGE);
	printf("PASS test_inject_garbage_more_than_height_caps\n");
}

int	main(void)
{
	test_init_empty();
	test_out_of_bounds_is_solid();
	test_inject_garbage_bottom_rows();
	test_inject_garbage_shifts_existing_blocks();
	test_inject_garbage_hole_is_empty();
	test_inject_garbage_zero_lines_is_noop();
	test_inject_garbage_negative_lines_is_noop();
	test_inject_garbage_more_than_height_caps();
	return (0);
}
