/* ************************************************************************** */
/*                                                                            */
/*   test_double.c - two players, one room, one match                         */
/*                                                                            */
/*   Two harness clients take the two seats of a Double room and play it      */
/*   through: the room holds both boards still before it begins, refuses      */
/*   inputs while it does, and ends the moment one player is left - which is  */
/*   the difference between two games in a room and a match. Nothing is       */
/*   asserted that a client could not see for itself.                         */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_readiness_is_the_rooms_and_survives_a_refresh(void);
static void	test_both_declaring_ready_opens_the_select_window(void);
static void	test_withdrawing_in_the_window_does_not_deal_the_match(void);
static void	test_a_dealt_match_is_held_before_it_begins(void);
static void	test_the_hold_refuses_inputs(void);
static void	test_each_snapshot_carries_the_other_board(void);
static void	test_a_move_reaches_the_other_players_view(void);
static void	test_a_top_out_ends_the_match_for_both_players(void);
static void	test_both_players_are_recorded_once_each(void);
static void	test_an_unowned_fighter_is_refused(void);
static void	test_a_declared_fighter_is_the_matchs(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static t_item_id	starter_character(t_fixture *fx, t_player_id pid);
static t_item_id	unowned_character(t_fixture *fx, t_player_id pid);
static int		simple(t_harness *hc, const char *method, const char *path,
					const char *body);
static int		seat_two(t_fixture *fx, t_harness *amber, t_harness *blake,
					char *room, size_t cap);
static int		start_match(t_fixture *fx, t_harness *owner, t_harness *joiner,
					const char *room);
static int		list_room(t_harness *hc, const char *path, t_body_room *out);
static void		play_path(const t_harness *hc, const char *room, char *out,
					size_t cap);
static int		wait_phase(t_harness *hc, t_body_state *out, t_body_phase phase,
					int window_ms);
static int		wait_opponent_col(t_harness *hc, t_body_state *out, int col,
					int window_ms);
static int		wait_result(t_harness *hc, t_body_state *out, int window_ms);
static int		top_out(t_harness *hc, const char *path);
static void		nap(int ms);
static int		recorded(t_fixture *fx, t_player_id pid, bool won);

int	main(void)
{
	test_readiness_is_the_rooms_and_survives_a_refresh();
	test_both_declaring_ready_opens_the_select_window();
	test_withdrawing_in_the_window_does_not_deal_the_match();
	test_a_dealt_match_is_held_before_it_begins();
	test_the_hold_refuses_inputs();
	test_each_snapshot_carries_the_other_board();
	test_a_move_reaches_the_other_players_view();
	test_a_top_out_ends_the_match_for_both_players();
	test_both_players_are_recorded_once_each();
	test_an_unowned_fighter_is_refused();
	test_a_declared_fighter_is_the_matchs();
	return (0);
}

/*
** The bug this route exists for: a seat used to become READY the moment it was
** occupied, so readiness meant "somebody is sitting here" and the room had no
** way to say the other thing. A player who withdrew it saw their own client
** agree, and be overruled half a second later by the next refresh - because
** the server had never had an opinion to change.
*/
static void	test_readiness_is_the_rooms_and_survives_a_refresh(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	amber;
	t_harness	blake;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(!snapshot.members[0].ready && !snapshot.members[1].ready);
	assert(simple(&amber, "READY", path, "ready 1") == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.members[0].ready);
	assert(simple(&amber, "READY", path, "ready 0") == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(!snapshot.members[0].ready);
	assert(list_room(&blake, path, &snapshot) == 200);
	assert(!snapshot.members[0].ready);
	assert(simple(&amber, "READY", path, "ready yes") == 400);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_readiness_is_the_rooms_and_survives_a_refresh\n");
}

/*
** Both seats declaring is what commits a Double room, and the player who
** declares last is as often the joiner as the owner - so the start cannot be
** the owner's request. Nobody sends START here.
**
** What declaring no longer does is deal the boards. It opens the
** character-select window instead, and the boards are dealt when both seats
** have named a fighter - which is the same READY route carrying one. Locking
** in early is what ends the window early: neither player waits out the clock
** once there is nothing left to decide.
*/
static void	test_both_declaring_ready_opens_the_select_window(void)
{
	t_body_state	state;
	t_body_room		snapshot;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	t_item_id		pick;
	char			room[ROOM_NAME_MAX];
	char			path[64];
	char			body[64];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&amber, "READY", path, "ready 1") == 200);
	assert(hc_wait_state(&amber, &state, 300) != 0);
	assert(simple(&blake, "READY", path, "ready 1") == 200);
	/* committed, holding a clock, and no board dealt to either of them */
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.select_ms > 0);
	assert(snapshot.members[0].character == 0);
	assert(snapshot.members[1].character == 0);
	assert(hc_wait_state(&amber, &state, 300) != 0);
	pick = starter_character(&fx, amber.player_id);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n", (unsigned)pick);
	assert(simple(&amber, "READY", path, body) == 200);
	/* one of two locked in is not two: still no board */
	assert(hc_wait_state(&amber, &state, 300) != 0);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.members[0].character == (uint32_t)pick);
	pick = starter_character(&fx, blake.player_id);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n", (unsigned)pick);
	assert(simple(&blake, "READY", path, body) == 200);
	assert(wait_phase(&amber, &state, BODY_PHASE_COUNTDOWN,
			HC_TIMEOUT_MS) == 0);
	assert(wait_phase(&blake, &state, BODY_PHASE_COUNTDOWN,
			HC_TIMEOUT_MS) == 0);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_both_declaring_ready_opens_the_select_window\n");
}

/*
** Withdrawing during the window is a withdrawal, not a lock. The character
** rides on the same request as the readiness, and it used to be recorded
** whichever way that readiness pointed - so `ready 0` with a fighter named
** filled the last empty seat in the room's locked-in count and the match was
** dealt on the strength of the request that asked for the opposite. A seat
** that withdraws gives its fighter back with it.
*/
static void	test_withdrawing_in_the_window_does_not_deal_the_match(void)
{
	t_body_state	state;
	t_body_room		snapshot;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	t_item_id		pick;
	char			room[ROOM_NAME_MAX];
	char			path[64];
	char			body[64];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&amber, "READY", path, "ready 1") == 200);
	assert(simple(&blake, "READY", path, "ready 1") == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	pick = starter_character(&fx, amber.player_id);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n", (unsigned)pick);
	assert(simple(&amber, "READY", path, body) == 200);
	/* the other seat withdraws, naming a fighter as it goes */
	pick = starter_character(&fx, blake.player_id);
	snprintf(body, sizeof(body), "ready 0\ncharacter %u\n", (unsigned)pick);
	assert(simple(&blake, "READY", path, body) == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.members[1].character == 0);
	assert(!snapshot.members[1].ready);
	/* no board was dealt: the room is still holding its clock */
	assert(hc_wait_state(&amber, &state, 300) != 0);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_withdrawing_in_the_window_does_not_deal_the_match\n");
}

/*
** Two clients cannot agree on a starting moment between themselves: each
** reaches its match screen when its own navigation gets there, and PAUSE -
** which is how Solo freezes its board for a 3-2-1 - is refused in a room with
** anybody else in it, because one player stopping their own clock is an
** advantage. So the room holds every board at once and both players are told
** the same number.
*/
static void	test_a_dealt_match_is_held_before_it_begins(void)
{
	t_body_state	amber_state;
	t_body_state	blake_state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	assert(wait_phase(&amber, &amber_state, BODY_PHASE_COUNTDOWN,
			HC_TIMEOUT_MS) == 0);
	assert(wait_phase(&blake, &blake_state, BODY_PHASE_COUNTDOWN,
			HC_TIMEOUT_MS) == 0);
	assert(amber_state.countdown_ms > 0);
	assert(amber_state.countdown_ms <= TETRISD_MATCH_COUNTDOWN_MS);
	assert(blake_state.countdown_ms > 0);
	assert(wait_phase(&amber, &amber_state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(wait_phase(&blake, &blake_state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(amber_state.countdown_ms == 0);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_a_dealt_match_is_held_before_it_begins\n");
}

/*
** The held game is active and unpaused - that is what makes the hold the
** room's rather than the game's - so the game would accept an input on its
** own account. A player who kept hard-dropping through their own countdown
** would start the match with a stack already under them.
*/
static void	test_the_hold_refuses_inputs(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	play_path(&amber, room, path, sizeof(path));
	assert(wait_phase(&amber, &state, BODY_PHASE_COUNTDOWN,
			HC_TIMEOUT_MS) == 0);
	assert(simple(&amber, "DROP", path, "HARD") == 409);
	assert(simple(&amber, "MOVE", path, "LEFT") == 409);
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(state.score == 0);
	assert(simple(&amber, "MOVE", path, "LEFT") == 200);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_the_hold_refuses_inputs\n");
}

/*
** Each player's snapshot carries the other's board. It rides inside the
** recipient's own frame because a client holds one STATE mailbox slot and a
** second push would free the first - and one message makes the two boards the
** same instant, which two could not promise.
*/
static void	test_each_snapshot_carries_the_other_board(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	play_path(&blake, room, path, sizeof(path));
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(state.opponent_count == 1);
	assert(state.opponents[0].player_id == blake.player_id);
	assert(strcmp(state.opponents[0].username, "blake") == 0);
	assert(state.opponents[0].alive);
	assert(wait_phase(&blake, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(state.opponent_count == 1);
	assert(state.opponents[0].player_id == amber.player_id);
	assert(strcmp(state.opponents[0].username, "amber") == 0);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_each_snapshot_carries_the_other_board\n");
}

/*
** A move by one player is a frame owed to both, because both frames now carry
** both boards. Marking only the player who acted would leave the other
** watching a board that froze whenever they stopped playing themselves.
*/
static void	test_a_move_reaches_the_other_players_view(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];
	int				before;

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	play_path(&blake, room, path, sizeof(path));
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	before = state.opponents[0].piece.col;
	assert(simple(&blake, "MOVE", path, "LEFT") == 200);
	assert(wait_opponent_col(&amber, &state, before - 1, HC_TIMEOUT_MS) == 0);
	assert(state.opponents[0].piece.col == before - 1);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_a_move_reaches_the_other_players_view\n");
}

/*
** The match is over when one player is left standing, not when the last one
** stops. The winner's board says nothing about it - it is active, with a
** piece on it, exactly like a board mid-match - so the verdict has to arrive
** as its own field or the survivor is never told.
*/
static void	test_a_top_out_ends_the_match_for_both_players(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	play_path(&amber, room, path, sizeof(path));
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(top_out(&amber, path) == 0);
	assert(wait_result(&amber, &state, HC_TIMEOUT_MS) == 0);
	assert(state.result == BODY_RESULT_LOST);
	assert(state.rank == 2);
	assert(state.phase == BODY_PHASE_TOP_OUT);
	assert(wait_result(&blake, &state, HC_TIMEOUT_MS) == 0);
	assert(state.result == BODY_RESULT_WON);
	assert(state.rank == 1);
	assert(state.phase != BODY_PHASE_TOP_OUT);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_a_top_out_ends_the_match_for_both_players\n");
}

/*
** UC-11's DB mapping: each player is persisted once, the winner with
** won=true. The loser is the one who topped out; the winner is credited for
** surviving rather than for anything their own board did, which is why "did
** not top out" is not the question asked at the recording site either.
*/
static void	test_both_players_are_recorded_once_each(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];
	char			other[96];

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	assert(start_match(&fx, &amber, &blake, room) == 200);
	play_path(&amber, room, path, sizeof(path));
	play_path(&blake, room, other, sizeof(other));
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	assert(simple(&blake, "DROP", other, "HARD") == 200);
	assert(top_out(&amber, path) == 0);
	assert(wait_result(&blake, &state, HC_TIMEOUT_MS) == 0);
	nap(200);
	hc_close(&amber);
	hc_close(&blake);
	server_stop(fx.srv);
	fx.srv = NULL;
	assert(recorded(&fx, amber.player_id, false) == 0);
	assert(recorded(&fx, blake.player_id, true) == 0);
	fx_stop(&fx);
	printf("PASS test_both_players_are_recorded_once_each\n");
}

/**
 * @brief Signs one player up and logs them in on their own connection.
 *
 * @param fx Running fixture server.
 * @param hc Harness client to bring up.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
static int	player(t_fixture *fx, t_harness *hc, const char *name)
{
	if (hc_connect(hc, fx) != 0)
		return (-1);
	if (hc_signup(hc, name, "hunter2") != 201)
		return (-1);
	if (hc_login(hc, name, "hunter2") != 200)
		return (-1);
	return (0);
}

/**
 * @brief Sends one request and reports only its status.
 *
 * @param hc Connected harness client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 when nothing came back.
 */
static int	simple(t_harness *hc, const char *method, const char *path,
			const char *body)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, method, path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Brings up two players and seats them both in one Double room.
 *
 * @param fx Running fixture server.
 * @param amber Receives the owner's connection.
 * @param blake Receives the joiner's connection.
 * @param room Receives the room's name.
 * @param cap Size of room.
 * @return 0 when both are seated, -1 otherwise.
 */
static int	seat_two(t_fixture *fx, t_harness *amber, t_harness *blake,
			char *room, size_t cap)
{
	char	path[64];

	if (player(fx, amber, "amber") != 0 || player(fx, blake, "blake") != 0)
		return (-1);
	if (hc_join_new(amber, "double", room, cap) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	if (simple(blake, "JOIN", path, NULL) != 200)
		return (-1);
	return (0);
}

/**
 * @brief Starts the room as its owner, and takes it all the way to a match.
 *
 * The owner's start no longer deals - it opens the character-select window,
 * exactly as the last readiness does, so that a match is reached the same way
 * whichever of the two routes asked for it. Locking both seats in is what
 * closes that window without waiting out its clock, which is why every test
 * that just wants boards on the wire goes through here.
 *
 * @param fx Running fixture, for the characters each player owns.
 * @param owner The connection that created the room.
 * @param joiner The other seat.
 * @param room The room's name.
 * @return 200 when the match was dealt, otherwise the first refusal.
 */
static int	start_match(t_fixture *fx, t_harness *owner, t_harness *joiner,
			const char *room)
{
	char	path[64];
	int		status;

	snprintf(path, sizeof(path), "/room/%s", room);
	status = simple(owner, "START", path, NULL);
	if (status != 200)
		return (status);
	status = hc_lock_in(owner, fx, path);
	if (status != 200)
		return (status);
	return (hc_lock_in(joiner, fx, path));
}

/**
 * @brief Reads one room's authoritative snapshot.
 *
 * @param hc Connected client, seated in that room.
 * @param path The /room/<name> path.
 * @param out Receives the decoded snapshot.
 * @return The status code the server answered.
 */
static int	list_room(t_harness *hc, const char *path, t_body_room *out)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, "LIST", path, NULL, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	if (status == 200 && resp.body != NULL)
		body_room_decode((const char *)resp.body, resp.body_len, out);
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Builds the path one player's own inputs are addressed to.
 *
 * @param hc The connection whose player is the subject.
 * @param room The room being played in.
 * @param out Receives the path.
 * @param cap Size of out.
 */
static void	play_path(const t_harness *hc, const char *room, char *out,
			size_t cap)
{
	snprintf(out, cap, "/room/%s/player/%llu", room,
		(unsigned long long)hc->player_id);
}

/**
 * @brief Reads snapshots until one arrives in the phase being waited for.
 *
 * @param hc Harness client to read.
 * @param out Receives the matching snapshot.
 * @param phase The phase to wait for.
 * @param window_ms How long to keep reading for.
 * @return 0 when the phase arrived, -1 when it did not.
 */
static int	wait_phase(t_harness *hc, t_body_state *out, t_body_phase phase,
			int window_ms)
{
	t_body_state	snap;

	while (hc_wait_state(hc, &snap, window_ms) == 0)
	{
		if (snap.phase == phase)
		{
			*out = snap;
			return (0);
		}
	}
	return (-1);
}

/**
 * @brief Reads snapshots until the opponent's piece reaches a column.
 *
 * Waiting for "the next frame" would not do: the tick that carries the move
 * is not necessarily the next one to arrive, and gravity produces frames of
 * its own in between.
 *
 * @param hc Harness client to read.
 * @param out Receives the matching snapshot.
 * @param col The column the opponent's piece is expected to reach.
 * @param window_ms How long to keep reading for.
 * @return 0 when the opponent's piece got there, -1 when it did not.
 */
static int	wait_opponent_col(t_harness *hc, t_body_state *out, int col,
			int window_ms)
{
	t_body_state	snap;

	while (hc_wait_state(hc, &snap, window_ms) == 0)
	{
		if (snap.opponent_count == 1 && snap.opponents[0].piece.col == col)
		{
			*out = snap;
			return (0);
		}
	}
	return (-1);
}

/**
 * @brief Finds the snapshot carrying a match verdict, wherever it was read.
 *
 * The already-filed one is checked first, because the frame that ends a match
 * is the one most likely to have crossed a request: the player who tops out
 * does so on the last of a run of drops, and hc_request files every snapshot
 * it reads while waiting for its own response. Reading only the socket looked
 * for a frame that had already been delivered - the same trap the real client
 * keeps `applied_seq` for.
 *
 * @param hc Harness client to read.
 * @param out Receives the snapshot carrying the result.
 * @param window_ms How long to keep reading for.
 * @return 0 when a result arrived, -1 when none did.
 */
static int	wait_result(t_harness *hc, t_body_state *out, int window_ms)
{
	t_body_state	snap;

	if (hc->has_state && hc->last_state.result != BODY_RESULT_NONE)
	{
		*out = hc->last_state;
		return (0);
	}
	while (hc_wait_state(hc, &snap, window_ms) == 0)
	{
		if (snap.result != BODY_RESULT_NONE)
		{
			*out = snap;
			return (0);
		}
	}
	return (-1);
}

/**
 * @brief Hard-drops until the stack reaches the ceiling.
 *
 * The refusal is what says the game has ended: an input into a game that is
 * no longer active is answered 409, so the loop ends on the server's word
 * rather than on a count of pieces.
 *
 * @param hc The connection playing.
 * @param path That player's own input path.
 * @return 0 once the game has ended, -1 when it never did.
 */
static int	top_out(t_harness *hc, const char *path)
{
	int	status;
	int	guard;

	status = 200;
	guard = 0;
	while (status == 200 && guard < 300)
	{
		status = simple(hc, "DROP", path, "HARD");
		guard++;
	}
	if (status != 409)
		return (-1);
	return (0);
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

/**
 * @brief Checks one player was recorded exactly once, with the expected
 *        verdict.
 *
 * The store is read directly rather than over the wire because nothing the
 * protocol answers with carries the win: LOGIN and PROFILE both report the
 * score, and a winner who never scored would be indistinguishable from a
 * player who was never recorded at all. The server is stopped before this
 * runs, so the store is closed and there is no question of reading it
 * mid-write.
 *
 * The player is looked up by id rather than by logging in, because tetrisd
 * salts and hashes a password before the store ever sees it - db_login with
 * the plaintext the test signed up with would simply not match.
 *
 * @param fx Fixture whose store is read; its server must already be stopped.
 * @param pid The player to look up.
 * @param won Whether this player is expected to have won.
 * @return 0 when the player was recorded once with that verdict, -1 otherwise.
 */
static int	recorded(t_fixture *fx, t_player_id pid, bool won)
{
	t_player	found;
	t_db		*db;
	int			ok;

	if (db_open(fx->cfg.data_dir, fx->cfg.config_dir, &db) != DB_OK)
		return (-1);
	ok = -1;
	if (db_get_player(db, pid, &found) == DB_OK
		&& found.games_played == 1
		&& found.games_won == (uint32_t)(won ? 1 : 0)
		&& found.leaderboard_score > 0)
		ok = 0;
	db_close(db);
	return (ok);
}

/*
** A character nobody bought buys nobody a roster. db_player_owns_character
** answers a predicate, so it is DB_TRUE that means owned - testing it against
** DB_OK would read every honest "no" as a database failure and let the
** declaration through.
**
** The refusal also has to leave the seat alone: a Double room starts itself
** once every seat has declared, so marking a seat ready on the way to
** answering 403 would start a match the player never agreed to.
*/
static void	test_an_unowned_fighter_is_refused(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	amber;
	t_harness	blake;
	char		room[ROOM_NAME_MAX];
	char		path[64];
	char		body[64];
	t_item_id	stranger;

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	stranger = unowned_character(&fx, amber.player_id);
	assert(stranger != 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		(unsigned)stranger);
	assert(simple(&amber, "READY", path, body) == 403);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.member_count == 2 && !snapshot.members[0].ready);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_an_unowned_fighter_is_refused\n");
}

/*
** A character the player owns is accepted, declared readiness stands, and the
** match starts on it. The point of carrying it here rather than reading the
** account's equipped one is that it is fixed for the match: deal_games copies
** it onto the game, so nothing done to the account afterwards can change
** which four abilities a level selects from.
**
** It is declared inside the select window and not before it. Opening the
** window clears every seat's choice on purpose - that is what stops a room
** from opening its next window with everyone already locked in from the last
** match - so a fighter named before both players had even committed would be
** thrown away, and the seat would be dealt whatever the account has equipped.
*/
static void	test_a_declared_fighter_is_the_matchs(void)
{
	t_body_state	state;
	t_body_room		snapshot;
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[64];
	char			body[64];
	t_item_id		owned;

	assert(fx_start(&fx) == 0);
	assert(seat_two(&fx, &amber, &blake, room, sizeof(room)) == 0);
	owned = starter_character(&fx, amber.player_id);
	assert(owned != 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&amber, "READY", path, "ready 1\n") == 200);
	assert(simple(&blake, "READY", path, "ready 1\n") == 200);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n", (unsigned)owned);
	assert(simple(&amber, "READY", path, body) == 200);
	owned = starter_character(&fx, blake.player_id);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n", (unsigned)owned);
	assert(simple(&blake, "READY", path, body) == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.members[0].character != 0);
	assert(wait_phase(&amber, &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) == 0);
	hc_close(&amber);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_a_declared_fighter_is_the_matchs\n");
}

/**
 * @brief The character a fresh account already owns.
 *
 * Read from the catalogue rather than written down, because ids carry gaps -
 * they live in players' owned lists and are never renumbered - so a literal
 * here would be a guess about config/characters.cfg.
 *
 * @param fx Running fixture, whose store is reopened read-only.
 * @param pid The player to ask about.
 * @return An owned character id, or 0 when the player owns none.
 */
static t_item_id	starter_character(t_fixture *fx, t_player_id pid)
{
	t_character	roster[BODY_CATALOGUE_MAX];
	t_db		*db;
	t_item_id	found;
	size_t		count;
	size_t		i;

	if (db_open(fx->cfg.data_dir, fx->cfg.config_dir, &db) != DB_OK)
		return (0);
	count = 0;
	if (db_characters(db, roster, BODY_CATALOGUE_MAX, &count) != DB_OK)
		count = 0;
	found = 0;
	i = 0;
	while (i < count && found == 0)
	{
		if (db_player_owns_character(db, pid, roster[i].character_id)
			== DB_TRUE)
			found = roster[i].character_id;
		i++;
	}
	db_close(db);
	return (found);
}

/**
 * @brief A character in the catalogue that this player does not own.
 *
 * @param fx Running fixture, whose store is reopened read-only.
 * @param pid The player to ask about.
 * @return An unowned character id, or 0 when the player owns them all.
 */
static t_item_id	unowned_character(t_fixture *fx, t_player_id pid)
{
	t_character	roster[BODY_CATALOGUE_MAX];
	t_db		*db;
	t_item_id	found;
	size_t		count;
	size_t		i;

	if (db_open(fx->cfg.data_dir, fx->cfg.config_dir, &db) != DB_OK)
		return (0);
	count = 0;
	if (db_characters(db, roster, BODY_CATALOGUE_MAX, &count) != DB_OK)
		count = 0;
	found = 0;
	i = 0;
	while (i < count && found == 0)
	{
		if (db_player_owns_character(db, pid, roster[i].character_id)
			== DB_FALSE)
			found = roster[i].character_id;
		i++;
	}
	db_close(db);
	return (found);
}
