#include "tetrisu.h"

// Static Functions
static app_provider_result_t	fixture_login(void *userdata,
				const char *username, const char *password,
				const char *domain,
				app_auth_view_model_t *view);
static app_provider_result_t	fixture_sign_up(void *userdata,
				const char *username, const char *password,
				const char *domain,
				app_auth_view_model_t *view);
static app_provider_result_t	fixture_load_profile(void *userdata,
				app_profile_view_model_t *view);
static app_provider_result_t	fixture_load_settings(void *userdata,
				app_settings_view_model_t *view);
static app_provider_result_t	fixture_load_catalogue(void *userdata,
				app_catalogue_kind_t kind, app_catalogue_view_model_t *view);
static app_provider_result_t	fixture_preview_login(void *userdata,
				app_auth_view_model_t *view);
static app_provider_result_t	fixture_load_leaderboard(void *userdata,
				app_leaderboard_view_model_t *view);
static app_provider_result_t	fixture_load_lobby(void *userdata,
				app_lobby_view_model_t *view);
static app_provider_result_t	fixture_load_room(void *userdata,
				const char *room_id, app_room_view_model_t *view);
static void	set_catalogue_item(app_catalogue_item_view_model_t *item,
				const char *id, const char *name, int price, bool owned,
				bool equipped);
static void	set_character_details(app_catalogue_item_view_model_t *item,
				const char *portrait, const char *const abilities[4],
				const char *const descriptions[4]);
static app_data_status_t	status_from_result(app_provider_result_t result);
static app_provider_result_t	load_provider_screen(
				const app_data_provider_t *provider, app_screen_t screen,
				bool offline, app_screen_view_model_t *view);
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
	provider->load_settings = fixture_load_settings;
	provider->load_catalogue = fixture_load_catalogue;
	provider->preview_login = fixture_preview_login;
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
	return (app_screen_view_load_for_session(provider, screen, false, view));
}

/**
 * @brief Loads a screen model with the current authenticated/offline mode.
 *
 * Offline Settings intentionally never asks the provider for account data.
 * This is the boundary that prevents a local terminal session from showing
 * fixture username, inventory, wallet, score, or rank values.
 */
app_provider_result_t	app_screen_view_load_for_session(
	const app_data_provider_t *provider, app_screen_t screen, bool offline,
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
		&& !offline && screen != APP_SCREEN_SOLO && screen != APP_SCREEN_QUIT;
	result = load_provider_screen(provider, screen, offline, view);
	view->status = status_from_result(result);
	return (result);
}

/**
 * @brief Runs the explicitly gated fixture sign-in seam.
 */
app_provider_result_t	app_provider_preview_sign_in(
	const app_data_provider_t *provider, app_auth_view_model_t *view)
{
	if (!app_ui_preview_enabled() || provider == NULL
		|| provider->preview_login == NULL || view == NULL)
		return (APP_PROVIDER_UNAVAILABLE);
	return (provider->preview_login(provider->userdata, view));
}

/**
 * @brief Copies current process-local controls into the Settings model.
 */
void	app_settings_apply_local_controls(app_settings_view_model_t *view,
	int music_volume, tetrisu_renderer_mode_t renderer_mode)
{
	if (view == NULL)
		return ;
	view->music_volume = music_volume;
	view->renderer_mode = renderer_mode;
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
	const char *username, const char *password, const char *domain,
	app_auth_view_model_t *view)
{
	(void)userdata;
	if (view == NULL || username == NULL || password == NULL
		|| domain == NULL || username[0] == '\0' || password[0] == '\0'
		|| domain[0] == '\0')
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
	const char *username, const char *password, const char *domain,
	app_auth_view_model_t *view)
{
	return (fixture_login(userdata, username, password, domain, view));
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
	snprintf(view->theme, sizeof(view->theme), "Classic");
	snprintf(view->portrait_asset, sizeof(view->portrait_asset),
		DEFAULT_MIRURUN_PATH);
	view->score = 125400;
	view->wallet_points = 3200;
	view->rank = 7;
	return (APP_PROVIDER_OK);
}

/**
 * @brief Combines profile, character inventory, and theme inventory.
 */
static app_provider_result_t	fixture_load_settings(void *userdata,
	app_settings_view_model_t *view)
{
	app_provider_result_t	result;

	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	result = fixture_load_profile(NULL, &view->profile);
	if (result != APP_PROVIDER_OK)
		return (result);
	result = fixture_load_catalogue(NULL, APP_CATALOGUE_CHARACTERS,
		&view->characters);
	if (result != APP_PROVIDER_OK)
		return (result);
	result = fixture_load_catalogue(NULL, APP_CATALOGUE_THEMES, &view->themes);
	if (result != APP_PROVIDER_OK)
		return (result);
	view->signed_in = view->profile.signed_in;
	snprintf(view->local_status, sizeof(view->local_status),
		"LOCAL UI PREVIEW PROFILE");
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies the deterministic account used only by the preview gate.
 */
static app_provider_result_t	fixture_preview_login(void *userdata,
	app_auth_view_model_t *view)
{
	(void)userdata;
	if (view == NULL || !app_ui_preview_enabled())
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "PreviewPlayer");
	snprintf(view->message, sizeof(view->message), "LOCAL UI PREVIEW SESSION");
	return (APP_PROVIDER_OK);
}

/**
 * @brief Supplies compact character or theme fixture catalogues.
 */
static app_provider_result_t	fixture_load_catalogue(void *userdata,
	app_catalogue_kind_t kind, app_catalogue_view_model_t *view)
{
	static const char	*mirurun_abilities[4] = {
		"Mirurun", "Inversion", "Pentaris", "Sirtet"
	};
	static const char	*mirurun_descriptions[4] = {
		"Removes player bottom 4 rows, not sent.",
		"Inverts opponent controls next 3 pieces.",
		"Sends 5 garbage lines.",
		"Inverts filled/empty normal cells in all occupied opponent rows."
	};
	static const char	*halloween_abilities[4] = {
		"Fry", "Dark", "Vampire", "Bomb"
	};
	static const char	*halloween_descriptions[4] = {
		"Fills bottom 3 rows, then clears/sends them after next piece.",
		"Blacks out opponent field except near active piece.",
		"Steals opponent crystals.",
		"Destroys random opponent-field blocks."
	};
	static const char	*princess_abilities[4] = {
		"Sol", "Mirror", "Paralysis", "Copy"
	};
	static const char	*princess_descriptions[4] = {
		"Clears 3 adjacent player-field columns, aimable, 3-second auto-fire.",
		"Steals opponent's next crystal power.",
		"Prevents opponent rotating next 3 pieces.",
		"Replaces player field with copy of opponent's."
	};
	static const char	*wolfman_abilities[4] = {
		"Cut", "Nue", "Pals", "Thwack"
	};
	static const char	*wolfman_descriptions[4] = {
		"Clears player's top 4 rows.",
		"Prevents opponent fast-dropping next 4 pieces.",
		"Incoming normal garbage lowers player stack briefly (power-raised lines excluded).",
		"For next 4 pieces, non-crystal blocks cascade after line clears."
	};

	(void)userdata;
	if (view == NULL || (kind != APP_CATALOGUE_CHARACTERS
			&& kind != APP_CATALOGUE_THEMES))
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->kind = kind;
	if (kind == APP_CATALOGUE_CHARACTERS)
	{
		view->count = 4;
		set_catalogue_item(&view->items[0], "mirurun", "Mirurun", 0,
			true, true);
		set_character_details(&view->items[0], DEFAULT_MIRURUN_PATH,
			mirurun_abilities, mirurun_descriptions);
		set_catalogue_item(&view->items[1], "halloween", "Halloween", 0,
			true, false);
		set_character_details(&view->items[1], HALLOWEEN_PORTRAIT_PATH,
			halloween_abilities, halloween_descriptions);
		set_catalogue_item(&view->items[2], "princess", "Princess", 0,
			true, false);
		set_character_details(&view->items[2], PRINCESS_PORTRAIT_PATH,
			princess_abilities, princess_descriptions);
		set_catalogue_item(&view->items[3], "wolfman", "Wolf-man", 0,
			true, false);
		set_character_details(&view->items[3], WOLFMAN_PORTRAIT_PATH,
			wolfman_abilities, wolfman_descriptions);
	}
	else
	{
		view->count = SETTINGS_THEME_SLOTS;
		set_catalogue_item(&view->items[0], "classic", "Classic", 0,
			true, true);
		snprintf(view->items[0].portrait_asset,
			sizeof(view->items[0].portrait_asset), "%s",
			SETTINGS_THEME_CLASSIC_PREVIEW_PATH);
		set_catalogue_item(&view->items[1], "design_ai_university",
			"Design AI University", 0, true, false);
		snprintf(view->items[1].portrait_asset,
			sizeof(view->items[1].portrait_asset), "%s",
			SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH);
		set_catalogue_item(&view->items[2], "snowman",
			"Do You Wanna Build a Snowman", 0, true, false);
		snprintf(view->items[2].portrait_asset,
			sizeof(view->items[2].portrait_asset), "%s",
			SETTINGS_THEME_SNOWMAN_PREVIEW_PATH);
		set_catalogue_item(&view->items[3], "haaland", "Haaland", 0,
			true, false);
		snprintf(view->items[3].portrait_asset,
			sizeof(view->items[3].portrait_asset), "%s",
			SETTINGS_THEME_HAALAND_PREVIEW_PATH);
		set_catalogue_item(&view->items[4], "al_merqaedes",
			"Al Merqaedes F1 Team", 0, true, false);
		snprintf(view->items[4].portrait_asset,
			sizeof(view->items[4].portrait_asset), "%s",
			SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH);
		set_catalogue_item(&view->items[5], "nuclear_ghandi",
			"Nuclear Ghandi", 0, true, false);
		snprintf(view->items[5].portrait_asset,
			sizeof(view->items[5].portrait_asset), "%s",
			SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH);
		set_catalogue_item(&view->items[6], "clauding", "Clauding", 0,
			true, false);
		snprintf(view->items[6].portrait_asset,
			sizeof(view->items[6].portrait_asset), "%s",
			SETTINGS_THEME_CLAUDING_PREVIEW_PATH);
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
		"SoftDrop", "LineDancer", "GhostPiece", "StackWitch",
		"NeonMino", "TinyTSpin"
	};
	static const uint64_t	scores[] = {
		980500, 744200, 631900, 525400, 499200, 410800, 388600, 302100,
		276400, 245900
	};
	int						index;

	(void)userdata;
	if (view == NULL)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	view->count = APP_LEADERBOARD_MAX_ENTRIES;
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
 * Character power copy is sourced from the Tetris Battle Gaiden reference on
 * Tetris.wiki. Keeping it in the typed fixture makes hover UI data-driven.
 */
static void	set_character_details(app_catalogue_item_view_model_t *item,
	const char *portrait, const char *const abilities[4],
	const char *const descriptions[4])
{
	int	index;

	snprintf(item->portrait_asset, sizeof(item->portrait_asset), "%s",
		portrait);
	index = 0;
	while (index < APP_CHARACTER_ABILITY_COUNT)
	{
		snprintf(item->abilities[index].name,
			sizeof(item->abilities[index].name), "%s", abilities[index]);
		snprintf(item->abilities[index].description,
			sizeof(item->abilities[index].description), "%s",
			descriptions[index]);
		index++;
	}
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
	bool offline, app_screen_view_model_t *view)
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
	else if (screen == APP_SCREEN_HOME)
	{
		set_screen_copy(view, "Profile and local settings model");
		if (provider == NULL || provider->load_profile == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_profile(provider->userdata,
				&view->data.profile);
	}
	else if (screen == APP_SCREEN_SETTINGS)
	{
		if (offline)
		{
			set_screen_copy(view, "Local settings; account data unavailable");
			memset(&view->data.settings, 0, sizeof(view->data.settings));
			view->data.settings.offline = true;
			snprintf(view->data.settings.local_status,
				sizeof(view->data.settings.local_status),
				"OFFLINE LOCAL SETTINGS - NO ACCOUNT DATA");
			return (APP_PROVIDER_OK);
		}
		set_screen_copy(view, "Profile, inventory, and local settings");
		if (provider == NULL || provider->load_settings == NULL)
			return (APP_PROVIDER_UNAVAILABLE);
		result = provider->load_settings(provider->userdata,
			&view->data.settings);
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
