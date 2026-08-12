/* ************************************************************************** */
/*                                                                            */
/*   test_battle_royale.c - many players, one room, one owner                  */
/*                                                                            */
/*   What separates a Battle Royale from a Double room is that it starts       */
/*   below capacity: everybody present being ready says nothing about          */
/*   whether the match should begin, so the owner decides, and one player      */
/*   leaving is an ordinary event rather than the end of the match. Both       */
/*   rules are asserted here from a client's point of view only.               */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

# define BR_SEATS	5

// Static Functions
static void	test_readiness_alone_does_not_start_a_battle_royale(void);
static void	test_the_owner_starts_a_battle_royale(void);
static void	test_a_non_owner_cannot_start_a_battle_royale(void);
static void	test_a_departure_does_not_cancel_a_crowded_window(void);
static void	test_a_departure_below_the_minimum_closes_the_window(void);
static void	test_the_last_undecided_player_leaving_deals_the_match(void);

static int	seat_many(t_fixture *fx, t_harness *players, int count,
				char *room, size_t cap);
static int	simple(t_harness *hc, const char *method, const char *path,
				const char *body);
static int	list_room(t_harness *hc, const char *path, t_body_room *out);
static void	ready_all(t_harness *players, int count, const char *path);
static void	close_all(t_harness *players, int count);

int	main(void)
{
	test_readiness_alone_does_not_start_a_battle_royale();
	test_the_owner_starts_a_battle_royale();
	test_a_non_owner_cannot_start_a_battle_royale();
	test_a_departure_does_not_cancel_a_crowded_window();
	test_a_departure_below_the_minimum_closes_the_window();
	test_the_last_undecided_player_leaving_deals_the_match();
	return (0);
}

/*
** Readiness commits a Double room because two players agreeing is the whole of
** that room's decision. A Battle Royale starts below capacity by design, so
** "everybody here is ready" is true of five people in a ninety-nine seat room
** and is not an answer to whether the match should begin.
**
** It used to be treated as one: the room opened its character-select window
** the moment the last person present declared, so a room filling up started
** itself under whoever happened to be joining.
*/
static void	test_readiness_alone_does_not_start_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(list_room(&players[0], path, &snapshot) == 200);
	/* committed as far as it goes, and waiting for somebody to say so */
	assert(snapshot.status == BODY_ROOM_READY);
	assert(snapshot.select_ms == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_readiness_alone_does_not_start_a_battle_royale\n");
}

/*
** The owner's START is what opens the window, which is the same route Double
** reaches a match by and the same verdict - what changes is only that nothing
** else opens it first.
*/
static void	test_the_owner_starts_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	assert(list_room(&players[0], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.select_ms > 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_the_owner_starts_a_battle_royale\n");
}

/*
** Whose call it is matters more here than in Double, because in a room of
** ninety-nine anybody could otherwise start a match the other ninety-eight
** were not finished assembling for.
*/
static void	test_a_non_owner_cannot_start_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[3], "START", path, NULL) == 403);
	assert(list_room(&players[0], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_READY);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_non_owner_cannot_start_a_battle_royale\n");
}

/*
** One player leaving used to cancel the window for everybody, unconditionally.
** In Double that is right - there is no match without the second player - but
** in a Battle Royale it handed any one of thirty people a cancel button, and a
** disconnect two seconds into character select ended the match for the rest.
**
** Above min_to_start the room is still startable, so the window keeps its own
** clock. The owner leaving is the sharper version of the same case, because
** succession moves somebody into their seat while the window is open.
*/
static void	test_a_departure_does_not_cancel_a_crowded_window(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* the owner walks out of the room they just started */
	assert(simple(&players[0], "LEAVE", path, NULL) == 200);
	assert(list_room(&players[1], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.select_ms > 0);
	assert(snapshot.member_count == BR_SEATS - 1);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_departure_does_not_cancel_a_crowded_window\n");
}

/*
** The other half of the same rule, and the reason it is one rule rather than a
** Battle Royale exception: below min_to_start the room cannot deal the match
** it was setting up, so the window closes and whoever is left goes back to
** waiting rather than sitting on a roster screen behind a clock with nothing
** on the other side of it.
*/
static void	test_a_departure_below_the_minimum_closes_the_window(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];
	int			i;

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* down to three, one under a Battle Royale's floor of four */
	i = 0;
	while (i < BR_SEATS - 3)
	{
		assert(simple(&players[i], "LEAVE", path, NULL) == 200);
		i++;
	}
	assert(list_room(&players[BR_SEATS - 1], path, &snapshot) == 200);
	assert(snapshot.status != BODY_ROOM_SELECTING);
	assert(snapshot.select_ms == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_departure_below_the_minimum_closes_the_window\n");
}

/*
** A window ends early when every seat has chosen. If the one seat still
** deciding leaves, the room is waiting for nobody - so the departure has to be
** re-asked, or the room sits out the full select clock before dealing a match
** everybody remaining had already locked in for.
*/
static void	test_the_last_undecided_player_leaving_deals_the_match(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	int				i;

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* everybody but the last seat locks in a fighter */
	i = 0;
	while (i < BR_SEATS - 1)
	{
		assert(hc_lock_in(&players[i], &fx, path) == 200);
		i++;
	}
	assert(simple(&players[BR_SEATS - 1], "LEAVE", path, NULL) == 200);
	/* dealt at once, without waiting out TETRISD_MATCH_SELECT_MS */
	assert(hc_wait_state(&players[0], &state, HC_TIMEOUT_MS) == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_the_last_undecided_player_leaving_deals_the_match\n");
}

/**
 * @brief Signs up `count` players and seats them all in one Battle Royale.
 *
 * @param fx Running fixture.
 * @param players Receives the connections; players[0] creates and owns.
 * @param count How many to seat.
 * @param room Receives the room's name.
 * @param cap Size of room.
 * @return 0 when every player is seated, -1 otherwise.
 */
static int	seat_many(t_fixture *fx, t_harness *players, int count,
		char *room, size_t cap)
{
	char	name[32];
	char	path[64];
	int		i;

	i = 0;
	while (i < count)
	{
		snprintf(name, sizeof(name), "player%d", i);
		if (hc_connect(&players[i], fx) != 0
			|| hc_signup(&players[i], name, "hunter2") != 201
			|| hc_login(&players[i], name, "hunter2") != 200)
			return (-1);
		i++;
	}
	if (hc_join_new(&players[0], "br", room, cap) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	i = 1;
	while (i < count)
	{
		if (simple(&players[i], "JOIN", path, NULL) != 200)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Declares every seated player ready, without naming a fighter.
 *
 * @param players The connections.
 * @param count How many there are.
 * @param path The room's path.
 */
static void	ready_all(t_harness *players, int count, const char *path)
{
	int	i;

	i = 0;
	while (i < count)
	{
		assert(simple(&players[i], "READY", path, "ready 1\n") == 200);
		i++;
	}
}

/**
 * @brief Closes every connection.
 *
 * @param players The connections.
 * @param count How many there are.
 */
static void	close_all(t_harness *players, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		hc_close(&players[i]);
		i++;
	}
}

/**
 * @brief Sends one request and reports only its status.
 *
 * @param hc The connection.
 * @param method HTTTP method.
 * @param path Request path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 on a transport failure.
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
 * @brief Reads a room snapshot.
 *
 * @param hc The connection.
 * @param path The room's path.
 * @param out Receives the decoded snapshot.
 * @return The status code, or -1 on a transport failure.
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
