/* ************************************************************************** */
/*                                                                            */
/*   test_clearing.c - a line clear is a phase, not an instant                */
/*                                                                            */
/*   lock_piece used to stamp, clear, score and spawn in one call, so no      */
/*   client could ever see the completed rows: by the time a snapshot went    */
/*   out they were gone and the next piece was falling. There was nothing to  */
/*   animate, which is why the animation only ever worked offline             */
/*   (docs/bugs/the_line_clear_never_reached_the_client.md).                  */
/*                                                                            */
/*   tetrisd now holds them for clear_duration_ms. That is a game rule and    */
/*   this is what it has to mean: the rows are still there, nothing is        */
/*   scored yet, no piece has spawned, and the player cannot act.             */
/*                                                                            */
/*   These drive t_game in-process rather than over the wire. Filling ten     */
/*   columns through MOVE and DROP would take dozens of pieces and depend on  */
/*   what the bag deals; setting the board directly asks the question the     */
/*   rule is about and nothing else.                                          */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_a_lock_that_clears_holds_the_rows(void);
static void	test_nothing_is_scored_until_the_clear_finishes(void);
static void	test_input_is_refused_while_the_rows_are_held(void);
static void	test_the_clear_finishes_and_deals_the_next_piece(void);
static void	test_a_lock_that_clears_nothing_does_not_hold(void);
static void	test_a_pause_stops_the_clear_where_it_is(void);
static void	test_the_snapshot_carries_the_held_rows(void);
static void	test_the_held_rows_survive_the_wire(void);
static void	test_clear_overshoot_advances_the_next_piece(void);

static void	fill_bottom_row(t_game *g);
static void	drop_to_lock(t_game *g);
static void	arm_a_clear(t_game *g);

int	main(void)
{
	test_a_lock_that_clears_holds_the_rows();
	test_nothing_is_scored_until_the_clear_finishes();
	test_input_is_refused_while_the_rows_are_held();
	test_the_clear_finishes_and_deals_the_next_piece();
	test_a_lock_that_clears_nothing_does_not_hold();
	test_a_pause_stops_the_clear_where_it_is();
	test_the_snapshot_carries_the_held_rows();
	test_the_held_rows_survive_the_wire();
	test_clear_overshoot_advances_the_next_piece();
	return (0);
}

/*
** The row the piece completed is named, and it is still filled in the board
** the server would send. Both halves matter: a count with no rows gives the
** client nothing to flash, and rows already emptied give it nothing to flash
** them on.
*/
static void	test_a_lock_that_clears_holds_the_rows(void)
{
	t_game	g;
	int		col;

	arm_a_clear(&g);
	assert(g.clearing_count == 1);
	assert(g.clearing_rows[0] == BOARD_HEIGHT - 1);
	assert(g.clearing_ms == 0);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		assert(board_get(&g.board, col, BOARD_HEIGHT - 1).type != CELL_EMPTY);
		col++;
	}
	printf("PASS test_a_lock_that_clears_holds_the_rows\n");
}

/*
** Scoring at lock and clearing later would let the client show a line count
** that had gone up for rows still on the board.
*/
static void	test_nothing_is_scored_until_the_clear_finishes(void)
{
	t_game		g;
	uint64_t	held;

	arm_a_clear(&g);
	held = g.score.total;
	assert(g.lines == 0);
	assert(g.charge.charges == 0);
	assert(game_gravity(&g, clear_duration_ms(g.level) - 1));
	assert(g.lines == 0);
	assert(g.score.total == held);
	assert(game_gravity(&g, 1));
	assert(g.lines == 1);
	printf("PASS test_nothing_is_scored_until_the_clear_finishes\n");
}

/*
** There is no piece to move: it locked, and the next one has not been dealt.
** Accepting an input here would drive whatever happened to be in g->piece.
*/
static void	test_input_is_refused_while_the_rows_are_held(void)
{
	t_game	g;

	arm_a_clear(&g);
	assert(!game_move(&g, -1));
	assert(!game_move(&g, 1));
	assert(!game_rotate(&g, 1));
	assert(!game_drop(&g, false));
	assert(!game_drop(&g, true));
	assert(!game_hold(&g));
	assert(g.clearing_count == 1);
	printf("PASS test_input_is_refused_while_the_rows_are_held\n");
}

/*
** When the duration is up the rows go, the score lands, the charge is banked
** and a piece is dealt - all the things the lock used to do immediately.
*/
static void	test_the_clear_finishes_and_deals_the_next_piece(void)
{
	t_game	g;
	int		col;
	int		filled;

	arm_a_clear(&g);
	assert(game_gravity(&g, clear_duration_ms(g.level)));
	assert(g.clearing_count == 0);
	assert(g.clearing_ms == 0);
	assert(g.lines == 1);
	assert(g.score.total > 0);
	assert(g.charge.line_remainder == 1 || g.charge.charges == 1);
	assert(g.last_clear == BODY_CLEAR_SINGLE);
	assert(!g.hold_used);
	col = 0;
	filled = 0;
	while (col < BOARD_WIDTH)
	{
		if (board_get(&g.board, col, BOARD_HEIGHT - 1).type != CELL_EMPTY)
			filled++;
		col++;
	}
	assert(filled < BOARD_WIDTH);
	assert(game_move(&g, -1) || game_move(&g, 1));
	printf("PASS test_the_clear_finishes_and_deals_the_next_piece\n");
}

/*
** The common case is unchanged: a piece that completes nothing spawns the
** next one on the spot, with no phase in between.
*/
static void	test_a_lock_that_clears_nothing_does_not_hold(void)
{
	t_game		g;
	t_body_state	snap;

	game_start(&g, 1, 4242u);
	assert(game_drop(&g, true));
	assert(g.clearing_count == 0);
	game_snapshot(&g, &snap);
	assert(snap.phase == BODY_PHASE_ACTIVE);
	assert(snap.clearing_count == 0);
	assert(game_move(&g, -1) || game_move(&g, 1));
	printf("PASS test_a_lock_that_clears_nothing_does_not_hold\n");
}

/*
** Pause stops the clear for the same reason it stops gravity: the time was
** not spent playing, so it is not owed to the animation either.
*/
static void	test_a_pause_stops_the_clear_where_it_is(void)
{
	t_game	g;
	int		held;

	arm_a_clear(&g);
	assert(game_gravity(&g, 20));
	held = g.clearing_ms;
	assert(held == 20);
	assert(game_pause(&g, true));
	assert(!game_gravity(&g, 1000));
	assert(g.clearing_ms == held);
	assert(g.clearing_count == 1);
	assert(game_pause(&g, false));
	assert(game_gravity(&g, clear_duration_ms(g.level)));
	assert(g.clearing_count == 0);
	printf("PASS test_a_pause_stops_the_clear_where_it_is\n");
}

/*
** The wire half: phase, count, offset and rows all reach the body the client
** decodes, and the offset is how far *through* the clear the server is - the
** client draws from it rather than timing one of its own.
*/
static void	test_the_snapshot_carries_the_held_rows(void)
{
	t_game			g;
	t_body_state	snap;

	arm_a_clear(&g);
	game_snapshot(&g, &snap);
	assert(snap.phase == BODY_PHASE_CLEARING);
	assert(snap.clearing_count == 1);
	assert(snap.clearing_rows[0] == BOARD_HEIGHT - 1);
	assert(snap.clearing_ms == 0);
	assert(game_gravity(&g, 40));
	game_snapshot(&g, &snap);
	assert(snap.clearing_ms == 40);
	assert(snap.phase == BODY_PHASE_CLEARING);
	assert(game_gravity(&g, clear_duration_ms(g.level)));
	game_snapshot(&g, &snap);
	assert(snap.phase == BODY_PHASE_ACTIVE);
	assert(snap.clearing_count == 0);
	printf("PASS test_the_snapshot_carries_the_held_rows\n");
}

/*
** The last link: the body the client actually receives. game_snapshot filling
** the struct proves nothing on its own if the codec drops the fields on the
** way out, and a clear that decoded as count 0 would look exactly like no
** clear at all.
*/
static void	test_the_held_rows_survive_the_wire(void)
{
	t_game			g;
	t_body_state	sent;
	t_body_state	received;
	char			body[TETRISD_BODY_MAX_BYTES];
	int				len;

	arm_a_clear(&g);
	assert(game_gravity(&g, 40));
	game_snapshot(&g, &sent);
	len = body_state_encode(&sent, body, sizeof(body));
	assert(len > 0);
	memset(&received, 0, sizeof(received));
	assert(body_state_decode(body, (size_t)len, &received) == 0);
	assert(received.phase == BODY_PHASE_CLEARING);
	assert(received.clearing_count == sent.clearing_count);
	assert(received.clearing_ms == sent.clearing_ms);
	assert(received.clearing_rows[0] == sent.clearing_rows[0]);
	assert(received.cells[BOARD_HEIGHT - 1][0].type != 0);
	printf("PASS test_the_held_rows_survive_the_wire\n");
}

/*
** A coalesced tick can finish the clear before its elapsed budget is spent.
** The remainder belongs to the piece dealt at the end of the clear.
*/
static void	test_clear_overshoot_advances_the_next_piece(void)
{
	t_game	g;
	t_piece	spawn;
	int		incoming;
	int		elapsed_ms;

	arm_a_clear(&g);
	incoming = g.next[0];
	spawn = piece_spawn((t_piece_type)incoming);
	elapsed_ms = clear_duration_ms(g.level) + gravity_interval_ms(g.level);
	assert(game_gravity(&g, elapsed_ms));
	assert(g.clearing_count == 0);
	assert(g.piece.type == (t_piece_type)incoming);
	assert(g.piece.row == spawn.row + 1);
	printf("PASS test_clear_overshoot_advances_the_next_piece\n");
}

/**
 * @brief Fills the bottom row so the next lock completes it.
 *
 * The row is filled outright rather than left one cell short for the piece to
 * plug, because which cell that would be depends on the shape the bag deals.
 * What is under test is the phase a completed row enters, not the placement
 * that completes it - and the phase begins where it always begins, at the
 * lock.
 *
 * @param g Game whose board is being set up.
 */
static void	fill_bottom_row(t_game *g)
{
	t_cell	cell;
	int		col;

	cell.type = CELL_FILLED;
	cell.color = 1;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		board_set(&g->board, col, BOARD_HEIGHT - 1, cell);
		col++;
	}
}

/**
 * @brief Hard drops the active piece so that it locks.
 *
 * @param g Game to drop in.
 */
static void	drop_to_lock(t_game *g)
{
	assert(game_drop(g, true));
}

/**
 * @brief Starts a game and locks a piece over one completed row.
 *
 * @param g Game to bring to a held clear.
 */
static void	arm_a_clear(t_game *g)
{
	game_start(g, 1, 20260809u);
	fill_bottom_row(g);
	drop_to_lock(g);
	assert(g->clearing_count > 0);
}
