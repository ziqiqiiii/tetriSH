#include "tetrisd.h"

// Static Functions
static int	input_target(t_reqctx *ctx, t_room_rt **out);
static int	body_token(t_reqctx *ctx, char *out, size_t cap);
static int	apply_input(t_reqctx *ctx, t_room_rt *rt, t_input_action action, int argument);

/**
 * @brief MOVE /room/<name>/player/<pid> - translate the falling piece.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when the move was blocked, 400 on a bad body.
 */
int	h_move(const t_htttp_message *msg, void *context)
{
	t_reqctx	*ctx;
	t_room_rt	*rt;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = input_target(ctx, &rt);
	if (status != 0)
		return (status);
	if (body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "LEFT") == 0)
		return (apply_input(ctx, rt, INPUT_MOVE, -1));
	if (strcmp(token, "RIGHT") == 0)
		return (apply_input(ctx, rt, INPUT_MOVE, 1));
	return (400);
}

/**
 * @brief ROTATE /room/<name>/player/<pid> - rotate the falling piece.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when every kick was blocked, 400 otherwise.
 */
int	h_rotate(const t_htttp_message *msg, void *context)
{
	t_reqctx	*ctx;
	t_room_rt	*rt;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = input_target(ctx, &rt);
	if (status != 0)
		return (status);
	if (body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "CW") == 0)
		return (apply_input(ctx, rt, INPUT_ROTATE, 1));
	if (strcmp(token, "CCW") == 0)
		return (apply_input(ctx, rt, INPUT_ROTATE, -1));
	return (400);
}

/**
 * @brief DROP /room/<name>/player/<pid> - soft or hard drop.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when applied, 409 when the game is not running, 400 otherwise.
 */
int	h_drop(const t_htttp_message *msg, void *context)
{
	t_reqctx	*ctx;
	t_room_rt	*rt;
	char		token[16];
	int			status;

	(void)msg;
	ctx = context;
	status = input_target(ctx, &rt);
	if (status != 0)
		return (status);
	if (body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "SOFT") == 0)
		return (apply_input(ctx, rt, INPUT_DROP, 0));
	if (strcmp(token, "HARD") == 0)
		return (apply_input(ctx, rt, INPUT_DROP, 1));
	return (400);
}

/**
 * @brief Spends one token from this connection's input budget.
 *
 * Inputs are the one route a client can send without being asked to, and a
 * flood of them costs the room's ticker real work under its mutex. The bucket
 * is sized from .tetrishrc far above what a person can press, so a client that
 * empties it is not playing - it is hammering, and gets told to slow down
 * rather than being served.
 *
 * The bucket belongs to the connection and is only touched by that
 * connection's own reader thread, so it needs no lock.
 *
 * @param cli Client spending a token.
 * @return true when a token was available, false when the budget is empty.
 */
bool	input_take_token(t_client *cli)
{
	uint64_t	now;
	int			cap;

	cap = cli->srv->cfg.input_burst * TD_TOKEN_SCALE;
	now = net_now_ms();
	if (cli->tokens_at_ms == 0)
	{
		cli->tokens = cap;
		cli->tokens_at_ms = now;
	}
	if (now > cli->tokens_at_ms)
	{
		cli->tokens += (int)((now - cli->tokens_at_ms)
				* (uint64_t)cli->srv->cfg.input_rate);
		cli->tokens_at_ms = now;
	}
	if (cli->tokens > cap)
		cli->tokens = cap;
	if (cli->tokens < TD_TOKEN_SCALE)
		return (false);
	cli->tokens -= TD_TOKEN_SCALE;
	return (true);
}

/**
 * @brief Resolves and authorises the room an input request addresses.
 *
 * The path names its own subject, so a player cannot drive somebody else's
 * board even with a valid session: the subject must be the player bound to
 * this connection.
 *
 * @param ctx Request context.
 * @param out Receives the addressed room runtime.
 * @return 0 when the request may proceed, otherwise the status to answer.
 */
static int	input_target(t_reqctx *ctx, t_room_rt **out)
{
	char		name[ROOM_NAME_MAX];
	const char	*path;
	const char	*sep;
	size_t		len;

	*out = NULL;
	if (!h_authorised(ctx))
		return (401);
	if (!input_take_token(ctx->cli))
		return (429);
	path = ctx->msg->path;
	if (path == NULL
		|| strncmp(path, TD_PATH_ROOM, strlen(TD_PATH_ROOM)) != 0)
		return (404);
	path += strlen(TD_PATH_ROOM);
	sep = strstr(path, "/player/");
	if (sep == NULL)
		return (404);
	len = (size_t)(sep - path);
	if (len == 0 || len >= sizeof(name))
		return (404);
	memcpy(name, path, len);
	name[len] = '\0';
	if (strtoull(sep + strlen("/player/"), NULL, 10) != ctx->cli->player_id)
		return (403);
	if (ctx->cli->room_index < 0 || strcmp(name, ctx->cli->room_name) != 0)
		return (409);
	*out = room_rt_at(ctx->srv, ctx->cli->room_index);
	if (*out == NULL)
		return (409);
	return (0);
}

/**
 * @brief Reads the request body as a single upper-case command word.
 *
 * @param ctx Request context.
 * @param out Buffer receiving the token.
 * @param cap Size of out.
 * @return 0 on success, -1 when the body is empty or too long to be a token.
 */
static int	body_token(t_reqctx *ctx, char *out, size_t cap)
{
	size_t	len;
	size_t	i;

	if (ctx->msg->body == NULL || ctx->msg->body_len == 0)
		return (-1);
	len = ctx->msg->body_len;
	while (len > 0 && (ctx->msg->body[len - 1] == '\n'
			|| ctx->msg->body[len - 1] == '\r'
			|| ctx->msg->body[len - 1] == ' '))
		len--;
	if (len == 0 || len >= cap)
		return (-1);
	i = 0;
	while (i < len)
	{
		out[i] = (char)toupper(ctx->msg->body[i]);
		i++;
	}
	out[len] = '\0';
	return (0);
}

/**
 * @brief Applies one input to the caller's own game, under the room's mutex.
 *
 * The move is marked dirty rather than pushed here: the room's ticker owns
 * the outgoing snapshots, so inputs and gravity produce one STATE stream
 * instead of two racing ones.
 *
 * @param ctx Request context.
 * @param rt Room runtime holding the game.
 * @param action Which input to apply.
 * @param argument Direction for a move or rotation, hard flag for a drop.
 * @return 200 when the input was applied, 409 when it was refused.
 */
static int	apply_input(t_reqctx *ctx, t_room_rt *rt, t_input_action action,
			int argument)
{
	t_game	*game;
	bool	ok;
	int		index;

	index = ctx->cli->slot_index - 1;
	if (index < 0 || index >= TD_MAX_GAMES)
		return (409);
	ok = false;
	pthread_mutex_lock(&rt->mutex);
	game = &rt->games[index];
	if (game->player_id == ctx->cli->player_id && game->active)
	{
		if (action == INPUT_MOVE)
			ok = game_move(game, argument);
		else if (action == INPUT_ROTATE)
			ok = game_rotate(game, argument);
		else
			ok = game_drop(game, argument != 0);
		if (ok)
			rt->dirty[index] = true;
	}
	pthread_mutex_unlock(&rt->mutex);
	if (!ok)
		return (req_refuse(ctx, "input-blocked"));
	return (200);
}
