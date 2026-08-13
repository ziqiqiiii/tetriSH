#include "tetrisd.h"

/*
** The four requests that drive the falling piece. Each one reads a single
** command word out of the body and hands the room an action; the room owns
** the board and decides whether it stands.
**
** Nothing here pushes a snapshot. An accepted input marks the game dirty and
** the tick encodes it, so inputs and gravity produce one STATE stream rather
** than two racing ones.
*/

// Static Functions
static int	apply_input(t_request_context *ctx, t_server_room *server_room, t_input_action action, int argument);

/**
 * @brief MOVE /room/<name>/player/<pid> - translate the falling piece.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when the move was blocked, 400 on a bad body.
 */
int	move_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room	*server_room;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	if (request_body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "LEFT") == 0)
		return (apply_input(ctx, server_room, INPUT_MOVE, -1));
	if (strcmp(token, "RIGHT") == 0)
		return (apply_input(ctx, server_room, INPUT_MOVE, 1));
	return (400);
}

/**
 * @brief ROTATE /room/<name>/player/<pid> - rotate the falling piece.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when every kick was blocked, 400 otherwise.
 */
int	rotate_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room	*server_room;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	if (request_body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "CW") == 0)
		return (apply_input(ctx, server_room, INPUT_ROTATE, 1));
	if (strcmp(token, "CCW") == 0)
		return (apply_input(ctx, server_room, INPUT_ROTATE, -1));
	return (400);
}

/**
 * @brief DROP /room/<name>/player/<pid> - soft or hard drop.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when the game is not running, 400 otherwise.
 */
int	drop_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room	*server_room;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	if (request_body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "SOFT") == 0)
		return (apply_input(ctx, server_room, INPUT_DROP, 0));
	if (strcmp(token, "HARD") == 0)
		return (apply_input(ctx, server_room, INPUT_DROP, 1));
	return (400);
}

/**
 * @brief HOLD /room/<name>/player/<pid> - swap the falling piece with hold.
 *
 * HOLD carries no body: there is nothing to say about it beyond asking, which
 * is why it is a method of its own rather than a word in DROP's body.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when the swap happened, 409 when the hold is already spent on
 *         this piece or the game is not running.
 */
int	hold_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room	*server_room;
	int			status;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	return (apply_input(ctx, server_room, INPUT_HOLD, 0));
}

/**
 * @brief Asks the room to apply one input to the caller's own game.
 *
 * The room owns the board and decides whether the move stands; this only turns
 * its answer into a status a client can read.
 *
 * @param ctx Request context.
 * @param server_room Room holding the game.
 * @param action Which input to apply.
 * @param argument Direction for a move or rotation, hard flag for a drop.
 * @return 200 when the input was applied, 409 when it was refused.
 */
static int	apply_input(t_request_context *ctx, t_server_room *server_room, t_input_action action,
			int argument)
{
	if (!server_room_input(server_room, ctx->cli, action, argument))
		return (request_refuse(ctx, "input-blocked"));
	return (200);
}
