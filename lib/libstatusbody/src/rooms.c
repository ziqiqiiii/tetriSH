#include "sb_util.h"

// Static Variables
static const char *const	g_modes[] = {
								"SINGLE", 
								"DOUBLE", 
								"BATTLE_ROYALE"
							};
static const char *const	g_statuses[] = {
								"WAITING", 
								"READY", 
								"IN_GAME", 
								"FINISHED"
							};

// Static Functions
static int	encode_row(const t_sb_room_row *row, char *out, size_t cap, size_t *off);
static int	decode_row(const char *line, t_sb_room_row *row);

/**
 * @brief Serialises the LIST /rooms body: one line per row in the form
 * `<name> <mode> <players>/<slots> <status> <owner>`.
 *
 * An empty list (count 0) is a valid empty body - the UC-03 empty state.
 * A room with no name or no owner yet has no wire form, so listing one is
 * EINVAL rather than a line that cannot be parsed back.
 *
 * @param rows The rows to serialise; may be NULL when count is 0.
 * @param count Number of rows.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes (0 for an empty list), or -1 with errno =
 *         EINVAL (NULL args with count > 0, bad enum value) or ERANGE
 *         (cap too small).
 */
int	sb_rooms_encode(const t_sb_room_row *rows, size_t count, char *out, size_t cap)
{
	size_t	off;
	size_t	i;

	if (!out || (!rows && count > 0))
		return (sb_fail(EINVAL));
	if (cap == 0)
		return (sb_fail(ERANGE));
	out[0] = '\0';
	off = 0;
	i = 0;
	while (i < count)
	{
		if (rows[i].mode > SB_MODE_BATTLE_ROYALE
			|| rows[i].status > SB_ROOM_FINISHED
			|| rows[i].name[0] == '\0' || rows[i].owner[0] == '\0')
			return (sb_fail(EINVAL));
		if (encode_row(&rows[i], out, cap, &off) != 0)
			return (sb_fail(ERANGE));
		i++;
	}
	return ((int)off);
}

/**
 * @brief Parses a LIST /rooms body into rows.
 *
 * An empty buffer decodes to zero rows, not an error.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param rows Caller array receiving the decoded rows.
 * @param cap Capacity of rows.
 * @param count Receives the number of rows decoded.
 * @return 0 on success, -1 with errno = EINVAL (NULL args), EBADMSG
 *         (malformed line, unknown mode/status token, overlong name/owner),
 *         or ERANGE (more rows than cap).
 */
int	sb_rooms_decode(const char *buf, size_t len, t_sb_room_row *rows, size_t cap, size_t *count)
{
	t_sb_cursor	c;
	char		line[SB_LINE_MAX];

	if (!buf || !rows || !count)
		return (sb_fail(EINVAL));
	*count = 0;
	c.p = buf;
	c.end = buf + len;
	while (!sb_at_end(&c))
	{
		if (*count == cap)
			return (sb_fail(ERANGE));
		if (sb_take_line(&c, line, sizeof(line)) != 0
			|| decode_row(line, &rows[*count]) != 0)
			return (sb_fail(EBADMSG));
		(*count)++;
	}
	return (0);
}

/**
 * @brief Writes one room row as its wire line.
 *
 * @param row The row to write.
 * @param out The body buffer.
 * @param cap Size of out.
 * @param off In/out write offset.
 * @return 0 on success, -1 when the buffer is exhausted.
 */
static int	encode_row(const t_sb_room_row *row, char *out, size_t cap,
		size_t *off)
{
	return (sb_append(out, cap, off, "%s %s %d/%d %s %s\n", row->name,
			g_modes[row->mode], row->players, row->slot_count,
			g_statuses[row->status], row->owner));
}

/**
 * @brief Parses one room line into a row.
 *
 * @param line The line text, newline already stripped.
 * @param row The row to fill.
 * @return 0 on success, -1 on a malformed field, unknown token, or a name
 *         or owner too long for its field.
 */
static int	decode_row(const char *line, t_sb_room_row *row)
{
	char	word[4][SB_LINE_MAX];
	int		n;
	int		mode;
	int		status;

	memset(row, 0, sizeof(*row));
	if (sscanf(line, "%1023s %1023s %d/%d %1023s %1023s%n", word[0], word[1],
			&row->players, &row->slot_count, word[2], word[3], &n) != 6
		|| line[n] != '\0' || row->players < 0 || row->slot_count < 0)
		return (-1);
	if (strlen(word[0]) >= SB_NAME_MAX || strlen(word[3]) >= SB_USER_MAX)
		return (-1);
	mode = sb_word_index(word[1], g_modes, 3);
	status = sb_word_index(word[2], g_statuses, 4);
	if (mode < 0 || status < 0)
		return (-1);
	strcpy(row->name, word[0]);
	strcpy(row->owner, word[3]);
	row->mode = (t_sb_mode)mode;
	row->status = (t_sb_room_status)status;
	return (0);
}
