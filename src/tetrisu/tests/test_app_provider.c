#include "tetrisu.h"

static void	test_fixture_provider_contract(void);
static void	test_every_screen_has_a_typed_model(void);
static void	test_fixture_models_are_marked_and_populated(void);
static void	test_missing_provider_is_unavailable(void);
static void	test_empty_leaderboard_status(void);
static t_app_provider_result	empty_leaderboard(void *userdata,
				t_app_leaderboard_view_model *view);

int	main(void)
{
	test_fixture_provider_contract();
	test_every_screen_has_a_typed_model();
	test_fixture_models_are_marked_and_populated();
	test_missing_provider_is_unavailable();
	test_empty_leaderboard_status();
	return (0);
}

static void	test_fixture_provider_contract(void)
{
	t_app_data_provider		provider;
	t_app_auth_view_model	auth;

	app_fixture_provider_init(&provider);
	assert(strcmp(provider.name, "local-fixtures") == 0);
	assert(provider.local_fixtures);
	assert(provider.login != NULL && provider.sign_up != NULL);
	assert(provider.load_profile != NULL && provider.load_catalogue != NULL);
	assert(provider.load_leaderboard != NULL && provider.load_lobby != NULL);
	assert(provider.load_room != NULL && provider.create_room != NULL);
	assert(provider.login(provider.userdata, "", "password", "example.com",
			&auth)
		== APP_PROVIDER_INVALID);
	assert(provider.login(provider.userdata, "PreviewPlayer", "password",
			"example.com", &auth) == APP_PROVIDER_OK);
	assert(auth.signed_in);
	assert(strcmp(auth.username, "PreviewPlayer") == 0);
	assert(strstr(auth.message, "LOCAL UI PREVIEW") != NULL);
	{
		t_app_room_view_model	room;

		assert(provider.load_room(provider.userdata, "arena-88", &room)
			== APP_PROVIDER_OK);
		assert(room.mode == APP_GAME_MODE_BATTLE_ROYALE);
		assert(room.required_players == WAITING_ROOM_ROYALE_MIN_PLAYERS);
		assert(room.capacity == APP_ROOM_MAX_PLAYERS);
		/* A joined room seats the players the lobby advertised. */
		assert(room.player_count == WAITING_ROOM_ROYALE_MIN_PLAYERS);
		assert(room.state == APP_ROOM_STATE_READY);
		assert(room.players[room.local_slot].ready);
		assert(room.chat_count > 0);
		/* A created room seats its owner alone, whatever the mode. */
		assert(provider.create_room(provider.userdata, APP_GAME_MODE_DOUBLE,
				&room) == APP_PROVIDER_OK);
		assert(room.mode == APP_GAME_MODE_DOUBLE);
		assert(room.player_count == 1);
		assert(room.players[0].owner && room.players[0].ready);
		assert(provider.create_room(provider.userdata, APP_GAME_MODE_NONE,
				&room) == APP_PROVIDER_INVALID);
	}
	printf("PASS test_fixture_provider_contract\n");
}

static void	test_every_screen_has_a_typed_model(void)
{
	t_app_data_provider			provider;
	t_app_screen_view_model	view;
	int							screen;

	app_fixture_provider_init(&provider);
	screen = APP_SCREEN_ENTRY;
	while (screen < APP_SCREEN_COUNT)
	{
		assert(app_screen_view_load(&provider, (t_app_screen)screen, &view)
			== APP_PROVIDER_OK);
		assert(view.screen == (t_app_screen)screen);
		assert(view.title[0] != '\0');
		assert(view.status == APP_DATA_READY);
		screen++;
	}
	assert(!view.local_preview);
	printf("PASS test_every_screen_has_a_typed_model\n");
}

static void	test_fixture_models_are_marked_and_populated(void)
{
	t_app_data_provider			provider;
	t_app_screen_view_model	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load(&provider, APP_SCREEN_MARKETPLACE, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview && view.data.marketplace.characters.count == 4);
	assert(strcmp(view.data.marketplace.characters.items[0].name,
			"Mirurun") == 0);
	assert(view.data.marketplace.characters.items[0].owned);
	assert(view.data.marketplace.themes.count == 7);
	assert(view.data.marketplace.profile.wallet_points == 3200);
	assert(app_screen_view_load(&provider, APP_SCREEN_LEADERBOARD, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview
		&& view.data.leaderboard.count == APP_LEADERBOARD_MAX_ENTRIES);
	assert(view.data.leaderboard.entries[0].position == 1);
	assert(view.data.leaderboard.entries[9].position == 10);
	assert(app_screen_view_load(&provider, APP_SCREEN_LOBBY, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview && view.data.lobby.count == 6);
	/* The lobby header carries the identity strip alongside the room list. */
	assert(view.data.lobby.profile.username[0] != '\0');
	assert(view.data.lobby.rooms[3].mode == APP_GAME_MODE_BATTLE_ROYALE);
	assert(view.data.lobby.rooms[3].capacity == APP_ROOM_MAX_PLAYERS);
	assert(view.data.lobby.rooms[1].state == APP_ROOM_STATE_IN_GAME);
	assert(app_screen_view_load(&provider, APP_SCREEN_WAITING_ROOM, &view)
		== APP_PROVIDER_OK);
	assert(view.data.room.player_count == WAITING_ROOM_DOUBLE_PLAYERS);
	assert(view.data.room.players[0].owner);
	assert(app_screen_view_load(&provider, APP_SCREEN_MULTIPLAYER_MODE, &view)
		== APP_PROVIDER_OK);
	assert(view.data.profile.signed_in);
	assert(app_screen_view_load(&provider, APP_SCREEN_BATTLE_ROYALE, &view)
		== APP_PROVIDER_OK);
	assert(view.data.match.mode == APP_GAME_MODE_BATTLE_ROYALE);
	assert(view.data.match.player_count == WAITING_ROOM_ROYALE_MIN_PLAYERS);
	printf("PASS test_fixture_models_are_marked_and_populated\n");
}

static void	test_missing_provider_is_unavailable(void)
{
	t_app_screen_view_model	view;

	assert(app_screen_view_load(NULL, APP_SCREEN_MARKETPLACE, &view)
		== APP_PROVIDER_UNAVAILABLE);
	assert(view.status == APP_DATA_UNAVAILABLE);
	assert(!view.local_preview);
	assert(app_screen_view_load(NULL, APP_SCREEN_SOLO, &view)
		== APP_PROVIDER_OK);
	assert(view.status == APP_DATA_READY);
	assert(strcmp(app_data_status_name(APP_DATA_ERROR), "ERROR") == 0);
	assert(strcmp(app_game_mode_name(APP_GAME_MODE_DOUBLE), "Double") == 0);
	printf("PASS test_missing_provider_is_unavailable\n");
}

static void	test_empty_leaderboard_status(void)
{
	t_app_data_provider		provider;
	t_app_screen_view_model	view;

	memset(&provider, 0, sizeof(provider));
	provider.load_leaderboard = empty_leaderboard;
	assert(app_screen_view_load(&provider, APP_SCREEN_LEADERBOARD, &view)
		== APP_PROVIDER_EMPTY);
	assert(view.status == APP_DATA_EMPTY);
	assert(view.data.leaderboard.count == 0);
	printf("PASS test_empty_leaderboard_status\n");
}

static t_app_provider_result	empty_leaderboard(void *userdata,
	t_app_leaderboard_view_model *view)
{
	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	return (APP_PROVIDER_EMPTY);
}
