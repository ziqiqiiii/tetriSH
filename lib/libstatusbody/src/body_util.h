#ifndef BODY_UTIL_H
# define BODY_UTIL_H

# include "statusbody.h"

# include <stdarg.h>

/*
** Private to libstatusbody - not part of the public API. The four codecs
** all write the same plaintext `key value` lines and re-parse them under
** the same rules, so the append/scan primitives live here once rather
** than four times.
**
** Every decoder works on a bounded cursor: the body need not be NUL-
** terminated, and no primitive here reads past `end`. Lines longer than
** BODY_LINE_MAX are rejected rather than truncated, which is what bounds a
** hostile body.
*/

# define BODY_LINE_MAX	1024

typedef struct s_body_cursor
{
	const char	*p;
	const char	*end;
}	t_body_cursor;

int		body_fail(int err);
int		body_append(char *out, size_t cap, size_t *off, const char *fmt, ...);
int		body_take_line(t_body_cursor *c, char *line, size_t cap);
bool	body_at_end(const t_body_cursor *c);
int		body_parse_u64(const char *s, uint64_t *out);
int		body_parse_int(const char *s, int *out, int min, int max);
int		body_word_index(const char *word, const char *const *table, size_t n);

#endif
