#include "sb_util.h"

#include <stdarg.h>

/**
 * @brief Sets errno and returns the codec failure value in one step.
 *
 * @param err The errno value to report.
 * @return Always -1.
 */
int	sb_fail(int err)
{
	errno = err;
	return (-1);
}

/**
 * @brief Appends a formatted fragment to a body buffer, never past cap.
 *
 * @param out The body buffer being built.
 * @param cap Size of out in bytes; not one byte beyond it is written.
 * @param off In/out write offset, advanced by the bytes appended.
 * @param fmt printf-style format for the fragment.
 * @return 0 on success, -1 when the fragment does not fit.
 */
int	sb_append(char *out, size_t cap, size_t *off, const char *fmt, ...)
{
	va_list	ap;
	int		n;

	if (*off >= cap)
		return (-1);
	va_start(ap, fmt);
	n = vsnprintf(out + *off, cap - *off, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= cap - *off)
		return (-1);
	*off += (size_t)n;
	return (0);
}

/**
 * @brief Copies the next newline-terminated line out of a bounded cursor.
 *
 * An unterminated or over-long trailing run is a malformed body, not a
 * final line, so a body of arbitrary junk cannot be parsed as content.
 *
 * @param c The cursor, advanced past the line and its newline on success.
 * @param line Destination for the NUL-terminated line, newline stripped.
 * @param cap Size of line in bytes.
 * @return 0 on success, -1 when no newline remains or the line overflows.
 */
int	sb_take_line(t_sb_cursor *c, char *line, size_t cap)
{
	const char	*nl;
	size_t		n;

	if (c->p >= c->end)
		return (-1);
	nl = memchr(c->p, '\n', (size_t)(c->end - c->p));
	if (!nl)
		return (-1);
	n = (size_t)(nl - c->p);
	if (n >= cap || memchr(c->p, '\0', n))
		return (-1);
	memcpy(line, c->p, n);
	line[n] = '\0';
	c->p = nl + 1;
	return (0);
}

/**
 * @brief Reports whether a cursor has consumed its whole body.
 *
 * @param c The cursor to test.
 * @return true when nothing is left, false when bytes remain.
 */
bool	sb_at_end(const t_sb_cursor *c)
{
	return (c->p == c->end);
}

/**
 * @brief Parses a whole unsigned 64-bit value, rejecting anything else.
 *
 * Strict by design: a sign, stray text, or a value past UINT64_MAX is a
 * malformed body rather than a clamped number.
 *
 * @param s The NUL-terminated text to parse.
 * @param out Receives the parsed value.
 * @return 0 on success, -1 on empty, non-numeric, signed, or overflowing
 *         input.
 */
int	sb_parse_u64(const char *s, uint64_t *out)
{
	char				*end;
	unsigned long long	v;

	if (!s || *s < '0' || *s > '9')
		return (-1);
	errno = 0;
	v = strtoull(s, &end, 10);
	if (errno == ERANGE || *end != '\0' || end == s)
		return (-1);
	*out = (uint64_t)v;
	return (0);
}

/**
 * @brief Parses a whole int and checks it against an inclusive range.
 *
 * @param s The NUL-terminated text to parse.
 * @param out Receives the parsed value.
 * @param min Lowest accepted value.
 * @param max Highest accepted value.
 * @return 0 on success, -1 on non-numeric, trailing, or out-of-range
 *         input.
 */
int	sb_parse_int(const char *s, int *out, int min, int max)
{
	char	*end;
	long	v;

	if (!s || *s == '\0')
		return (-1);
	errno = 0;
	v = strtol(s, &end, 10);
	if (errno == ERANGE || *end != '\0' || end == s)
		return (-1);
	if (v < (long)min || v > (long)max)
		return (-1);
	*out = (int)v;
	return (0);
}

/**
 * @brief Maps a wire keyword to its index in an enum's name table.
 *
 * @param word The keyword read off the wire.
 * @param table The enum's names, in enum order.
 * @param n Number of entries in table.
 * @return The matching index, or -1 when the keyword is unknown.
 */
int	sb_word_index(const char *word, const char *const *table, size_t n)
{
	size_t	i;

	if (!word)
		return (-1);
	i = 0;
	while (i < n)
	{
		if (strcmp(word, table[i]) == 0)
			return ((int)i);
		i++;
	}
	return (-1);
}
