#include "sb_util.h"

#include <inttypes.h>

// Static Functions
static int	decode_row(const char *line, t_sb_lb_row *row);

/**
 * @brief Serialises leaderboard rows, one `<rank> <username> <score>` line
 * per row, rank ascending.
 *
 * An empty board (count 0) is a valid empty body - UC-21 ext 2a.
 *
 * @param rows The rows to serialise; may be NULL when count is 0.
 * @param count Number of rows.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes (0 for an empty board), or -1 with errno =
 *         EINVAL (NULL args with count > 0, unrepresentable field) or
 *         ERANGE (cap too small).
 */
int	sb_leaderboard_encode(const t_sb_lb_row *rows, size_t count, char *out, size_t cap)
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
		if (rows[i].rank < 0 || rows[i].username[0] == '\0')
			return (sb_fail(EINVAL));
		if (sb_append(out, cap, &off, "%d %s %" PRIu64 "\n", rows[i].rank,
				rows[i].username, rows[i].score) != 0)
			return (sb_fail(ERANGE));
		i++;
	}
	return ((int)off);
}

/**
 * @brief Parses a leaderboard body into rows.
 *
 * An empty buffer decodes to zero rows, not an error.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param rows Caller array receiving the decoded rows.
 * @param cap Capacity of rows.
 * @param count Receives the number of rows decoded.
 * @return 0 on success, -1 with errno = EINVAL (NULL args), EBADMSG
 *         (malformed line, overlong username), or ERANGE (more rows than
 *         cap).
 */
int	sb_leaderboard_decode(const char *buf, size_t len, t_sb_lb_row *rows, size_t cap, size_t *count)
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
 * @brief Parses one leaderboard line into a row.
 *
 * @param line The line text, newline already stripped.
 * @param row The row to fill.
 * @return 0 on success, -1 on a malformed rank, username, or score.
 */
static int	decode_row(const char *line, t_sb_lb_row *row)
{
	char	name[SB_LINE_MAX];
	char	score[SB_LINE_MAX];
	int		n;

	memset(row, 0, sizeof(*row));
	if (sscanf(line, "%d %1023s %1023s%n", &row->rank, name, score, &n) != 3
		|| line[n] != '\0' || row->rank < 0)
		return (-1);
	if (strlen(name) >= SB_USER_MAX)
		return (-1);
	if (sb_parse_u64(score, &row->score) != 0)
		return (-1);
	strcpy(row->username, name);
	return (0);
}
