/* ************************************************************************** */
/*                                                                            */
/*   net_smoke.c - tetrisu's client, driven against a running tetrisd         */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_net_solo.sh starts one and runs this against it. Everything here    */
/*   goes over a real socket, a real libtetrissh handshake and real HTTTP -   */
/*   there is no fixture standing in for the wire.                            */
/*                                                                            */
/*   What it guards is the authority boundary docs/tetrisu-local-to-tetrisd   */
/*   .md draws: the client serialises actions and renders the snapshots it    */
/*   is sent, and never decides anything about a board itself.                */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

// Static Functions
static int	check_a_snapshot_is_the_whole_board(t_net_client *net);
static int	check_an_action_moves_the_server_piece(t_net_client *net);
static int	check_a_sent_action_needs_no_reply(t_net_client *net);
static int	check_hold_round_trips(t_net_client *net);
static int	check_pause_stops_the_server_clock(t_net_client *net);
static int	check_a_refusal_says_why(t_net_client *net);
static int	check_the_client_never_moves_its_own_board(t_net_client *net);

static int	sign_in(t_net_client *net, const char *name);
static int	settle(t_net_client *net, int ms);
static void	drain(t_net_client *net);
static int	boards_match(const t_board *left, const t_board *right);
static void	report(const char *name, int ok, int *failures);
static void	nap(int ms);

int	main(void)
{
	t_net_config	cfg;
	t_net_client	net;
	char			name[NET_USER_MAX];
	int				failures;

	net_config_load(&cfg);
	if (net_connect(&net, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			net.error);
		return (1);
	}
	snprintf(name, sizeof(name), "smoke%d", (int)getpid());
	if (sign_in(&net, name) != 0)
	{
		printf("FAIL: could not sign in as %s\n", name);
		net_disconnect(&net);
		return (1);
	}
	failures = 0;
	report("a snapshot is the whole board",
		check_a_snapshot_is_the_whole_board(&net), &failures);
	report("an action moves the server's piece",
		check_an_action_moves_the_server_piece(&net), &failures);
	report("a sent action needs no reply",
		check_a_sent_action_needs_no_reply(&net), &failures);
	report("hold round trips", check_hold_round_trips(&net), &failures);
	report("pause stops the server clock",
		check_pause_stops_the_server_clock(&net), &failures);
	report("a refusal says why", check_a_refusal_says_why(&net), &failures);
	report("the client never moves its own board",
		check_the_client_never_moves_its_own_board(&net), &failures);
	net_solo_leave(&net);
	net_disconnect(&net);
	return (failures != 0);
}

/*
** The first snapshot has to arrive on its own: the client asks for a game and
** the server starts pushing. Everything the renderer needs is in it, which is
** what makes it safe for the client to own no board of its own.
*/
static int	check_a_snapshot_is_the_whole_board(t_net_client *net)
{
	t_solo_game	game;

	if (net_solo_start(net, NULL) != 0)
		return (0);
	if (settle(net, 600) != 0)
		return (0);
	memset(&game, 0, sizeof(game));
	if (!net_solo_apply(net, &game))
		return (0);
	if (game.level < 1 || game.phase != SOLO_ACTIVE)
		return (0);
	if (game.next[0] > PIECE_L || game.next[2] > PIECE_L)
		return (0);
	return (net->state_snapshot.seq > 0 && !game.has_hold);
}

/*
** A move is a request, and what the player sees move is the piece in the next
** snapshot. The client does not touch the board to make that happen.
*/
static int	check_an_action_moves_the_server_piece(t_net_client *net)
{
	t_net_result	result;
	int				before;
	int				after;

	drain(net);
	before = net->state_snapshot.piece.col;
	if (net_solo_action(net, SOLO_MOVE_LEFT, &result) != 0
		|| result.status != 200)
		return (0);
	if (settle(net, 800) != 0)
		return (0);
	after = net->state_snapshot.piece.col;
	return (after == before - 1);
}

/*
** The path a held key uses: several moves go out back to back, nobody waits
** for a verdict, and the board still ends up where the requests said. This is
** the whole of the latency work - the wire is unchanged and only the waiting
** is gone - so what it has to prove is that the server sees the same requests
** and the client still learns the outcome from the snapshot.
**
** Three actions rather than one, because one would pass even if net_send had
** quietly kept the round trip. Soft drop is the axis that made this urgent -
** it is the repeat that outran the server's input budget - and it is also the
** one with no wall to clamp against three rows from the top.
**
** The piece may also have fallen under gravity while this ran, so the
** assertion is that it moved at least as far as was asked, not exactly.
*/
static int	check_a_sent_action_needs_no_reply(t_net_client *net)
{
	int	before;
	int	sent;

	drain(net);
	if (net->state_snapshot.phase != BODY_PHASE_ACTIVE)
		return (0);
	before = net->state_snapshot.piece.row;
	sent = 0;
	while (sent < 3)
	{
		if (net_solo_send_action(net, SOLO_SOFT_DROP) != 0)
			return (0);
		sent++;
	}
	if (settle(net, 800) != 0)
		return (0);
	if (net->state_snapshot.piece.row < before + 3)
		return (0);
	/* The three replies are still on the socket; draining must not choke. */
	return (net_pump(net) >= 0 && net->state == NET_IN_GAME);
}

/*
** The hold slot starts empty, takes the falling piece, and refuses a second
** hold until a piece has landed.
*/
static int	check_hold_round_trips(t_net_client *net)
{
	t_net_result	result;
	int				held;

	drain(net);
	held = net->state_snapshot.piece.type;
	if (net_solo_action(net, SOLO_HOLD, &result) != 0 || result.status != 200)
		return (0);
	if (settle(net, 800) != 0)
		return (0);
	if (net->state_snapshot.hold != held || !net->state_snapshot.hold_used)
		return (0);
	if (net_solo_action(net, SOLO_HOLD, &result) != 0 || result.status != 409)
		return (0);
	if (net_solo_action(net, SOLO_HARD_DROP, &result) != 0
		|| result.status != 200)
		return (0);
	if (settle(net, 800) != 0)
		return (0);
	return (!net->state_snapshot.hold_used);
}

/*
** Pause is the server's, and the phase says so. While it is on the piece does
** not fall, and resuming does not hand back the time as a drop.
*/
static int	check_pause_stops_the_server_clock(t_net_client *net)
{
	t_net_result	result;
	int				row;

	if (net_solo_pause(net, true, &result) != 0 || result.status != 200)
		return (0);
	if (settle(net, 800) != 0
		|| net->state_snapshot.phase != BODY_PHASE_PAUSED)
		return (0);
	row = net->state_snapshot.piece.row;
	nap(700);
	(void)net_pump(net);
	if (net->state_snapshot.piece.row != row)
		return (0);
	if (net_solo_action(net, SOLO_MOVE_LEFT, &result) != 0
		|| result.status != 409)
		return (0);
	if (net_solo_pause(net, false, &result) != 0 || result.status != 200)
		return (0);
	if (settle(net, 800) != 0)
		return (0);
	return (net->state_snapshot.phase == BODY_PHASE_ACTIVE
		&& net->state_snapshot.piece.row == row);
}

/*
** A refusal carries the server's own reason word, and the HUD feedback is
** built from that rather than from a bare status - a fresh game cannot afford
** anything, and "no charge" is a different thing to say than "not now".
*/
static int	check_a_refusal_says_why(t_net_client *net)
{
	t_net_result	result;
	t_solo_game		game;

	if (net_solo_ability(net, SOLO_ABILITY_MIRURUN, &result) != 0)
		return (0);
	if (result.status != 409 || result.reason[0] == '\0')
		return (0);
	memset(&game, 0, sizeof(game));
	net_solo_ability_feedback(&result, SOLO_ABILITY_MIRURUN, &game);
	if (game.ability_result == SOLO_ABILITY_RESULT_ACTIVATED)
		return (0);
	if (net_solo_ability(net, SOLO_ABILITY_PENTARIS, &result) != 0)
		return (0);
	return (result.status == 409
		&& strcmp(result.reason, "no-target") == 0);
}

/*
** The acceptance check that matters most: a client cannot change its board by
** withholding or fabricating a STATE. Writing all over the view model and
** then applying the server's snapshot has to put everything back, because
** the snapshot is the whole of the board and not a patch on it.
*/
static int	check_the_client_never_moves_its_own_board(t_net_client *net)
{
	t_solo_game	game;
	t_solo_game	forged;
	t_cell		block;

	drain(net);
	memset(&game, 0, sizeof(game));
	if (!net_solo_apply(net, &game))
		return (0);
	forged = game;
	block.type = CELL_FILLED;
	block.color = 7;
	board_set(&forged.board, 0, 0, block);
	board_set(&forged.board, 9, 19, block);
	forged.scoring.total = 999999;
	forged.crystal_charge = 10;
	forged.active.col = 0;
	if (!net_solo_apply(net, &forged))
		return (0);
	if (!boards_match(&forged.board, &game.board))
		return (0);
	return (forged.scoring.total == game.scoring.total
		&& forged.crystal_charge == game.crystal_charge
		&& forged.active.col == game.active.col
		&& forged.active.row == game.active.row);
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
 * @brief Waits for a snapshot newer than the one already in hand.
 *
 * Waiting for "any snapshot" would return the one from before the request
 * under test, so every check after the first would assert against a stale
 * board. The sequence number is what makes "newer" a question with an answer.
 *
 * @param net Connected client.
 * @param ms How long to keep waiting.
 * @return 0 when a newer snapshot arrived, -1 on a timeout or a lost session.
 */
static int	settle(t_net_client *net, int ms)
{
	struct pollfd	pfd;
	uint64_t		was;
	int				waited;

	was = net->last_seq;
	waited = 0;
	while (waited < ms)
	{
		pfd.fd = net_fd(net);
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (pfd.fd < 0)
			return (-1);
		if (poll(&pfd, 1, 25) > 0 && net_pump(net) < 0)
			return (-1);
		if (net->has_state && net->last_seq > was)
			return (0);
		waited += 25;
	}
	return (-1);
}

/**
 * @brief Takes whatever the server has already sent, without waiting.
 *
 * A check that wants "where the board is now" before it acts must not wait
 * for a newer snapshot: with nothing happening, the next one is a gravity
 * step away, and waiting for it would time out on an idle board.
 *
 * @param net Connected client.
 */
static void	drain(t_net_client *net)
{
	(void)net_pump(net);
}

/**
 * @brief Compares two boards cell by cell.
 *
 * memcmp would be comparing the padding in t_cell as well, which no code
 * promises anything about - the two boards can be equal in every cell and
 * still differ in bytes nobody wrote.
 *
 * @param left One board.
 * @param right The other.
 * @return Nonzero when every cell matches.
 */
static int	boards_match(const t_board *left, const t_board *right)
{
	int	row;
	int	col;

	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (board_get(left, col, row).type != board_get(right, col,
					row).type
				|| board_get(left, col, row).color != board_get(right, col,
					row).color)
				return (0);
			col++;
		}
		row++;
	}
	return (1);
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
