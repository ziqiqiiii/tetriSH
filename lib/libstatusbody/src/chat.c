#include "body_util.h"

// Static Variables
static const char	*g_kinds[] = {
	"player", "system"
};

// Static Functions
static bool	chat_valid(const t_body_chat *chat);
static bool	printable_line(const char *text);
static int	decode_head(t_body_cursor *cursor, t_body_chat *chat);
static int	decode_author(t_body_cursor *cursor, t_body_chat *chat);
static int	read_value(t_body_cursor *cursor, const char *key, char *value,
				size_t cap);

/**
 * @brief Serialises one line of a room's feed.
 *
 * A system message carries no sender line at all rather than an empty one,
 * so "who wrote this" has one representation per kind and a decoder never
 * has to treat a blank name as an author.
 *
 * @param in Message to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length, or -1 with errno set to EINVAL or ERANGE.
 */
int	body_chat_encode(const t_body_chat *in, char *out, size_t cap)
{
	size_t	offset;

	if (!in || !out || !chat_valid(in))
		return (body_fail(EINVAL));
	offset = 0;
	if (body_append(out, cap, &offset, "seq %" PRIu64 "\n", in->seq) != 0
		|| body_append(out, cap, &offset, "at %" PRIu64 "\n", in->at) != 0
		|| body_append(out, cap, &offset, "kind %s\n",
			g_kinds[in->system]) != 0)
		return (body_fail(ERANGE));
	if (!in->system && body_append(out, cap, &offset, "sender %s\n",
			in->sender) != 0)
		return (body_fail(ERANGE));
	if (body_append(out, cap, &offset, "text %s\n", in->text) != 0)
		return (body_fail(ERANGE));
	return ((int)offset);
}

/**
 * @brief Parses one room feed line.
 *
 * @param buf Received body bytes, which need not be NUL-terminated.
 * @param len Number of body bytes.
 * @param out Receives the fully overwritten message.
 * @return 0, or -1 with errno set to EINVAL or EBADMSG.
 */
int	body_chat_decode(const char *buf, size_t len, t_body_chat *out)
{
	t_body_cursor	cursor;

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	cursor.p = buf;
	cursor.end = buf + len;
	if (decode_head(&cursor, out) != 0
		|| decode_author(&cursor, out) != 0
		|| read_value(&cursor, "text", out->text, sizeof(out->text)) != 0
		|| !body_at_end(&cursor) || !chat_valid(out))
		return (body_fail(EBADMSG));
	return (0);
}

/**
 * @brief Checks a complete message before it reaches the wire.
 *
 * @param chat Message to validate.
 * @return true when every field is representable on one line.
 */
static bool	chat_valid(const t_body_chat *chat)
{
	if (chat->seq == 0 || !printable_line(chat->text))
		return (false);
	if (chat->system)
		return (chat->sender[0] == '\0');
	return (chat->sender[0] != '\0' && strchr(chat->sender, ' ') == NULL
		&& printable_line(chat->sender));
}

/**
 * @brief Checks that text is non-empty and survives one line of the body.
 *
 * The body is line-based, so a newline would end the line early and the
 * remainder would re-decode as another key. Every other control character
 * is refused with it: a feed is drawn into a terminal, and an escape
 * sequence arriving as chat is somebody else's cursor.
 *
 * @param text NUL-terminated candidate.
 * @return true when text is non-empty and entirely printable.
 */
static bool	printable_line(const char *text)
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

/**
 * @brief Reads the sequence, timestamp and kind lines in schema order.
 *
 * @param cursor Bounded body cursor.
 * @param chat Message being decoded.
 * @return 0 on success, -1 on a malformed head.
 */
static int	decode_head(t_body_cursor *cursor, t_body_chat *chat)
{
	char	value[BODY_LINE_MAX];
	int		index;

	if (read_value(cursor, "seq", value, sizeof(value)) != 0
		|| body_parse_u64(value, &chat->seq) != 0
		|| read_value(cursor, "at", value, sizeof(value)) != 0
		|| body_parse_u64(value, &chat->at) != 0
		|| read_value(cursor, "kind", value, sizeof(value)) != 0)
		return (-1);
	index = body_word_index(value, g_kinds, 2);
	if (index < 0)
		return (-1);
	chat->system = index == 1;
	return (0);
}

/**
 * @brief Reads the sender line, which only a player message carries.
 *
 * @param cursor Bounded body cursor.
 * @param chat Message being decoded, with its kind already read.
 * @return 0 on success, -1 when a player message has no usable sender.
 */
static int	decode_author(t_body_cursor *cursor, t_body_chat *chat)
{
	if (chat->system)
		return (0);
	return (read_value(cursor, "sender", chat->sender, sizeof(chat->sender)));
}

/**
 * @brief Reads one named value line.
 *
 * @param cursor Bounded body cursor.
 * @param key Expected key.
 * @param value Destination for the value.
 * @param cap Size of value.
 * @return 0 on success, -1 on a missing or malformed line.
 */
static int	read_value(t_body_cursor *cursor, const char *key, char *value,
	size_t cap)
{
	char	line[BODY_LINE_MAX];
	size_t	key_len;
	size_t	value_len;

	if (body_take_line(cursor, line, sizeof(line)) != 0)
		return (-1);
	key_len = strlen(key);
	if (strncmp(line, key, key_len) != 0 || line[key_len] != ' ')
		return (-1);
	value_len = strlen(line + key_len + 1);
	if (value_len == 0 || value_len >= cap)
		return (-1);
	memcpy(value, line + key_len + 1, value_len + 1);
	return (0);
}
