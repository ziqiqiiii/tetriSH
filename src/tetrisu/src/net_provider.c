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
static t_app_provider_result	stub_settings(void *userdata,
				t_app_settings_view_model *view);
static t_app_provider_result	stub_catalogue(void *userdata,
				t_app_catalogue_kind kind,
				t_app_catalogue_view_model *view);
static t_app_provider_result	stub_leaderboard(void *userdata,
				t_app_leaderboard_view_model *view);
static t_app_provider_result	net_load_lobby(void *userdata,
				t_app_lobby_view_model *view);
static t_app_provider_result	net_load_room(void *userdata,
				const char *room_id, t_app_room_view_model *view);
static t_app_provider_result	net_create_room(void *userdata,
				t_app_game_mode mode,
				t_app_room_view_model *view);
static t_app_game_mode		map_body_mode(t_body_mode mode);
static t_app_room_state		map_body_status(t_body_room_status status);
static t_app_game_mode		mode_from_room_name(const char *name);

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
	provider->load_settings = stub_settings;
	provider->load_catalogue = stub_catalogue;
	provider->preview_login = NULL;
	provider->load_leaderboard = stub_leaderboard;
	provider->load_lobby = net_load_lobby;
	provider->load_room = net_load_room;
	provider->create_room = net_create_room;
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

static t_app_provider_result	net_load_profile(void *userdata,
			t_app_profile_view_model *view)
{
	t_app_net_session	*session;

	if (userdata == NULL || view == NULL)
		return (APP_PROVIDER_INVALID);
	session = (t_app_net_session *)userdata;
	if (session->net.state < NET_AUTHED || session->username[0] == '\0')
		return (APP_PROVIDER_UNAVAILABLE);
	memset(view, 0, sizeof(*view));
	view->signed_in = true;
	snprintf(view->username, sizeof(view->username), "%s", session->username);
	view->score = (uint64_t)session->score;
	view->wallet_points = (int)session->wallet;
	view->rank = -1;
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	stub_settings(void *userdata,
		t_app_settings_view_model *view)
{
	(void)userdata;
	(void)view;
	return (APP_PROVIDER_UNAVAILABLE);
}

static t_app_provider_result	stub_catalogue(void *userdata,
		t_app_catalogue_kind kind, t_app_catalogue_view_model *view)
{
	(void)userdata;
	(void)kind;
	(void)view;
	return (APP_PROVIDER_UNAVAILABLE);
}

static t_app_provider_result	stub_leaderboard(void *userdata,
		t_app_leaderboard_view_model *view)
{
	(void)userdata;
	(void)view;
	return (APP_PROVIDER_UNAVAILABLE);
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

static t_app_game_mode	mode_from_room_name(const char *name)
{
	if (name == NULL)
		return (APP_GAME_MODE_NONE);
	if (strncmp(name, "BR-", 3) == 0 || strncmp(name, "arena", 5) == 0)
		return (APP_GAME_MODE_BATTLE_ROYALE);
	if (strncmp(name, "D-", 2) == 0 || strncmp(name, "duel", 4) == 0)
		return (APP_GAME_MODE_DOUBLE);
	return (APP_GAME_MODE_NONE);
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
	memset(view, 0, sizeof(*view));
	if (net_result_field(&result, "room", session->net.room,
			sizeof(session->net.room)) == NULL)
		return (APP_PROVIDER_INVALID);
	snprintf(view->id, sizeof(view->id), "%s", session->net.room);
	view->mode = mode;
	view->state = APP_ROOM_STATE_WAITING;
	view->capacity = (mode == APP_GAME_MODE_DOUBLE)
		? WAITING_ROOM_DOUBLE_PLAYERS : APP_ROOM_MAX_PLAYERS;
	view->required_players = (mode == APP_GAME_MODE_DOUBLE)
		? WAITING_ROOM_DOUBLE_PLAYERS : WAITING_ROOM_ROYALE_MIN_PLAYERS;
	view->player_count = 1;
	view->local_slot = 0;
	snprintf(view->players[0].username, sizeof(view->players[0].username),
		"%s", session->username);
	view->players[0].owner = true;
	view->players[0].ready = false;
	session->net.state = NET_IN_ROOM;
	return (APP_PROVIDER_OK);
}

static t_app_provider_result	net_load_room(void *userdata,
			const char *room_id, t_app_room_view_model *view)
{
t_app_net_session	*session;
	t_net_result		result;
	char			path[NET_PATH_MAX];
	char			field[32];
	const char		*role;
	int			slot;

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
	memset(view, 0, sizeof(*view));
	if (net_result_field(&result, "room", session->net.room,
			sizeof(session->net.room)) == NULL)
		snprintf(session->net.room, sizeof(session->net.room), "%s",
			room_id);
	snprintf(view->id, sizeof(view->id), "%s", session->net.room);
	view->mode = mode_from_room_name(session->net.room);
	if (net_result_field(&result, "slot", field, sizeof(field)) != NULL)
		slot = (int)strtol(field, NULL, 10);
	else
		slot = 0;
	view->local_slot = slot;
	if (slot >= 0 && slot < APP_ROOM_MAX_PLAYERS)
	{
		snprintf(view->players[slot].username,
			sizeof(view->players[slot].username), "%s", session->username);
		role = net_result_field(&result, "role", field, sizeof(field));
		view->players[slot].owner = (role != NULL && strcmp(role, "owner")
			== 0);
		view->players[slot].ready = false;
	}
	view->state = APP_ROOM_STATE_WAITING;
	view->capacity = (view->mode == APP_GAME_MODE_DOUBLE)
		? WAITING_ROOM_DOUBLE_PLAYERS : APP_ROOM_MAX_PLAYERS;
	view->required_players = (view->mode == APP_GAME_MODE_DOUBLE)
		? WAITING_ROOM_DOUBLE_PLAYERS : WAITING_ROOM_ROYALE_MIN_PLAYERS;
	/*
	 * tetrisd does not expose the full waiting-room roster; only the local
	 * slot is known. The remaining seats stay empty and the screen renders
	 * them as unoccupied, which is honest about what the server serves.
	 */
	view->player_count = 1;
	session->net.state = NET_IN_ROOM;
	return (APP_PROVIDER_OK);
}