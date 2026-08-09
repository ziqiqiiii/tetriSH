#include "tetrisbrain.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static t_piece	landed_piece(t_board *b);

void	test_a_piece_in_the_air_is_never_on_the_clock(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;

	board_init(&b);
	p = piece_spawn(PIECE_O);
	lockdown_init(&lock, &p);
	assert(!lockdown_grounded(&b, &p));
	assert(!lockdown_tick(&lock, false, LOCKDOWN_DELAY_MS * 4));
	assert(lock.elapsed_ms == 0);
	printf("PASS test_a_piece_in_the_air_is_never_on_the_clock\n");
}

void	test_a_landed_piece_locks_after_the_delay(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;

	board_init(&b);
	p = landed_piece(&b);
	lockdown_init(&lock, &p);
	assert(lockdown_grounded(&b, &p));
	assert(!lockdown_tick(&lock, true, LOCKDOWN_DELAY_MS - 1));
	assert(lockdown_tick(&lock, true, 1));
	printf("PASS test_a_landed_piece_locks_after_the_delay\n");
}

void	test_a_move_buys_the_delay_back(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;
	int			i;

	board_init(&b);
	p = landed_piece(&b);
	lockdown_init(&lock, &p);
	i = 0;
	while (i < LOCKDOWN_MAX_RESETS)
	{
		assert(!lockdown_tick(&lock, true, LOCKDOWN_DELAY_MS - 1));
		lockdown_on_shift(&lock, true);
		assert(lock.elapsed_ms == 0);
		i++;
	}
	printf("PASS test_a_move_buys_the_delay_back\n");
}

void	test_the_sixteenth_move_buys_nothing(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;
	int			i;

	board_init(&b);
	p = landed_piece(&b);
	lockdown_init(&lock, &p);
	i = 0;
	while (i < LOCKDOWN_MAX_RESETS + 4)
	{
		lockdown_on_shift(&lock, true);
		i++;
	}
	assert(lock.resets == LOCKDOWN_MAX_RESETS);
	lockdown_tick(&lock, true, LOCKDOWN_DELAY_MS - 1);
	lockdown_on_shift(&lock, true);
	assert(lock.elapsed_ms == LOCKDOWN_DELAY_MS - 1);
	assert(lockdown_tick(&lock, true, 1));
	printf("PASS test_the_sixteenth_move_buys_nothing\n");
}

void	test_a_move_in_the_air_costs_nothing(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;

	board_init(&b);
	p = piece_spawn(PIECE_T);
	lockdown_init(&lock, &p);
	lockdown_on_shift(&lock, false);
	assert(lock.resets == 0);
	printf("PASS test_a_move_in_the_air_costs_nothing\n");
}

void	test_falling_to_a_new_row_refills_the_budget(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;
	int			i;

	board_init(&b);
	p = piece_spawn(PIECE_T);
	lockdown_init(&lock, &p);
	i = 0;
	while (i < LOCKDOWN_MAX_RESETS)
	{
		lockdown_on_shift(&lock, true);
		i++;
	}
	assert(lock.resets == LOCKDOWN_MAX_RESETS);
	assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
	lockdown_on_fall(&lock, &p);
	assert(lock.resets == 0);
	assert(lock.lowest_row == p.row);
	printf("PASS test_falling_to_a_new_row_refills_the_budget\n");
}

void	test_a_row_already_reached_refills_nothing(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;

	board_init(&b);
	p = piece_spawn(PIECE_T);
	assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
	lockdown_init(&lock, &p);
	lockdown_on_shift(&lock, true);
	assert(piece_move(&b, &p, 0, -1) == BRAIN_OK);
	lockdown_on_fall(&lock, &p);
	assert(lock.resets == 1);
	printf("PASS test_a_row_already_reached_refills_nothing\n");
}

void	test_leaving_the_ground_restarts_the_half_second(void)
{
	t_board		b;
	t_piece		p;
	t_lockdown	lock;

	board_init(&b);
	p = landed_piece(&b);
	lockdown_init(&lock, &p);
	assert(!lockdown_tick(&lock, true, LOCKDOWN_DELAY_MS - 1));
	assert(!lockdown_tick(&lock, false, 1));
	assert(!lockdown_tick(&lock, true, LOCKDOWN_DELAY_MS - 1));
	assert(lockdown_tick(&lock, true, 1));
	printf("PASS test_leaving_the_ground_restarts_the_half_second\n");
}

/**
 * @brief Hard-drops a fresh O piece so the caller has one resting on the floor.
 *
 * @param b Board to drop through.
 * @return The landed piece.
 */
static t_piece	landed_piece(t_board *b)
{
	t_piece	p;

	p = piece_spawn(PIECE_O);
	piece_hard_drop(b, &p);
	return (p);
}

int	main(void)
{
	test_a_piece_in_the_air_is_never_on_the_clock();
	test_a_landed_piece_locks_after_the_delay();
	test_a_move_buys_the_delay_back();
	test_the_sixteenth_move_buys_nothing();
	test_a_move_in_the_air_costs_nothing();
	test_falling_to_a_new_row_refills_the_budget();
	test_a_row_already_reached_refills_nothing();
	test_leaving_the_ground_restarts_the_half_second();
	return (0);
}
