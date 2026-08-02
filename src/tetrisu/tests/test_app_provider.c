#include "tetrisu.h"

static void	test_fixture_provider_contract(void);
static void	test_every_screen_has_a_typed_model(void);
static void	test_fixture_models_are_marked_and_populated(void);
static void	test_missing_provider_is_unavailable(void);
static void	test_empty_leaderboard_status(void);
static app_provider_result_t	empty_leaderboard(void *userdata,
				app_leaderboard_view_model_t *view);

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
	app_data_provider_t		provider;
	app_auth_view_model_t	auth;

	app_fixture_provider_init(&provider);
	assert(strcmp(provider.name, "local-fixtures") == 0);
	assert(provider.local_fixtures);
	assert(provider.login != NULL && provider.sign_up != NULL);
	assert(provider.load_profile != NULL && provider.load_catalogue != NULL);
	assert(provider.load_leaderboard != NULL && provider.load_lobby != NULL);
	assert(provider.load_room != NULL);
	assert(provider.login(provider.userdata, "", "password", "example.com",
			&auth)
		== APP_PROVIDER_INVALID);
	assert(provider.login(provider.userdata, "PreviewPlayer", "password",
			"example.com", &auth) == APP_PROVIDER_OK);
	assert(auth.signed_in);
	assert(strcmp(auth.username, "PreviewPlayer") == 0);
	assert(strstr(auth.message, "LOCAL UI PREVIEW") != NULL);
	{
		app_room_view_model_t	room;

		assert(provider.load_room(provider.userdata, "BR-008", &room)
			== APP_PROVIDER_OK);
		assert(room.mode == APP_GAME_MODE_BATTLE_ROYALE);
		assert(room.required_players == 4);
	}
	printf("PASS test_fixture_provider_contract\n");
}

static void	test_every_screen_has_a_typed_model(void)
{
	app_data_provider_t			provider;
	app_screen_view_model_t	view;
	int							screen;

	app_fixture_provider_init(&provider);
	screen = APP_SCREEN_ENTRY;
	while (screen < APP_SCREEN_COUNT)
	{
		assert(app_screen_view_load(&provider, (app_screen_t)screen, &view)
			== APP_PROVIDER_OK);
		assert(view.screen == (app_screen_t)screen);
		assert(view.title[0] != '\0');
		assert(view.status == APP_DATA_READY);
		screen++;
	}
	assert(!view.local_preview);
	printf("PASS test_every_screen_has_a_typed_model\n");
}

static void	test_fixture_models_are_marked_and_populated(void)
{
	app_data_provider_t			provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load(&provider, APP_SCREEN_MARKETPLACE, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview && view.data.catalogue.count == 4);
	assert(strcmp(view.data.catalogue.items[0].name, "Mirurun") == 0);
	assert(view.data.catalogue.items[0].owned);
	assert(app_screen_view_load(&provider, APP_SCREEN_LEADERBOARD, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview
		&& view.data.leaderboard.count == APP_LEADERBOARD_MAX_ENTRIES);
	assert(view.data.leaderboard.entries[0].position == 1);
	assert(view.data.leaderboard.entries[9].position == 10);
	assert(app_screen_view_load(&provider, APP_SCREEN_LOBBY, &view)
		== APP_PROVIDER_OK);
	assert(view.local_preview && view.data.lobby.count == 2);
	assert(view.data.lobby.rooms[1].mode
		== APP_GAME_MODE_BATTLE_ROYALE);
	assert(app_screen_view_load(&provider, APP_SCREEN_WAITING_ROOM, &view)
		== APP_PROVIDER_OK);
	assert(view.data.room.player_count == 2);
	assert(view.data.room.players[0].owner);
	assert(app_screen_view_load(&provider, APP_SCREEN_BATTLE_ROYALE, &view)
		== APP_PROVIDER_OK);
	assert(view.data.match.mode == APP_GAME_MODE_BATTLE_ROYALE);
	printf("PASS test_fixture_models_are_marked_and_populated\n");
}

static void	test_missing_provider_is_unavailable(void)
{
	app_screen_view_model_t	view;

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
	app_data_provider_t		provider;
	app_screen_view_model_t	view;

	memset(&provider, 0, sizeof(provider));
	provider.load_leaderboard = empty_leaderboard;
	assert(app_screen_view_load(&provider, APP_SCREEN_LEADERBOARD, &view)
		== APP_PROVIDER_EMPTY);
	assert(view.status == APP_DATA_EMPTY);
	assert(view.data.leaderboard.count == 0);
	printf("PASS test_empty_leaderboard_status\n");
}

static app_provider_result_t	empty_leaderboard(void *userdata,
	app_leaderboard_view_model_t *view)
{
	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	return (APP_PROVIDER_EMPTY);
}
