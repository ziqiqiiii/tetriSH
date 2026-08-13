#include "body_util.h"

// Static Functions
static int	encode_row(const t_body_player_row *row, char *out, size_t cap, size_t *off);
static int	decode_row(const char *line, t_body_player_row *row);

/**
 * @brief Serialises the UC-26 PLAYERS listing, one
 * `<connection> <username> <room>` line per connection.
 *
 * A connection that has not logged in writes BODY_ANONYMOUS in the username
 * column, and one sitting in no room writes `-`. Both are placeholders rather
 * than empty fields: the row is positional, so an empty field would shift
 * every column after it.
 *
 * An empty listing (count 0) is a valid empty body - UC-26 ext 2a.
 *
 * @param rows The rows to serialise; may be NULL when count is 0.
 * @param count Number of rows.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes (0 when nobody is connected), or -1 with
 *         errno = EINVAL (NULL args with count > 0, an authenticated row with
 *         no name, a field carrying a space) or ERANGE (cap too small).
 */
int	body_players_encode(const t_body_player_row *rows, size_t count, char *out, size_t cap)
{
	size_t	off;
	size_t	i;

	if (!out || (!rows && count > 0))
		return (body_fail(EINVAL));
	if (cap == 0)
		return (body_fail(ERANGE));
	out[0] = '\0';
	off = 0;
	i = 0;
	while (i < count)
	{
		if (encode_row(&rows[i], out, cap, &off) != 0)
			return (-1);
		i++;
	}
	return ((int)off);
}

/**
 * @brief Parses a PLAYERS body into rows.
 *
 * An empty buffer decodes to zero rows, not an error. The placeholders are
 * mapped back: BODY_ANONYMOUS clears `authenticated` and leaves the name
 * empty, `-` leaves the room empty.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param rows Caller array receiving the decoded rows.
 * @param cap Capacity of rows.
 * @param count Receives the number of rows decoded.
 * @return 0 on success, -1 with errno = EINVAL (NULL args), EBADMSG
 *         (malformed line, overlong name or room), or ERANGE (more rows than
 *         cap).
 */
int	body_players_decode(const char *buf, size_t len, t_body_player_row *rows, size_t cap, size_t *count)
{
	t_body_cursor	c;
	char			line[BODY_LINE_MAX];

	if (!buf || !rows || !count)
		return (body_fail(EINVAL));
	*count = 0;
	c.p = buf;
	c.end = buf + len;
	while (!body_at_end(&c))
	{
		if (*count == cap)
			return (body_fail(ERANGE));
		if (body_take_line(&c, line, sizeof(line)) != 0
			|| decode_row(line, &rows[*count]) != 0)
			return (body_fail(EBADMSG));
		(*count)++;
	}
	return (0);
}

/**
 * @brief Writes one player row, substituting the two placeholders.
 *
 * @param row The row to write.
 * @param out Caller buffer.
 * @param cap Size of out.
 * @param off Running write offset, advanced on success.
 * @return 0 on success, -1 with errno already set by body_fail.
 */
static int	encode_row(const t_body_player_row *row, char *out, size_t cap, size_t *off)
{
	const char	*name;
	const char	*room;

	if (row->authenticated && row->username[0] == '\0')
		return (body_fail(EINVAL));
	name = BODY_ANONYMOUS;
	if (row->authenticated)
		name = row->username;
	room = "-";
	if (row->room[0] != '\0')
		room = row->room;
	if (strchr(name, ' ') != NULL || strchr(room, ' ') != NULL)
		return (body_fail(EINVAL));
	if (body_append(out, cap, off, "%" PRIu64 " %s %s\n", row->connection,
			name, room) != 0)
		return (body_fail(ERANGE));
	return (0);
}

/**
 * @brief Parses one player line into a row.
 *
 * @param line The line text, newline already stripped.
 * @param row The row to fill.
 * @return 0 on success, -1 on a malformed id, name, or room.
 */
static int	decode_row(const char *line, t_body_player_row *row)
{
	char	conn[BODY_LINE_MAX];
	char	name[BODY_LINE_MAX];
	char	room[BODY_LINE_MAX];
	int		n;

	memset(row, 0, sizeof(*row));
	if (sscanf(line, "%1023s %1023s %1023s%n", conn, name, room, &n) != 3
		|| line[n] != '\0')
		return (-1);
	if (body_parse_u64(conn, &row->connection) != 0)
		return (-1);
	if (strlen(name) >= BODY_USER_MAX || strlen(room) >= BODY_NAME_MAX)
		return (-1);
	if (strcmp(name, BODY_ANONYMOUS) != 0)
	{
		row->authenticated = true;
		strcpy(row->username, name);
	}
	if (strcmp(room, "-") != 0)
		strcpy(row->room, room);
	return (0);
}
