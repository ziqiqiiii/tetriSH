#include "tetrisu.h"

// Static Functions
static bool	sayable(const char *text);

/**
 * @brief Files one line of the room's feed, dropping the oldest when full.
 *
 * The ring is the only history that exists - tetrisd keeps none - so a full
 * one loses its oldest line rather than refusing the newest. `chat_received`
 * counts every line that ever arrived, which is what a screen compares
 * against to notice a line it has not drawn yet.
 *
 * @param net Client whose feed the line belongs to.
 * @param line The decoded message.
 */
void	net_chat_take(t_net_client *net, const t_body_chat *line)
{
	size_t	slot;

	if (net == NULL || line == NULL)
		return ;
	if (net->chat_held == NET_CHAT_HISTORY)
	{
		net->chat_head = (net->chat_head + 1) % NET_CHAT_HISTORY;
		net->chat_held--;
	}
	slot = (net->chat_head + net->chat_held) % NET_CHAT_HISTORY;
	net->chat[slot] = *line;
	net->chat_held++;
	net->chat_received++;
}

/**
 * @brief Reports how many lines of the feed this client is still holding.
 *
 * @param net Client to ask.
 * @return The number of lines held, never more than NET_CHAT_HISTORY.
 */
size_t	net_chat_held(const t_net_client *net)
{
	if (net == NULL)
		return (0);
	return (net->chat_held);
}

/**
 * @brief Borrows one held line, oldest first.
 *
 * Index 0 is the oldest line still held, so a panel drawing top to bottom
 * walks the index upwards and does not have to know where the ring wrapped.
 *
 * @param net Client to read.
 * @param index Position from the oldest held line.
 * @return Borrowed pointer to the line, or NULL when index is past the end.
 */
const t_body_chat	*net_chat_at(const t_net_client *net, size_t index)
{
	if (net == NULL || index >= net->chat_held)
		return (NULL);
	return (&net->chat[(net->chat_head + index) % NET_CHAT_HISTORY]);
}

/**
 * @brief Posts one line to a room's feed.
 *
 * Nothing is filed locally on the way out. The server echoes the message back
 * to its sender along with everybody else, so a client that also appended it
 * here would draw it twice - and the copy it drew would be the one without a
 * server sequence number.
 *
 * The text is checked here as well as at the server. That is not distrust of
 * the answer: a message with a newline in it would be refused with a 400 the
 * player has to interpret, and refusing it in the composer is a better place
 * to find out.
 *
 * @param net Connected client, seated in the room.
 * @param room The room's name, as the server spells it.
 * @param text The message.
 * @param out Receives the server's answer; may be NULL.
 * @return 0 when the server accepted it, -1 otherwise.
 */
int	net_chat_send(t_net_client *net, const char *room, const char *text,
		t_net_result *out)
{
	t_net_result	local;
	char			path[NET_PATH_MAX];
	char			body[BODY_CHAT_TEXT_MAX + 16];

	if (out == NULL)
		out = &local;
	memset(out, 0, sizeof(*out));
	if (net == NULL || room == NULL || room[0] == '\0' || text == NULL
		|| !sayable(text) || strlen(text) >= BODY_CHAT_TEXT_MAX)
		return (-1);
	if (net->state < NET_IN_ROOM)
		return (-1);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	snprintf(body, sizeof(body), "text %s\n", text);
	if (net_request(net, "CHAT", path, body, out) != 0)
		return (-1);
	if (out->status != 200)
		return (-1);
	return (0);
}

/**
 * @brief Checks that a message is non-empty and safe to put on one line.
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
