/* ************************************************************************** */
/*                                                                            */
/*   test_bot_pool.c - the accounts a player's bots log in as                 */
/*                                                                            */
/*   A bot is an ordinary client, so almost nothing here is about bots: it is */
/*   about the two rules that make an ordinary client safe to hand out. The   */
/*   accounts exist without anybody creating them, a person cannot take one,  */
/*   and - the one that is not obvious - a second bot cannot take one that is */
/*   already connected.                                                       */
/*                                                                            */
/*   That last rule is inverted from a person's. A player's second LOGIN      */
/*   displaces the first, because a player knows they logged in twice and     */
/*   wants the session in front of them. Two bots are two different players'  */
/*   rooms, and displacing would throw a stranger out of a match in progress. */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_the_pool_exists_without_anybody_making_it(void);
static void	test_a_person_cannot_sign_up_as_a_bot(void);
static void	test_a_taken_account_is_refused_not_displaced(void);
static void	test_a_person_is_still_displaced(void);
static void	test_a_bot_never_reaches_the_leaderboard(void);

static void		bot_password(int index, char *out, size_t cap);
static int		bot_login(t_harness *hc, int index);
static int		still_authed(t_harness *hc);
static int		play_and_leave(t_fixture *fx, t_harness *hc, int drops);
static size_t	read_board(t_harness *hc, t_body_leaderboard_row *rows,
					size_t cap);

int	main(void)
{
	test_the_pool_exists_without_anybody_making_it();
	test_a_person_cannot_sign_up_as_a_bot();
	test_a_taken_account_is_refused_not_displaced();
	test_a_person_is_still_displaced();
	test_a_bot_never_reaches_the_leaderboard();
	return (0);
}

/*
** Nothing signs the pool up. It is made at boot from the configured count, so
** the first thing a bot binary does is log in to an account it was never told
** about - which is the whole reason the pool is the server's and not the
** client's to create.
*/
static void	test_the_pool_exists_without_anybody_making_it(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(bot_login(&hc, 0) == 200);
	hc_close(&hc);
	assert(hc_connect(&hc, &fx) == 0);
	assert(bot_login(&hc, TETRISD_DEFAULT_BOT_ACCOUNTS - 1) == 200);
	hc_close(&hc);
	/* One past the end was never made. */
	assert(hc_connect(&hc, &fx) == 0);
	assert(bot_login(&hc, TETRISD_DEFAULT_BOT_ACCOUNTS) == 401);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_the_pool_exists_without_anybody_making_it\n");
}

/*
** The prefix is refused at sign-up, which is the other half of the pool being
** the only way to create one. Without it a player could take BOT_05 before
** the server did and sit unranked in the seat a bot was going to use.
*/
static void	test_a_person_cannot_sign_up_as_a_bot(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "BOT_99", "hunter2") == 400);
	assert(hc_signup(&hc, "bot_99", "hunter2") == 400);
	/* Only the prefix is reserved; the word is not. */
	assert(hc_signup(&hc, "BOT", "hunter2") == 201);
	assert(hc_signup(&hc, "robot_99", "hunter2") == 201);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_person_cannot_sign_up_as_a_bot\n");
}

/*
** Two players filling their own rooms draw from one pool, so the second one
** to ask for BOT_01 has to be told no rather than handed it. Displacing would
** take a bot out of somebody else's match in progress - and the player it
** happened to would see a seat empty for no reason they could observe.
**
** The first connection is still usable afterwards, which is the half that
** would be missed by only checking the status code: a guard that answered 409
** after killing the previous client would look identical from here.
*/
static void	test_a_taken_account_is_refused_not_displaced(void)
{
	t_fixture	fx;
	t_harness	first;
	t_harness	second;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&first, &fx) == 0);
	assert(bot_login(&first, 0) == 200);
	assert(hc_connect(&second, &fx) == 0);
	assert(bot_login(&second, 0) == 409);
	/* A different account in the pool is still free. */
	assert(bot_login(&second, 1) == 200);
	assert(still_authed(&first));
	hc_close(&first);
	hc_close(&second);
	fx_stop(&fx);
	printf("PASS test_a_taken_account_is_refused_not_displaced\n");
}

/*
** The rule is inverted for bot accounts only. A person's second login still
** displaces their first, because they know they logged in twice.
*/
static void	test_a_person_is_still_displaced(void)
{
	t_fixture	fx;
	t_harness	first;
	t_harness	second;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&first, &fx) == 0);
	assert(hc_signup(&first, "amber", "hunter2") == 201);
	assert(hc_login(&first, "amber", "hunter2") == 200);
	assert(hc_connect(&second, &fx) == 0);
	assert(hc_login(&second, "amber", "hunter2") == 200);
	assert(!still_authed(&first));
	hc_close(&first);
	hc_close(&second);
	fx_stop(&fx);
	printf("PASS test_a_person_is_still_displaced\n");
}

/*
** A bot plays real games through the same award_game every player does, so
** this is the end-to-end half of the store's guard: whatever a bot scores, the
** route that answers LEADERBOARD never names it.
*/
static void	test_a_bot_never_reaches_the_leaderboard(void)
{
	t_fixture				fx;
	t_harness				bot;
	t_harness				person;
	t_body_leaderboard_row	rows[TETRISD_LEADERBOARD_ROWS];

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&bot, &fx) == 0);
	assert(bot_login(&bot, 0) == 200);
	assert(hc_connect(&person, &fx) == 0);
	assert(hc_signup(&person, "amber", "hunter2") == 201);
	assert(hc_login(&person, "amber", "hunter2") == 200);
	assert(play_and_leave(&fx, &bot, 12) == 0);
	assert(play_and_leave(&fx, &person, 8) == 0);
	assert(read_board(&person, rows, TETRISD_LEADERBOARD_ROWS) == 1);
	assert(strcmp(rows[0].username, "amber") == 0);
	hc_close(&bot);
	hc_close(&person);
	fx_stop(&fx);
	printf("PASS test_a_bot_never_reaches_the_leaderboard\n");
}

/**
 * @brief The plaintext password of the nth pool account.
 *
 * The bot binary derives it the same way, from the same two constants, which
 * is what lets a bot log in without a route that hands out credentials.
 *
 * @param index Zero-based account number.
 * @param out Buffer receiving the password.
 * @param cap Size of out.
 */
static void	bot_password(int index, char *out, size_t cap)
{
	char	name[DB_MAX_USERNAME];

	bot_pool_name(index, name, sizeof(name));
	snprintf(out, cap, "%s%s", TETRISD_BOT_SECRET, name);
}

/**
 * @brief Log a connected client in as the nth pool account.
 *
 * @param hc Connected client.
 * @param index Zero-based account number.
 * @return The response status code.
 */
static int	bot_login(t_harness *hc, int index)
{
	char	name[DB_MAX_USERNAME];
	char	password[TETRISD_PASSWORD_MAX];

	bot_pool_name(index, name, sizeof(name));
	bot_password(index, password, sizeof(password));
	return (hc_login(hc, name, password));
}

/**
 * @brief Is this connection still the player it logged in as?
 *
 * Asked with a request rather than by looking at the socket, because a
 * displaced client is closed by the server and a refused login leaves it
 * exactly as it was - and only a request can tell those apart from here.
 *
 * @param hc The client to ask.
 * @return 1 when an authenticated request still answers, 0 otherwise.
 */
static int	still_authed(t_harness *hc)
{
	t_htttp_message	resp;
	char			path[64];
	int				ok;

	snprintf(path, sizeof(path), "%s%llu", TETRISD_ROUTE_PLAYER_PREFIX,
		(unsigned long long)hc->player_id);
	if (hc_request(hc, "PROFILE", path, NULL, &resp) != 0)
		return (0);
	ok = resp.status_code == 200;
	htttp_message_free(&resp);
	return (ok);
}

/**
 * @brief Play a Single game to a finish and leave, so a result is recorded.
 *
 * @param fx The running server.
 * @param hc An authenticated client.
 * @param drops How many pieces to hard-drop before leaving.
 * @return 0 on success, -1 when any step failed.
 */
static int	play_and_leave(t_fixture *fx, t_harness *hc, int drops)
{
	t_htttp_message	resp;
	char			room[ROOM_NAME_MAX];
	char			path[256];
	int				i;

	(void)fx;
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
	return (0);
}

/**
 * @brief Read the leaderboard back as rows.
 *
 * @param hc An authenticated client.
 * @param rows Destination array.
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
