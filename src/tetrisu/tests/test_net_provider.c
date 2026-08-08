#include "tetrisu.h"

static void	test_load_profile_reflects_session_cache(void);
static void	test_unserved_screens_are_unavailable(void);
static void	test_login_failure_is_invalid(void);
static void	test_lobby_create_room_unauthed_returns_unavailable(void);

int	main(void)
{
	test_load_profile_reflects_session_cache();
	test_unserved_screens_are_unavailable();
	test_login_failure_is_invalid();
	test_lobby_create_room_unauthed_returns_unavailable();
	printf("\nnet provider tests done\n");
	return (0);
}

static void	test_load_profile_reflects_session_cache(void)
{
	t_app_data_provider		provider;
	t_app_net_session		session;
	t_app_profile_view_model	view;

	memset(&session, 0, sizeof(session));
	session.net.state = NET_AUTHED;
	snprintf(session.net.username, sizeof(session.net.username), "amber");
	snprintf(session.username, sizeof(session.username), "amber");
	session.score = 500;
	session.wallet = 200;
	app_net_provider_init(&provider, &session);
	assert(provider.load_profile != NULL);
	assert(provider.load_profile(&session, &view) == APP_PROVIDER_OK);
	assert(view.signed_in == true);
	assert(strcmp(view.username, "amber") == 0);
	assert(view.score == 500);
	assert(view.wallet_points == 200);
	assert(view.rank == -1);
	printf("PASS test_load_profile_reflects_session_cache\n");
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