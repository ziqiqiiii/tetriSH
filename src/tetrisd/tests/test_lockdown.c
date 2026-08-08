/* ************************************************************************** */
/*                                                                            */
/*   test_lockdown.c - a landed piece is still the player's                   */
/*                                                                            */
/*   The server used to lock a piece the instant gravity could not move it    */
/*   down, so the only way to place one was to have it already in the right   */
/*   column when it touched the stack. Sliding a piece under an overhang -    */
/*   the thing the Guideline's Extended Placement lock down exists for - was  */
/*   impossible online while working offline, because tetrisu's local rules   */
/*   already had the delay.                                                   */
/*                                                                            */
/*   These drive t_game in-process. Placing a piece over a gap through MOVE   */
/*   and DROP alone would depend on what the bag deals; setting the board and */
/*   the piece directly asks about the delay and nothing else.                */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_a_landed_piece_does_not_lock_at_once(void);
static void	test_the_delay_runs_out(void);
static void	test_a_move_buys_the_delay_back(void);
static void	test_the_resets_run_out_too(void);
static void	test_a_piece_can_be_slid_into_a_gap(void);
static void	test_a_soft_drop_into_the_floor_no_longer_locks(void);
static void	test_a_hard_drop_still_locks_at_once(void);
static void	test_a_new_lowest_row_refills_the_budget(void);

static void	open_game(t_game *g, int gap_cols);
static void	settle(t_game *g);
static void	tick_ms(t_game *g, int total_ms);
static bool	has_locked(const t_game *g);

int	main(void)
{
	test_a_landed_piece_does_not_lock_at_once();
	test_the_delay_runs_out();
	test_a_move_buys_the_delay_back();
	test_the_resets_run_out_too();
	test_a_piece_can_be_slid_into_a_gap();
	test_a_soft_drop_into_the_floor_no_longer_locks();
	test_a_hard_drop_still_locks_at_once();
	test_a_new_lowest_row_refills_the_budget();
	return (0);
}

/*
** Touching down is not landing. For the length of the delay the piece is
** where the player left it and the board is untouched.
*/
static void	test_a_landed_piece_does_not_lock_at_once(void)
{
	t_game	g;

	open_game(&g, 0);
	settle(&g);
	tick_ms(&g, LOCKDOWN_DELAY_MS - TETRISD_DEFAULT_TICK_MS);
	assert(!has_locked(&g));
	assert(g.piece.type == PIECE_O);
	printf("PASS test_a_landed_piece_does_not_lock_at_once\n");
}

/*
** And it is a delay, not a reprieve: a player who does nothing loses the
** piece where it sits.
*/
static void	test_the_delay_runs_out(void)
{
	t_game	g;

	open_game(&g, 0);
	settle(&g);
	tick_ms(&g, LOCKDOWN_DELAY_MS + TETRISD_DEFAULT_TICK_MS);
	assert(has_locked(&g));
	printf("PASS test_the_delay_runs_out\n");
}

/*
** Every move refreshes the half-second, which is what makes the delay usable
** rather than merely present.
*/
static void	test_a_move_buys_the_delay_back(void)
{
	t_game	g;
	int		i;

	open_game(&g, 0);
	settle(&g);
	i = 0;
	while (i < 4)
	{
		tick_ms(&g, LOCKDOWN_DELAY_MS - TETRISD_DEFAULT_TICK_MS);
		assert(!has_locked(&g));
		assert(game_move(&g, -1));
		i++;
	}
	assert(!has_locked(&g));
	printf("PASS test_a_move_buys_the_delay_back\n");
}

/*
** Fifteen of them. The sixteenth buys nothing, or a player could hold a piece
** above the stack for the rest of the game.
*/
static void	test_the_resets_run_out_too(void)
{
	t_game	g;
	int		i;

	open_game(&g, 0);
	settle(&g);
	i = 0;
	while (i < LOCKDOWN_MAX_RESETS)
	{
		assert(game_move(&g, 1 - (i % 2) * 2));
		i++;
	}
	assert(g.lockdown.resets == LOCKDOWN_MAX_RESETS);
	tick_ms(&g, LOCKDOWN_DELAY_MS - TETRISD_DEFAULT_TICK_MS * 2);
	assert(!has_locked(&g));
	assert(game_move(&g, -1));
	tick_ms(&g, TETRISD_DEFAULT_TICK_MS * 2);
	assert(has_locked(&g));
	printf("PASS test_the_resets_run_out_too\n");
}

/*
** The whole point. The bottom row is filled except for its two leftmost
** cells; the piece comes to rest on the ledge beside them, is walked over the
** gap while the delay runs, and drops in. Locking on contact would have left
** it stranded on top of the ledge.
*/
static void	test_a_piece_can_be_slid_into_a_gap(void)
{
	t_game	g;
	int		i;

	open_game(&g, 2);
	settle(&g);
	assert(g.piece.row == BOARD_HEIGHT - 3);
	i = 0;
	while (i < 4)
	{
		tick_ms(&g, LOCKDOWN_DELAY_MS - TETRISD_DEFAULT_TICK_MS);
		assert(game_move(&g, -1));
		i++;
	}
	assert(g.piece.col == 0);
	tick_ms(&g, gravity_interval_ms(g.level) + LOCKDOWN_DELAY_MS
		+ TETRISD_DEFAULT_TICK_MS);
	assert(board_get(&g.board, 0, BOARD_HEIGHT - 1).type != CELL_EMPTY);
	assert(board_get(&g.board, 1, BOARD_HEIGHT - 1).type != CELL_EMPTY);
	printf("PASS test_a_piece_can_be_slid_into_a_gap\n");
}

/*
** Soft drop is gravity in a hurry and nothing else. It used to lock a piece
** that was already resting, which took the delay away from exactly the
** players who reach the floor fastest.
*/
static void	test_a_soft_drop_into_the_floor_no_longer_locks(void)
{
	t_game	g;

	open_game(&g, 0);
	settle(&g);
	assert(!game_drop(&g, false));
	assert(!has_locked(&g));
	assert(game_move(&g, -1));
	printf("PASS test_a_soft_drop_into_the_floor_no_longer_locks\n");
}

/*
** Hard drop is the input that means "and I am done with it", so it is the one
** thing the delay does not apply to.
*/
static void	test_a_hard_drop_still_locks_at_once(void)
{
	t_game	g;

	open_game(&g, 0);
	assert(game_drop(&g, true));
	assert(has_locked(&g));
	printf("PASS test_a_hard_drop_still_locks_at_once\n");
}

/*
** Falling past every row it has reached before is what earns a piece a fresh
** budget - progress downward, not shuffling sideways.
*/
static void	test_a_new_lowest_row_refills_the_budget(void)
{
	t_game	g;
	int		i;

	open_game(&g, 2);
	settle(&g);
	i = 0;
	while (i < LOCKDOWN_MAX_RESETS)
	{
		assert(game_move(&g, 1 - (i % 2) * 2));
		i++;
	}
	assert(g.lockdown.resets == LOCKDOWN_MAX_RESETS);
	while (g.piece.col > 0)
		assert(game_move(&g, -1));
	assert(game_drop(&g, false));
	assert(g.lockdown.resets == 0);
	printf("PASS test_a_new_lowest_row_refills_the_budget\n");
}

/**
 * @brief Starts a game with a known piece over a floor with a known gap.
 *
 * The bag deals whatever it deals, so the piece is replaced with an O: it is
 * two cells wide in every rotation, which makes the column arithmetic in
 * these tests the same before and after a rotation.
 *
 * @param g Game to open.
 * @param gap_cols How many leftmost cells of the bottom row to leave empty.
 */
static void	open_game(t_game *g, int gap_cols)
{
	t_cell	cell;
	int		col;

	game_start(g, 1, 20260809u);
	cell.type = CELL_FILLED;
	cell.color = 1;
	col = gap_cols;
	while (col < BOARD_WIDTH)
	{
		board_set(&g->board, col, BOARD_HEIGHT - 1, cell);
		col++;
	}
	g->piece = piece_spawn(PIECE_O);
	lockdown_init(&g->lockdown, &g->piece);
}

/**
 * @brief Soft drops the piece until it is resting, without locking it.
 *
 * Soft drop is used rather than gravity so no time passes: the piece arrives
 * with its whole delay ahead of it, which is what each test then spends.
 *
 * @param g Game whose piece is brought to rest.
 */
static void	settle(t_game *g)
{
	while (game_drop(g, false))
		;
	assert(lockdown_grounded(&g->board, &g->piece));
	assert(g->lockdown.elapsed_ms == 0);
}

/**
 * @brief Advances the game the way the reactor does, one tick at a time.
 *
 * The lock delay is charged per tick, so a test that handed over half a
 * second in one call would be measuring something the server never does.
 *
 * @param g Game to advance.
 * @param total_ms Milliseconds to hand over in all.
 */
static void	tick_ms(t_game *g, int total_ms)
{
	int	spent;

	spent = 0;
	while (spent < total_ms)
	{
		game_gravity(g, TETRISD_DEFAULT_TICK_MS);
		spent += TETRISD_DEFAULT_TICK_MS;
	}
}

/**
 * @brief Reports whether the piece under test has been stamped into the board.
 *
 * The row above the floor is empty in every one of these set-ups until
 * something locks there, so a filled cell in it is the lock and nothing else.
 *
 * @param g Game to inspect.
 * @return true when a piece has locked above the floor.
 */
static bool	has_locked(const t_game *g)
{
	int	col;

	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (board_get(&g->board, col, BOARD_HEIGHT - 2).type != CELL_EMPTY)
			return (true);
		col++;
	}
	return (false);
}
