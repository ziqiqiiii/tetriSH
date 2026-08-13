#include "tetrisd.h"

/*
** TARGET is the whole of a player's say in who their garbage goes to, and it
** is deliberately small: a mode, and nothing else. A player cannot name a
** person - every mode narrows a set and the room draws from it - so there is
** no seat, no player id and no board in this request.
**
** It is its own method rather than a field on MOVE because MOVE is send-only
** and must keep one shape, and because pressing a mode key is not a piece
** movement: it happens a few times a match rather than a few times a second.
*/

// Static Functions
static bool	read_mode(t_request_context *ctx, t_target_mode *out);

/**
 * @brief TARGET /room/<name> - declare which kind of rival to aim at.
 *
 * It is rate limited on the chat bucket and not on the input one, for the
 * reason chat is: it is a person pressing a key a handful of times a match,
 * and spending an input token on it would make choosing a mode cost a piece
 * movement in the middle of a game.
 *
 * @param msg The request (unused; the body is read through the context).
 * @param context The request context.
 * @return 200 when the mode was recorded, 400 on a body that is not a mode,
 *         404 when the caller is not in that room, 409 when the room is not
 *         playing a match this player has a board in, 429 when rate limited.
 */
int	target_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	t_target_mode		mode;
	const char			*name;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (!rate_limit_take_chat_token(ctx->cli))
		return (429);
	name = request_room_name(ctx);
	if (name == NULL)
		return (404);
	server_room = server_room_resolve(ctx->srv, ctx->cli, name);
	if (server_room == NULL)
		return (404);
	if (!read_mode(ctx, &mode))
		return (400);
	if (!server_room_is_arena(server_room))
		return (request_refuse(ctx, "not-battle-royale"));
	if (!server_room_set_target(server_room, ctx->cli, mode))
		return (request_refuse(ctx, "not-playing"));
	return (200);
}

/**
 * @brief Reads the one word this request carries.
 *
 * @param ctx Request context holding the body.
 * @param out Receives the mode.
 * @return true when the body named one, false otherwise.
 */
static bool	read_mode(t_request_context *ctx, t_target_mode *out)
{
	char	token[32];

	if (request_body_field(ctx, "mode", token, sizeof(token)) == NULL)
		return (false);
	if (strcmp(token, "random") == 0)
		*out = TARGET_RANDOM;
	else if (strcmp(token, "ko") == 0)
		*out = TARGET_KO;
	else if (strcmp(token, "attackers") == 0)
		*out = TARGET_ATTACKERS;
	else if (strcmp(token, "badges") == 0)
		*out = TARGET_BADGES;
	else
		return (false);
	return (true);
}
