#ifndef SB_UTIL_H
# define SB_UTIL_H

# include "statusbody.h"

/*
** Private to libstatusbody - not part of the public API. The four codecs
** all write the same plaintext `key value` lines and re-parse them under
** the same rules, so the append/scan primitives live here once rather
** than four times.
**
** Every decoder works on a bounded cursor: the body need not be NUL-
** terminated, and no primitive here reads past `end`. Lines longer than
** SB_LINE_MAX are rejected rather than truncated, which is what bounds a
** hostile body.
*/

# define SB_LINE_MAX	1024

typedef struct s_sb_cursor
{
	const char	*p;
	const char	*end;
}	t_sb_cursor;

int		sb_fail(int err);
int		sb_append(char *out, size_t cap, size_t *off, const char *fmt, ...);
int		sb_take_line(t_sb_cursor *c, char *line, size_t cap);
bool	sb_at_end(const t_sb_cursor *c);
int		sb_parse_u64(const char *s, uint64_t *out);
int		sb_parse_int(const char *s, int *out, int min, int max);
int		sb_word_index(const char *word, const char *const *table, size_t n);

#endif
