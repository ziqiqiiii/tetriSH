#include "tetrisd.h"

// Static Functions
static int		serialise_chat(const t_server_room *server_room,
					const t_body_chat *chat, unsigned char **bytes,
					size_t *len);
static void		deliver(t_server_room *server_room, const unsigned char *bytes,
					size_t len);

/**
 * @brief Sends one line of the room's feed to everyone sitting in it.
 *
 * The room stamps the message rather than the caller: `seq` is the room's own
 * counter and `at` is read here, so two lines of one feed can never carry the
 * same number and a caller cannot get the order wrong.
 *
 * Delivery is per-seat and best-effort. A player whose chat ring is full loses
 * the oldest line they had not read; a player who has gone loses the message
 * entirely. Neither is an error worth reporting upwards - the feed is not
 * something a room can fail at (UC-09 E1).
 *
 * Failing to *build* the message is a different thing, and is reported. The
 * counter is therefore only advanced once the bytes exist: a seq spent on a
 * line nobody received would leave a permanent hole in the feed, and the
 * sender would have been told the number of a message that was never sent.
 *
 * @param server_room Room whose members receive the message.
 * @param chat Message to send; its seq and at are overwritten here.
 * @return true when the line was built and handed to every seat.
 */
bool	room_chat_broadcast(t_server_room *server_room, t_body_chat *chat)
{
	unsigned char	*bytes;
	size_t			len;

	if (server_room == NULL || server_room->room == NULL || chat == NULL)
		return (false);
	chat->seq = server_room->chat_seq + 1;
	chat->at = clock_now_ms();
	if (serialise_chat(server_room, chat, &bytes, &len) != 0)
	{
		chat->seq = 0;
		return (false);
	}
	server_room->chat_seq = chat->seq;
	deliver(server_room, bytes, len);
	free(bytes);
	return (true);
}

/**
 * @brief Sends a message the server itself wrote to everyone in the room.
 *
 * Narration is chat with no author, not a second mechanism: it travels the
 * same lane, in the same body, numbered by the same counter, so a client
 * draws one ordered feed rather than merging two.
 *
 * Text that will not fit is dropped rather than truncated - half a sentence
 * about who owns the room is worse than none, and every narration this server
 * writes is well inside the field.
 *
 * @param server_room Room to narrate to.
 * @param fmt printf-style format for the message text.
 */
void	room_narrate(t_server_room *server_room, const char *fmt, ...)
{
	t_body_chat	chat;
	va_list		args;
	int			written;

	if (server_room == NULL || fmt == NULL)
		return ;
	memset(&chat, 0, sizeof(chat));
	chat.system = true;
	va_start(args, fmt);
	written = vsnprintf(chat.text, sizeof(chat.text), fmt, args);
	va_end(args);
	if (written < 0 || (size_t)written >= sizeof(chat.text))
		return ;
	(void)room_chat_broadcast(server_room, &chat);
}

/**
 * @brief Encodes and serialises one feed line into a pushed CHAT message.
 *
 * @param server_room Room the message belongs to, naming the path.
 * @param chat Fully stamped message.
 * @param bytes Receives the serialised message, owned by the caller.
 * @param len Receives its length.
 * @return 0 on success, -1 when the message could not be built.
 */
static int	serialise_chat(const t_server_room *server_room,
	const t_body_chat *chat, unsigned char **bytes, size_t *len)
{
	t_htttp_message	msg;
	char			path[TETRISD_CONFIG_LINE_MAX];
	char			body[TETRISD_BODY_MAX_BYTES];
	int				body_len;
	int				rc;

	body_len = body_chat_encode(chat, body, sizeof(body));
	if (body_len <= 0)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", server_room->room->name);
	htttp_message_init(&msg);
	rc = -1;
	if (htttp_message_make_request(&msg, "CHAT", path) == HTTTP_OK
		&& htttp_message_set_header(&msg, "Content-Type",
			HTTTP_CONTENT_TYPE_CHAT) == HTTTP_OK
		&& htttp_message_set_body(&msg, body, (size_t)body_len) == HTTTP_OK
		&& htttp_serialize(&msg, bytes, len) == HTTTP_OK)
		rc = 0;
	htttp_message_free(&msg);
	return (rc);
}

/**
 * @brief Hands every occupied seat its own copy of the serialised message.
 *
 * Each outbox frees what it is given, so one buffer cannot be handed to
 * several of them; the message is serialised once and copied per recipient,
 * which is cheaper than building it again for each.
 *
 * A seat that cannot be given its copy is skipped, not returned on: losing
 * one player's line is what the lane is already allowed to do, while giving
 * up here would silently cut off every seat after it - so in a 99-slot room
 * the feed would reach whoever happened to sit before the failure.
 *
 * @param server_room Room whose seats receive the message.
 * @param bytes Serialised message; still owned by the caller afterwards.
 * @param len Length of bytes.
 */
static void	deliver(t_server_room *server_room, const unsigned char *bytes,
	size_t len)
{
	const t_slot	*slot;
	unsigned char	*copy;
	int				index;

	index = 0;
	while (index < server_room->room->slot_count)
	{
		slot = &server_room->room->slots[index];
		index++;
		if (!slot->occupied)
			continue ;
		copy = malloc(len);
		if (copy == NULL)
			continue ;
		memcpy(copy, bytes, len);
		if (registry_enqueue_chat(&server_room->srv->reg,
				slot->membership.player_id, copy, len) != 0)
			free(copy);
	}
}
