/* ************************************************************************** */
/*                                                                            */
/*   test_leaderboard.c - the ranking a finished game writes into             */
/*                                                                            */
/*   tetrisd has recorded every finished and forfeited game since M1, but     */
/*   nothing could read the result back, so a player's score went into the    */
/*   store and out of sight. LEADERBOARD /leaderboard is the                  */
/*   route that answers, and this is what has to be true of it: it is         */
/*   authenticated, registering is what puts a player on the board rather     */
/*   than winning, and a game that ended appears in it, in rank order.        */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_a_fresh_account_ranks_with_no_score(void);
static void	test_an_unauthenticated_read_is_refused(void);
static void	test_another_path_is_not_found(void);
static void	test_a_finished_game_reaches_the_board(void);
static void	test_higher_scores_rank_first(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static size_t	read_board(t_harness *hc, t_body_leaderboard_row *rows,
					size_t cap);
static int		play_and_leave(t_fixture *fx, t_harness *hc, const char *name,
					int drops);

int	main(void)
{
	test_a_fresh_account_ranks_with_no_score();
	test_an_unauthenticated_read_is_refused();
	test_another_path_is_not_found();
	test_a_finished_game_reaches_the_board();
	test_higher_scores_rank_first();
	return (0);
}

/*
** Registering is what puts a player on the board, not winning: the store
** indexes every account by (score, id) from signup, so a player who has
** finished nothing is ranked last with nought points rather than absent.
** Filtering those out here would give the route a different idea of rank
** than db_rank has.
*/
static void	test_a_fresh_account_ranks_with_no_score(void)
{
	t_body_leaderboard_row	rows[TETRISD_LEADERBOARD_ROWS];
	t_fixture				fx;
	t_harness				hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(read_board(&hc, rows, TETRISD_LEADERBOARD_ROWS) == 1);
	assert(rows[0].rank == 1);
	assert(strcmp(rows[0].username, "amber") == 0);
	assert(rows[0].score == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_fresh_account_ranks_with_no_score\n");
}

/*
** The ranking is not a secret, but a connection that has not said who it is
** has no business asking tetrisd for anything.
*/
static void	test_an_unauthenticated_read_is_refused(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_request(&hc, "LEADERBOARD", TETRISD_ROUTE_LEADERBOARD, NULL,
			&resp) == 0);
	assert(resp.status_code == 401);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_unauthenticated_read_is_refused\n");
}

/*
** The method does not name the resource on its own: a LEADERBOARD sent at
** some other path is asking for something that is not there.
*/
static void	test_another_path_is_not_found(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_request(&hc, "LEADERBOARD", "/rooms", NULL, &resp) == 0);
	assert(resp.status_code == 404);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_another_path_is_not_found\n");
}

/*
** The end-to-end claim: play a Single game, leave it, and the score that
** is recorded on the spot is the score the board reports.
*/
static void	test_a_finished_game_reaches_the_board(void)
{
	t_body_leaderboard_row	rows[TETRISD_LEADERBOARD_ROWS];
	t_fixture				fx;
	t_harness				hc;

	assert(fx_start(&fx) == 0);
	assert(play_and_leave(&fx, &hc, "amber", 3) == 0);
	assert(player(&fx, &hc, "wren") == 0);
	assert(read_board(&hc, rows, TETRISD_LEADERBOARD_ROWS) == 2);
	assert(rows[0].rank == 1);
	assert(strcmp(rows[0].username, "amber") == 0);
	assert(rows[0].score > 0);
	assert(rows[1].rank == 2);
	assert(strcmp(rows[1].username, "wren") == 0);
	assert(rows[1].score == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_finished_game_reaches_the_board\n");
}

/*
** Rank is the position in the answer, and the answer is ordered by score.
** More hard drops is more score, so the busier player comes first.
*/
static void	test_higher_scores_rank_first(void)
{
	t_body_leaderboard_row	rows[TETRISD_LEADERBOARD_ROWS];
	t_fixture				fx;
	t_harness				hc;

	assert(fx_start(&fx) == 0);
	assert(play_and_leave(&fx, &hc, "amber", 1) == 0);
	assert(play_and_leave(&fx, &hc, "wren", 6) == 0);
	assert(player(&fx, &hc, "juno") == 0);
	assert(read_board(&hc, rows, TETRISD_LEADERBOARD_ROWS) == 3);
	assert(rows[0].rank == 1 && strcmp(rows[0].username, "wren") == 0);
	assert(rows[1].rank == 2 && strcmp(rows[1].username, "amber") == 0);
	assert(rows[2].rank == 3 && strcmp(rows[2].username, "juno") == 0);
	assert(rows[0].score > rows[1].score);
	assert(rows[2].score == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_higher_scores_rank_first\n");
}

/**
 * @brief Connects, registers and signs in one throwaway player.
 *
 * @param fx Running fixture.
 * @param hc Harness to bring up.
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
 * @brief Reads the leaderboard and decodes the rows it answered with.
 *
 * @param hc Signed-in harness.
 * @param rows Receives the decoded rows.
 * @param cap Capacity of rows.
 * @return Number of rows read.
 */
static size_t	read_board(t_harness *hc, t_body_leaderboard_row *rows,
			size_t cap)
{
	t_htttp_message	resp;
	size_t			count;

	count = 0;
	if (hc_request(hc, "LEADERBOARD", TETRISD_ROUTE_LEADERBOARD, NULL,
			&resp) != 0)
		return (0);
	assert(resp.status_code == 200);
	if (resp.body != NULL && resp.body_len > 0)
		assert(body_leaderboard_decode((const char *)resp.body,
				resp.body_len, rows, cap, &count) == 0);
	htttp_message_free(&resp);
	return (count);
}

/**
 * @brief Plays a short Single game as one player and forfeits it.
 *
 * Leaving is one of the three ways a game ends and the quickest
 * to drive: the score is recorded on the spot, which is the only part the
 * leaderboard cares about. The harness is closed before returning, so the
 * next player gets a connection of their own.
 *
 * @param fx Running fixture.
 * @param hc Harness to use and close.
 * @param name Username to play as.
 * @param drops How many pieces to hard drop before leaving.
 * @return 0 on success, -1 otherwise.
 */
static int	play_and_leave(t_fixture *fx, t_harness *hc, const char *name,
			int drops)
{
	t_htttp_message	resp;
	char			room[ROOM_NAME_MAX];
	char			path[256];
	int				i;

	if (player(fx, hc, name) != 0)
		return (-1);
	if (hc_join_new(hc, "single", room, sizeof(room)) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	if (hc_request(hc, "START", path, NULL, &resp) != 0)
		return (-1);
	htttp_message_free(&resp);
	snprintf(path, sizeof(path), "/room/%s/player/%llu", room,
		(unsigned long long)hc->player_id);
	i = 0;
	while (i < drops)
	{
		if (hc_request(hc, "DROP", path, "HARD\n", &resp) != 0)
			return (-1);
		htttp_message_free(&resp);
		i++;
	}
	snprintf(path, sizeof(path), "/room/%s", room);
	if (hc_request(hc, "LEAVE", path, NULL, &resp) != 0)
		return (-1);
	htttp_message_free(&resp);
	hc_close(hc);
	return (0);
}
