#include "tetrisu.h"

/*
** net_solo_apply, fed snapshots by hand.
**
** No socket and no server: a t_net_client holding a t_body_state is all the
** decode side needs, and building one directly is what makes the awkward
** cases reachable - a clear halfway through, a label that has not changed, a
** board the client tried to keep. The suites that need a real tetrisd are in
** tests/integration.
**
** What this guards is the half of the authority boundary that faces inward.
** Everything the renderer draws in an online game arrives through this one
** function, and for a long time three fields it needs never got copied: the
** clearing rows, how far through the clear the server was, and which clear it
** was. The animation could not run, because nothing had told it what to run
** on (docs/bugs/the_line_clear_never_reached_the_client.md).
*/

// Static Functions
static void	test_a_held_clear_reaches_the_view_model(void);
static void	test_the_offset_is_the_servers_not_a_local_timer(void);
static void	test_a_finished_clear_puts_the_rows_back(void);
static void	test_the_score_banner_fires_once_per_clear(void);
static void	test_a_label_without_a_score_change_does_not_fire(void);
static void	test_a_snapshot_overwrites_a_board_the_client_invented(void);

static void	blank_snapshot(t_body_state *snap);
static void	file_snapshot(t_net_client *net, const t_body_state *snap);

int	main(void)
{
	test_a_held_clear_reaches_the_view_model();
	test_the_offset_is_the_servers_not_a_local_timer();
	test_a_finished_clear_puts_the_rows_back();
	test_the_score_banner_fires_once_per_clear();
	test_a_label_without_a_score_change_does_not_fire();
	test_a_snapshot_overwrites_a_board_the_client_invented();
	return (0);
}

/*
** The three fields the renderer reads - phase, which rows, how far through -
** all arrive together, because they arrive in one snapshot.
*/
static void	test_a_held_clear_reaches_the_view_model(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;

	solo_game_init(&game, 1u);
	blank_snapshot(&snap);
	snap.phase = BODY_PHASE_CLEARING;
	snap.clearing_count = 2;
	snap.clearing_rows[0] = 18;
	snap.clearing_rows[1] = 19;
	snap.clearing_ms = 0;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.phase == SOLO_CLEARING);
	assert(game.clear_count == 2);
	assert(game.clear_rows[0] == 18 && game.clear_rows[1] == 19);
	assert(solo_game_row_is_clearing(&game, 18));
	assert(solo_game_row_is_clearing(&game, 19));
	assert(!solo_game_row_is_clearing(&game, 17));
	printf("PASS test_a_held_clear_reaches_the_view_model\n");
}

/*
** The client does not time the animation. It draws the offset the server
** sent, which is what stops it flashing rows tetrisd has already taken away.
*/
static void	test_the_offset_is_the_servers_not_a_local_timer(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;

	solo_game_init(&game, 1u);
	blank_snapshot(&snap);
	snap.phase = BODY_PHASE_CLEARING;
	snap.clearing_count = 1;
	snap.clearing_rows[0] = 19;
	snap.clearing_ms = 40;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.clear_elapsed_ms == 40);
	/* Presentation runs the countdown and the fades; it must not run this. */
	(void)solo_game_update_presentation(&game, 100);
	assert(game.clear_elapsed_ms == 40);
	snap.seq = 2;
	snap.clearing_ms = 120;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.clear_elapsed_ms == 120);
	printf("PASS test_the_offset_is_the_servers_not_a_local_timer\n");
}

/*
** When the server finishes, the client stops: count back to zero, phase back
** to active, and no row left marked.
*/
static void	test_a_finished_clear_puts_the_rows_back(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;

	solo_game_init(&game, 1u);
	blank_snapshot(&snap);
	snap.phase = BODY_PHASE_CLEARING;
	snap.clearing_count = 1;
	snap.clearing_rows[0] = 19;
	snap.clearing_ms = 180;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	blank_snapshot(&snap);
	snap.seq = 2;
	snap.phase = BODY_PHASE_ACTIVE;
	snap.lines = 1;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.phase == SOLO_ACTIVE);
	assert(game.clear_count == 0);
	assert(!solo_game_row_is_clearing(&game, 19));
	printf("PASS test_a_finished_clear_puts_the_rows_back\n");
}

/*
** TETRIS rather than SINGLE, and the points are read as the jump in the total
** the same snapshot carries, because the award itself is not on the wire.
*/
static void	test_the_score_banner_fires_once_per_clear(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;

	solo_game_init(&game, 1u);
	blank_snapshot(&snap);
	snap.last_clear = BODY_CLEAR_TETRIS;
	snap.score = 800;
	snap.lines = 4;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.score_event_active);
	assert(game.last_lines == 4);
	assert(game.last_score.total_awarded == 800);
	assert(solo_game_score_event_opacity(&game) > 0);
	printf("PASS test_the_score_banner_fires_once_per_clear\n");
}

/*
** last_clear stays set until the next lock, so every snapshot in between
** carries it. Re-firing on the label alone would restart the banner sixteen
** times over one clear and it would never fade.
*/
static void	test_a_label_without_a_score_change_does_not_fire(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;

	solo_game_init(&game, 1u);
	blank_snapshot(&snap);
	snap.last_clear = BODY_CLEAR_SINGLE;
	snap.score = 100;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.score_event_active);
	(void)solo_game_update_presentation(&game, 50);
	assert(game.score_event_elapsed_ms == 50);
	snap.seq = 2;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(game.score_event_elapsed_ms == 50);
	printf("PASS test_a_label_without_a_score_change_does_not_fire\n");
}

/*
** The acceptance check the whole boundary exists for, at unit scale: a
** snapshot is the board, not a patch on it, so anything the client wrote is
** gone the moment one arrives.
*/
static void	test_a_snapshot_overwrites_a_board_the_client_invented(void)
{
	t_net_client	net;
	t_body_state	snap;
	t_solo_game		game;
	t_cell			block;

	solo_game_init(&game, 1u);
	block.type = CELL_FILLED;
	block.color = 7;
	board_set(&game.board, 0, 0, block);
	game.scoring.total = 999999;
	game.clear_count = 4;
	blank_snapshot(&snap);
	snap.score = 12;
	file_snapshot(&net, &snap);
	assert(net_solo_apply(&net, &game));
	assert(board_get(&game.board, 0, 0).type == CELL_EMPTY);
	assert(game.scoring.total == 12);
	assert(game.clear_count == 0);
	printf("PASS test_a_snapshot_overwrites_a_board_the_client_invented\n");
}

/**
 * @brief An empty active-phase snapshot to vary one field of at a time.
 *
 * @param snap Snapshot to blank.
 */
static void	blank_snapshot(t_body_state *snap)
{
	memset(snap, 0, sizeof(*snap));
	snap->seq = 1;
	snap->phase = BODY_PHASE_ACTIVE;
	snap->hold = BODY_HOLD_EMPTY;
	snap->level = 1;
}

/**
 * @brief Files a snapshot on a client the way a STATE push would.
 *
 * @param net Client to blank and load.
 * @param snap Snapshot it is holding.
 */
static void	file_snapshot(t_net_client *net, const t_body_state *snap)
{
	memset(net, 0, sizeof(*net));
	net->fd = -1;
	net->state_snapshot = *snap;
	net->has_state = true;
}
