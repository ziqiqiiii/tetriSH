#include "tetrisu.h"

/*
** The network-backed app data provider. One t_app_net_session (opened at
** CHECK_SERVER, bound at LOGIN, reused for Solo and the lobby) drives the
** vtable. Slots tetrisd does not serve yet return APP_PROVIDER_UNAVAILABLE so
** the screens surface their account-needed copy rather than a fabricated
** answer.
*/

// Static Functions
static t_app_provider_result	net_sign_up_action(void *userdata,
				const char *username, const char *password,
				const char *domain, t_app_auth_view_model *view);
static t_app_provider_result	net_login_action(void *userdata,
				const char *username, const char *password,
				const char *domain, t_app_auth_view_model *view);
static t_app_provider_result	net_load_profile(void *userdata,
				t_app_profile_view_model *view);
static t_app_provider_result	net_load_settings(void *userdata,
				t_app_settings_view_model *view);
static t_app_provider_result	net_load_catalogue(void *userdata,
				t_app_catalogue_kind kind,
				t_app_catalogue_view_model *view);
static t_app_provider_result	net_buy_item(void *userdata,
				t_app_catalogue_kind kind, uint32_t item_id,
				t_app_settings_view_model *view);
static t_app_provider_result	net_equip_item(void *userdata,
				t_app_catalogue_kind kind, uint32_t item_id,
				t_app_settings_view_model *view);
static t_app_provider_result	write_result(int status);
static void			build_settings(t_app_net_session *session,
				const t_body_profile *profile,
				t_app_settings_view_model *view);
static const t_body_catalogue	*session_catalogue(t_app_net_session *session);
static void			apply_profile(t_app_net_session *session,
				const t_body_profile *profile,
				t_app_profile_view_model *view);
static void			fill_catalogue(const t_body_profile *profile,
				const t_body_catalogue *store, t_app_catalogue_kind kind,
				t_app_catalogue_view_model *view);
static void			fill_item(t_app_catalogue_item_view_model *item,
				const t_body_catalogue_item *row, t_app_catalogue_kind kind,
				const t_body_profile *profile, const t_theme_assets *assets);
static bool			owns(const uint32_t *ids, size_t count, uint32_t id);
static const char		*item_name(const t_body_catalogue_item *rows,
				size_t count, uint32_t id);
static void			character_portrait(const t_body_profile *profile,
				const t_body_catalogue *store, const char *slug, char *out,
				size_t cap);
static t_app_provider_result	net_load_leaderboard(void *userdata,
				t_app_leaderboard_view_model *view);
static t_app_provider_result	net_load_lobby(void *userdata,
				t_app_lobby_view_model *view);
static t_app_provider_result	net_load_room(void *userdata,
				const char *room_id, t_app_room_view_model *view);
static t_app_provider_result	net_create_room(void *userdata,
				t_app_game_mode mode,
				t_app_room_view_model *view);
static t_app_provider_result	net_refresh_room(void *userdata,
				const char *room_id, t_app_room_view_model *view);
static t_app_provider_result	net_leave_room(void *userdata,
				const char *room_id);
static t_app_provider_result	net_ready_room(void *userdata,
					const char *room_id, bool ready, uint32_t character,
					t_app_room_view_model *view);
static t_app_provider_result	net_start_room(void *userdata,
				const char *room_id, t_app_room_view_model *view);
static t_app_provider_result	net_send_chat(void *userdata,
				const char *room_id, const char *text,
				t_app_room_view_model *view);
static void			fill_chat(const t_net_client *net,
				t_app_room_view_model *view);
static t_app_game_mode		map_body_mode(t_body_mode mode);
static t_app_room_state		map_body_status(t_body_room_status status);
static bool			map_room_snapshot(t_app_net_session *session,
				const t_body_room *body, t_app_room_view_model *view);
static bool			session_ready_for_credentials(t_app_net_session *session);

void	app_net_provider_init(t_app_data_provider *provider,
		t_app_net_session *session)
{
	if (provider == NULL)
		return ;
	memset(provider, 0, sizeof(*provider));
	provider->name = "network";
	provider->local_fixtures = false;
	provider->userdata = session;
	provider->login = net_login_action;
	provider->sign_up = net_sign_up_action;
	provider->load_profile = net_load_profile;
	provider->load_settings = net_load_settings;
	provider->load_catalogue = net_load_catalogue;
	provider->preview_login = NULL;
	provider->load_leaderboard = net_load_leaderboard;
	provider->load_lobby = net_load_lobby;
	provider->load_room = net_load_room;
	provider->create_room = net_create_room;
	provider->refresh_room = net_refresh_room;
	provider->leave_room = net_leave_room;
	provider->start_room = net_start_room;
	provider->ready_room = net_ready_room;
	provider->send_chat = net_send_chat;
	provider->buy_item = net_buy_item;
	provider->equip_item = net_equip_item;
}

static t_app_provider_result	net_sign_up_action(void *userdata,
			const char *username, const char *password,
			const char *domain, t_app_auth_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;

	(void)domain;
	if (userdata == NULL || username == NULL || password == NULL
		|| username[0] == '\0' || password[0] == '\0' || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (!session_ready_for_credentials(session))
		return (APP_PROVIDER_UNAVAILABLE);
	memset(&result, 0, sizeof(result));
	if (net_signup(&session->net, username, password, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 201)
		return (APP_PROVIDER_INVALID);
	memset(view, 0, sizeof(*view));
	snprintf(session->username, sizeof(session->username), "%s", username);
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	net_login_action(void *userdata,
			const char *username, const char *password,
			const char *domain, t_app_auth_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	char				field[32];

	(void)domain;
	if (userdata == NULL || username == NULL || password == NULL
		|| username[0] == '\0' || password[0] == '\0' || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (!session_ready_for_credentials(session))
		return (APP_PROVIDER_UNAVAILABLE);
	memset(&result, 0, sizeof(result));
	if (net_login(&session->net, username, password, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_INVALID);
	snprintf(session->username, sizeof(session->username), "%s",
		session->net.username);
	session->score = 0;
	session->wallet = 0;
	if (net_result_field(&result, "score", field, sizeof(field)) != NULL)
		session->score = (int64_t)strtoll(field, NULL, 10);
	if (net_result_field(&result, "wallet", field, sizeof(field)) != NULL)
		session->wallet = (int64_t)strtoll(field, NULL, 10);
	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "%s", username);
	snprintf(view->message, sizeof(view->message), "WELCOME TO TETRISU!");
	return (APP_PROVIDER_OK);
}

/*
** PROFILE /player/<pid> - the account, as tetrisd holds it.
**
** The wallet, the rank and the loadout used to be guessed here: the score and
** wallet were whatever LOGIN happened to mention, and rank was -1 because
** nothing knew it. All three are now read, which is what makes the number on
** the Marketplace the number a purchase is actually charged against.
*/
static t_app_provider_result	net_load_profile(void *userdata,
			t_app_profile_view_model *view)
{
	t_app_net_session	*session;
	t_body_profile		profile;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED || session->username[0] == '\0')
		return (APP_PROVIDER_UNAVAILABLE);
	if (net_profile(&session->net, &profile) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	apply_profile(session, &profile, view);
	return (APP_PROVIDER_OK);
}

/*
** Settings and the Marketplace are the same model: the profile that owns the
** wallet, and both catalogues marked with what this player owns and has
** equipped. Two reads rather than one, because ownership is a fact about the
** account and the price list is a fact about the server - and only the first
** of them changes when somebody buys something.
*/
static t_app_provider_result	net_load_settings(void *userdata,
		t_app_settings_view_model *view)
{
	t_app_net_session		*session;
	const t_body_catalogue	*store;
	t_body_profile			profile;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	if (net_profile(&session->net, &profile) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	store = session_catalogue(session);
	if (store == NULL)
		return (APP_PROVIDER_UNAVAILABLE);
	build_settings(session, &profile, view);
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	net_load_catalogue(void *userdata,
		t_app_catalogue_kind kind, t_app_catalogue_view_model *view)
{
	t_app_net_session		*session;
	const t_body_catalogue	*store;
	t_body_profile			profile;

	if (userdata == NULL || view == NULL
		|| (kind != APP_CATALOGUE_CHARACTERS && kind != APP_CATALOGUE_THEMES))
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	if (net_profile(&session->net, &profile) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	store = session_catalogue(session);
	if (store == NULL)
		return (APP_PROVIDER_UNAVAILABLE);
	fill_catalogue(&profile, store, kind, view);
	return (APP_PROVIDER_OK);
}

/*
** BUY and EQUIP. Both answer with the profile tetrisd holds afterwards, so
** the caller replaces its copy rather than patching it: the wallet, the owned
** list and the equipped id move together, and a screen that debited its own
** balance would be showing a number nobody had agreed to.
**
** A refusal writes nothing at all. The screen keeps drawing the account as it
** was, which is what it still is.
*/
static t_app_provider_result	net_buy_item(void *userdata,
		t_app_catalogue_kind kind, uint32_t item_id,
		t_app_settings_view_model *view)
{
	t_app_net_session	*session;
	t_body_profile		profile;
	int					status;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	status = 0;
	if (net_buy(&session->net, kind == APP_CATALOGUE_CHARACTERS, item_id,
			&profile, &status) != 0)
		return (write_result(status));
	build_settings(session, &profile, view);
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	net_equip_item(void *userdata,
		t_app_catalogue_kind kind, uint32_t item_id,
		t_app_settings_view_model *view)
{
	t_app_net_session	*session;
	t_body_profile		profile;
	int					status;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	status = 0;
	if (net_equip(&session->net, kind == APP_CATALOGUE_CHARACTERS, item_id,
			&profile, &status) != 0)
		return (write_result(status));
	build_settings(session, &profile, view);
	return (APP_PROVIDER_OK);
}

/**
 * @brief Maps a refused store write onto a provider result.
 *
 * A status tetrisd chose is the server saying no to this request, which the
 * screen has copy for; anything else means the answer never arrived, which is
 * a different thing to tell the player.
 *
 * @param status The status tetrisd answered with, or 0 when none did.
 * @return INVALID for a refusal, UNAVAILABLE for a transport failure.
 */
static t_app_provider_result	write_result(int status)
{
	if (status >= 400 && status < 500)
		return (APP_PROVIDER_INVALID);
	return (APP_PROVIDER_UNAVAILABLE);
}

/**
 * @brief Builds the whole Settings/Marketplace model from one profile.
 *
 * Shared by the load and by both writes, so what a purchase leaves on screen
 * is built by the same code that built what was there before it - the wallet,
 * the owned flags and the equipped flags cannot drift apart because they are
 * never written separately.
 *
 * The local controls are deliberately left zeroed: volume and renderer mode
 * describe this terminal, not this account, and the caller re-applies them.
 *
 * @param session Signed-in session holding the cached catalogue.
 * @param profile The profile tetrisd answered with.
 * @param view Receives the model.
 */
static void	build_settings(t_app_net_session *session,
		const t_body_profile *profile, t_app_settings_view_model *view)
{
	const t_body_catalogue	*store;

	memset(view, 0, sizeof(*view));
	store = session_catalogue(session);
	if (store == NULL)
		return ;
	apply_profile(session, profile, &view->profile);
	fill_catalogue(profile, store, APP_CATALOGUE_CHARACTERS,
		&view->characters);
	fill_catalogue(profile, store, APP_CATALOGUE_THEMES, &view->themes);
	view->signed_in = true;
}

/**
 * @brief Returns the store front, fetching it the first time it is asked for.
 *
 * @param session Signed-in session holding the cache.
 * @return The catalogue, or NULL when it could not be read.
 */
static const t_body_catalogue	*session_catalogue(t_app_net_session *session)
{
	if (session->has_catalogue)
		return (&session->catalogue);
	if (net_catalogue(&session->net, &session->catalogue) != 0)
		return (NULL);
	session->has_catalogue = true;
	return (&session->catalogue);
}

/**
 * @brief Maps one decoded profile onto the view model every screen reads.
 *
 * The session's cached score and wallet are refreshed here too, because the
 * lobby's identity strip reads them straight off the session rather than
 * through a profile load.
 *
 * @param session Session whose cached totals are updated.
 * @param profile The profile tetrisd answered with.
 * @param view Receives the mapped profile.
 */
static void	apply_profile(t_app_net_session *session,
		const t_body_profile *profile, t_app_profile_view_model *view)
{
	const t_body_catalogue	*store;
	const char				*name;

	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "%s", profile->username);
	view->score = profile->score;
	view->wallet_points = (int)profile->wallet;
	view->rank = profile->rank;
	session->score = (int64_t)profile->score;
	session->wallet = (int64_t)profile->wallet;
	snprintf(session->username, sizeof(session->username), "%s",
		profile->username);
	store = session_catalogue(session);
	if (store == NULL)
		return ;
	name = item_name(store->characters, store->character_count,
			profile->equipped_character);
	if (name != NULL)
		snprintf(view->character, sizeof(view->character), "%s", name);
	name = item_name(store->themes, store->theme_count,
			profile->equipped_theme);
	if (name != NULL)
		snprintf(view->theme, sizeof(view->theme), "%s", name);
	character_portrait(profile, store,
		catalogue_character_slug(profile->equipped_character),
		view->portrait_asset, sizeof(view->portrait_asset));
}

/**
 * @brief Fills one catalogue panel from the store front and the account.
 *
 * Price and name come from the store, owned and equipped from the profile,
 * and the artwork from this machine - the three sources the screen needs and
 * the reason a tile cannot be drawn from any one of them alone.
 *
 * @param profile The account, for ownership and the loadout.
 * @param store The store front, for ids, names and prices.
 * @param kind Which panel is being filled.
 * @param view Receives the panel.
 */
static void	fill_catalogue(const t_body_profile *profile,
		const t_body_catalogue *store, t_app_catalogue_kind kind,
		t_app_catalogue_view_model *view)
{
	const t_body_catalogue_item	*rows;
	t_theme_assets				assets;
	size_t						count;
	size_t						i;

	memset(view, 0, sizeof(*view));
	view->kind = kind;
	/* Built once for the panel rather than per tile: every character on the
	** shelf is drawn in the same equipped theme, and the answer does not
	** change between rows. */
	tetrisu_theme_assets_build_by_id(&assets, profile->equipped_theme,
		item_name(store->themes, store->theme_count, profile->equipped_theme));
	if (kind == APP_CATALOGUE_CHARACTERS)
	{
		rows = store->characters;
		count = store->character_count;
	}
	else
	{
		rows = store->themes;
		count = store->theme_count;
	}
	if (count > APP_CATALOGUE_MAX_ITEMS)
		count = APP_CATALOGUE_MAX_ITEMS;
	i = 0;
	while (i < count)
	{
		fill_item(&view->items[i], &rows[i], kind, profile, &assets);
		i++;
	}
	view->count = (int)count;
}

/**
 * @brief Writes one shelf tile from its catalogue row and the account.
 *
 * A character's artwork belongs to the theme, so its tile is drawn from the
 * equipped theme's assets rather than from any file the character owns on its
 * own - which is why a panel of characters needs a theme to be filled at all.
 * Leaving that out is what emptied every character tile on the shelves: the
 * row carried an id, a name and a price, and no path for the renderer to draw.
 *
 * @param item The tile to fill.
 * @param row The catalogue row tetrisd sent.
 * @param kind Which catalogue the row belongs to.
 * @param profile The account, for ownership and the loadout.
 * @param assets The equipped theme's artwork, already resolved.
 */
static void	fill_item(t_app_catalogue_item_view_model *item,
		const t_body_catalogue_item *row, t_app_catalogue_kind kind,
		const t_body_profile *profile, const t_theme_assets *assets)
{
	const char	*slug;
	const char	*path;

	memset(item, 0, sizeof(*item));
	item->item_id = row->id;
	item->price = (int)row->price;
	snprintf(item->name, sizeof(item->name), "%s", row->name);
	if (kind == APP_CATALOGUE_CHARACTERS)
	{
		slug = catalogue_character_slug(row->id);
		item->owned = owns(profile->owned_characters,
				profile->owned_character_count, row->id);
		item->equipped = (profile->equipped_character == row->id);
		catalogue_character_abilities(row->id, item->abilities);
		path = tetrisu_theme_character_path(assets, slug);
		if (path != NULL)
			snprintf(item->portrait_asset, sizeof(item->portrait_asset), "%s",
				path);
	}
	else
	{
		slug = catalogue_theme_slug(row->id);
		item->owned = owns(profile->owned_themes, profile->owned_theme_count,
				row->id);
		item->equipped = (profile->equipped_theme == row->id);
		if (catalogue_theme_preview(row->id) != NULL)
			snprintf(item->portrait_asset, sizeof(item->portrait_asset), "%s",
				catalogue_theme_preview(row->id));
	}
	if (slug != NULL)
		snprintf(item->id, sizeof(item->id), "%s", slug);
}

/**
 * @brief Reports whether an owned-id list holds one id.
 *
 * @param ids The owned ids.
 * @param count How many ids the list holds.
 * @param id The id to look for.
 * @return true when the id is present.
 */
static bool	owns(const uint32_t *ids, size_t count, uint32_t id)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		if (ids[i] == id)
			return (true);
		i++;
	}
	return (false);
}

/**
 * @brief Looks up one catalogue row's name by its id.
 *
 * A scan, not an index: catalogue ids carry gaps, so a row's id is not its
 * position in the answer.
 *
 * @param rows The catalogue rows.
 * @param count How many rows there are.
 * @param id The id to look for.
 * @return The name, or NULL when no row carries that id.
 */
static const char	*item_name(const t_body_catalogue_item *rows, size_t count,
			uint32_t id)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		if (rows[i].id == id)
			return (rows[i].name);
		i++;
	}
	return (NULL);
}

/**
 * @brief Resolves the equipped character's portrait within the equipped theme.
 *
 * A character's artwork belongs to the theme, so the portrait is whichever
 * file the equipped theme ships for that character - which is why both ids
 * are needed to answer what looks like a question about one of them.
 *
 * @param profile The account, for both equipped ids.
 * @param store The store front, for the theme's display name.
 * @param slug The equipped character's local slug; may be NULL.
 * @param out Buffer receiving the path.
 * @param cap Size of out.
 */
static void	character_portrait(const t_body_profile *profile,
		const t_body_catalogue *store, const char *slug, char *out, size_t cap)
{
	t_theme_assets	assets;
	const char		*path;

	if (slug == NULL)
		return ;
	tetrisu_theme_assets_build_by_id(&assets, profile->equipped_theme,
		item_name(store->themes, store->theme_count, profile->equipped_theme));
	path = tetrisu_theme_character_path(&assets, slug);
	if (path != NULL)
		snprintf(out, cap, "%s", path);
}

/*
** LEADERBOARD /leaderboard - the ranking tetrisd has been recording all
** along.
**
** Every finished or forfeited game is written to the store, so
** the scores a player earns in Solo against a live server were already
** counted; this screen simply had no way to ask for them and answered "not
** served" while the preview build showed fixtures.
**
** An empty table is a successful read, not a failure: a server nobody has
** finished a game on has a leaderboard, and it has nought rows.
*/
static t_app_provider_result	net_load_leaderboard(void *userdata,
		t_app_leaderboard_view_model *view)
{
	t_app_net_session		*session;
	t_net_result			result;
	t_body_leaderboard_row	rows[APP_LEADERBOARD_MAX_ENTRIES];
	size_t					count;
	int						i;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "LEADERBOARD",
			TETRISU_ROUTE_LEADERBOARD, NULL, &result) != 0
		|| result.status != 200)
		return (APP_PROVIDER_UNAVAILABLE);
	count = 0;
	if (body_leaderboard_decode(result.body, strlen(result.body), rows,
			APP_LEADERBOARD_MAX_ENTRIES, &count) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	memset(view, 0, sizeof(*view));
	view->count = (int)count;
	i = 0;
	while (i < view->count)
	{
		view->entries[i].position = rows[i].rank;
		snprintf(view->entries[i].username,
			sizeof(view->entries[i].username), "%s", rows[i].username);
		view->entries[i].score = rows[i].score;
		i++;
	}
	return (APP_PROVIDER_OK);
}

static t_app_game_mode	map_body_mode(t_body_mode mode)
{
	if (mode == BODY_MODE_DOUBLE)
		return (APP_GAME_MODE_DOUBLE);
	if (mode == BODY_MODE_BATTLE_ROYALE)
		return (APP_GAME_MODE_BATTLE_ROYALE);
	return (APP_GAME_MODE_NONE);
}

static t_app_room_state	map_body_status(t_body_room_status status)
{
	if (status == BODY_ROOM_READY)
		return (APP_ROOM_STATE_READY);
	if (status == BODY_ROOM_IN_GAME)
		return (APP_ROOM_STATE_IN_GAME);
	if (status == BODY_ROOM_FINISHED)
		return (APP_ROOM_STATE_FINISHED);
	return (APP_ROOM_STATE_WAITING);
}

/*
** Credentials need a connection that is not signed in as anybody yet.
**
** tetrisd answers a LOGIN on an already-authenticated connection with 409:
** identity belongs to the socket, so the only way to sign in as
** somebody else - or as the same player again after backing out to the auth
** screen - is on a socket that has not claimed a player. And a session lost
** while a game was running leaves the handle offline while the form still
** reads ONLINE, because nothing told the form.
**
** Both cases are the same repair: throw this connection away and dial again.
** The alternative was what the player saw - a filled-in form that refused
** every sign-in until they retyped the server address to force a reconnect.
*/
static bool	session_ready_for_credentials(t_app_net_session *session)
{
	if (session->net.state == NET_CONNECTED)
		return (true);
	if (session->cfg.host[0] == '\0')
		net_config_load(&session->cfg);
	session->connected = net_connect(&session->net, &session->cfg) == 0;
	return (session->connected);
}

static t_app_provider_result	net_load_lobby(void *userdata,
			t_app_lobby_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	t_body_room_row		rows[APP_LOBBY_MAX_ROOMS];
	size_t				count;
	int					i;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	memset(view, 0, sizeof(*view));
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "LIST", TETRISU_ROUTE_ROOMS, NULL,
			&result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_UNAVAILABLE);
	count = 0;
	if (body_rooms_decode(result.body, strlen(result.body), rows,
			APP_LOBBY_MAX_ROOMS, &count) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	view->count = (int)count;
	i = 0;
	while (i < view->count)
	{
		snprintf(view->rooms[i].id, sizeof(view->rooms[i].id), "%s",
			rows[i].name);
		snprintf(view->rooms[i].owner, sizeof(view->rooms[i].owner),
			"%s", rows[i].owner);
		view->rooms[i].mode = map_body_mode(rows[i].mode);
		view->rooms[i].state = map_body_status(rows[i].status);
		view->rooms[i].players = rows[i].players;
		view->rooms[i].capacity = rows[i].slot_count;
		i++;
	}
	view->profile.signed_in = true;
	snprintf(view->profile.username, sizeof(view->profile.username), "%s",
		session->username);
	view->profile.score = (uint64_t)session->score;
	view->profile.wallet_points = (int)session->wallet;
	view->profile.rank = -1;
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	net_create_room(void *userdata,
				t_app_game_mode mode, t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	char			body[32];

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	if (mode != APP_GAME_MODE_DOUBLE && mode != APP_GAME_MODE_BATTLE_ROYALE)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	if (mode == APP_GAME_MODE_DOUBLE)
		snprintf(body, sizeof(body), "mode double\n");
	else
		snprintf(body, sizeof(body), "mode battle-royale\n");
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "JOIN", TETRISU_ROUTE_ROOMS, body,
			&result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 201)
		return (APP_PROVIDER_INVALID);
	if (net_result_field(&result, "room", session->net.room,
			sizeof(session->net.room)) == NULL)
		return (APP_PROVIDER_INVALID);
	session->net.state = NET_IN_ROOM;
	return (net_refresh_room(userdata, session->net.room, view));
}

static t_app_provider_result	net_load_room(void *userdata,
				const char *room_id, t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	char			path[NET_PATH_MAX];

	if (userdata == NULL || view == NULL || room_id == NULL
		|| room_id[0] == '\0')
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED)
		return (APP_PROVIDER_UNAVAILABLE);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room_id);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "JOIN", path, NULL, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200 && result.status != 201)
		return (APP_PROVIDER_INVALID);
	if (net_result_field(&result, "room", session->net.room,
			sizeof(session->net.room)) == NULL)
		snprintf(session->net.room, sizeof(session->net.room), "%s",
			room_id);
	session->net.state = NET_IN_ROOM;
	return (net_refresh_room(userdata, session->net.room, view));
}

/**
 * @brief Reads an authoritative waiting-room snapshot without joining again.
 *
 * @param userdata Network session.
 * @param room_id Room currently occupied by this client.
 * @param view Receives the decoded room model.
 * @return Provider result.
 */
static t_app_provider_result	net_refresh_room(void *userdata,
	const char *room_id, t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	t_body_room			snapshot;
	char				path[NET_PATH_MAX];

	if (userdata == NULL || room_id == NULL || room_id[0] == '\0'
		|| view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_IN_ROOM)
		return (APP_PROVIDER_UNAVAILABLE);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room_id);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "LIST", path, NULL, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_INVALID);
	if (body_room_decode(result.body, strlen(result.body), &snapshot) != 0
		|| !map_room_snapshot(session, &snapshot, view))
		return (APP_PROVIDER_INVALID);
	/*
	 * Binding the play path here rather than at START is what lets a player
	 * who did not start the match receive it. The path is what every pushed
	 * STATE is matched against, and it used to be written only by the call
	 * that takes a room for Solo - so a joiner had none, and discarded every
	 * frame the server sent them. It also makes the first snapshot the signal
	 * that the match has begun, which beats waiting half a second for a poll
	 * of the room to say so.
	 */
	(void)net_match_join(&session->net, room_id);
	fill_chat(&session->net, view);
	return (APP_PROVIDER_OK);
}

/**
 * @brief Posts one line to the room's feed and re-reads the room.
 *
 * Nothing is appended locally. The server sends the sender their own line
 * along with everybody else's, so the refresh below is what puts it on the
 * screen - in the server's order, with the server's sequence number, exactly
 * as the other players will see it.
 *
 * @param userdata Network session.
 * @param room_id Room the message is for.
 * @param text The message.
 * @param view Receives the room as it stands afterwards, feed included.
 * @return Provider result; INVALID when the server refused the message.
 */
static t_app_provider_result	net_send_chat(void *userdata,
	const char *room_id, const char *text, t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;

	if (userdata == NULL || room_id == NULL || room_id[0] == '\0'
		|| text == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_IN_ROOM)
		return (APP_PROVIDER_UNAVAILABLE);
	if (net_chat_send(&session->net, room_id, text, &result) != 0)
	{
		if (result.status == 0)
			return (APP_PROVIDER_UNAVAILABLE);
		return (APP_PROVIDER_INVALID);
	}
	return (net_refresh_room(userdata, room_id, view));
}

/**
 * @brief Copies the client's feed ring onto the room a screen will draw.
 *
 * The ring is longer than the panel, so the tail is what is taken: a feed
 * shows what was said most recently, and dropping the top of it is what the
 * ring itself already does one line at a time.
 *
 * @param net Client holding the received lines.
 * @param view Room model receiving them, oldest of the kept lines first.
 */
static void	fill_chat(const t_net_client *net, t_app_room_view_model *view)
{
	const t_body_chat	*line;
	size_t				held;
	size_t				first;
	size_t				index;

	held = net_chat_held(net);
	first = 0;
	if (held > APP_ROOM_CHAT_MAX)
		first = held - APP_ROOM_CHAT_MAX;
	view->chat_count = 0;
	index = first;
	while (index < held)
	{
		line = net_chat_at(net, index);
		if (line == NULL)
			break ;
		view->chat[view->chat_count].system = line->system;
		snprintf(view->chat[view->chat_count].author,
			sizeof(view->chat[view->chat_count].author), "%s", line->sender);
		/*
		 * A line the server will carry is longer than one the panel draws, so
		 * the cut is stated here rather than left to snprintf. It is the
		 * display that is short, not the message.
		 */
		snprintf(view->chat[view->chat_count].text,
			sizeof(view->chat[view->chat_count].text), "%.*s",
			APP_ROOM_CHAT_TEXT_MAX - 1, line->text);
		view->chat_count++;
		index++;
	}
}

/**
 * @brief Leaves the server room and clears the client-side binding.
 *
 * @param userdata Network session.
 * @param room_id Room currently occupied by this client.
 * @return Provider result.
 */
static t_app_provider_result	net_leave_room(void *userdata,
	const char *room_id)
{
	t_app_net_session	*session;
	t_net_result		result;
	char				path[NET_PATH_MAX];

	if (userdata == NULL || room_id == NULL || room_id[0] == '\0')
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_IN_ROOM)
		return (APP_PROVIDER_UNAVAILABLE);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room_id);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "LEAVE", path, NULL, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_INVALID);
	session->net.room[0] = '\0';
	session->net.play_path[0] = '\0';
	session->net.state = NET_AUTHED;
	session->net.has_state = false;
	session->net.last_seq = 0;
	session->net.applied_seq = 0;
	net_chat_reset(&session->net);
	return (APP_PROVIDER_OK);
}

/**
 * @brief Starts the room on the server and returns its post-start snapshot.
 *
 * @param userdata Network session.
 * @param room_id Room the owner is starting.
 * @param view Receives the authoritative in-game room model.
 * @return Provider result.
 */
/**
 * @brief Declares this client ready, or withdraws it, and re-reads the room.
 *
 * The answer is the room as the server now sees it, so the roster the screen
 * draws is the one every other player is being shown - which is the whole
 * point of the declaration having left the client.
 *
 * @param userdata Network session.
 * @param room_id Room the declaration is for.
 * @param ready true to declare ready, false to withdraw it.
 * @param view Receives the room as it stands afterwards.
 * @return Provider result.
 */
static t_app_provider_result	net_ready_room(void *userdata,
	const char *room_id, bool ready, uint32_t character,
	t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	char				path[NET_PATH_MAX];
	char				body[64];

	if (userdata == NULL || room_id == NULL || room_id[0] == '\0'
		|| view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_IN_ROOM)
		return (APP_PROVIDER_UNAVAILABLE);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room_id);
	/*
	 * A character of 0 is left off the body rather than sent as a zero. The
	 * server reads an absent field as "whatever the account has equipped",
	 * which is the honest thing for a client with nothing to offer to say.
	 */
	if (character != 0)
		snprintf(body, sizeof(body), "ready %d\ncharacter %u\n",
			ready ? 1 : 0, character);
	else
		snprintf(body, sizeof(body), "ready %d\n", ready ? 1 : 0);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "READY", path, body, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_INVALID);
	return (net_refresh_room(userdata, room_id, view));
}

static t_app_provider_result	net_start_room(void *userdata,
	const char *room_id, t_app_room_view_model *view)
{
	t_app_net_session	*session;
	t_net_result		result;
	char				path[NET_PATH_MAX];

	if (userdata == NULL || room_id == NULL || room_id[0] == '\0'
		|| view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state != NET_IN_ROOM)
		return (APP_PROVIDER_UNAVAILABLE);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room_id);
	memset(&result, 0, sizeof(result));
	if (net_request(&session->net, "START", path, NULL, &result) != 0)
		return (APP_PROVIDER_UNAVAILABLE);
	if (result.status != 200)
		return (APP_PROVIDER_INVALID);
	session->net.state = NET_IN_GAME;
	return (net_refresh_room(userdata, room_id, view));
}

/**
 * @brief Maps a shared room body into the waiting-room presentation model.
 *
 * Members are compacted in server slot order. The UI needs stable ordering,
 * not the sparse server index; the local member is found by authenticated id.
 *
 * @param session Authenticated network session.
 * @param body Decoded shared room body.
 * @param view Presentation model to replace.
 * @return true when the body fits and contains the local player.
 */
static bool	map_room_snapshot(t_app_net_session *session,
	const t_body_room *body, t_app_room_view_model *view)
{
	size_t	index;

	if (body->member_count > APP_ROOM_MAX_PLAYERS
		|| body->slot_count > APP_ROOM_MAX_PLAYERS)
		return (false);
	memset(view, 0, sizeof(*view));
	snprintf(view->id, sizeof(view->id), "%s", body->name);
	view->mode = map_body_mode(body->mode);
	view->state = map_body_status(body->status);
	view->required_players = body->min_to_start;
	view->capacity = body->slot_count;
	view->player_count = (int)body->member_count;
	view->local_slot = -1;
	index = 0;
	while (index < body->member_count)
	{
		snprintf(view->players[index].username,
			sizeof(view->players[index].username), "%s",
			body->members[index].username);
		view->players[index].owner = body->members[index].owner;
		view->players[index].ready = body->members[index].ready;
		if (body->members[index].player_id == session->net.player_id)
			view->local_slot = (int)index;
		index++;
	}
	if (view->local_slot < 0)
		return (false);
	if (view->state == APP_ROOM_STATE_IN_GAME)
	{
		session->net.state = NET_IN_GAME;
		snprintf(session->net.play_path, sizeof(session->net.play_path),
			"%s%s/player/%llu", TETRISU_ROUTE_ROOM, body->name,
			(unsigned long long)session->net.player_id);
	}
	else
		session->net.state = NET_IN_ROOM;
	return (true);
}
