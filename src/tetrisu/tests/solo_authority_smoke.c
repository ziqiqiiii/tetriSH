/* ************************************************************************** */
/*                                                                            */
/*   solo_authority_smoke.c - Solo played the way the loop plays it           */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_solo_authority.sh starts one and runs this against it.              */
/*                                                                            */
/*   net_smoke drives the wire. This drives the layer above it - the one      */
/*   solo_mode.c actually calls - because that is where the two clocks meet.  */
/*   tetrisd starts gravity the moment it accepts a START; tetrisu holds a    */
/*   2.4 second 3-2-1 in front of the player and refuses input for the        */
/*   length of it. Nothing reconciled those, so an online game dropped two    */
/*   rows through a countdown the player could not act during, and then       */
/*   never accepted an input at all, because the countdown timer only ticked  */
/*   on the offline path (docs/bugs/).                                        */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

// Static Functions
static int	check_the_countdown_holds_the_server_clock(t_solo_authority *auth,
				t_solo_game *game);
static int	check_the_countdown_hands_the_board_back(t_solo_authority *auth,
				t_solo_game *game);
static int	check_input_reaches_the_server(t_solo_authority *auth,
				t_solo_game *game);
static int	check_gravity_runs_once_the_hold_is_over(t_solo_authority *auth,
				t_solo_game *game);
static int	check_a_landed_piece_can_still_be_moved(t_solo_authority *auth,
				t_solo_game *game);
static int	check_restart_re_arms_the_countdown(t_solo_authority *auth,
				t_solo_game *game);
static int	check_a_real_clear_reaches_the_client(t_solo_authority *auth,
				t_solo_game *game);
static int	check_a_lost_session_falls_offline(t_solo_authority *auth,
				t_solo_game *game);

static int	run_countdown(t_solo_authority *auth, t_solo_game *game);
static void	settle(t_solo_authority *auth, t_solo_game *game, int ms);
static int	sign_in(t_net_client *net, const char *name);
static void	report(const char *name, int ok, int *failures);
static void	nap(int ms);

static void	plan_placement(const t_solo_game *game, int *turns, int *column);
static int	score_placement(const t_board *board, const t_piece *piece,
				int row);
static int	target_row(const t_board *board);
static int	row_cells(const t_board *board, int row);
static int	row_holes(const t_board *board, int row);
static int	play_placement(t_solo_authority *auth, t_solo_game *game,
				int turns, int column);
static int	send_action(t_solo_authority *auth, t_solo_action action);
static int	soft_drop_to_floor(t_solo_authority *auth);

/**
 * @brief Entry point - sign in, then play Solo through the authority.
 */
int	main(void)
{
	t_net_config		cfg;
	t_net_client		net;
	t_solo_authority	authority;
	t_solo_game			game;
	char				name[NET_USER_MAX];
	int					failures;

	net_config_load(&cfg);
	memset(&net, 0, sizeof(net));
	if (net_connect(&net, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			net.error);
		return (1);
	}
	snprintf(name, sizeof(name), "auth%d", (int)getpid());
	if (sign_in(&net, name) != 0)
	{
		printf("FAIL: could not sign in as %s\n", name);
		net_disconnect(&net);
		return (1);
	}
	solo_authority_open(&authority, &net, &game, 12345u);
	solo_game_start_countdown(&game);
	failures = 0;
	report("the countdown holds the server's clock",
		check_the_countdown_holds_the_server_clock(&authority, &game),
		&failures);
	report("the countdown hands the board back",
		check_the_countdown_hands_the_board_back(&authority, &game),
		&failures);
	report("input reaches the server once play starts",
		check_input_reaches_the_server(&authority, &game), &failures);
	report("gravity runs once the hold is over",
		check_gravity_runs_once_the_hold_is_over(&authority, &game),
		&failures);
	report("a landed piece can still be moved",
		check_a_landed_piece_can_still_be_moved(&authority, &game), &failures);
	report("restart re-arms the countdown",
		check_restart_re_arms_the_countdown(&authority, &game), &failures);
	report("a real clear arrives as a phase with its rows",
		check_a_real_clear_reaches_the_client(&authority, &game), &failures);
	report("a lost session falls back to the local rules",
		check_a_lost_session_falls_offline(&authority, &game), &failures);
	solo_authority_close(&authority);
	net_disconnect(&net);
	return (failures != 0);
}

/*
** Opening an online game leaves the server paused. The proof is wall-clock:
** level one gravity is 1000 ms, so a piece left alone for longer than that
** and still on the same row is a piece nothing is dropping.
**
** Input is refused for the same reason it is refused offline - the player is
** watching a countdown, not playing yet - and refused here rather than sent
** and rejected, so the server never sees an input for a game it has paused.
*/
static int	check_the_countdown_holds_the_server_clock(t_solo_authority *auth,
			t_solo_game *game)
{
	int	row;
	int	col;

	if (!solo_authority_is_online(auth) || !auth->countdown_hold)
		return (0);
	settle(auth, game, 400);
	row = auth->net->state_snapshot.piece.row;
	col = auth->net->state_snapshot.piece.col;
	if (solo_authority_action(auth, game, SOLO_MOVE_LEFT))
		return (0);
	nap(1300);
	settle(auth, game, 0);
	return (auth->net->state_snapshot.piece.row == row
		&& auth->net->state_snapshot.piece.col == col
		&& game->countdown_active);
}

/*
** The countdown is the client's animation, and online it is the only timer
** the client still owns. Feeding the authority the milliseconds the loop
** would have fed it has to run it down and hand the board back - which is
** the regression: online_update applied snapshots and advanced nothing, so
** countdown_active stayed true for the rest of the game and every keypress
** was swallowed by the gate in front of it.
*/
static int	check_the_countdown_hands_the_board_back(t_solo_authority *auth,
			t_solo_game *game)
{
	if (!game->countdown_active)
		return (0);
	if (run_countdown(auth, game) != 0)
		return (0);
	settle(auth, game, 400);
	return (!game->countdown_active && !auth->countdown_hold
		&& !game->paused
		&& auth->net->state_snapshot.phase == BODY_PHASE_ACTIVE);
}

/*
** With the hold released, the authority is a courier again: the action goes
** out, the server moves its piece, and the snapshot that comes back is what
** the view model shows.
*/
static int	check_input_reaches_the_server(t_solo_authority *auth,
			t_solo_game *game)
{
	int	before;

	settle(auth, game, 200);
	before = auth->net->state_snapshot.piece.col;
	if (solo_authority_action(auth, game, SOLO_MOVE_LEFT))
		return (0);
	settle(auth, game, 800);
	if (auth->net->state_snapshot.piece.col != before - 1)
		return (0);
	return (game->active.col == auth->net->state_snapshot.piece.col);
}

/*
** The other half of the hold: once it is over the server's clock is running
** again, so a piece left alone does fall. Without this the release could be
** a no-op and the first check would still pass.
*/
static int	check_gravity_runs_once_the_hold_is_over(t_solo_authority *auth,
			t_solo_game *game)
{
	int	row;

	settle(auth, game, 200);
	row = auth->net->state_snapshot.piece.row;
	nap(1300);
	settle(auth, game, 0);
	return (auth->net->state_snapshot.piece.row != row);
}

/*
** Lock delay, over the wire.
**
** The piece is soft dropped until the server refuses - which is what a piece
** resting on the floor now answers, where it used to lock instead - and then
** moved. If the server still owns that piece the snapshot comes back one
** column across and on the same row. If it had locked on contact the row
** would be back at the top with a new piece on it.
**
** This is the difference the player reported: without it a piece can only be
** placed in the column it happened to land in, and nothing can be slid under
** an overhang.
*/
static int	check_a_landed_piece_can_still_be_moved(t_solo_authority *auth,
			t_solo_game *game)
{
	int	row;
	int	col;

	settle(auth, game, 200);
	if (soft_drop_to_floor(auth) < 1)
		return (0);
	settle(auth, game, 40);
	row = auth->net->state_snapshot.piece.row;
	col = auth->net->state_snapshot.piece.col;
	if (col > 0 && send_action(auth, SOLO_MOVE_LEFT) != 0)
		return (0);
	if (col <= 0 && send_action(auth, SOLO_MOVE_RIGHT) != 0)
		return (0);
	settle(auth, game, 40);
	if (auth->net->state_snapshot.piece.row != row)
		return (0);
	if (col > 0)
		return (auth->net->state_snapshot.piece.col == col - 1);
	return (auth->net->state_snapshot.piece.col == col + 1);
}

/*
** Restarting is starting, so it arms the same hold: a fresh game that came
** back without one would drop the player straight into a live board behind a
** countdown they cannot play through.
*/
static int	check_restart_re_arms_the_countdown(t_solo_authority *auth,
			t_solo_game *game)
{
	if (!solo_authority_restart(auth, game, 999u))
		return (0);
	if (!game->countdown_active || !auth->countdown_hold)
		return (0);
	if (solo_authority_action(auth, game, SOLO_MOVE_RIGHT))
		return (0);
	if (run_countdown(auth, game) != 0)
		return (0);
	settle(auth, game, 400);
	return (!auth->countdown_hold && !game->paused);
}

/*
** Actually clear a line against the real server, and catch it mid-clear.
**
** The whole chain in one check: tetrisd holding the completed rows, encoding
** them into STATE, and net_solo_apply putting them where the renderer looks.
** Before the fix a clear happened between two ticks and no client could ever
** observe one (docs/bugs/the_line_clear_never_reached_the_client.md).
**
** This plays rather than replaying a script, because there is no script to
** record: tetrisd seeds each bag from its clock, so the pieces differ every
** run. What is fixed is the *strategy* - fill the floor row - and the search
** behind it runs on libtetrisbrain, the same rules the server applies, so the
** two cannot disagree about a kick or a landing row. The assertion is a
** property ("a clear arrives, and its rows are still filled"), not a trace,
** which is what makes it deterministic in outcome while the pieces are not.
**
** It gives up after twenty-four pieces rather than hanging. A run that
** cannot clear a row inside twenty-four placements is a failure worth
** seeing.
*/
static int	check_a_real_clear_reaches_the_client(t_solo_authority *auth,
			t_solo_game *game)
{
	int	pieces;
	int	turns;
	int	column;
	int	row;

	/* Start from a clean board: the checks above spend seconds watching
	 * gravity, and the pieces that locked while they waited leave gaps no
	 * hard drop can reach - a floor row that can never complete. */
	if (!solo_authority_restart(auth, game, 4242u))
		return (0);
	if (run_countdown(auth, game) != 0)
		return (0);
	pieces = 0;
	settle(auth, game, 300);
	while (pieces < 24 && auth->net->state_snapshot.phase
		!= BODY_PHASE_CLEARING)
	{
		/* A stack that reached the ceiling before a row completed is a bad
		 * run, not a broken server: deal a fresh board and keep going inside
		 * the same budget. */
		if (game->phase != SOLO_ACTIVE)
		{
			if (!solo_authority_restart(auth, game, 77u)
				|| run_countdown(auth, game) != 0)
				return (0);
			settle(auth, game, 300);
			if (game->phase != SOLO_ACTIVE)
				return (0);
		}
		plan_placement(game, &turns, &column);
		if (play_placement(auth, game, turns, column) != 0)
			return (0);
		settle(auth, game, 40);
		pieces++;
		if (auth->net->state_snapshot.phase == BODY_PHASE_CLEARING)
			break ;
		/* Let the token bucket refill between placements. */
		settle(auth, game, 120);
	}
	if (auth->net->state_snapshot.phase != BODY_PHASE_CLEARING)
		return (0);
	if (game->phase != SOLO_CLEARING || game->clear_count <= 0)
		return (0);
	row = game->clear_rows[0];
	if (row < 0 || row >= BOARD_HEIGHT || !solo_game_row_is_clearing(game, row))
		return (0);
	/* The named row is still filled in the very board that named it. */
	if (board_get(&game->board, 0, row).type == CELL_EMPTY
		|| board_get(&game->board, BOARD_WIDTH - 1, row).type == CELL_EMPTY)
		return (0);
	settle(auth, game, 400);
	return (game->clear_count == 0 && game->total_lines > 0);
}

/*
** Losing the session mid-game is not the end of the game (solo_authority.c):
** the local rules take over from the last position both sides agreed on. The
** hold has to go with it, or the offline game would inherit a flag that only
** ever meant something to a server.
*/
static int	check_a_lost_session_falls_offline(t_solo_authority *auth,
			t_solo_game *game)
{
	settle(auth, game, 200);
	net_disconnect(auth->net);
	(void)solo_authority_update(auth, game, 16);
	if (solo_authority_is_online(auth) || auth->countdown_hold)
		return (0);
	if (!auth->lost)
		return (0);
	return (solo_authority_fd(auth) < 0);
}

/**
 * @brief Feeds the authority enough milliseconds to run the countdown out.
 *
 * The loop hands it whatever wall-clock passed; a test hands it the same
 * total in steps, because a countdown that only ends after 2.4 real seconds
 * is 2.4 seconds this suite would spend watching an animation.
 *
 * @param auth Authority to advance.
 * @param game View model holding the countdown.
 * @return 0 when the countdown finished, -1 when it did not.
 */
static int	run_countdown(t_solo_authority *auth, t_solo_game *game)
{
	int	steps;

	steps = 0;
	while (game->countdown_active && steps < 64)
	{
		(void)solo_authority_update(auth, game, SOLO_COUNTDOWN_STEP_MS / 2);
		steps++;
	}
	if (game->countdown_active)
		return (-1);
	return (0);
}

/**
 * @brief Waits for the server to push, then drains what it pushed.
 *
 * @param auth Authority to pump.
 * @param game View model the snapshots land in.
 * @param ms Milliseconds to wait before draining.
 */
static void	settle(t_solo_authority *auth, t_solo_game *game, int ms)
{
	if (ms > 0)
		nap(ms);
	(void)solo_authority_update(auth, game, 0);
}

/**
 * @brief Registers a throwaway player and binds this connection to it.
 *
 * @param net Connected client.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
static int	sign_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	if (net_signup(net, name, "hunter2", &result) != 0
		|| result.status != 201)
		return (-1);
	if (net_login(net, name, "hunter2", &result) != 0
		|| result.status != 200)
		return (-1);
	return (0);
}

/**
 * @brief Prints one check's verdict in the runner's format.
 *
 * @param name What was checked.
 * @param ok Nonzero when it held.
 * @param failures Counter incremented when it did not.
 */
static void	report(const char *name, int ok, int *failures)
{
	if (ok)
	{
		printf("PASS: %s\n", name);
		return ;
	}
	printf("FAIL: %s\n", name);
	(*failures)++;
}

/**
 * @brief Picks the placement that puts the most cells in the floor row.
 *
 * Every rotation and every reachable column is tried against the board the
 * server last sent, using libtetrisbrain - so the kick, the collision and the
 * landing row are the server's own answers, not an approximation of them.
 * The plan is computed from one snapshot and executed in order, which is
 * exactly how the server will apply it.
 *
 * The plan names the column to end up in rather than how far to move,
 * because a rotation kicks differently depending on the row the piece is at,
 * and gravity moves that row while the plan is being sent. A relative shift
 * computed before rotating therefore lands somewhere else - which is how this
 * check spoiled its own board roughly once in thirty runs.
 *
 * @param game View model holding the server's board and falling piece.
 * @param turns Receives how many clockwise rotations to send.
 * @param column Receives the column the piece should end up in.
 */
static void	plan_placement(const t_solo_game *game, int *turns, int *column)
{
	t_piece	candidate;
	t_piece	rotated;
	int		best;
	int		value;
	int		turn;
	int		step;
	int		kick;
	int		row;

	*turns = 0;
	*column = game->active.col;
	best = INT32_MIN;
	row = target_row(&game->board);
	turn = 0;
	rotated = game->active;
	while (turn < 4)
	{
		step = -(BOARD_WIDTH);
		while (step <= BOARD_WIDTH)
		{
			candidate = rotated;
			if (piece_move(&game->board, &candidate, step, 0) == BRAIN_OK)
			{
				value = score_placement(&game->board, &candidate, row);
				if (value > best)
				{
					best = value;
					*turns = turn;
					*column = candidate.col;
				}
			}
			step++;
		}
		kick = 0;
		if (piece_rotate_with_kick(&game->board, &rotated, 1, &kick)
			!= BRAIN_OK)
			break ;
		turn++;
	}
}

/**
 * @brief Rates one landing by how much of the floor row it completes.
 *
 * Filling the floor row is the whole objective, because a completed row is
 * the event under test. Landing deep is the tie-break, so the stack stays
 * flat and the next piece has somewhere to go rather than burying a gap the
 * search can never fill.
 *
 * @param board The board as the server last sent it.
 * @param piece A candidate placement, already moved and rotated.
 * @return Higher is better.
 */
static int	score_placement(const t_board *board, const t_piece *piece,
			int row)
{
	t_board	after;
	t_piece	landed;

	board_copy(&after, board);
	landed = *piece;
	piece_hard_drop(&after, &landed);
	piece_stamp(&after, &landed);
	return (row_cells(&after, row) * 1000 - row_holes(&after, row) * 100000
		+ landed.row);
}

/**
 * @brief Names the lowest row a hard drop can still complete.
 *
 * Not always the floor. S and Z overhang in every rotation, so the first one
 * dealt onto flat ground roofs a column no matter where it goes - the hole is
 * the piece, not a mistake by the search. Row nineteen is then dead for the
 * rest of the game, and a solver that kept aiming at it would place another
 * twenty pieces at a row it could never finish. Moving the target up one is
 * what a player does without thinking about it.
 *
 * @param board Board to read.
 * @return The lowest row with no unreachable cell, or 0 if there is none.
 */
static int	target_row(const t_board *board)
{
	int	row;

	row = BOARD_HEIGHT - 1;
	while (row > 0)
	{
		if (row_holes(board, row) == 0 && row_cells(board, row) < BOARD_WIDTH)
			return (row);
		row--;
	}
	return (0);
}

/**
 * @brief Counts the filled cells in one row.
 *
 * @param board Board to measure.
 * @param row Row to count.
 * @return How many of that row's cells are occupied.
 */
static int	row_cells(const t_board *board, int row)
{
	int	count;
	int	col;

	count = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (board_get(board, col, row).type != CELL_EMPTY)
			count++;
		col++;
	}
	return (count);
}

/**
 * @brief Counts cells in one row that are empty with anything above them.
 *
 * Pieces only ever hard drop here, so a gap with something over it - at any
 * height, not just the row directly above - can never be filled again. One of
 * those makes the row permanently incomplete, which is why its cost outweighs
 * any number of cells a placement could add: a search that traded a hole for
 * a cell would spoil the row it was trying to finish and then stall on it
 * until the piece budget ran out.
 *
 * @param board Board to measure.
 * @param row Row to inspect.
 * @return How many of that row's cells are unreachable.
 */
static int	row_holes(const t_board *board, int row)
{
	int	count;
	int	col;
	int	above;

	count = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		above = 0;
		while (board_get(board, col, row).type == CELL_EMPTY && above < row)
		{
			if (board_get(board, col, above).type != CELL_EMPTY)
			{
				count++;
				break ;
			}
			above++;
		}
		col++;
	}
	return (count);
}

/**
 * @brief Sends one planned placement: rotations, then moves, then the drop.
 *
 * The column is read back from the server after the rotations rather than
 * assumed, so a kick that landed somewhere other than the plan expected is
 * corrected instead of compounded.
 *
 * @param auth Authority in charge.
 * @param game View model, re-read between the rotations and the moves.
 * @param turns Clockwise rotations to send.
 * @param column The column to end up in.
 * @return 0 when every request was accepted, -1 otherwise.
 */
static int	play_placement(t_solo_authority *auth, t_solo_game *game,
			int turns, int column)
{
	int	sent;
	int	guard;

	sent = 0;
	while (sent < turns)
	{
		if (send_action(auth, SOLO_ROTATE_CW) != 0)
			return (-1);
		sent++;
	}
	if (turns > 0)
		settle(auth, game, 40);
	guard = column - game->active.col;
	while (guard != 0)
	{
		if (guard < 0 && send_action(auth, SOLO_MOVE_LEFT) != 0)
			return (-1);
		if (guard > 0 && send_action(auth, SOLO_MOVE_RIGHT) != 0)
			return (-1);
		if (guard < 0)
			guard++;
		else
			guard--;
	}
	return (send_action(auth, SOLO_HARD_DROP));
}

/**
 * @brief Sends one action, waiting out the input rate limit if it bites.
 *
 * A 429 is the server working as designed, not a failure - the plan just has
 * to be sent slower. Anything else refused means the placement the search
 * validated was rejected, and the check should say so rather than carry on
 * with a board it no longer predicts.
 *
 * @param auth Authority whose session sends the request.
 * @param action The action to send.
 * @return 0 when the server accepted it, -1 otherwise.
 */
static int	send_action(t_solo_authority *auth, t_solo_action action)
{
	t_net_result	result;
	int				tries;

	tries = 0;
	while (tries < 4)
	{
		memset(&result, 0, sizeof(result));
		if (net_solo_action(auth->net, action, &result) != 0)
			return (-1);
		if (result.status == 200)
			return (0);
		if (result.status != 429)
			return (-1);
		nap(250);
		tries++;
	}
	return (-1);
}

/**
 * @brief Soft drops until the server refuses, and reports how far it fell.
 *
 * A refusal is the answer being looked for, not a failure: the piece is on
 * the floor and a soft drop has nowhere to put it. It is deliberately not
 * routed through send_action, which treats any non-200 as the plan having
 * gone wrong.
 *
 * @param auth Authority whose session sends the requests.
 * @return Rows fallen, or -1 when the session or the server misbehaved.
 */
static int	soft_drop_to_floor(t_solo_authority *auth)
{
	t_net_result	result;
	int				rows;

	rows = 0;
	while (rows < BOARD_HEIGHT + 1)
	{
		memset(&result, 0, sizeof(result));
		if (net_solo_action(auth->net, SOLO_SOFT_DROP, &result) != 0)
			return (-1);
		if (result.status == 409)
			return (rows);
		if (result.status == 429)
			nap(250);
		else if (result.status == 200)
			rows++;
		else
			return (-1);
	}
	return (-1);
}

/**
 * @brief Sleeps for a number of milliseconds.
 *
 * @param ms Milliseconds to sleep.
 */
static void	nap(int ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
