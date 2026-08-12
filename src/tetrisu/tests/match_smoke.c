/* ************************************************************************** */
/*                                                                            */
/*   match_smoke.c - two clients, one Double room, against a live tetrisd     */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_net_double.sh starts one and runs this against it.                  */
/*                                                                            */
/*   Both clients are this process, on two sockets. That is not a shortcut -  */
/*   it is what lets one program assert on both halves of the same instant:   */
/*   that the board one player is steering is the board the other is being    */
/*   shown, in the same frame. Two processes could only agree by talking to   */
/*   each other, which is the very thing under test.                          */
/*                                                                            */
/*   Nothing here draws anything. The whole point of keeping tetrisu_net.h    */
/*   free of notcurses is that the rule about who owns the board is testable  */
/*   with no terminal in the room.                                            */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"
#include "tetrisu_bot.h"

// Static Functions
static int	check_both_are_dealt_and_held(t_net_client *owner,
				t_net_client *joiner);
static int	check_each_sees_the_other(t_net_client *owner,
				t_net_client *joiner);
static int	check_a_move_crosses(t_net_client *owner, t_net_client *joiner);
static int	check_the_joiner_may_play(t_net_client *joiner);
static int	check_a_top_out_decides_the_match(t_net_client *owner,
				t_net_client *joiner);
static int	check_the_room_survives_the_match(t_net_client *owner,
				t_net_client *joiner, const char *room);
static int	check_every_ability_reaches_the_server(t_net_client *owner,
				t_net_client *joiner);
static int	check_an_ability_that_was_paid_for_lands(t_net_client *owner);

static int	earn_charge(t_net_client *net, int wanted, int budget);
static int	place_one_piece(t_net_client *net);
static int	rows_were_fried(const t_body_state *snap, int rows);
static int	act(t_net_client *net, t_solo_action action);
static int	settle_active(t_net_client *net, int tries);
static int	settle_after(t_net_client *net, uint64_t seq, int tries);

static int	sign_up_and_in(t_net_client *net, const char *name);
static int	open_room(t_net_client *owner, char *room, size_t cap);
static int	join_room(t_net_client *net, const char *room);
static int	start_room(t_net_client *net, const char *room);
static int	lock_in(t_net_client *net, const char *room);
static int	settle_state(t_net_client *net, int tries);
static int	wait_playing(t_net_client *net, int tries);
static int	wait_result(t_net_client *net, int tries);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - two players take a Double room and play it.
 *
 * @return 0 when every check passed, 1 otherwise.
 */
int	main(void)
{
	t_net_config	cfg;
	t_net_client	owner;
	t_net_client	joiner;
	char			room[NET_ROOM_MAX];
	char			name[NET_USER_MAX];
	int				failures;

	net_config_load(&cfg);
	memset(&owner, 0, sizeof(owner));
	memset(&joiner, 0, sizeof(joiner));
	if (net_connect(&owner, &cfg) != 0 || net_connect(&joiner, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d\n", cfg.host, cfg.port);
		return (1);
	}
	snprintf(name, sizeof(name), "own%d", (int)getpid());
	if (!sign_up_and_in(&owner, name))
		return (printf("FAIL: could not register %s\n", name), 1);
	snprintf(name, sizeof(name), "joi%d", (int)getpid());
	if (!sign_up_and_in(&joiner, name))
		return (printf("FAIL: could not register %s\n", name), 1);
	if (!open_room(&owner, room, sizeof(room)) || !join_room(&joiner, room)
		|| !start_room(&owner, room) || !lock_in(&owner, room)
		|| !lock_in(&joiner, room))
		return (printf("FAIL: could not open a Double room\n"), 1);
	failures = 0;
	report("both boards are dealt and held",
		check_both_are_dealt_and_held(&owner, &joiner), &failures);
	report("each snapshot carries the other board",
		check_each_sees_the_other(&owner, &joiner), &failures);
	report("a move crosses to the other view",
		check_a_move_crosses(&owner, &joiner), &failures);
	report("the joiner may play without having started",
		check_the_joiner_may_play(&joiner), &failures);
	report("every ability level reaches the server",
		check_every_ability_reaches_the_server(&owner, &joiner), &failures);
	report("an ability that was paid for lands",
		check_an_ability_that_was_paid_for_lands(&owner), &failures);
	report("a top-out decides the match for both",
		check_a_top_out_decides_the_match(&owner, &joiner), &failures);
	report("the room is still there to play again",
		check_the_room_survives_the_match(&owner, &joiner, room), &failures);
	net_disconnect(&owner);
	net_disconnect(&joiner);
	return (failures != 0);
}

/*
** A dealt match is held before it begins, and both players are told the same
** number. Neither client could arrange that for itself: each arrives at its
** match screen at a different moment, and PAUSE is refused in a room with
** anybody else in it.
*/
static int	check_both_are_dealt_and_held(t_net_client *owner,
			t_net_client *joiner)
{
	if (!settle_state(owner, 40) || !settle_state(joiner, 40))
		return (0);
	if (owner->state_snapshot.phase != BODY_PHASE_COUNTDOWN)
		return (0);
	if (joiner->state_snapshot.phase != BODY_PHASE_COUNTDOWN)
		return (0);
	if (owner->state_snapshot.countdown_ms <= 0)
		return (0);
	if (joiner->state_snapshot.countdown_ms <= 0)
		return (0);
	return (1);
}

/*
** The opponent rides inside the recipient's own frame, so this is also the
** check that the two boards a client draws came from one instant.
*/
static int	check_each_sees_the_other(t_net_client *owner,
			t_net_client *joiner)
{
	if (!wait_playing(owner, 1200) || !wait_playing(joiner, 1200))
		return (0);
	if (owner->state_snapshot.opponent_count != 1)
		return (0);
	if (owner->state_snapshot.opponents[0].player_id != joiner->player_id)
		return (0);
	if (strcmp(owner->state_snapshot.opponents[0].username,
			joiner->username) != 0)
		return (0);
	if (joiner->state_snapshot.opponent_count != 1)
		return (0);
	if (joiner->state_snapshot.opponents[0].player_id != owner->player_id)
		return (0);
	return (1);
}

/*
** One player's move is a frame owed to both, because both frames carry both
** boards. Before the dirty flag was spread across the room, the opponent's
** board only moved when the watcher moved.
*/
static int	check_a_move_crosses(t_net_client *owner, t_net_client *joiner)
{
	t_net_result	result;
	int				before;
	int				tries;

	before = owner->state_snapshot.opponents[0].piece.col;
	if (net_match_action(joiner, SOLO_MOVE_LEFT, &result) != 0
		|| result.status != 200)
		return (0);
	tries = 0;
	while (tries < 200)
	{
		if (net_pump(owner) < 0)
			return (0);
		if (owner->state_snapshot.opponents[0].piece.col == before - 1)
			return (1);
		usleep(5000);
		tries++;
	}
	return (0);
}

/*
** The player who joined never sent START, so nothing in their own requests
** ever told them a game had begun. The snapshot addressed to their play path
** is what does, and without it every input they sent was refused before it
** reached the socket - which the loop reads as a lost session.
*/
static int	check_the_joiner_may_play(t_net_client *joiner)
{
	t_net_result	result;

	if (joiner->state != NET_IN_GAME)
		return (0);
	if (net_match_action(joiner, SOLO_ROTATE_CW, &result) != 0)
		return (0);
	if (result.status != 200 && result.status != 409)
		return (0);
	if (net_match_action(joiner, SOLO_MOVE_RIGHT, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

/*
** Surviving is the whole of winning. The loser tops out; the winner is told
** so by a field of its own, because their board is active with a piece on it
** exactly like a board mid-match.
*/
static int	check_a_top_out_decides_the_match(t_net_client *owner,
			t_net_client *joiner)
{
	t_net_result	result;
	int				guard;

	guard = 0;
	while (guard < 300)
	{
		if (net_match_action(owner, SOLO_HARD_DROP, &result) != 0)
			return (0);
		if (result.status == 409)
			break ;
		guard++;
	}
	if (guard >= 300)
		return (0);
	if (!wait_result(owner, 200) || !wait_result(joiner, 200))
		return (0);
	if (owner->state_snapshot.result != BODY_RESULT_LOST
		|| owner->state_snapshot.rank != 2)
		return (0);
	if (joiner->state_snapshot.result != BODY_RESULT_WON
		|| joiner->state_snapshot.rank != 1)
		return (0);
	return (1);
}

/*
** All four levels, over the wire, in a room with somebody to aim at.
**
** No charge has been earned yet, so every one of them is refused - and that
** is the assertion rather than a limitation of it. `no-charge` is the answer
** only a server that got as far as looking the ability up can give: it has
** read the character off the account, found what that character offers at
** that level, and priced it. A level nobody offers, or a body that is not a
** level at all, never reaches the meter and is refused differently.
**
** The refusal is also a thing the player is shown, so the snapshot has to
** carry it. The server remembers refused activations exactly as it remembers
** accepted ones, and the frame that follows names the level that was tried.
**
** What this cannot reach is an accepted activation, because charge is earned
** by clearing lines and this check has none. The one after it plays for the
** charge and spends it; every transform in the catalogue is asserted
** in-process besides, one consequence each, by
** src/tetrisd/tests/test_ability_matrix.c.
*/
static int	check_every_ability_reaches_the_server(t_net_client *owner,
			t_net_client *joiner)
{
	t_net_result	result;
	int				level;

	level = SOLO_ABILITY_MIRURUN;
	while (level <= SOLO_ABILITY_SIRTET)
	{
		if (net_match_ability(owner, (t_solo_ability)level, &result) != 0)
			return (0);
		if (result.status != 409
			|| strcmp(result.reason, "no-charge") != 0)
			return (0);
		level++;
	}
	/* the held frame predates the request; the next one is the answer */
	owner->has_state = false;
	if (!settle_state(owner, 400))
		return (0);
	if (owner->state_snapshot.last_ability.level != SOLO_ABILITY_SIRTET
		|| owner->state_snapshot.last_ability.accepted)
		return (0);
	/* nothing was spent, and nothing crossed to the other board */
	if (owner->state_snapshot.charge != 0)
		return (0);
	if (joiner->state_snapshot.pending != 0)
		return (0);
	return (1);
}

/*
** The one accepted activation that crosses a socket.
**
** Everything else about the catalogue is asserted in process by
** src/tetrisd/tests/test_ability_matrix.c, and the check above can only reach
** the refusal, because charge is earned by clearing lines and no test can
** deal itself the pieces to clear one.
**
** So this one plays for it. Every snapshot carries the whole board and the
** falling piece, and libtetrisbrain is linked in on this side too, so the bot
** can try every placement of the piece it was actually given and take the one
** leaving the fewest holes. That clears four lines - two charges, one level-1
** activation - inside a few dozen pieces.
**
** Nothing was opened in the server to make this reachable, which is the point:
** the bag is the bag tetrisd dealt itself, no route was added, and the charge
** spent here is charge that was played for. A test-only way to top up a meter
** would have asserted that the top-up worked.
**
** Halloween is what a fresh account has equipped and its level 1 is Fry: the
** caster's own bottom three rows are overwritten with garbage, and the effect
** that burns them and sends them on is armed. Both halves ride the snapshot,
** so both are asserted here rather than only the status.
**
** The board assertion is the shape of those rows and not a cell count. Fry
** replaces the bottom rows rather than pushing the stack up, so on a board
** with anything on it the count barely moves - it went up by one the first
** time this was written, which said nothing at all about whether Fry had run.
*/
static int	check_an_ability_that_was_paid_for_lands(t_net_client *owner)
{
	t_net_result	result;
	int				charge_before;

	if (!earn_charge(owner, ability_cost(1), 120))
		return (0);
	charge_before = owner->state_snapshot.charge;
	if (net_match_ability(owner, SOLO_ABILITY_MIRURUN, &result) != 0
		|| result.status != 200)
		return (0);
	owner->has_state = false;
	if (!settle_state(owner, 400))
		return (0);
	if (owner->state_snapshot.last_ability.level != SOLO_ABILITY_MIRURUN
		|| !owner->state_snapshot.last_ability.accepted)
		return (0);
	if (owner->state_snapshot.charge != charge_before - ability_cost(1))
		return (0);
	if (owner->state_snapshot.effect_fry <= 0)
		return (0);
	return (rows_were_fried(&owner->state_snapshot, 3));
}

/**
 * @brief Plays the board until the meter holds what an activation costs.
 *
 * @param net Client with a match running.
 * @param wanted How much charge to bank.
 * @param budget How many pieces to spend getting there.
 * @return 1 once the charge is banked, 0 when the budget ran out.
 */
static int	earn_charge(t_net_client *net, int wanted, int budget)
{
	while (budget > 0)
	{
		if (!settle_active(net, 600))
			return (0);
		if (net->state_snapshot.charge >= wanted)
			return (1);
		if (!place_one_piece(net))
			return (0);
		budget--;
	}
	return (0);
}

/**
 * @brief Rotates, slides and drops the falling piece where it does least harm.
 *
 * The column is chosen twice: once to pick the rotation, and again from the
 * snapshot that came back after rotating. A rotation near a wall kicks the
 * piece sideways, so the column that was planned before it is not the column
 * the piece is in afterwards - and a plan made against the wrong column is
 * what builds the holes this bot exists to avoid.
 *
 * The wait after the drop is not politeness, it is the whole loop. Without it
 * the next piece is planned from the snapshot before this one landed - same
 * board, same piece - so every piece after the first is placed by a plan for
 * the one before it. That stacks straight to the ceiling without ever
 * completing a row, which is exactly what it did.
 *
 * The bot is built here rather than carried between pieces, and that is the
 * same bot: only the easy tier keeps anything from one piece to the next, and
 * this plays at normal, where the state a t_bot holds is never read.
 *
 * @param net Client with a match running.
 * @return 1 when the piece was dropped and the board that took it has
 *         arrived, 0 on a transport failure.
 */
static int	place_one_piece(t_net_client *net)
{
	t_bot		bot;
	uint64_t	seq;
	int			rotation;
	int			target;
	int			turns;

	bot_init(&bot, BOT_NORMAL, BAG_FALLBACK_SEED);
	seq = net->state_snapshot.seq;
	if (bot_plan(&bot, &net->state_snapshot, true, &rotation, &target))
	{
		turns = (rotation - net->state_snapshot.piece.rotation + 4) % 4;
		while (turns > 0)
		{
			if (!act(net, SOLO_ROTATE_CW))
				return (0);
			turns--;
		}
		/* best effort: a rotation the stack refused owes no frame at all */
		settle_after(net, seq, 200);
	}
	if (bot_plan(&bot, &net->state_snapshot, false, &rotation, &target))
	{
		turns = target - net->state_snapshot.piece.col;
		while (turns != 0)
		{
			if (!act(net, turns < 0 ? SOLO_MOVE_LEFT : SOLO_MOVE_RIGHT))
				return (0);
			turns += (turns < 0) - (turns > 0);
		}
	}
	seq = net->state_snapshot.seq;
	if (!act(net, SOLO_HARD_DROP))
		return (0);
	return (settle_after(net, seq, 600));
}

/**
 * @brief Reports whether the bottom rows are the ones Fry leaves behind.
 *
 * Garbage is a full row with one column missing, and the rows a single
 * activation lays down share that column - which is what makes this the shape
 * of one Fry rather than of a stack that happened to be nearly full.
 *
 * @param snap Snapshot to read the board from.
 * @param rows How many rows from the bottom to check.
 * @return 1 when every one of them is garbage over the same hole, 0 otherwise.
 */
static int	rows_were_fried(const t_body_state *snap, int rows)
{
	int	row;
	int	col;
	int	hole;
	int	shared;

	shared = -1;
	row = BODY_BOARD_ROWS - rows;
	while (row < BODY_BOARD_ROWS)
	{
		hole = -1;
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (snap->cells[row][col].type == CELL_EMPTY && hole >= 0)
				return (0);
			if (snap->cells[row][col].type == CELL_EMPTY)
				hole = col;
			col++;
		}
		if (hole < 0 || (shared >= 0 && hole != shared))
			return (0);
		shared = hole;
		row++;
	}
	return (1);
}

/**
 * @brief Sends one input and reports only whether the socket survived it.
 *
 * A refused input is not a failure here. Sliding into a wall is refused, and
 * the bot slides until it is - that is how it knows it has arrived.
 *
 * @param net Client with a match running.
 * @param action The input to send.
 * @return 1 when the server answered, 0 on a transport failure.
 */
static int	act(t_net_client *net, t_solo_action action)
{
	t_net_result	result;

	return (net_match_action(net, action, &result) == 0);
}

/**
 * @brief Pumps until the board is one that will accept an input.
 *
 * @param net Client to pump.
 * @param tries How many 2 ms attempts to make.
 * @return 1 once the board is active, 0 otherwise.
 */
static int	settle_active(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state && net->state_snapshot.countdown_ms == 0
			&& net->state_snapshot.phase == BODY_PHASE_ACTIVE)
			return (1);
		usleep(2000);
		tries--;
	}
	return (0);
}

/**
 * @brief Pumps until the frame that answers the input just sent has arrived.
 *
 * An input that changed anything marks the game dirty, so the frame is owed
 * within one tick. Waiting for the sequence number to move is what makes the
 * next plan a plan about where the piece is now.
 *
 * The sequence to beat is passed in rather than read here, because the reply
 * to the request may already have carried the frame: net_request files a
 * STATE that crosses it, so a wait that sampled the sequence on entry would
 * be waiting for a second frame nothing owes it.
 *
 * @param net Client to pump.
 * @param seq The sequence number held before the input went out.
 * @param tries How many 2 ms attempts to make.
 * @return 1 once a newer snapshot is held, 0 otherwise.
 */
static int	settle_after(t_net_client *net, uint64_t seq, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state && net->state_snapshot.seq != seq)
			return (1);
		usleep(2000);
		tries--;
	}
	return (0);
}

/*
** A match ends; the room does not. Both players ask for the room they were
** already sitting in and are given the seats they never left, and the owner
** deals another match without either of them going back to the lobby to find
** the other again. Joining a room you are already in is the same request the
** client makes when it comes back from the results screen, which is why it is
** the one asserted here rather than a fresh LIST.
*/
static int	check_the_room_survives_the_match(t_net_client *owner,
			t_net_client *joiner, const char *room)
{
	owner->state = NET_IN_ROOM;
	joiner->state = NET_IN_ROOM;
	if (!join_room(owner, room) || !join_room(joiner, room))
		return (0);
	/*
	 * The last match's verdict is still the held snapshot, and it would
	 * satisfy wait_playing on its first look - no countdown, not counting
	 * down. Dropping it is what makes the wait below wait for the new deal
	 * rather than recognise the old one.
	 */
	owner->has_state = false;
	joiner->has_state = false;
	if (!start_room(owner, room) || !lock_in(owner, room)
		|| !lock_in(joiner, room))
		return (0);
	if (!wait_playing(owner, 900) || !wait_playing(joiner, 900))
		return (0);
	if (owner->state_snapshot.result != BODY_RESULT_NONE
		|| joiner->state_snapshot.result != BODY_RESULT_NONE)
		return (0);
	return (1);
}

/**
 * @brief Registers one account and binds this connection to it.
 *
 * @param net Connected client.
 * @param name Username to register.
 * @return 1 on success, 0 otherwise.
 */
static int	sign_up_and_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	if (net_signup(net, name, "hunter2", &result) != 0 || result.status != 201)
		return (0);
	if (net_login(net, name, "hunter2", &result) != 0 || result.status != 200)
		return (0);
	snprintf(net->username, sizeof(net->username), "%s", name);
	return (1);
}

/**
 * @brief Creates a Double room and binds the creator's play path to it.
 *
 * @param owner Client creating the room.
 * @param room Receives the room's name.
 * @param cap Size of room.
 * @return 1 on success, 0 otherwise.
 */
static int	open_room(t_net_client *owner, char *room, size_t cap)
{
	t_net_result	result;

	if (net_request(owner, "JOIN", TETRISU_ROUTE_ROOMS, "mode double\n",
			&result) != 0 || result.status != 201)
		return (0);
	if (net_result_field(&result, "room", room, cap) == NULL)
		return (0);
	owner->state = NET_IN_ROOM;
	return (net_match_join(owner, room) == 0);
}

/**
 * @brief Takes the second seat and binds that client's play path.
 *
 * @param net Client joining.
 * @param room The room's name.
 * @return 1 on success, 0 otherwise.
 */
static int	join_room(t_net_client *net, const char *room)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "JOIN", path, NULL, &result) != 0
		|| result.status != 200)
		return (0);
	net->state = NET_IN_ROOM;
	return (net_match_join(net, room) == 0);
}

/**
 * @brief Starts the match, as the room's owner.
 *
 * The request opens the character-select window rather than dealing, because
 * a room with an opponent in it reaches a match one way whichever route asked
 * for it. The boards arrive once both seats have locked in.
 *
 * @param net The owner's client.
 * @param room The room's name.
 * @return 1 on success, 0 otherwise.
 */
static int	start_room(t_net_client *net, const char *room)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "START", path, NULL, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

/**
 * @brief Declares ready and locks this client's equipped fighter in.
 *
 * The id comes from the player's own profile rather than from the catalogue,
 * because what is equipped is owned by definition - and an unowned id is the
 * one thing the route refuses.
 *
 * @param net Client seated in the room.
 * @param room The room's name.
 * @return 1 once the declaration was accepted, 0 otherwise.
 */
static int	lock_in(t_net_client *net, const char *room)
{
	t_body_profile	profile;
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			body[64];

	if (net_profile(net, &profile) != 0)
		return (0);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		profile.equipped_character);
	if (net_request(net, "READY", path, body, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

/**
 * @brief Pumps until a first snapshot has arrived.
 *
 * @param net Client to pump.
 * @param tries How many 5 ms attempts to make.
 * @return 1 once a snapshot is held, 0 otherwise.
 */
static int	settle_state(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state)
			return (1);
		usleep(5000);
		tries--;
	}
	return (0);
}

/**
 * @brief Pumps until the countdown has run out and play has begun.
 *
 * @param net Client to pump.
 * @param tries How many 5 ms attempts to make.
 * @return 1 once the held board is released, 0 otherwise.
 */
static int	wait_playing(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state && net->state_snapshot.countdown_ms == 0
			&& net->state_snapshot.phase != BODY_PHASE_COUNTDOWN)
			return (1);
		usleep(5000);
		tries--;
	}
	return (0);
}

/**
 * @brief Pumps until a snapshot carries a match verdict.
 *
 * @param net Client to pump.
 * @param tries How many 5 ms attempts to make.
 * @return 1 once a result has arrived, 0 otherwise.
 */
static int	wait_result(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net->has_state
			&& net->state_snapshot.result != BODY_RESULT_NONE)
			return (1);
		if (net_pump(net) < 0)
			return (0);
		usleep(5000);
		tries--;
	}
	return (0);
}

/**
 * @brief Prints one check's verdict and counts the failures.
 *
 * @param name What was checked.
 * @param ok Whether it held.
 * @param failures Running failure count.
 */
static void	report(const char *name, int ok, int *failures)
{
	if (ok)
		printf("PASS: %s\n", name);
	else
	{
		printf("FAIL: %s\n", name);
		(*failures)++;
	}
}
