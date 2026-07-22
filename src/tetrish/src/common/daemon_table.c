/* strptime() is an X/Open extension; request it before any header is pulled
 * in so the declaration is visible under the project's strict -Werror build. */
#define _XOPEN_SOURCE 700

#include "common.h"

/* Lower bound for the name column, so short names still line up under the
 * "name" header rather than collapsing against the pid column. */
#define NAME_COL_MIN 14

/**
 * @brief Width of the name column for a set of daemons.
 *
 * Sized to the longest name present so a long entry (for example
 * "deamon_eskimo.1") cannot push the pid column out of alignment, with a
 * floor of NAME_COL_MIN to keep narrow tables from looking cramped.
 *
 * @param names Array of names to measure, and the stride between them. Taking
 *              a base pointer plus an explicit stride lets callers measure the
 *              name fields of a struct array without copying them out.
 * @param count Number of entries to measure.
 * @param stride Byte distance between consecutive names.
 * @return Column width in characters.
 */
int	daemon_name_width(const char *names, int count, size_t stride)
{
	int		width;
	size_t	len;

	width = NAME_COL_MIN;
	for (int i = 0; i < count; ++i)
	{
		len = strlen(names + (size_t)i * stride);
		if ((int)len > width)
			width = (int)len;
	}
	return (width);
}

/**
 * @brief Convert a registry timestamp into an elapsed "H:MM:SS" string.
 *
 * Parses the ctime-formatted timestamp written by dspawn back into a time_t
 * and renders the difference from now. Falls back to "--" when the timestamp
 * cannot be parsed or lies in the future.
 *
 * @param ts Timestamp string as stored in the registry.
 * @param out Destination buffer.
 * @param out_size Size of the destination buffer.
 */
void	format_uptime(const char *ts, char *out, size_t out_size)
{
	struct tm	tm;
	time_t		started;
	long		secs;

	memset(&tm, 0, sizeof(tm));
	tm.tm_isdst = -1;
	if (!ts || !strptime(ts, "%a %b %d %H:%M:%S %Y", &tm))
	{
		snprintf(out, out_size, "--");
		return ;
	}
	started = mktime(&tm);
	secs = (long)difftime(time(NULL), started);
	if (started == (time_t)-1 || secs < 0)
	{
		snprintf(out, out_size, "--");
		return ;
	}
	snprintf(out, out_size, "%ld:%02ld:%02ld", secs / 3600,
		(secs % 3600) / 60, secs % 60);
}

/**
 * @brief Print the dimmed column labels and the rule underneath them.
 *
 * @param indent Leading text occupying the index column (for example "    "
 *               in dcheck, or "#   " in dkill).
 * @param name_width Width of the name column, from daemon_name_width().
 */
void	daemon_table_header(const char *indent, int name_width)
{
	int	rule;

	printf("  %s%s%-*s %-7s %-7s %-8s%s\n", CL_DIM, indent, name_width,
		"name", "pid", "state", "uptime", CL_RESET);
	/* Rule spans the indent plus every column and its separating space. */
	rule = (int)strlen(indent) + name_width + 1 + 7 + 1 + 7 + 1 + 8;
	printf("  %s", CL_DIM);
	for (int i = 0; i < rule; ++i)
		printf("-");
	printf("%s\n", CL_RESET);
}

/**
 * @brief Print one daemon row: name, pid, state, uptime.
 *
 * Live daemons show a green "up" plus their elapsed uptime; dead ones show a
 * red "down" and a dash.
 *
 * @param indent Leading text occupying the index column.
 * @param name_width Width of the name column, from daemon_name_width().
 * @param name Daemon name as registered.
 * @param pid Daemon process id.
 * @param alive Non-zero when /proc/<pid> still exists.
 * @param ts Registry timestamp used to compute the uptime.
 */
void	daemon_table_row(const char *indent, int name_width, const char *name,
		int pid, int alive, const char *ts)
{
	char	uptime[32];

	if (alive)
		format_uptime(ts, uptime, sizeof(uptime));
	else
		snprintf(uptime, sizeof(uptime), "-");

	/* Indent carries the row index in dkill; dimmed to match the header. */
	printf("  %s%s%s%-*s %s%-7d%s %s%-7s%s %s%-8s%s\n",
		CL_DIM, indent, CL_RESET, name_width, name,
		CL_BLUE, pid, CL_RESET,
		alive ? CL_GREEN : CL_RED, alive ? "up" : "down", CL_RESET,
		CL_DIM, uptime, CL_RESET);
}
