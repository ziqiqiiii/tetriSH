#include "body_util.h"

/**
 * @brief Serialises the UC-27 DROPPED body as one `dropped <n>` line.
 *
 * @param in The counter to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args) or
 *         ERANGE (cap too small).
 */
int	body_dropped_encode(const t_body_dropped *in, char *out, size_t cap)
{
	size_t	off;

	if (!in || !out)
		return (body_fail(EINVAL));
	if (cap == 0)
		return (body_fail(ERANGE));
	out[0] = '\0';
	off = 0;
	if (body_append(out, cap, &off, "dropped %" PRIu64 "\n", in->dropped) != 0)
		return (body_fail(ERANGE));
	return ((int)off);
}

/**
 * @brief Parses a DROPPED body back into the counter.
 *
 * An empty body is refused rather than read as zero: a server with nothing
 * dropped still sends the line, so an empty body means the answer never
 * arrived.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded counter, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing or wrong key, malformed value, trailing content).
 */
int	body_dropped_decode(const char *buf, size_t len, t_body_dropped *out)
{
	t_body_cursor	c;
	char			line[BODY_LINE_MAX];

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	c.p = buf;
	c.end = buf + len;
	if (body_at_end(&c) || body_take_line(&c, line, sizeof(line)) != 0)
		return (body_fail(EBADMSG));
	if (strncmp(line, "dropped ", 8) != 0
		|| body_parse_u64(line + 8, &out->dropped) != 0)
		return (body_fail(EBADMSG));
	if (!body_at_end(&c))
		return (body_fail(EBADMSG));
	return (0);
}
