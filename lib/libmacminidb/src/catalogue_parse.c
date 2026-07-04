/* ************************************************************************** */
/*                                                                            */
/*   catalogue_parse.c — shared config-file scanning primitives (§6)          */
/*                                                                            */
/*   The line/field plumbing behind the character and theme parsers: open a   */
/*   <config_dir>/<name> file, yield one significant line at a time (blank     */
/*   and '#'-comment lines skipped, trailing newline stripped), and split a    */
/*   line into '|'-separated fields in place. No hard-coded paths — the dir    */
/*   is passed in from .tetrishrc (CLAUDE.md).                                 */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Open <config_dir>/<name> for reading.
 *
 * Joins the directory and file name into a bounded path and opens it read-only.
 * A path that would overflow the buffer, or a file that cannot be opened, both
 * yield NULL so the caller can fail the load cleanly.
 *
 * @param config_dir Directory holding the config files (from .tetrishrc).
 * @param name The config file name (DB_CHAR_CFG_NAME / DB_THEME_CFG_NAME).
 * @return The open read stream, or NULL on a path overflow or open failure.
 */
FILE	*catalogue_open(const char *config_dir, const char *name)
{
	char	path[PATH_MAX];
	int		n;

	n = snprintf(path, sizeof(path), "%s/%s", config_dir, name);
	if (n < 0 || (size_t)n >= sizeof(path))
		return (NULL);
	return (fopen(path, "r"));
}

/**
 * @brief Read the next significant line into a buffer, skipping comments.
 *
 * Pulls lines via fgets until one is neither blank nor a '#' comment, strips the
 * trailing newline, and returns it. Lines longer than cap are read up to the cap
 * (the remainder is discarded on the next fgets), which is acceptable for the
 * fixed-shape config rows. Returns 0 at end of file.
 *
 * @param f The open config stream.
 * @param line Destination buffer.
 * @param cap Size of line in bytes.
 * @return 1 if a significant line was read, 0 at end of file.
 */
int	catalogue_next_line(FILE *f, char *line, size_t cap)
{
	size_t	len;

	while (fgets(line, (int)cap, f))
	{
		len = strlen(line);
		if (len && line[len - 1] == '\n')
			line[--len] = '\0';
		if (len == 0 || line[0] == '#')
			continue ;
		return (1);
	}
	return (0);
}

/**
 * @brief Split a line into '|'-separated fields in place.
 *
 * Replaces each '|' with a NUL and records the start of every field, so the
 * fields alias the caller's line buffer (no allocation). Stops once max fields
 * are recorded; any further '|' are left inside the last field.
 *
 * @param line The line to split (mutated: separators become NULs).
 * @param fields Destination array receiving up to max field pointers.
 * @param max Capacity of the fields array.
 * @return The number of fields found (1..max).
 */
int	catalogue_split(char *line, char **fields, int max)
{
	int	n;

	n = 0;
	fields[n++] = line;
	while (*line && n < max)
	{
		if (*line == '|')
		{
			*line = '\0';
			fields[n++] = line + 1;
		}
		line++;
	}
	return (n);
}
