#include "coreipc.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char	*g_level_names[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

/**
 * @brief Build one fixed-size log record from caller-supplied fields.
 *
 * Zeroes the whole struct first so padding never carries stale bytes over
 * IPC, then truncates component and msg to their fixed fields (always
 * NUL-terminated). Identical inputs produce byte-identical records.
 *
 * @param out Destination record (caller-owned storage).
 * @param level One of the CIPC_LOG_* levels.
 * @param timestamp_ms Milliseconds since the epoch, chosen by the caller.
 * @param pid The emitting process id (t_log_record is per-process tagged).
 * @param component Short producer tag, e.g. "tetrisd"; NULL means "".
 * @param msg The formatted log text; must not be NULL.
 * @return 0 on success, -1 with errno = EINVAL on NULL out/msg or bad level.
 */
int	lr_make(t_log_record *out, t_log_level level, uint64_t timestamp_ms,
		uint32_t pid, const char *component, const char *msg)
{
	if (!out || !msg || level > CIPC_LOG_ERROR)
	{
		errno = EINVAL;
		return (-1);
	}
	memset(out, 0, sizeof(*out));
	out->magic = CIPC_LOG_MAGIC;
	out->version = CIPC_LOG_VERSION;
	out->level = (uint8_t)level;
	out->timestamp_ms = timestamp_ms;
	out->pid = pid;
	if (component)
		strncpy(out->component, component, CIPC_LOG_COMPONENT_MAX - 1);
	strncpy(out->msg, msg, CIPC_LOG_MSG_MAX - 1);
	out->msg_len = (uint16_t)strlen(out->msg);
	return (0);
}

/**
 * @brief Check that a byte buffer really is one well-formed log record.
 *
 * tetrislogd calls this on every datagram before trusting any field, so a
 * short read, a stray writer on the socket path, or a version mismatch is
 * rejected instead of parsed.
 *
 * @param buf The received bytes.
 * @param len Number of bytes received.
 * @return 0 when valid, -1 with errno = EINVAL (NULL buf) or
 *         EBADMSG (wrong size, magic, version, level, or msg framing).
 */
int	lr_validate(const void *buf, size_t len)
{
	const t_log_record	*rec;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	rec = (const t_log_record *)buf;
	if (len != sizeof(*rec)
		|| rec->magic != CIPC_LOG_MAGIC
		|| rec->version != CIPC_LOG_VERSION
		|| rec->level > CIPC_LOG_ERROR
		|| rec->msg_len >= CIPC_LOG_MSG_MAX
		|| rec->msg[rec->msg_len] != '\0'
		|| !memchr(rec->component, '\0', CIPC_LOG_COMPONENT_MAX))
	{
		errno = EBADMSG;
		return (-1);
	}
	return (0);
}

/**
 * @brief Map a level to its fixed display name.
 *
 * @param level The level to name.
 * @return A static string ("DEBUG".."ERROR"), or "UNKNOWN" out of range.
 */
const char	*lr_level_name(t_log_level level)
{
	if (level > CIPC_LOG_ERROR)
		return ("UNKNOWN");
	return (g_level_names[level]);
}

/**
 * @brief Parse a .tetrishrc log_level value (debug|info|warning|error).
 *
 * Case-insensitive, so config files and tetrisctl arguments both work.
 *
 * @param name The level name to parse.
 * @return The t_log_level value, or -1 with errno = EINVAL on no match.
 */
int	lr_level_parse(const char *name)
{
	int	i;

	if (name)
	{
		i = 0;
		while (i <= CIPC_LOG_ERROR)
		{
			if (strcasecmp(name, g_level_names[i]) == 0)
				return (i);
			i++;
		}
	}
	errno = EINVAL;
	return (-1);
}

/**
 * @brief Render one record as the single line tetrislogd writes to disk.
 *
 * Format: "<timestamp_ms> <LEVEL> <component>[<pid>]: <msg>\n". Writes into
 * the caller's buffer only - this library never touches a file or stream.
 *
 * @param rec A record that passed lr_validate (fields are trusted here).
 * @param out Destination buffer.
 * @param cap Size of out in bytes.
 * @return The line length excluding the NUL, or -1 with errno = EINVAL
 *         (NULL args) or ERANGE (cap too small for the full line).
 */
int	lr_format_line(const t_log_record *rec, char *out, size_t cap)
{
	int	n;

	if (!rec || !out)
	{
		errno = EINVAL;
		return (-1);
	}
	n = snprintf(out, cap, "%" PRIu64 " %-7s %s[%" PRIu32 "]: %s\n",
			rec->timestamp_ms, lr_level_name((t_log_level)rec->level),
			rec->component, rec->pid, rec->msg);
	if (n < 0 || (size_t)n >= cap)
	{
		errno = ERANGE;
		return (-1);
	}
	return (n);
}
