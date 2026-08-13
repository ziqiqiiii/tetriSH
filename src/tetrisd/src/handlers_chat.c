#include "tetrisd.h"

// Static Functions
static bool	sayable(const char *text);
static int	read_text(t_request_context *ctx, char *raw, size_t raw_cap,
				char *out, size_t cap);

/**
 * @brief CHAT /room/<name> - posts one line to the room's feed (UC-09).
 *
 * The order of the refusals is the specification's, and it matters: rate limit,
 * membership, muting, then the text - so a refusal never tells a client more
 * about a room than it is entitled to know. The sender is on the receiving end
 * too, which is what makes a `seq` meaningful.
 *
 * @param msg The request (unused; the body is read through the context).
 * @param context The request context.
 * @return 200 when broadcast, 429 rate-limited, 404 when not in that room,
 *         403 when muted, 400 when the text is unsendable, 500 when the line
 *         could not be built.
 */
int	chat_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	t_body_chat			chat;
	const char			*name;
	char				raw[TETRISD_CHAT_TEXT_RAW_MAX];

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
	if (server_room_is_muted(server_room, ctx->cli->player_id))
	{
		request_body_printf(ctx, "reason muted\n");
		return (403);
	}
	memset(&chat, 0, sizeof(chat));
	snprintf(chat.sender, sizeof(chat.sender), "%s", ctx->cli->username);
	if (read_text(ctx, raw, sizeof(raw), chat.text, sizeof(chat.text)) != 0)
		return (400);
	if (!room_chat_broadcast(server_room, &chat))
	{
		request_body_printf(ctx, "reason unsendable\n");
		return (500);
	}
	request_body_printf(ctx, "room %s\nseq %llu\n", name,
		(unsigned long long)chat.seq);
	return (200);
}

/**
 * @brief Reads the message text and says which way it was unsendable.
 *
 * The line is read into a buffer twice the width of the field it must fit, so
 * a merely-too-long message can be named "too-long" rather than "bad-text" -
 * the two ask the client for different fixes.
 *
 * @param ctx Request context holding the body.
 * @param raw Wide scratch buffer the line is read into first.
 * @param raw_cap Size of raw.
 * @param out Receives the text once it is known to fit and be sayable.
 * @param cap Size of out, which is the width the wire field really has.
 * @return 0 when out holds a sendable message, -1 with the reason written into
 *         the response body.
 */
static int	read_text(t_request_context *ctx, char *raw, size_t raw_cap,
			char *out, size_t cap)
{
	if (request_body_field(ctx, "text", raw, raw_cap) == NULL
		|| !sayable(raw))
	{
		request_body_printf(ctx, "reason bad-text\n");
		return (-1);
	}
	if (strlen(raw) >= cap)
	{
		request_body_printf(ctx, "reason too-long\nlimit %zu\n", cap - 1);
		return (-1);
	}
	memcpy(out, raw, strlen(raw) + 1);
	return (0);
}

/**
 * @brief Checks that a message can be put on the wire and drawn safely.
 *
 * What is left to refuse is a message that is empty or carries a control
 * character - an escape sequence arriving as chat is somebody else's cursor.
 * The body codec enforces the same rule; failing here makes it a 400.
 *
 * @param text The candidate message.
 * @return true when the text is non-empty and entirely printable.
 */
static bool	sayable(const char *text)
{
	size_t	index;

	if (text[0] == '\0')
		return (false);
	index = 0;
	while (text[index] != '\0')
	{
		if ((unsigned char)text[index] < 0x20
			|| (unsigned char)text[index] == 0x7f)
			return (false);
		index++;
	}
	return (true);
}
