#include "tetrisu.h"

// Static Functions
static app_provider_result_t	fixture_login(void *userdata,
				const char *username, const char *password,
				app_auth_view_model_t *view);
static app_provider_result_t	fixture_sign_up(void *userdata,
				const char *username, const char *password,
				app_auth_view_model_t *view);
static app_provider_result_t	fixture_load_profile(void *userdata,
				app_profile_view_model_t *view);
static app_provider_result_t	fixture_load_catalogue(void *userdata,
				app_catalogue_kind_t kind, app_catalogue_view_model_t *view);
static app_provider_result_t	fixture_load_leaderboard(void *userdata,
				app_leaderboard_view_model_t *view);
static app_provider_result_t	fixture_load_lobby(void *userdata,
				app_lobby_view_model_t *view);
static app_provider_result_t	fixture_load_room(void *userdata,
				const char *room_id, app_room_view_model_t *view);
static void	set_catalogue_item(app_catalogue_item_view_model_t *item,
				const char *id, const char *name, int price, bool owned,
				bool equipped);
static app_data_status_t	status_from_result(app_provider_result_t result);
static app_provider_result_t	load_provider_screen(
				const app_data_provider_t *provider, app_screen_t screen,
				app_screen_view_model_t *view);
static void	set_screen_copy(app_screen_view_model_t *view,
				const char *subtitle);

/**
 * @brief Installs deterministic, visibly marked data for UI development.
 *
 * These fixtures are deliberately isolated behind the same provider contract
 * a future network adapter will implement.
 */
void	app_fixture_provider_init(app_data_provider_t *provider)
{
	if (provider == NULL)
		return ;
	memset(provider, 0, sizeof(*provider));
	provider->name = "local-fixtures";
	provider->local_fixtures = true;
	provider->login = fixture_login;
	provider->sign_up = fixture_sign_up;
	provider->load_profile = fixture_load_profile;
	provider->load_catalogue = fixture_load_catalogue;
	provider->load_leaderboard = fixture_load_leaderboard;
	provider->load_lobby = fixture_load_lobby;
	provider->load_room = fixture_load_room;
}

/**
 * @brief Loads the typed model required by one application screen.
 */
app_provider_result_t	app_screen_view_load(
	const app_data_provider_t *provider, app_screen_t screen,
	app_screen_view_model_t *view)
{
	app_provider_result_t	result;

	if (view == NULL || screen < APP_SCREEN_ENTRY
		|| screen >= APP_SCREEN_COUNT)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->screen = screen;
	snprintf(view->title, sizeof(view->title), "%s", app_screen_name(screen));
	view->local_preview = provider != NULL && provider->local_fixtures
		&& screen != APP_SCREEN_SOLO && screen != APP_SCREEN_QUIT;
	result = load_provider_screen(provider, screen, view);
	view->status = status_from_result(result);
	return (result);
}

/**
 * @brief Returns a compact label for loading, empty, and failure UI.
 */
const char	*app_data_status_name(app_data_status_t status)
{
	static const char	*names[] = {
		"IDLE", "LOADING", "READY", "EMPTY", "UNAVAILABLE", "ERROR"
	};

	if (status < APP_DATA_IDLE || status > APP_DATA_ERROR)
		return ("ERROR");
	return (names[status]);
}

/**
 * @brief Returns the reader-facing game-mode label.
 */
const char	*app_game_mode_name(app_game_mode_t mode)
{
	if (mode == APP_GAME_MODE_DOUBLE)
		return ("Double");
	if (mode == APP_GAME_MODE_BATTLE_ROYALE)
		return ("Battle Royale");
	return ("None");
}

/**
 * @brief Fixture authentication accepts non-empty preview credentials.
 */
static app_provider_result_t	fixture_login(void *userdata,
	const char *username, const char *password, app_auth_view_model_t *view)
{
	(void)userdata;
	if (view == NULL || username == NULL || password == NULL
		|| username[0] == '\0' || password[0] == '\0')
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "%s", username);
	snprintf(view->message, sizeof(view->message),
		"LOCAL UI PREVIEW SESSION");
	return (APP_PROVIDER_OK);
}

/**
 * @brief Fixture sign-up shares the local preview authentication contract.
 */
static app_provider_result_t	fixture_sign_up(void *userdata,
	const char *username, const char *password, app_auth_view_model_t *view)
{
	return (fixture_login(userdata, username, password, view));
}

/**
 * @brief Supplies a deterministic preview profile.
 */
static app_provider_result_t	fixture_load_profile(void *userdata,
	app_profile_view_model_t *view)
{
	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "PreviewPlayer");
	snprintf(view->character, sizeof(view->character), "Mirurun");
	snprintf(view->theme, sizeof(view->theme), "Classic Temple");
	view->score = 125400;
	view->wallet_points = 3200;
	view->rank = 7;
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies compact character or theme fixture catalogues.
 */
static app_provider_result_t	fixture_load_catalogue(void *userdata,
	app_catalogue_kind_t kind, app_catalogue_view_model_t *view)
{
	(void)userdata;
	if (view == NULL || (kind != APP_CATALOGUE_CHARACTERS
			&& kind != APP_CATALOGUE_THEMES))
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->kind = kind;
	view->count = 3;
	if (kind == APP_CATALOGUE_CHARACTERS)
	{
		set_catalogue_item(&view->items[0], "mirurun", "Mirurun", 0,
			true, true);
		set_catalogue_item(&view->items[1], "inversion", "Inversion", 1800,
			false, false);
		set_catalogue_item(&view->items[2], "pentarisu", "Pentarisu", 2400,
			false, false);
	}
	else
	{
		set_catalogue_item(&view->items[0], "temple", "Classic Temple", 0,
			true, true);
		set_catalogue_item(&view->items[1], "neon", "Neon Arcade", 1200,
			true, false);
		set_catalogue_item(&view->items[2], "moon", "Moon Shrine", 2200,
			false, false);
	}
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies a ranked local preview list.
 */
static app_provider_result_t	fixture_load_leaderboard(void *userdata,
	app_leaderboard_view_model_t *view)
{
	static const char	*names[] = {
		"BlockBunny", "Tetromancer", "MoonStack", "PreviewPlayer",
		"SoftDrop"
	};
	static const uint64_t	scores[] = {
		980500, 744200, 631900, 125400, 99200
	};
	int						index;

	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->count = 5;
	index = 0;
	while (index < view->count)
	{
		view->entries[index].position = index + 1;
		snprintf(view->entries[index].username,
			sizeof(view->entries[index].username), "%s", names[index]);
		view->entries[index].score = scores[index];
		index++;
	}
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies two rooms for lobby layout work.
 */
static app_provider_result_t	fixture_load_lobby(void *userdata,
	app_lobby_view_model_t *view)
{
	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->count = 2;
	snprintf(view->rooms[0].id, sizeof(view->rooms[0].id), "ROOM-042");
	snprintf(view->rooms[0].owner, sizeof(view->rooms[0].owner), "BlockBunny");
	view->rooms[0].mode = APP_GAME_MODE_DOUBLE;
	view->rooms[0].players = 1;
	view->rooms[0].capacity = 2;
	snprintf(view->rooms[1].id, sizeof(view->rooms[1].id), "BR-008");
	snprintf(view->rooms[1].owner, sizeof(view->rooms[1].owner), "MoonStack");
	view->rooms[1].mode = APP_GAME_MODE_BATTLE_ROYALE;
	view->rooms[1].players = 4;
	view->rooms[1].capacity = 8;
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies a waiting-room snapshot with explicit ready states.
 */
static app_provider_result_t	fixture_load_room(void *userdata,
	const char *room_id, app_room_view_model_t *view)
{
	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	snprintf(view->id, sizeof(view->id), "%s",
		room_id == NULL || room_id[0] == '\0' ? "ROOM-042" : room_id);
	view->mode = strncmp(view->id, "BR-", 3) == 0
		? APP_GAME_MODE_BATTLE_ROYALE : APP_GAME_MODE_DOUBLE;
	view->required_players = view->mode == APP_GAME_MODE_DOUBLE ? 2 : 4;
	view->player_count = 2;
	snprintf(view->players[0].username,
		sizeof(view->players[0].username), "PreviewPlayer");
	view->players[0].owner = true;
	view->players[0].ready = true;
	snprintf(view->players[1].username,
		sizeof(view->players[1].username), "BlockBunny");
	view->players[1].ready = false;
	return (APP_PROVIDER_OK);
}

/**
 * @brief Writes one bounded fixture catalogue item.
 */
static void	set_catalogue_item(app_catalogue_item_view_model_t *item,
	const char *id, const char *name, int price, bool owned, bool equipped)
{
	snprintf(item->id, sizeof(item->id), "%s", id);
	snprintf(item->name, sizeof(item->name), "%s", name);
	item->price = price;
	item->owned = owned;
	item->equipped = equipped;
}

/**
 * @brief Maps provider outcomes to screen presentation states.
 */
static app_data_status_t	status_from_result(app_provider_result_t result)
{
	if (result == APP_PROVIDER_OK)
		return (APP_DATA_READY);
	if (result == APP_PROVIDER_EMPTY)
		return (APP_DATA_EMPTY);
	if (result == APP_PROVIDER_UNAVAILABLE)
		return (APP_DATA_UNAVAILABLE);
	if (result == APP_PROVIDER_INVALID || result == APP_PROVIDER_ERROR)
		return (APP_DATA_ERROR);
	return (APP_DATA_IDLE);
}

/**
 * @brief Dispatches one screen to the provider method for its typed model.
 */
static app_provider_result_t	load_provider_screen(
	const app_data_provider_t *provider, app_screen_t screen,
	app_screen_view_model_t *view)
{
	app_provider_result_t	result;

	result = APP_PROVIDER_OK;
	if (screen == APP_SCREEN_ENTRY)
		set_screen_copy(view, "Login, sign up, or play offline");
	else if (screen == APP_SCREEN_LOGIN || screen == APP_SCREEN_SIGN_UP)
	{
		set_screen_copy(view, "Authentication model ready");
		snprintf(view->data.auth.message, sizeof(view->data.auth.message),
			"Provider awaiting credentials");
	}
	else if (screen == APP_SCREEN_HOME || screen == APP_SCREEN_SETTINGS)
	{
		set_screen_copy(view, "Profile and local settings model");
		if (provider == NULL || provider->load_profile == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_profile(provider->userdata,
				&view->data.profile);
	}
	else if (screen == APP_SCREEN_SOLO)
		set_screen_copy(view, "Local endless Solo");
	else if (screen == APP_SCREEN_MARKETPLACE)
	{
		set_screen_copy(view, "Character catalogue model");
		if (provider == NULL || provider->load_catalogue == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_catalogue(provider->userdata,
				APP_CATALOGUE_CHARACTERS, &view->data.catalogue);
	}
	else if (screen == APP_SCREEN_LEADERBOARD)
	{
		set_screen_copy(view, "Top-player ranking model");
		if (provider == NULL || provider->load_leaderboard == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_leaderboard(provider->userdata,
				&view->data.leaderboard);
	}
	else if (screen == APP_SCREEN_LOBBY)
	{
		set_screen_copy(view, "Room browser model");
		if (provider == NULL || provider->load_lobby == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_lobby(provider->userdata, &view->data.lobby);
	}
	else if (screen == APP_SCREEN_CREATE_ROOM_MODAL
		|| screen == APP_SCREEN_WAITING_ROOM)
	{
		set_screen_copy(view, screen == APP_SCREEN_CREATE_ROOM_MODAL
			? "Double or Battle Royale room" : "Ready-state room model");
		if (provider == NULL || provider->load_room == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_room(provider->userdata, "ROOM-042",
				&view->data.room);
	}
	else if (screen == APP_SCREEN_DOUBLE
		|| screen == APP_SCREEN_BATTLE_ROYALE)
	{
		set_screen_copy(view, "Server-authoritative match snapshot");
		snprintf(view->data.match.room_id,
			sizeof(view->data.match.room_id), "%s",
			screen == APP_SCREEN_DOUBLE ? "ROOM-042" : "BR-008");
		view->data.match.mode = screen == APP_SCREEN_DOUBLE
			? APP_GAME_MODE_DOUBLE : APP_GAME_MODE_BATTLE_ROYALE;
		view->data.match.player_count = screen == APP_SCREEN_DOUBLE ? 2 : 8;
		snprintf(view->data.match.status,
			sizeof(view->data.match.status), "Waiting for server state");
	}
	else if (screen == APP_SCREEN_QUIT)
		result = APP_PROVIDER_OK;
	else
		result = APP_PROVIDER_INVALID;
	return (result);
}

/**
 * @brief Sets one bounded screen subtitle.
 */
static void	set_screen_copy(app_screen_view_model_t *view,
	const char *subtitle)
{
	snprintf(view->subtitle, sizeof(view->subtitle), "%s", subtitle);
}
