#include "tetrisd.h"

/*
** READY - a player saying whether they are ready for the match to begin.
**
** It exists because readiness had no home. A seat became READY the moment it
** was occupied, which made the word mean "somebody is sitting here" and left
** the room unable to express the other answer at all: a client that withdrew
** its readiness held that locally, and its own next refresh of the room
** overruled it half a second later with the server's unchanging yes.
**
** So readiness is the room's now, and this is where a player moves it. A
** Double room starts on its own once every seat has declared, which is what
** the waiting room's [R] has always looked like it did.
*/

// Static Functions
static int	read_ready(t_request_context *ctx, bool *ready);
static int	read_character(t_request_context *ctx, t_item_id *character);

/**
 * @brief READY /room/<name> - declare or withdraw readiness for a match.
 *
 * Starting is left to the room rather than done here. Double begins when the
 * last seat declares, and the tick that follows is what deals the boards -
 * so the answer to this request is the room as it stands, and the client
 * learns the match began the same way it learns everything else about it,
 * from the snapshot that arrives next.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 with the room snapshot, 400 on a bad body, 403 when the named
 *         character is not owned, 404 when the caller is not seated in that
 *         room, 409 when the room is already in game.
 */
int	ready_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	t_body_room			snapshot;
	t_item_id			character;
	bool				ready;
	int					len;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (read_ready(ctx, &ready) != 0)
		return (400);
	if (read_character(ctx, &character) != 0)
		return (403);
	server_room = server_room_resolve(ctx->srv, ctx->cli,
			request_room_name(ctx));
	if (server_room == NULL)
		return (404);
	if (!server_room_set_ready(server_room, ctx->cli, ready, character))
		return (request_refuse(ctx, "already-started"));
	if (server_room_all_ready(server_room)
		&& !server_room_is_solo(server_room))
		(void)server_room_autostart(server_room);
	if (!server_room_snapshot(server_room, &snapshot))
		return (500);
	len = body_room_encode(&snapshot, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief Reads the `ready <0|1>` field a declaration carries.
 *
 * A body that omits it is refused rather than read as either answer: this is
 * the one request whose whole content is which of the two a player meant.
 *
 * @param ctx Request context holding the body.
 * @param ready Receives what was declared.
 * @return 0 on success, -1 when the field is missing or not 0 or 1.
 */
static int	read_ready(t_request_context *ctx, bool *ready)
{
	char	value[16];

	if (request_body_field(ctx, "ready", value, sizeof(value)) == NULL)
		return (-1);
	if (strcmp(value, "1") == 0)
		*ready = true;
	else if (strcmp(value, "0") == 0)
		*ready = false;
	else
		return (-1);
	return (0);
}

/**
 * @brief Reads the optional `character <id>` a declaration may carry, and
 *        checks the player owns it.
 *
 * The character is chosen for a match and not for an account, which is why it
 * travels here rather than through EQUIP: equipping is an account-wide change,
 * and making one in order to play one game is the wrong scope. deal_games
 * reads it once, so a purchase or an equip made mid-match cannot change which
 * four abilities a level selects from.
 *
 * An absent field is not an error - it means "whatever the account has
 * equipped", which is what a client with no selector sends and what every
 * Single game sends. An unowned one is, because it would otherwise buy the
 * player a roster they never paid for.
 *
 * db_player_owns_character answers a predicate, so it is tested against
 * DB_TRUE: DB_FALSE is a successful read meaning "does not own it", and
 * comparing it to DB_OK would read every refusal as a database failure.
 *
 * @param ctx Request context holding the body.
 * @param character Receives the declared id, or 0 when none was named.
 * @return 0 when there is nothing to refuse, -1 when the id is not owned.
 */
static int	read_character(t_request_context *ctx, t_item_id *character)
{
	char	value[32];

	*character = 0;
	if (request_body_field(ctx, "character", value, sizeof(value)) == NULL)
		return (0);
	*character = (t_item_id)strtoul(value, NULL, 10);
	if (*character == 0)
		return (0);
	if (db_player_owns_character(ctx->srv->db, ctx->cli->player_id,
			*character) != DB_TRUE)
		return (-1);
	return (0);
}
