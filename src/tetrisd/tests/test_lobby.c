/* ************************************************************************** */
/*                                                                            */
/*   test_lobby.c - rooms as a player sees them                               */
/*                                                                            */
/*   Creating, listing, joining, leaving, and starting - and, just as much,   */
/*   the refusals: a full room, a room already in game, a start requested by  */
/*   somebody who does not own the room. Every verdict is asserted through    */
/*   its status code, because that is all a client gets.                      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_join_creates_a_room_and_lists_it(void);
static void	test_a_full_room_is_refused(void);
static void	test_unknown_room_is_not_found(void);
static void	test_leaving_removes_an_empty_room(void);
static void	test_only_the_owner_starts_the_game(void);
static void	test_start_needs_enough_players(void);
static void	test_ownership_passes_to_a_successor(void);
static void	test_room_snapshot_tracks_live_roster(void);
static void	test_a_destroyed_room_does_not_take_its_successor_with_it(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static int		simple(t_harness *hc, const char *method, const char *path, const char *body);
static size_t	list_rooms(t_harness *hc, t_body_room_row *rows, size_t cap);
static int		list_room(t_harness *hc, const char *path, t_body_room *room);

int	main(void)
{
	test_join_creates_a_room_and_lists_it();
	test_a_full_room_is_refused();
	test_unknown_room_is_not_found();
	test_leaving_removes_an_empty_room();
	test_only_the_owner_starts_the_game();
	test_start_needs_enough_players();
	test_ownership_passes_to_a_successor();
	test_room_snapshot_tracks_live_roster();
	test_a_destroyed_room_does_not_take_its_successor_with_it();
	return (0);
}

static void	test_join_creates_a_room_and_lists_it(void)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_fixture		fx;
	t_harness		hc;
	char			room[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(list_rooms(&hc, rows, LOBBY_MAX_ROOMS) == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	assert(strcmp(room, "S-01") == 0);
	assert(list_rooms(&hc, rows, LOBBY_MAX_ROOMS) == 1);
	assert(strcmp(rows[0].name, "S-01") == 0);
	assert(rows[0].mode == BODY_MODE_SINGLE);
	assert(rows[0].players == 1);
	assert(rows[0].slot_count == 1);
	assert(rows[0].status == BODY_ROOM_READY);
	assert(strcmp(rows[0].owner, "amber") == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_join_creates_a_room_and_lists_it\n");
}

static void	test_a_full_room_is_refused(void)
{
	t_fixture	fx;
	t_harness	amber;
	t_harness	blake;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(player(&fx, &blake, "blake") == 0);
	assert(hc_join_new(&amber, "single", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&blake, "JOIN", path, NULL) == 409);
	assert(simple(&amber, "START", path, NULL) == 200);
	assert(simple(&blake, "JOIN", path, NULL) == 409);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_a_full_room_is_refused\n");
}

static void	test_unknown_room_is_not_found(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(simple(&hc, "JOIN", "/room/S-99", NULL) == 404);
	assert(simple(&hc, "LEAVE", "/room/S-99", NULL) == 404);
	assert(simple(&hc, "START", "/room/S-99", NULL) == 404);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_unknown_room_is_not_found\n");
}

static void	test_leaving_removes_an_empty_room(void)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_fixture		fx;
	t_harness		hc;
	char			room[ROOM_NAME_MAX];
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&hc, "LEAVE", path, NULL) == 200);
	assert(list_rooms(&hc, rows, LOBBY_MAX_ROOMS) == 0);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_leaving_removes_an_empty_room\n");
}

static void	test_only_the_owner_starts_the_game(void)
{
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
	assert(simple(&blake, "START", path, NULL) == 403);
	assert(simple(&amber, "START", path, NULL) == 200);
	assert(simple(&amber, "START", path, NULL) == 409);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_only_the_owner_starts_the_game\n");
}

static void	test_start_needs_enough_players(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "amber") == 0);
	assert(hc_join_new(&hc, "double", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&hc, "START", path, NULL) == 409);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_start_needs_enough_players\n");
}

static void	test_ownership_passes_to_a_successor(void)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(player(&fx, &blake, "blake") == 0);
	assert(hc_join_new(&amber, "double", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&blake, "JOIN", path, NULL) == 200);
	assert(simple(&amber, "LEAVE", path, NULL) == 200);
	assert(list_rooms(&blake, rows, LOBBY_MAX_ROOMS) == 1);
	assert(strcmp(rows[0].owner, "blake") == 0);
	assert(rows[0].players == 1);
	assert(simple(&blake, "START", path, NULL) == 409);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_ownership_passes_to_a_successor\n");
}

/**
 * The waiting-room client needs more than the lobby's player count: it needs
 * the ordered seats, names, roles, and current room state. This case follows
 * that projection across join and owner succession through the real protocol.
 */
static void	test_room_snapshot_tracks_live_roster(void)
{
	t_fixture	fx;
	t_harness	amber;
	t_harness	blake;
	t_harness	casey;
	t_body_room	snapshot;
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(player(&fx, &blake, "blake") == 0);
	assert(player(&fx, &casey, "casey") == 0);
	assert(hc_join_new(&amber, "double", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.member_count == 1);
	assert(snapshot.members[0].slot == 1);
	assert(snapshot.members[0].owner && snapshot.members[0].ready);
	assert(strcmp(snapshot.members[0].username, "amber") == 0);
	assert(list_room(&casey, path, &snapshot) == 404);
	assert(simple(&blake, "JOIN", path, NULL) == 200);
	assert(list_room(&amber, path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_READY);
	assert(snapshot.member_count == 2);
	assert(strcmp(snapshot.members[1].username, "blake") == 0);
	assert(!snapshot.members[1].owner && snapshot.members[1].ready);
	assert(simple(&amber, "LEAVE", path, NULL) == 200);
	assert(list_room(&blake, path, &snapshot) == 200);
	assert(snapshot.member_count == 1);
	assert(strcmp(snapshot.members[0].username, "blake") == 0);
	assert(snapshot.members[0].owner);
	hc_close(&casey);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_room_snapshot_tracks_live_roster\n");
}

/**
 * @brief Connects, registers, and logs in one player.
 *
 * @param fx Running fixture.
 * @param hc Client to bring up.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
/*
** A room the lobby destroyed leaves an index behind, and the lobby hands that
** index straight back out. If the runtime attached to it is not blanked with
** it, the server's tick is still walking the *old* room through the new one's
** state: it finds no active games, decides the game is over, and clears every
** slot - evicting whoever just created the room, seconds after they did.
**
** The tick period is stretched so the second room is certainly created inside
** the window a tick would land in, which is what makes this deterministic
** rather than a race the suite would only lose sometimes.
*/
static void	test_a_destroyed_room_does_not_take_its_successor_with_it(void)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_fixture		fx;
	t_harness		amber;
	t_harness		blake;
	char			room[ROOM_NAME_MAX];
	char			path[96];

	assert(fx_start(&fx) == 0);
	server_stop(fx.srv);
	fx.cfg.tick_ms = 1000;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	assert(player(&fx, &amber, "amber") == 0);
	assert(hc_join_new(&amber, "single", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&amber, "START", path, NULL) == 200);
	assert(simple(&amber, "LEAVE", path, NULL) == 200);
	assert(player(&fx, &blake, "blake") == 0);
	assert(hc_join_new(&blake, "single", room, sizeof(room)) == 201);
	usleep(1500 * 1000);
	assert(list_rooms(&blake, rows, LOBBY_MAX_ROOMS) == 1);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&blake, "START", path, NULL) == 200);
	hc_close(&blake);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_a_destroyed_room_does_not_take_its_successor_with_it\n");
}

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
 * @brief Lists the lobby and decodes the rows.
 *
 * @param hc Connected client.
 * @param rows Receives the decoded rows.
 * @param cap Capacity of rows.
 * @return Number of rows listed.
 */
static size_t	list_rooms(t_harness *hc, t_body_room_row *rows, size_t cap)
{
	t_htttp_message	resp;
	size_t			count;

	count = 0;
	if (hc_request(hc, "LIST", TETRISD_ROUTE_ROOMS, NULL, &resp) != 0)
		return (0);
	assert(resp.status_code == 200);
	if (resp.body != NULL && resp.body_len > 0)
		assert(body_rooms_decode((const char *)resp.body, resp.body_len, rows, cap, &count) == 0);
	htttp_message_free(&resp);
	return (count);
}

/**
 * @brief Lists and decodes one detailed waiting-room snapshot.
 *
 * @param hc Connected client making the request.
 * @param path Room path.
 * @param room Receives a decoded 200 response.
 * @return Response status, or -1 on transport/codec failure.
 */
static int	list_room(t_harness *hc, const char *path, t_body_room *room)
{
	t_htttp_message	response;
	int				status;

	if (hc_request(hc, "LIST", path, NULL, &response) != 0)
		return (-1);
	status = (int)response.status_code;
	if (status == 200 && (response.body == NULL
			|| body_room_decode((const char *)response.body, response.body_len,
				room) != 0))
		status = -1;
	htttp_message_free(&response);
	return (status);
}
