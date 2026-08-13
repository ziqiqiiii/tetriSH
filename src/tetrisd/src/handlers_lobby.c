#include "tetrisd.h"

// Static Functions
static int			read_mode(t_request_context *ctx, t_game_mode *out);
static int			create_room(t_request_context *ctx, t_game_mode mode);
static int			join_room(t_request_context *ctx, const char *name);
static t_server_room	*addressed_room(t_request_context *ctx, const char **name);
static int			start_status(t_request_context *ctx, t_start_verdict verdict);
static int			list_room(t_request_context *ctx);

/**
 * @brief Checks that the request really comes from the player it claims.
 *
 * libtetrissh authenticates the bytes, not the claimed identity, so Player-Id
 * is compared against the player bound to this connection at LOGIN.
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
 * Every room is listed, in-game ones included: the status field tells a player
 * whether they can join. The store front answers here rather than under a
 * method of its own, because listing a collection is what LIST already means.
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
	if (ctx->msg->path != NULL
		&& strncmp(ctx->msg->path, TETRISD_ROUTE_ROOM_PREFIX,
			strlen(TETRISD_ROUTE_ROOM_PREFIX)) == 0)
		return (list_room(ctx));
	if (ctx->msg->path == NULL
		|| strcmp(ctx->msg->path, TETRISD_ROUTE_ROOMS) != 0)
		return (404);
	count = server_rooms_list(ctx->srv, rows, LOBBY_MAX_ROOMS);
	len = body_rooms_encode(rows, count, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief Lists the detailed room snapshot visible to one seated player.
 *
 * @param ctx Authenticated request context.
 * @return 200 with a room body, 404 when the caller is not in that room, or
 *         500 when the projection cannot be encoded.
 */
static int	list_room(t_request_context *ctx)
{
	t_server_room	*server_room;
	t_body_room		snapshot;
	const char		*name;
	int				len;

	server_room = addressed_room(ctx, &name);
	(void)name;
	if (server_room == NULL)
		return (404);
	if (!server_room_snapshot(server_room, &snapshot))
		return (500);
	len = body_room_encode(&snapshot, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief JOIN - takes a slot in a room, creating one when none is named.
 *
 * `JOIN /rooms` with a mode creates a room and seats the requester as its
 * owner; `JOIN /room/<name>` joins an existing one. Clients cannot name rooms,
 * so creation addresses the collection instead.
 *
 * This is the only place a binding that no longer holds is thrown away. Being
 * in a room refuses this request; being in *this* room does not - a room
 * outlives the match played in it, so a client returning from a results screen
 * names the room it is still sitting in.
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
	bool		creating;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	creating = strcmp(ctx->msg->path, TETRISD_ROUTE_ROOMS) == 0;
	name = NULL;
	if (!creating)
		name = request_room_name(ctx);
	if (server_room_resolve(ctx->srv, ctx->cli, NULL) != NULL
		&& (name == NULL
			|| strcmp(name, ctx->cli->binding.room_name) != 0))
	{
		request_body_printf(ctx, "reason already-in-room\nroom %s\n",
			ctx->cli->binding.room_name);
		return (409);
	}
	server_room_unbind(ctx->cli);
	if (creating)
	{
		if (read_mode(ctx, &mode) != 0)
			return (400);
		return (create_room(ctx, mode));
	}
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
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "conn %u %s left %s",
		ctx->cli->conn_id, ctx->cli->username, name);
	server_room_forfeit(ctx->srv, ctx->cli);
	outbox_drop_room_pushes(&ctx->cli->outbox);
	request_body_printf(ctx, "room %s\nstatus left\n", name);
	return (200);
}

/**
 * @brief START /room/<name> - the owner begins the game.
 *
 * The room decides the verdict; this handler only turns it into a status. What
 * "accepted" leaves behind depends on the mode, so the answer names it: a
 * Single room is playing by the time this returns, and any room with an
 * opponent in it is still choosing its fighters.
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
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "conn %u started the game in %s",
		ctx->cli->conn_id, name);
	request_body_printf(ctx, "room %s\nstatus %s\n", name,
		server_room_is_solo(server_room) ? "in-game" : "selecting");
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
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "conn %u %s created room %s",
		ctx->cli->conn_id, ctx->cli->username, ctx->cli->binding.room_name);
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
	logger_emit(&ctx->srv->log, COREIPC_LOG_INFO, "conn %u %s joined %s",
		ctx->cli->conn_id, ctx->cli->username, name);
	request_body_printf(ctx, "room %s\nslot %d\nrole player\n", ctx->cli->binding.room_name,
		slot);
	return (200);
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
 * The path must name the room the player holds a slot in, so a binding left
 * over from a finished game resolves to nothing. Nothing is repaired here;
 * JOIN owns that.
 *
 * @param ctx Request context holding the path.
 * @param name Receives the room name from the path, for the answer body.
 * @return The room, or NULL when the caller is not seated in the room the
 *         path names.
 */
static t_server_room	*addressed_room(t_request_context *ctx,
			const char **name)
{
	*name = request_room_name(ctx);
	if (*name == NULL)
		return (NULL);
	return (server_room_resolve(ctx->srv, ctx->cli, *name));
}

