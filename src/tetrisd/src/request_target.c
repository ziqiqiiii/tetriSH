#include "tetrisd.h"

// Static Functions
static bool	take_token(int *tokens, uint64_t *at_ms, int cap, int rate);

/*
** What every request that drives a game has to establish before it may touch
** one: that the connection is authenticated, that it is not hammering, that
** the path addresses a room this player is actually sitting in, and that the
** subject in the path is this player and not somebody else.
**
** It lives in its own file because two families of handler ask it - the ones
** that drive the falling piece (handlers_input.c) and the ones that act on
** the game session around it (handlers_game.c) - and a check that decides
** who may act on whose board is not something to keep two copies of.
*/

/**
 * @brief Resolves and authorises the room a game request addresses.
 *
 * The path names its own subject, so a player cannot drive somebody else's
 * board even with a valid session: the subject must be the player bound to
 * this connection.
 *
 * @param ctx Request context.
 * @param out Receives the addressed room runtime.
 * @return 0 when the request may proceed, otherwise the status to answer.
 */
int	request_input_target(t_request_context *ctx, t_server_room **out)
{
	char		name[ROOM_NAME_MAX];
	const char	*path;
	const char	*sep;
	size_t		len;

	*out = NULL;
	if (!request_is_authorised(ctx))
		return (401);
	if (!rate_limit_take_token(ctx->cli))
		return (429);
	path = ctx->msg->path;
	if (path == NULL
		|| strncmp(path, TETRISD_ROUTE_ROOM_PREFIX, strlen(TETRISD_ROUTE_ROOM_PREFIX)) != 0)
		return (404);
	path += strlen(TETRISD_ROUTE_ROOM_PREFIX);
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
	*out = server_room_resolve(ctx->srv, ctx->cli, name);
	if (*out == NULL)
		return (409);
	return (0);
}

/**
 * @brief Extracts the room name from a /room/<name> path.
 *
 * Shared rather than duplicated because three families of handler address a
 * room by name now - the lobby's LEAVE and START, LIST's room snapshot, and
 * CHAT - and two spellings of "which room is this" would eventually disagree
 * about a trailing slash.
 *
 * @param ctx Request context holding the path.
 * @return Borrowed pointer to the name, or NULL when the path is not a room.
 */
const char	*request_room_name(const t_request_context *ctx)
{
	const char	*name;
	size_t		prefix_len;

	prefix_len = strlen(TETRISD_ROUTE_ROOM_PREFIX);
	if (ctx->msg->path == NULL
		|| strncmp(ctx->msg->path, TETRISD_ROUTE_ROOM_PREFIX,
			prefix_len) != 0)
		return (NULL);
	name = ctx->msg->path + prefix_len;
	if (name[0] == '\0' || strchr(name, '/') != NULL
		|| strlen(name) >= ROOM_NAME_MAX)
		return (NULL);
	return (name);
}

/**
 * @brief Reads a decimal player id off the front of a path segment.
 *
 * Ids are issued from 1, so 0 is free to mean "that was not an id" and every
 * caller can compare the answer against the connection's player without a
 * second success flag to test first.
 *
 * @param text Start of the id, mid-path.
 * @param end Receives the first character after the id; may be NULL.
 * @return The id, or 0 when no digits start the segment.
 */
t_player_id	request_player_id(const char *text, const char **end)
{
	unsigned long long	value;
	char				*stop;

	if (end != NULL)
		*end = text;
	if (text == NULL || *text < '0' || *text > '9')
		return (0);
	errno = 0;
	value = strtoull(text, &stop, 10);
	if (errno != 0)
		return (0);
	if (end != NULL)
		*end = stop;
	return ((t_player_id)value);
}

/**
 * @brief Reads the request body as a single upper-case command word.
 *
 * @param ctx Request context.
 * @param out Buffer receiving the token.
 * @param cap Size of out.
 * @return 0 on success, -1 when the body is empty or too long to be a token.
 */
int	request_body_token(t_request_context *ctx, char *out, size_t cap)
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
 * @brief Spends one token from this connection's input budget.
 *
 * Inputs are the one route a client can send without being asked to, and a
 * flood of them costs the reactor real work it owes every other room. The
 * bucket is sized from .tetrishrc far above what a person can press, so a
 * client that empties it is not playing - it is hammering, and gets told to
 * slow down rather than being served.
 *
 * The bucket belongs to the connection and is only touched by the reactor, so
 * it needs no lock.
 *
 * @param cli Client spending a token.
 * @return true when a token was available, false when the budget is empty.
 */
bool	rate_limit_take_token(t_client *cli)
{
	return (take_token(&cli->tokens, &cli->tokens_at_ms,
			cli->srv->cfg.input_burst * TETRISD_TOKEN_SCALE,
			cli->srv->cfg.input_rate));
}

/**
 * @brief Spends one token from this connection's chat budget.
 *
 * A separate bucket from the input one, and that separation is the point: the
 * two police unrelated floods, so charging a line of chat against the budget a
 * player needs to move a piece meant a busy match could answer 429 to somebody
 * typing, and a talkative player could lose their own inputs. A person types
 * far slower than they press, so this bucket is much smaller and refills much
 * more slowly than the one beside it.
 *
 * @param cli Client spending a token.
 * @return true when a token was available, false when the budget is empty.
 */
bool	rate_limit_take_chat_token(t_client *cli)
{
	return (take_token(&cli->chat_tokens, &cli->chat_tokens_at_ms,
			TETRISD_CHAT_BURST * TETRISD_TOKEN_SCALE,
			TETRISD_CHAT_RATE_PER_SEC));
}

/**
 * @brief Refills a scaled token level without overflowing its integer store.
 *
 * The ceiling is tested before multiplication. Once elapsed time can fill the
 * missing part of the bucket, the exact product is irrelevant and returning
 * the cap avoids narrowing a potentially enormous uint64_t product to int.
 *
 * @param tokens Current scaled token level.
 * @param elapsed_ms Idle time to refill.
 * @param cap Maximum scaled token level.
 * @param rate Whole tokens added per second (one scaled unit per millisecond).
 * @return The refilled level, clamped to the bucket's valid range.
 */
int	rate_limit_refill_level(int tokens, uint64_t elapsed_ms, int cap,
		int rate)
{
	uint64_t	missing;
	uint64_t	fill_ms;

	if (cap <= 0)
		return (0);
	if (tokens < 0)
		tokens = 0;
	if (tokens >= cap)
		return (cap);
	if (rate <= 0 || elapsed_ms == 0)
		return (tokens);
	missing = (uint64_t)(cap - tokens);
	fill_ms = (missing + (uint64_t)rate - 1) / (uint64_t)rate;
	if (elapsed_ms >= fill_ms)
		return (cap);
	return (tokens + (int)(elapsed_ms * (uint64_t)rate));
}

/**
 * @brief Refills one token bucket by however long it has been idle, then
 *        spends a token from it.
 *
 * Shared by both buckets so there is one piece of refill arithmetic rather
 * than two that could drift. A zero timestamp means the bucket has never been
 * used, which starts it full - a connection's first request is never the one
 * that is going too fast.
 *
 * @param tokens The bucket's level, in thousandths of a token.
 * @param at_ms When the bucket was last refilled.
 * @param cap The bucket's ceiling, in thousandths of a token.
 * @param rate Refill rate in whole tokens per second.
 * @return true when a token was available, false when the budget is empty.
 */
static bool	take_token(int *tokens, uint64_t *at_ms, int cap, int rate)
{
	uint64_t	now;

	now = clock_now_ms();
	if (*at_ms == 0)
	{
		*tokens = cap;
		*at_ms = now;
	}
	if (now > *at_ms)
	{
		*tokens = rate_limit_refill_level(*tokens, now - *at_ms, cap, rate);
		*at_ms = now;
	}
	if (*tokens > cap)
		*tokens = cap;
	if (*tokens < TETRISD_TOKEN_SCALE)
		return (false);
	*tokens -= TETRISD_TOKEN_SCALE;
	return (true);
}
