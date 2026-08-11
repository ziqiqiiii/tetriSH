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

// Static Functions
static int	check_both_are_dealt_and_held(t_net_client *owner,
				t_net_client *joiner);
static int	check_each_sees_the_other(t_net_client *owner,
				t_net_client *joiner);
static int	check_a_move_crosses(t_net_client *owner, t_net_client *joiner);
static int	check_the_joiner_may_play(t_net_client *joiner);
static int	check_a_top_out_decides_the_match(t_net_client *owner,
				t_net_client *joiner);

static int	sign_up_and_in(t_net_client *net, const char *name);
static int	open_room(t_net_client *owner, char *room, size_t cap);
static int	join_room(t_net_client *net, const char *room);
static int	start_room(t_net_client *net, const char *room);
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
		|| !start_room(&owner, room))
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
	report("a top-out decides the match for both",
		check_a_top_out_decides_the_match(&owner, &joiner), &failures);
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
