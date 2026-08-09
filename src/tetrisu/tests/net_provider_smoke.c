/* ************************************************************************** */
/*                                                                            */
/*   net_provider_smoke.c - the network app provider, against a live tetrisd  */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_net_provider.sh starts one and runs this against it. Everything      */
/*   here goes over a real socket, a real libtetrissh handshake and real       */
/*   HTTTP - the same wire net_smoke uses, but driving the app data provider   */
/*   vtable that the tetrisu UI now invokes at CHECK SERVER, SIGN UP, LOGIN,   */
/*   Multiplayer -> Lobby and Create Room.                                    */
/*                                                                            */
/*   What it guards is the seam Block 1-4 of the net-mode wiring added: the   */
/*   provider maps a live tetrisd session onto the same view models the       */
/*   fixture provider filled, so the screens render real account, lobby and    */
/*   room data rather than fabricated preview fixtures.                       */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

// Static Functions
static int	check_signup_and_login(t_app_net_session *session,
				t_app_data_provider *provider, const char *name);
static int	check_profile_reflects_login(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_lobby_lists_the_room(t_app_data_provider *provider,
				t_app_net_session *session);
static int	check_create_and_join_room(t_app_data_provider *provider,
				t_app_net_session *session,
				t_app_net_session *helper);
static int	check_signing_in_again_works(t_app_data_provider *provider,
				t_app_net_session *session, const char *name);
static int	check_a_dead_session_redials(t_app_data_provider *provider,
				t_app_net_session *session, const char *name);
static int	check_leaderboard_lists_the_account(t_app_data_provider *provider,
				t_app_net_session *session, const char *name);
static int	leave_room(t_app_net_session *session);
static int	sign_in_raw(t_net_client *net, const char *name);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - connect, then drive the provider vtable end to end.
 */
int	main(void)
{
	t_net_config		cfg;
	t_app_net_session	session;
	t_app_net_session	helper;
	t_app_data_provider	provider;
	char			name[NET_USER_MAX];
	int			failures;

	net_config_load(&cfg);
	memset(&session, 0, sizeof(session));
	if (net_connect(&session.net, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			session.net.error);
		return (1);
	}
	session.connected = true;
	app_net_provider_init(&provider, &session);
	snprintf(name, sizeof(name), "prov%d", (int)getpid());
	failures = 0;
	report("signup and login through the provider",
		check_signup_and_login(&session, &provider, name), &failures);
	report("profile reflects the login body",
		check_profile_reflects_login(&provider, &session), &failures);
	report("lobby lists the created room",
		check_lobby_lists_the_room(&provider, &session), &failures);
	report("create and join a room",
		check_create_and_join_room(&provider, &session, &helper),
		&failures);
	report("signing in again from the auth screen works",
		check_signing_in_again_works(&provider, &session, name), &failures);
	report("a lost session redials on the next sign-in",
		check_a_dead_session_redials(&provider, &session, name), &failures);
	report("the leaderboard lists this account",
		check_leaderboard_lists_the_account(&provider, &session, name),
		&failures);
	net_disconnect(&session.net);
	return (failures != 0);
}

/**
 * @brief SIGN UP then LOGIN through the provider, mirroring the UI buttons.
 *
 * The provider's sign_up and login slots are what the auth screen calls after
 * CHECK SERVER reports online. This asserts the round trip: a fresh player is
 * registered, then the same connection is bound to it, and the view model the
 * screen reads says signed_in with the right username.
 */
static int	check_signup_and_login(t_app_net_session *session,
			t_app_data_provider *provider, const char *name)
{
	t_app_auth_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->sign_up(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (provider->login(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (!view.signed_in)
		return (0);
	if (strcmp(view.username, name) != 0)
		return (0);
	if (strcmp(view.message, "WELCOME TO TETRISU!") != 0)
		return (0);
	if (session->net.state < NET_AUTHED)
		return (0);
	if (strcmp(session->username, name) != 0)
		return (0);
	return (1);
}

/**
 * @brief load_profile reads the account, rather than remembering the login.
 *
 * A fresh account has zero score and wallet, a real rank, and item 1 of each
 * kind equipped. Score and wallet used to come from whatever LOGIN mentioned
 * and rank was a hardcoded -1; all four are read from PROFILE now, which is
 * what lets the Marketplace spend a balance the server agrees with.
 */
static int	check_profile_reflects_login(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_profile_view_model	view;

	memset(&view, 0, sizeof(view));
	if (provider->load_profile(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (!view.signed_in)
		return (0);
	if (strcmp(view.username, session->username) != 0)
		return (0);
	if (view.score != 0)
		return (0);
	if (view.wallet_points != 0)
		return (0);
	/*
	** Rank was -1 here for as long as nothing knew it. It is read now, and a
	** registered account always has one - the store indexes every player by
	** (score, id) from signup, so a fresh account ranks last rather than
	** nowhere.
	*/
	if (view.rank < 1)
		return (0);
	/* Signup grants and equips item 1 of each kind (UC-01). */
	if (strcmp(view.character, "Halloween") != 0)
		return (0);
	if (strcmp(view.theme, "Default") != 0)
		return (0);
	return (1);
}

/**
 * @brief LIST /rooms through the provider returns the room this session owns.
 *
 * Create first so the list is non-empty, then load_lobby. The provider decodes
 * the body_rooms body into the same summary view model the fixture provider
 * filled, so the lobby renderer is unchanged.
 */
static int	check_lobby_lists_the_room(t_app_data_provider *provider,
			t_app_net_session *session)
{
	t_app_lobby_view_model	lobby;
	t_app_room_view_model	room;
	int			i;
	int			found;

	memset(&room, 0, sizeof(room));
	if (provider->create_room(session, APP_GAME_MODE_DOUBLE, &room)
		!= APP_PROVIDER_OK)
		return (0);
	if (room.id[0] == '\0')
		return (0);
	memset(&lobby, 0, sizeof(lobby));
	if (provider->load_lobby(session, &lobby) != APP_PROVIDER_OK)
		return (0);
	if (lobby.count < 1)
		return (0);
	if (!lobby.profile.signed_in
		|| strcmp(lobby.profile.username, session->username) != 0)
		return (0);
	found = 0;
	i = 0;
	while (i < lobby.count)
	{
		if (strcmp(lobby.rooms[i].id, room.id) == 0
			&& lobby.rooms[i].mode == APP_GAME_MODE_DOUBLE)
			found = 1;
		i++;
	}
	if (!found)
		return (0);
	if (leave_room(session) != 0)
		return (0);
	return (1);
}

/**
 * @brief create_room then load_room (join by id) on a different client's room.
 *
 * tetrisd refuses a JOIN when the caller is already in a room (409), so the
 * join target is a room a second session created. This is the realistic flow:
 * one player opens a room, another finds it in the lobby and joins by id.
 */
static int	check_create_and_join_room(t_app_data_provider *provider,
			t_app_net_session *session,
			t_app_net_session *helper)
{
	t_app_room_view_model	created;
	t_app_room_view_model	joined;
	t_net_config		cfg;
	char			helper_name[NET_USER_MAX];

	memset(helper, 0, sizeof(*helper));
	net_config_load(&cfg);
	if (net_connect(&helper->net, &cfg) != 0)
		return (0);
	helper->connected = true;
	snprintf(helper_name, sizeof(helper_name), "help%d", (int)getpid());
	if (sign_in_raw(&helper->net, helper_name) != 0)
		return (0);
	snprintf(helper->username, sizeof(helper->username), "%s", helper_name);
	memset(&created, 0, sizeof(created));
	if (provider->create_room(helper, APP_GAME_MODE_DOUBLE, &created)
		!= APP_PROVIDER_OK)
		return (0);
	if (created.id[0] == '\0' || created.mode != APP_GAME_MODE_DOUBLE)
		return (0);
	if (created.player_count != 1 || !created.players[0].owner)
		return (0);
	memset(&joined, 0, sizeof(joined));
	if (provider->load_room(session, created.id, &joined) != APP_PROVIDER_OK)
		return (0);
	if (strcmp(joined.id, created.id) != 0)
		return (0);
	if (joined.mode != APP_GAME_MODE_DOUBLE)
		return (0);
	if (joined.player_count != 1)
		return (0);
	if (strcmp(joined.players[joined.local_slot].username, session->username)
		!= 0)
		return (0);
	if (joined.players[joined.local_slot].owner)
		return (0);
	(void)leave_room(session);
	(void)leave_room(helper);
	net_disconnect(&helper->net);
	return (1);
}

/**
 * @brief Sends LEAVE /room/<name> on the session's current room, raw.
 *
 * The provider vtable has no leave slot (the UI's Back action drives it
 * elsewhere), so the test reaches past the provider for this one control
 * message. It clears the binding so the next JOIN is not refused as
 * already-in-room.
 */
static int	leave_room(t_app_net_session *session)
{
	char	path[NET_PATH_MAX];
	t_net_result	result;

	if (session->net.state < NET_IN_ROOM || session->net.room[0] == '\0')
		return (0);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM,
		session->net.room);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "LEAVE", path, NULL, &result) != 0)
		return (-1);
	if (result.status != 200)
		return (-1);
	session->net.state = NET_AUTHED;
	session->net.room[0] = '\0';
	session->net.play_path[0] = '\0';
	session->net.has_state = false;
	return (0);
}

/**
 * @brief LOGIN a second time on a session that is already signed in.
 *
 * Backing out of Home to the auth screen and pressing SIGN IN again is one of
 * the shortest paths through this client, and it was broken: identity belongs
 * to the socket (ADR-0001), so tetrisd answers a LOGIN on an authenticated
 * connection with 409, and the screen showed a filled-in form that refused
 * every attempt until the player retyped the server address - which forced
 * the reconnect that was the actual repair.
 *
 * The provider now redials for them, so this asserts the second sign-in
 * succeeds and lands on a connection that is genuinely bound.
 */
static int	check_signing_in_again_works(t_app_data_provider *provider,
			t_app_net_session *session, const char *name)
{
	t_app_auth_view_model	view;

	if (session->net.state < NET_AUTHED)
		return (0);
	memset(&view, 0, sizeof(view));
	if (provider->login(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	if (!view.signed_in || strcmp(view.username, name) != 0)
		return (0);
	if (session->net.state < NET_AUTHED || session->net.player_id == 0)
		return (0);
	return (session->connected);
}

/**
 * @brief LOGIN after the session died the way a dropped game kills it.
 *
 * solo_authority.c disconnects when a session is lost mid-game, which left
 * the handle offline while the auth form still read ONLINE - nothing told the
 * form. The next sign-in has to dial again rather than send a request down a
 * socket that is not there.
 */
static int	check_a_dead_session_redials(t_app_data_provider *provider,
			t_app_net_session *session, const char *name)
{
	t_app_auth_view_model	view;

	net_disconnect(&session->net);
	if (session->net.state != NET_OFFLINE)
		return (0);
	memset(&view, 0, sizeof(view));
	if (provider->login(session, name, "hunter2", "127.0.0.1", &view)
		!= APP_PROVIDER_OK)
		return (0);
	return (view.signed_in && session->net.state >= NET_AUTHED);
}

/**
 * @brief LEADERBOARD /leaderboard through the provider.
 *
 * The Leaderboard screen answered "not served" against a live session while
 * the preview build showed fixtures, because tetrisd had no route for the
 * ranking it had been recording since M1. This asserts the round trip: the
 * account that just signed up is on the board, ranks are one-based and
 * ascending, and the view model the screen reads is the one the fixture
 * provider used to fill.
 */
static int	check_leaderboard_lists_the_account(t_app_data_provider *provider,
			t_app_net_session *session, const char *name)
{
	t_app_leaderboard_view_model	view;
	int								found;
	int								i;

	memset(&view, 0, sizeof(view));
	if (provider->load_leaderboard(session, &view) != APP_PROVIDER_OK)
		return (0);
	if (view.count < 1)
		return (0);
	found = 0;
	i = 0;
	while (i < view.count)
	{
		if (view.entries[i].position != i + 1)
			return (0);
		if (strcmp(view.entries[i].username, name) == 0)
			found = 1;
		i++;
	}
	return (found);
}

/**
 * @brief Registers and signs in a throwaway player on a raw connection.
 *
 * Mirrors net_smoke.c's sign_in helper, used here for the second session that
 * creates the room the primary joins.
 */
static int	sign_in_raw(t_net_client *net, const char *name)
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