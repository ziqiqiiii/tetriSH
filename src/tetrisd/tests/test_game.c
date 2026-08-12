/* ************************************************************************** */
/*                                                                            */
/*   test_game.c - Single mode, end to end                                    */
/*                                                                            */
/*   A player signs up, logs in, takes a room, starts a game, and plays it:   */
/*   pieces move under their inputs, gravity pulls without them, the game     */
/*   ends when the stack reaches the ceiling, and the result is recorded.     */
/*   Nothing is asserted that a client could not see for itself.              */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_starting_a_game_pushes_the_first_state(void);
static void	test_inputs_move_the_piece(void);
static void	test_hard_drop_locks_a_piece(void);
static void	test_gravity_falls_without_input(void);
static void	test_inputs_for_another_player_are_refused(void);
static void	test_topping_out_ends_and_records_the_game(void);
static void	test_a_disconnect_ends_the_match_and_the_survivor_wins(void);
static void	test_a_finished_game_frees_the_player(void);
static void	test_an_input_flood_is_rate_limited(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static int		simple(t_harness *hc, const char *method, const char *path, const char *body);
static int		start_single(t_fixture *fx, t_harness *hc, char *play_path, size_t cap);
static int		latest_state(t_harness *hc, t_body_state *out, int window_ms);
static int		wait_result(t_harness *hc, t_body_state *out, int window_ms);
static void		nap(int ms);
static int64_t	login_score(t_fixture *fx, const char *name);

int	main(void)
{
	test_starting_a_game_pushes_the_first_state();
	test_inputs_move_the_piece();
	test_hard_drop_locks_a_piece();
	test_gravity_falls_without_input();
	test_inputs_for_another_player_are_refused();
	test_topping_out_ends_and_records_the_game();
	test_a_disconnect_ends_the_match_and_the_survivor_wins();
	test_a_finished_game_frees_the_player();
	test_an_input_flood_is_rate_limited();
	return (0);
}

/*
** A player cannot send inputs faster than a person can press keys, and a
** client that tries is told to slow down rather than being allowed to spend
** the tick on it. The bucket is configured tiny here so the refusal
** is deterministic; the shipped defaults sit far above human play.
*/
static void	test_an_input_flood_is_rate_limited(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	char			play[96];
	int				status;
	int				i;

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.cfg.input_burst = 3;
	fx.cfg.input_rate = 1;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	status = 200;
	i = 0;
	while (i < 12 && status != 429)
	{
		status = simple(&hc, "MOVE", play, "LEFT");
		i++;
	}
	assert(status == 429);
	assert(hc_request(&hc, "MOVE", play, "LEFT", &resp) == 0);
	assert(resp.status_code == 429);
	assert(htttp_message_get_header(&resp, "Retry-After") != NULL);
	htttp_message_free(&resp);
	assert(simple(&hc, "LIST", TETRISD_ROUTE_ROOMS, NULL) == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_input_flood_is_rate_limited\n");
}

static void	test_starting_a_game_pushes_the_first_state(void)
{
	t_body_state	state;
	t_fixture	fx;
	t_harness	hc;
	char		play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	assert(hc_wait_state(&hc, &state, HC_TIMEOUT_MS) == 0);
	assert(state.phase == BODY_PHASE_ACTIVE);
	assert(state.score == 0);
	assert(state.lines == 0);
	assert(state.level == 1);
	assert(state.piece.type >= 0 && state.piece.type <= 6);
	assert(state.next[0] >= 0 && state.next[0] <= 6);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_starting_a_game_pushes_the_first_state\n");
}

static void	test_inputs_move_the_piece(void)
{
	t_body_state	before;
	t_body_state	after;
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	int			status;
	int			guard;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	assert(latest_state(&hc, &before, 200) == 0);
	assert(simple(&hc, "MOVE", play, "LEFT") == 200);
	assert(latest_state(&hc, &after, 200) == 0);
	assert(after.piece.col < before.piece.col);
	status = 200;
	guard = 0;
	while (status == 200 && guard < 20)
	{
		status = simple(&hc, "MOVE", play, "LEFT");
		guard++;
	}
	assert(status == 409);
	assert(simple(&hc, "MOVE", play, "SIDEWAYS") == 400);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_inputs_move_the_piece\n");
}

static void	test_hard_drop_locks_a_piece(void)
{
	t_body_state	state;
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	int			filled;
	int			col;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "ROTATE", play, "CW") == 200);
	assert(simple(&hc, "DROP", play, "HARD") == 200);
	assert(latest_state(&hc, &state, 200) == 0);
	filled = 0;
	col = 0;
	while (col < BODY_BOARD_COLS)
	{
		if (state.cells[BODY_BOARD_ROWS - 1][col].type != 0)
			filled++;
		col++;
	}
	assert(filled > 0);
	assert(state.score > 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_hard_drop_locks_a_piece\n");
}

static void	test_gravity_falls_without_input(void)
{
	t_body_state	before;
	t_body_state	after;
	t_fixture	fx;
	t_harness	hc;
	char		play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	assert(latest_state(&hc, &before, 200) == 0);
	assert(latest_state(&hc, &after, 2500) == 0);
	assert(after.piece.row > before.piece.row || after.seq > before.seq);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_gravity_falls_without_input\n");
}

static void	test_inputs_for_another_player_are_refused(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	char		forged[96];
	char		room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	snprintf(room, sizeof(room), "S-01");
	snprintf(forged, sizeof(forged), "/room/%s/player/%llu", room, (unsigned long long)(hc.player_id + 1));
	assert(simple(&hc, "MOVE", forged, "LEFT") == 403);
	snprintf(forged, sizeof(forged), "/room/S-42/player/%llu", (unsigned long long)hc.player_id);
	assert(simple(&hc, "MOVE", forged, "LEFT") == 409);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_inputs_for_another_player_are_refused\n");
}

static void	test_topping_out_ends_and_records_the_game(void)
{
	t_body_state	state;
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	int			status;
	int			guard;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	status = 200;
	guard = 0;
	while (status == 200 && guard < 200)
	{
		status = simple(&hc, "DROP", play, "HARD");
		guard++;
	}
	assert(status == 409);
	assert(latest_state(&hc, &state, 300) == 0);
	assert(state.phase == BODY_PHASE_TOP_OUT);
	nap(200);
	hc_close(&hc);
	assert(login_score(&fx, "amber") > 0);
	fx_stop(&fx);
	printf("PASS test_topping_out_ends_and_records_the_game\n");
}

/*
** UC-11 5a: a Double match with one player left in it is over, and the player
** left in it has won. This used to assert the opposite - that the room played
** on - which was the honest description of a server that had no idea two games
** in one room were being played against each other: it ticked the survivor's
** board alone until they too were starved out, and then recorded two losses.
**
** The first frame is the countdown, because a dealt match is held still before
** it begins. The last one carries a result, because nothing about the winner's
** board says they won: it is active, with a piece on it, exactly like a board
** in the middle of a match.
*/
static void	test_a_disconnect_ends_the_match_and_the_survivor_wins(void)
{
	t_body_state	state;
	t_fixture	fx;
	t_harness	amber;
	t_harness	blake;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(player(&fx, &blake, "blake") == 0);
	assert(hc_join_new(&amber, "double", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&blake, "JOIN", path, NULL) == 200);
	/*
	 * The owner's start opens the select window rather than dealing, so the
	 * match this test needs is the one both players lock themselves into.
	 */
	assert(simple(&amber, "START", path, NULL) == 200);
	assert(hc_lock_in(&amber, &fx, path) == 200);
	assert(hc_lock_in(&blake, &fx, path) == 200);
	assert(hc_wait_state(&blake, &state, HC_TIMEOUT_MS) == 0);
	assert(state.phase == BODY_PHASE_COUNTDOWN);
	assert(state.countdown_ms > 0);
	hc_close(&amber);
	assert(wait_result(&blake, &state, HC_TIMEOUT_MS) == 0);
	assert(state.result == BODY_RESULT_WON);
	assert(state.rank == 1);
	assert(state.phase != BODY_PHASE_TOP_OUT);
	hc_close(&blake);
	fx_stop(&fx);
	printf("PASS test_a_disconnect_ends_the_match_and_the_survivor_wins\n");
}

static void	test_a_finished_game_frees_the_player(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	char		room[ROOM_NAME_MAX];
	int			status;
	int			guard;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(start_single(&fx, &hc, play, sizeof(play)) == 0);
	status = 200;
	guard = 0;
	while (status == 200 && guard < 200)
	{
		status = simple(&hc, "DROP", play, "HARD");
		guard++;
	}
	assert(status == 409);
	nap(300);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	assert(room[0] != '\0');
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_finished_game_frees_the_player\n");
}

/**
 * @brief Connects, registers, and logs in one player.
 *
 * @param fx Running fixture.
 * @param hc Client to bring up.
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
 * @brief Sends a request and returns only its status code.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 when the exchange failed.
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
 * @brief Takes a Single room, starts it, and builds the input path to use.
 *
 * @param fx Running fixture (unused beyond context).
 * @param hc Logged-in client.
 * @param play_path Receives /room/<name>/player/<pid>.
 * @param cap Size of play_path.
 * @return 0 on success, -1 otherwise.
 */
static int	start_single(t_fixture *fx, t_harness *hc, char *play_path,
			size_t cap)
{
	char	room[ROOM_NAME_MAX];
	char	path[64];

	(void)fx;
	if (hc_join_new(hc, "single", room, sizeof(room)) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	if (simple(hc, "START", path, NULL) != 200)
		return (-1);
	snprintf(play_path, cap, "/room/%s/player/%llu", room,
		(unsigned long long)hc->player_id);
	return (0);
}

/**
 * @brief Collects STATE pushes for a while and keeps the last one.
 *
 * Snapshots are latest-wins on the wire too, so a test that wants "where the
 * game is now" waits out the window rather than trusting the first frame, and
 * falls back to the last snapshot the harness saw while sending requests.
 *
 * @param hc Connected client.
 * @param out Receives the most recent snapshot seen.
 * @param window_ms How long to keep collecting.
 * @return 0 when at least one snapshot arrived, -1 otherwise.
 */
static int	latest_state(t_harness *hc, t_body_state *out, int window_ms)
{
	t_body_state	snap;
	int			seen;

	seen = 0;
	while (hc_wait_state(hc, &snap, window_ms) == 0)
	{
		*out = snap;
		seen = 1;
		window_ms = 60;
	}
	if (seen == 0 && hc->has_state)
	{
		*out = hc->last_state;
		seen = 1;
	}
	if (seen == 0)
		return (-1);
	return (0);
}

/**
 * @brief Waits for the snapshot that carries a match result.
 *
 * A match ends on a tick, and the frames before that tick are ordinary play,
 * so the interesting one is not simply the next one or the last one - it is
 * the first that has a verdict on it.
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
 * @brief Logs in on a fresh connection and reports the stored score.
 *
 * @param fx Running fixture.
 * @param name Username to log in as.
 * @return The player's leaderboard score, or -1 when the login failed.
 */
static int64_t	login_score(t_fixture *fx, const char *name)
{
	t_htttp_message	resp;
	t_harness		hc;
	char			body[128];
	char			text[TETRISD_BODY_MAX_BYTES];
	const char		*score;
	int64_t			value;

	if (hc_connect(&hc, fx) != 0)
		return (-1);
	snprintf(body, sizeof(body), "username %s\npassword hunter2\n", name);
	value = -1;
	if (hc_request(&hc, "LOGIN", TETRISD_ROUTE_SESSION, body, &resp) == 0)
	{
		if (resp.status_code == 200 && resp.body_len < sizeof(text))
		{
			memcpy(text, resp.body, resp.body_len);
			text[resp.body_len] = '\0';
			score = strstr(text, "score ");
			if (score != NULL)
				value = (int64_t)strtoll(score + 6, NULL, 10);
		}
		htttp_message_free(&resp);
	}
	hc_close(&hc);
	return (value);
}
