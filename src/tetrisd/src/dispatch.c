#include "tetrisd.h"

// Static Variables
static const t_htttp_route	g_routes[] = {
	{"SIGNUP", 0u, signup_handler},
	{"LOGIN", 0u, login_handler},
	{"LIST", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, list_handler},
	{"JOIN", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, join_handler},
	{"LEAVE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, leave_handler},
	{"START", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, start_handler},
	{"MOVE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, move_handler},
	{"ROTATE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, rotate_handler},
	{"DROP", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, drop_handler},
	{"HOLD", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, hold_handler},
	{"PAUSE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, pause_handler},
	{"RESTART", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, restart_handler},
	{"ABILITY", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, ability_handler},
	{"LEADERBOARD", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, leaderboard_handler},
	{"PROFILE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, profile_handler},
	{"BUY", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, buy_handler},
	{"EQUIP", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, equip_handler},
	{"CHAT", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, chat_handler}
};

// Static Functions
static unsigned int	result_status(t_htttp_result res, const t_htttp_message *msg);
static const char	*reason_for(unsigned int status);
static bool			body_declares_itself(const t_htttp_message *msg);

/**
 * @brief Turns one decrypted frame into exactly one response.
 *
 * Parsing, validation, and routing all live in libhtttp; what belongs here is
 * the mapping from its failures to protocol answers, so a malformed or
 * oversized message is visible to the client rather than silently dropped.
 *
 * @param cli Client the frame arrived on.
 * @param frame Decrypted plaintext of one HTTTP message.
 * @param len Length of frame.
 */
void	client_handle_frame(t_client *cli, const unsigned char *frame, size_t len)
{
	t_htttp_message	msg;
	t_request_context		ctx;
	t_htttp_result	res;
	int				status;

	if (cli == NULL || frame == NULL)
		return ;
	htttp_message_init(&msg);
	res = htttp_parse(frame, len, &msg);
	if (res != HTTTP_OK)
	{
		request_reply(cli, result_status(res, NULL), NULL, 0);
		htttp_message_free(&msg);
		return ;
	}
	if (!body_declares_itself(&msg))
	{
		request_reply(cli, 400u, NULL, 0);
		htttp_message_free(&msg);
		return ;
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.cli = cli;
	ctx.srv = cli->srv;
	ctx.msg = &msg;
	status = 0;
	res = htttp_dispatch(&msg, g_routes, sizeof(g_routes) / sizeof(g_routes[0]), &ctx, &status);
	if (res != HTTTP_OK)
		status = (int)result_status(res, &msg);
	request_reply(cli, (unsigned int)status, ctx.body, ctx.body_len);
	htttp_message_free(&msg);
}

/**
 * @brief Builds one response and queues it on the client's outbox.
 *
 * Every response carries a Date (the protocol requires it) and, once the
 * connection is bound to a player, that player's id - which is what a client
 * echoes back in Player-Id on later requests. A refusal for going too fast
 * says when to try again, so a client can back off rather than guess.
 *
 * @param cli Client to answer.
 * @param status Status code to send.
 * @param body Body bytes, or NULL for an empty response.
 * @param body_len Length of body.
 */
void	request_reply(t_client *cli, unsigned int status, const char *body, size_t body_len)
{
	t_htttp_message	resp;
	char			date[HTTTP_DATE_BUFSIZE];
	char			pid[32];

	htttp_message_init(&resp);
	if (htttp_message_make_response(&resp, status, reason_for(status)) == HTTTP_OK)
	{
		if (htttp_format_date(time(NULL), date) == HTTTP_OK)
			htttp_message_set_header(&resp, "Date", date);
		if (cli->state == CLI_AUTHED)
		{
			snprintf(pid, sizeof(pid), "%llu", (unsigned long long)cli->player_id);
			htttp_message_set_header(&resp, "Player-Id", pid);
		}
		if (status == 429u)
			htttp_message_set_header(&resp, "Retry-After", "1");
		if (body != NULL && body_len > 0)
		{
			htttp_message_set_header(&resp, "Content-Type", HTTTP_CONTENT_TYPE_STATUS);
			htttp_message_set_body(&resp, body, body_len);
		}
		client_send(cli, &resp, false);
	}
	htttp_message_free(&resp);
}

/**
 * @brief Reads one `key value` line out of a request body.
 *
 * Command bodies are the same plaintext line format the status bodies use,
 * so requests need no separate codec: one key per line, value to end of line.
 *
 * @param ctx Request context holding the message.
 * @param key Key to look for.
 * @param out Buffer receiving the value.
 * @param cap Size of out.
 * @return out on success, NULL when the key is absent or does not fit.
 */
const char	*request_body_field(const t_request_context *ctx, const char *key, char *out,
			size_t cap)
{
	const unsigned char	*body;
	size_t				key_len;
	size_t				i;
	size_t				n;

	if (ctx == NULL || ctx->msg == NULL || ctx->msg->body == NULL || out == NULL)
		return (NULL);
	body = ctx->msg->body;
	key_len = strlen(key);
	i = 0;
	while (i < ctx->msg->body_len)
	{
		n = i;
		while (n < ctx->msg->body_len && body[n] != '\n')
			n++;
		if (n - i > key_len + 1 && memcmp(&body[i], key, key_len) == 0 && body[i + key_len] == ' ')
		{
			i += key_len + 1;
			if (n - i >= cap)
				return (NULL);
			memcpy(out, &body[i], n - i);
			out[n - i] = '\0';
			return (out);
		}
		i = n + 1;
	}
	return (NULL);
}

/**
 * @brief Formats this request's response body.
 *
 * Response bodies are the same `key value` lines requests use, so handlers
 * write them with one printf-style call instead of hand-rolling a buffer.
 *
 * @param ctx Request context whose body is written.
 * @param fmt printf-style format for the whole body.
 */
void	request_body_printf(t_request_context *ctx, const char *fmt, ...)
{
	va_list	ap;
	int		n;

	if (ctx == NULL || fmt == NULL)
		return ;
	va_start(ap, fmt);
	n = vsnprintf(ctx->body, sizeof(ctx->body), fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= sizeof(ctx->body))
	{
		ctx->body_len = 0;
		return ;
	}
	ctx->body_len = (size_t)n;
}

/**
 * @brief Refuses a request with 409 and says which verdict caused it.
 *
 * The room domain reports *why* it said no; throwing that away at the
 * protocol boundary would leave a player unable to tell a full room from one
 * already in game.
 *
 * @param ctx Request context, whose body receives the reason.
 * @param reason Short machine-readable verdict name.
 * @return Always 409.
 */
int	request_refuse(t_request_context *ctx, const char *reason)
{
	request_body_printf(ctx, "reason %s\n", reason);
	return (409);
}

/**
 * @brief Checks that a request carrying a body says what that body is.
 *
 * Content-Type is required on any message with a body, and the only body a
 * client ever sends is a command. Guessing instead of checking would mean the
 * server decides what the client meant, which is exactly what a protocol is
 * for avoiding.
 *
 * @param msg The parsed request.
 * @return true when the message may proceed, false when it must be refused.
 */
static bool	body_declares_itself(const t_htttp_message *msg)
{
	const char	*type;

	if (msg->body == NULL || msg->body_len == 0)
		return (true);
	type = htttp_message_get_header(msg, "Content-Type");
	if (type == NULL)
		return (false);
	return (strcmp(type, HTTTP_CONTENT_TYPE_COMMAND) == 0);
}

/**
 * @brief Maps a libhtttp failure onto the status the client should see.
 *
 * The distinction that matters is between "you sent nonsense" (400), "you are
 * not who you claim" (401), "that is too big" (413), and "no such method"
 * (501) - a client cannot fix what it cannot tell apart.
 *
 * @param res The libhtttp result to map.
 * @param msg The parsed message, or NULL when parsing itself failed.
 * @return The status code to answer with.
 */
static unsigned int	result_status(t_htttp_result res, const t_htttp_message *msg)
{
	const char	*player;

	if (res == HTTTP_ERR_TOO_LARGE)
		return (413u);
	if (res == HTTTP_ERR_NO_HANDLER)
		return (501u);
	if (res == HTTTP_ERR_NO_MEMORY)
		return (500u);
	if (res == HTTTP_ERR_MISSING_REQUIRED_HEADER)
	{
		player = NULL;
		if (msg != NULL)
			player = htttp_message_get_header(msg, "Player-Id");
		if (player == NULL || player[0] == '\0')
			return (401u);
		return (400u);
	}
	return (400u);
}

/**
 * @brief Returns the reason phrase for a status, including ones libhtttp omits.
 *
 * @param status Status code being answered with.
 * @return Reason phrase text, never NULL.
 */
static const char	*reason_for(unsigned int status)
{
	const char	*reason;

	reason = htttp_reason_phrase(status);
	if (reason != NULL)
		return (reason);
	if (status == 501u)
		return ("Not Implemented");
	return ("Error");
}
