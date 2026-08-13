#include "body_util.h"

// Static Functions
static int	decode_counts(t_body_cursor *c, t_body_health *out);
static int	decode_tick(t_body_cursor *c, t_body_health *out);
static int	decode_sink(t_body_cursor *c, t_body_health *out);
static int	key_u64(t_body_cursor *c, const char *key, uint64_t *out);

/**
 * @brief Serialises the UC-22 Health body, one key per line.
 *
 * The tick line carries the literal word `configured` after its value. That
 * label is part of the format rather than decoration: nothing here measures an
 * observed rate, and a bare number would read as one.
 *
 * @param in The health report to serialise.
 * @param out Caller buffer receiving the body text.
 * @param cap Size of out; never written past.
 * @return Body length in bytes, or -1 with errno = EINVAL (NULL args, a
 *         negative pid, count or interval) or ERANGE (cap too small).
 */
int	body_health_encode(const t_body_health *in, char *out, size_t cap)
{
	size_t	off;

	if (!in || !out)
		return (body_fail(EINVAL));
	if (in->pid < 0 || in->connections < 0 || in->rooms < 0 || in->tick_ms < 0)
		return (body_fail(EINVAL));
	if (cap == 0)
		return (body_fail(ERANGE));
	out[0] = '\0';
	off = 0;
	if (body_append(out, cap, &off, "pid %" PRId64 "\n", in->pid) != 0
		|| body_append(out, cap, &off, "uptime_ms %" PRIu64 "\n",
			in->uptime_ms) != 0
		|| body_append(out, cap, &off, "connections %d\n",
			in->connections) != 0
		|| body_append(out, cap, &off, "rooms %d\n", in->rooms) != 0
		|| body_append(out, cap, &off, "tick_ms %d configured\n",
			in->tick_ms) != 0
		|| body_append(out, cap, &off, "sink %s\n",
			in->sink_reaching ? "reaching" : "unreachable") != 0)
		return (body_fail(ERANGE));
	return ((int)off);
}

/**
 * @brief Parses a Health body back into a report.
 *
 * Strict: every key present, in encode order, with no trailing content.
 *
 * @param buf The received body bytes (need not be NUL-terminated).
 * @param len Number of body bytes.
 * @param out Receives the decoded report, fully overwritten on success.
 * @return 0 on success, -1 with errno = EINVAL (NULL args) or EBADMSG
 *         (missing key, malformed value, missing label, trailing junk).
 */
int	body_health_decode(const char *buf, size_t len, t_body_health *out)
{
	t_body_cursor	c;
	uint64_t		pid;

	if (!buf || !out)
		return (body_fail(EINVAL));
	memset(out, 0, sizeof(*out));
	c.p = buf;
	c.end = buf + len;
	if (key_u64(&c, "pid", &pid) != 0 || pid > INT64_MAX
		|| key_u64(&c, "uptime_ms", &out->uptime_ms) != 0
		|| decode_counts(&c, out) != 0
		|| decode_tick(&c, out) != 0
		|| decode_sink(&c, out) != 0)
		return (body_fail(EBADMSG));
	if (!body_at_end(&c))
		return (body_fail(EBADMSG));
	out->pid = (int64_t)pid;
	return (0);
}

/**
 * @brief Reads the `connections` and `rooms` lines.
 *
 * @param c Cursor positioned at the connections line.
 * @param out Report receiving both counts.
 * @return 0 on success, -1 on a missing key or an out-of-range value.
 */
static int	decode_counts(t_body_cursor *c, t_body_health *out)
{
	uint64_t	connections;
	uint64_t	rooms;

	if (key_u64(c, "connections", &connections) != 0 || connections > INT_MAX)
		return (-1);
	if (key_u64(c, "rooms", &rooms) != 0 || rooms > INT_MAX)
		return (-1);
	out->connections = (int)connections;
	out->rooms = (int)rooms;
	return (0);
}

/**
 * @brief Reads the `tick_ms <n> configured` line.
 *
 * The label is required. A body that dropped it would decode to the same
 * number while claiming something the server never said.
 *
 * @param c Cursor positioned at the tick line.
 * @param out Report receiving the interval.
 * @return 0 on success, -1 on a missing key, bad value, or missing label.
 */
static int	decode_tick(t_body_cursor *c, t_body_health *out)
{
	char	line[BODY_LINE_MAX];
	int		tick;
	int		used;

	if (body_take_line(c, line, sizeof(line)) != 0)
		return (-1);
	used = 0;
	if (sscanf(line, "tick_ms %d %n", &tick, &used) != 1 || used == 0)
		return (-1);
	if (strcmp(line + used, "configured") != 0 || tick < 0)
		return (-1);
	out->tick_ms = tick;
	return (0);
}

/**
 * @brief Reads the `sink reaching|unreachable` line.
 *
 * @param c Cursor positioned at the sink line.
 * @param out Report receiving the verdict.
 * @return 0 on success, -1 on a missing key or an unrecognised word.
 */
static int	decode_sink(t_body_cursor *c, t_body_health *out)
{
	char	line[BODY_LINE_MAX];

	if (body_take_line(c, line, sizeof(line)) != 0)
		return (-1);
	if (strcmp(line, "sink reaching") == 0)
		return (out->sink_reaching = true, 0);
	if (strcmp(line, "sink unreachable") == 0)
		return (out->sink_reaching = false, 0);
	return (-1);
}

/**
 * @brief Reads one `<key> <unsigned>` line, requiring that exact key.
 *
 * @param c Cursor positioned at the line.
 * @param key The key the line must open with.
 * @param out Receives the parsed value.
 * @return 0 on success, -1 on a missing line, wrong key, or bad number.
 */
static int	key_u64(t_body_cursor *c, const char *key, uint64_t *out)
{
	char	line[BODY_LINE_MAX];
	size_t	key_len;

	if (body_take_line(c, line, sizeof(line)) != 0)
		return (-1);
	key_len = strlen(key);
	if (strncmp(line, key, key_len) != 0 || line[key_len] != ' ')
		return (-1);
	return (body_parse_u64(line + key_len + 1, out));
}
