#include "tetrisu.h"

static void	test_profile_is_asked_for_not_remembered(void);
static void	test_store_writes_need_a_session(void);
static void	test_unserved_screens_are_unavailable(void);
static void	test_login_failure_is_invalid(void);
static void	test_lobby_create_room_unauthed_returns_unavailable(void);

int	main(void)
{
	test_profile_is_asked_for_not_remembered();
	test_store_writes_need_a_session();
	test_unserved_screens_are_unavailable();
	test_login_failure_is_invalid();
	test_lobby_create_room_unauthed_returns_unavailable();
	printf("\nnet provider tests done\n");
	return (0);
}

/*
** The profile used to be answered out of the session: score and wallet were
** whatever LOGIN happened to mention and rank was a hardcoded -1, so the
** Marketplace spent a balance nobody had checked. It is a read now, which
** means a connection that cannot make one has no profile to give rather than
** a remembered one.
**
** The socket is deliberately left unconnected here - what is under test is
** that the provider refuses before it reaches for it. A profile that really
** arrives is what tests/integration/test_net_store.sh asserts, against a
** real tetrisd.
*/
static void	test_profile_is_asked_for_not_remembered(void)
{
	t_app_data_provider		provider;
	t_app_net_session		session;
	t_app_profile_view_model	view;

	memset(&session, 0, sizeof(session));
	session.net.state = NET_CONNECTED;
	snprintf(session.username, sizeof(session.username), "amber");
	session.score = 500;
	session.wallet = 200;
	app_net_provider_init(&provider, &session);
	assert(provider.load_profile != NULL);
	assert(provider.load_profile(&session, &view) == APP_PROVIDER_UNAVAILABLE);
	printf("PASS test_profile_is_asked_for_not_remembered\n");
}

/*
** Buying and equipping are wired to the provider, and both refuse a
** connection that has not signed in - the client never decides either of them
** locally, so there is nothing it can do without a server.
*/
static void	test_store_writes_need_a_session(void)
{
	t_app_data_provider			provider;
	t_app_net_session			session;
	t_app_settings_view_model	view;

	memset(&session, 0, sizeof(session));
	session.net.state = NET_CONNECTED;
	app_net_provider_init(&provider, &session);
	assert(provider.buy_item != NULL && provider.equip_item != NULL);
	assert(provider.buy_item(&session, APP_CATALOGUE_CHARACTERS, 2, &view)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.equip_item(&session, APP_CATALOGUE_THEMES, 3, &view)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.buy_item(&session, APP_CATALOGUE_CHARACTERS, 2, NULL)
		== APP_PROVIDER_INVALID);
	printf("PASS test_store_writes_need_a_session\n");
}

static void	test_unserved_screens_are_unavailable(void)
{
	t_app_data_provider			provider;
	t_app_net_session			session;
	t_app_settings_view_model		settings;
	t_app_leaderboard_view_model	leaderboard;
	t_app_lobby_view_model			lobby;
	t_app_room_view_model			room;

	memset(&session, 0, sizeof(session));
	app_net_provider_init(&provider, &session);
	assert(provider.load_settings(&session, &settings)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.load_leaderboard(&session, &leaderboard)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.load_lobby(&session, &lobby) == APP_PROVIDER_UNAVAILABLE);
	assert(provider.load_room(&session, "duel-42", &room)
		== APP_PROVIDER_UNAVAILABLE);
	printf("PASS test_unserved_screens_are_unavailable\n");
}

static void	test_login_failure_is_invalid(void)
{
	t_app_data_provider		provider;
	t_app_net_session		session;
	t_app_auth_view_model		view;
	const char			username[] = "x";

	memset(&session, 0, sizeof(session));
	app_net_provider_init(&provider, &session);
	assert(provider.sign_up(&session, username, "p", "h", &view)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.login(&session, "", "p", "h", &view)
		== APP_PROVIDER_INVALID);
	printf("PASS test_login_failure_is_invalid\n");
}

static void	test_lobby_create_room_unauthed_returns_unavailable(void)
{
	t_app_data_provider		provider;
	t_app_net_session		session;
	t_app_lobby_view_model	lobby;
	t_app_room_view_model	room;

	memset(&session, 0, sizeof(session));
	session.net.state = NET_CONNECTED;
	snprintf(session.username, sizeof(session.username), "guest");
	app_net_provider_init(&provider, &session);
	assert(provider.load_lobby(&session, &lobby)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.create_room(&session, APP_GAME_MODE_DOUBLE, &room)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.load_room(&session, "D-99", &room)
		== APP_PROVIDER_UNAVAILABLE);
	assert(provider.create_room(&session, APP_GAME_MODE_NONE, &room)
		== APP_PROVIDER_INVALID);
	printf("PASS test_lobby_create_room_unauthed_returns_unavailable\n");
}