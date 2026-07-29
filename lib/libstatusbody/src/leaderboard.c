#include "statusbody.h"

/**
 * @brief Serialises the UC-21 leaderboard body: one line per rank in the
 * form `<rank> <username> <score>`, rank ascending.
 *
 * An empty board (count 0) is a valid empty body (UC-21 ext 2a).
 *
 * @param rows The rows to serialise; may be NULL when count is 0.
 * @param count Number of rows.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes (0 for an empty board), or -1 with errno =
 *         EINVAL (NULL args with count > 0) or ERANGE (cap too small).
 */
int	sb_leaderboard_encode(const t_sb_lb_row *rows, size_t count, char *out, size_t cap)
{
	/* TODO: snprintf "<rank> <username> <score>\n" per row. */
	(void)rows;
	(void)count;
	(void)out;
	(void)cap;
	errno = ENOSYS;
	return (-1);
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
 *         (non-numeric rank/score, malformed line, overlong name), or
 *         ERANGE (more rows than cap).
 */
int	sb_leaderboard_decode(const char *buf, size_t len, t_sb_lb_row *rows, size_t cap, size_t *count)
{
	/* TODO: split lines within len; parse rank/name/score with caps. */
	(void)buf;
	(void)len;
	(void)rows;
	(void)cap;
	(void)count;
	errno = ENOSYS;
	return (-1);
}
