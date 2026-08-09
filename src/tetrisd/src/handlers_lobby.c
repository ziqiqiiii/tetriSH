#include "tetrisd.h"

// Static Functions
static size_t		list_rooms(t_server *srv, t_body_room_row *rows);
static int			read_mode(t_request_context *ctx, t_game_mode *out);
static int			create_room(t_request_context *ctx, t_game_mode mode);
static const char	*room_name_from_path(const t_request_context *ctx);
static int			join_room(t_request_context *ctx, const char *name);
static t_server_room	*addressed_room(t_request_context *ctx, const char **name);
static int			start_status(t_request_context *ctx, t_start_verdict verdict);

/**
 * @brief Checks that the request really comes from the player it claims.
 *
 * libtetrissh authenticates the server and the bytes, not the client's
 * claimed identity, so Player-Id is compared against the player bound to this
 * connection at LOGIN - a forged header buys nothing (ADR-0001).
 *
 * @param ctx Request context.
 * @return true when the connection is authenticated and the header matches.
 */
bool	request_is_authorised(t_request_context *ctx)
{
	const char	*header;
	char		*end;
	uint64_t	claimed;

	if (ctx == NULL || ctx->cli->state != CLI_AUTHED)
		return (false);
	header = htttp_message_get_header(ctx->msg, "Player-Id");
	if (header == NULL || header[0] == '\0')
		return (false);
	errno = 0;
	claimed = (uint64_t)strtoull(header, &end, 10);
	if (errno != 0 || *end != '\0')
		return (false);
	return (claimed == ctx->cli->player_id);
}

/**
 * @brief LIST /rooms - the lobby as the client displays it, or LIST /store.
 *
 * Every room is listed, in-game ones included: the status field is what tells
 * a player whether they can join, and hiding rooms would only make the lobby
 * look emptier than it is.
 *
 * The store front answers here rather than under a method of its own, because
 * "list the collection at this path" is what LIST already means; the two
 * collections differ in what they hold, not in what is being asked.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 with the room rows or the catalogue, 401 when unauthenticated,
 *         404 for any other path.
 */
int	list_handler(const t_htttp_message *msg, void *context)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_request_context		*ctx;
	size_t			count;
	int				len;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (ctx->msg->path != NULL
		&& strcmp(ctx->msg->path, TETRISD_ROUTE_STORE) == 0)
		return (store_list_catalogue(ctx));
	if (ctx->msg->path == NULL
		|| strcmp(ctx->msg->path, TETRISD_ROUTE_ROOMS) != 0)
		return (404);
	count = list_rooms(ctx->srv, rows);
	len = body_rooms_encode(rows, count, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief JOIN - takes a slot in a room, creating one when none is named.
 *
 * `JOIN /rooms` with a mode creates a room and seats the requester as its
 * owner; `JOIN /room/<name>` joins an existing one. Rooms cannot be named by
 * clients - the lobby assigns S-01, D-02, BR-03 - so creation has no name to
 * address and uses the collection instead.
 *
 * This is where a binding that no longer holds is thrown away, and the only
 * place: a player whose last game ended is asking for another room, so the
 * repair belongs to the request that wants it rather than to whichever
 * predicate happened to notice.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 201 on create, 200 on join, 4xx with the verdict otherwise.
 */
int	join_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_game_mode	mode;
	const char	*name;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (server_room_resolve(ctx->srv, ctx->cli, NULL) != NULL)
	{
		request_body_printf(ctx, "reason already-in-room\nroom %s\n",
			ctx->cli->binding.room_name);
		return (409);
	}
	server_room_unbind(ctx->cli);
	if (strcmp(ctx->msg->path, TETRISD_ROUTE_ROOMS) == 0)
	{
		if (read_mode(ctx, &mode) != 0)
			return (400);
		return (create_room(ctx, mode));
	}
	name = room_name_from_path(ctx);
	if (name == NULL)
		return (404);
	return (join_room(ctx, name));
}

/**
 * @brief LEAVE /room/<name> - gives up a slot, forfeiting a game in progress.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 on success, 404 when the caller is not in that room.
 */
int	leave_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	const char	*name;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (addressed_room(ctx, &name) == NULL)
		return (404);
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "%s left %s",
		ctx->cli->username, name);
	server_room_forfeit(ctx->srv, ctx->cli);
	request_body_printf(ctx, "room %s\nstatus left\n", name);
	return (200);
}

/**
 * @brief START /room/<name> - the owner begins the game.
 *
 * The room decides the verdict; this handler only turns it into a status and,
 * when accepted, deals every seated player a board and marks the room playing.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when the game started, 403 for a non-owner, 409 otherwise.
 */
int	start_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	const char			*name;
	t_start_verdict		verdict;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	server_room = addressed_room(ctx, &name);
	if (server_room == NULL)
		return (404);
	verdict = server_room_start(server_room, ctx->cli);
	if (verdict != START_ACCEPTED)
		return (start_status(ctx, verdict));
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "game started in %s", name);
	request_body_printf(ctx, "room %s\nstatus in-game\n", name);
	return (200);
}

/**
 * @brief Creates a room and seats its owner.
 *
 * @param ctx Request context.
 * @param mode Mode the room is created with.
 * @return 201 on success, 409 when the lobby is full or seating failed.
 */
static int	create_room(t_request_context *ctx, t_game_mode mode)
{
	int	slot;

	slot = server_room_open(ctx->srv, ctx->cli, mode);
	if (slot < 0)
		return (request_refuse(ctx, "lobby-full"));
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "%s created room %s",
		ctx->cli->username, ctx->cli->binding.room_name);
	request_body_printf(ctx, "room %s\nslot %d\nrole owner\n", ctx->cli->binding.room_name, slot);
	return (201);
}

/**
 * @brief Seats a player in an existing room.
 *
 * @param ctx Request context.
 * @param name Room display name from the path.
 * @return 200 on success, 404 when unknown, 409 when full or in game.
 */
static int	join_room(t_request_context *ctx, const char *name)
{
	t_server_room	*server_room;
	t_join_verdict	verdict;
	int				slot;

	server_room = server_room_find(ctx->srv, name);
	if (server_room == NULL)
		return (404);
	verdict = server_room_seat(server_room, ctx->cli, &slot);
	if (verdict == JOIN_FULL)
		return (request_refuse(ctx, "full"));
	if (verdict == JOIN_IN_GAME)
		return (request_refuse(ctx, "in-game"));
	if (slot < 0)
		return (request_refuse(ctx, "seat-refused"));
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "%s joined %s",
		ctx->cli->username, name);
	request_body_printf(ctx, "room %s\nslot %d\nrole player\n", ctx->cli->binding.room_name,
		slot);
	return (200);
}

/**
 * @brief Extracts the room name from a /room/<name> path.
 *
 * @param ctx Request context holding the path.
 * @return Borrowed pointer to the name, or NULL when the path is not a room.
 */
static const char	*room_name_from_path(const t_request_context *ctx)
{
	const char	*name;

	if (ctx->msg->path == NULL
		|| strncmp(ctx->msg->path, TETRISD_ROUTE_ROOM_PREFIX, strlen(TETRISD_ROUTE_ROOM_PREFIX)) != 0)
		return (NULL);
	name = ctx->msg->path + strlen(TETRISD_ROUTE_ROOM_PREFIX);
	if (name[0] == '\0' || strchr(name, '/') != NULL
		|| strlen(name) >= ROOM_NAME_MAX)
		return (NULL);
	return (name);
}

/**
 * @brief Reads the requested mode from a create request's body.
 *
 * @param ctx Request context.
 * @param out Receives the mode; Single when the body omits it.
 * @return 0 on success, -1 when the named mode is not a mode.
 */
static int	read_mode(t_request_context *ctx, t_game_mode *out)
{
	char	value[32];

	*out = MODE_SINGLE;
	if (request_body_field(ctx, "mode", value, sizeof(value)) == NULL)
		return (0);
	if (strcmp(value, "single") == 0)
		return (0);
	if (strcmp(value, "double") == 0)
	{
		*out = MODE_DOUBLE;
		return (0);
	}
	if (strcmp(value, "br") == 0 || strcmp(value, "battle-royale") == 0)
	{
		*out = MODE_BATTLE_ROYALE;
		return (0);
	}
	return (-1);
}

/**
 * @brief Turns a start verdict into a status and a reason the player can read.
 *
 * A bare refusal tells a player nothing: the room domain already knows *why*
 * it said no, so the reason travels with the status rather than being thrown
 * away at the boundary.
 *
 * @param ctx Request context, whose body receives the reason.
 * @param verdict Verdict from the room domain.
 * @return 403 for a non-owner, 409 for too few players or a running game.
 */
static int	start_status(t_request_context *ctx, t_start_verdict verdict)
{
	if (verdict == START_NOT_OWNER)
	{
		request_body_printf(ctx, "reason not-owner\n");
		return (403);
	}
	if (verdict == START_TOO_FEW_PLAYERS)
		return (request_refuse(ctx, "too-few-players"));
	return (request_refuse(ctx, "already-started"));
}

/**
 * @brief Resolves the room a request addresses to the one the caller sits in.
 *
 * The path must name the room the player holds a slot in, so this is one
 * question rather than two: a binding left over from a finished game names a
 * room the player is no longer a member of, and resolves to nothing. Nothing
 * is repaired here - LEAVE and START have nothing to gain from clearing a
 * binding they have just refused to act on.
 *
 * @param ctx Request context holding the path.
 * @param name Receives the room name from the path, for the answer body.
 * @return The room, or NULL when the caller is not seated in the room the
 *         path names.
 */
static t_server_room	*addressed_room(t_request_context *ctx,
			const char **name)
{
	*name = room_name_from_path(ctx);
	if (*name == NULL)
		return (NULL);
	return (server_room_resolve(ctx->srv, ctx->cli, *name));
}

/**
 * @brief Projects the lobby onto wire-facing room rows.
 *
 * Rooms nobody is sitting in are skipped: they have no owner to name, and an
 * empty room is not something a player can meaningfully join.
 *
 * @param srv Server whose lobby is listed.
 * @param rows Receives up to LOBBY_MAX_ROOMS rows.
 * @return Number of rows written.
 */
static size_t	list_rooms(t_server *srv, t_body_room_row *rows)
{
	t_room_snapshot	snaps[LOBBY_MAX_ROOMS];
	size_t			count;
	size_t			written;
	size_t			i;

	count = lobby_list_all(&srv->lobby, snaps, LOBBY_MAX_ROOMS);
	i = 0;
	written = 0;
	while (i < count)
	{
		if (snaps[i].summary.players > 0)
		{
			memset(&rows[written], 0, sizeof(rows[written]));
			snprintf(rows[written].name, sizeof(rows[written].name), "%s",
				snaps[i].summary.name);
			rows[written].mode = (t_body_mode)snaps[i].summary.mode;
			rows[written].players = snaps[i].summary.players;
			rows[written].slot_count = snaps[i].summary.slot_count;
			rows[written].status = (t_body_room_status)snaps[i].summary.status;
			snprintf(rows[written].owner, sizeof(rows[written].owner), "%s",
				snaps[i].summary.owner_name);
			written++;
		}
		i++;
	}
	return (written);
}
