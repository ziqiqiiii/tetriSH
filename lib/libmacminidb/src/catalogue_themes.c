/* ************************************************************************** */
/*                                                                            */
/*   catalogue_themes.c — parse the theme catalogue (§6)                       */
/*                                                                            */
/*   Reads themes.cfg line by line into the catalogue's theme table. Each row  */
/*   is "id | name | cost | description"; a theme is cosmetic (no gameplay      */
/*   effect) but has a wallet_points price, a short name, and a description.    */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static int	parse_theme_row(char **f, int n, t_theme *out);

/**
 * @brief Load every theme row from <config_dir>/themes.cfg.
 *
 * Opens the file, then appends one t_theme per significant line until the table
 * cap is reached. A missing file or a malformed row fails the whole load
 * (DB_IO_ERROR) so the catalogue is all-or-nothing rather than silently partial.
 *
 * @param config_dir Directory holding themes.cfg (from .tetrishrc).
 * @param c The catalogue whose themes[] / theme_count are filled.
 * @return DB_OK on a clean load, DB_IO_ERROR on open/parse/overflow failure.
 */
t_db_result	catalogue_parse_themes(const char *config_dir, t_catalogue *c)
{
	FILE	*f;
	char	line[DB_CFG_LINE_MAX];
	char	*fields[8];
	int		n;

	f = catalogue_open(config_dir, DB_THEME_CFG_NAME);
	if (!f)
		return (DB_IO_ERROR);
	while (catalogue_next_line(f, line, sizeof(line)))
	{
		if (c->theme_count >= DB_MAX_THEMES)
			return (fclose(f), DB_IO_ERROR);
		n = catalogue_split(line, fields, 8);
		if (parse_theme_row(fields, n, &c->themes[c->theme_count]) != 0)
			return (fclose(f), DB_IO_ERROR);
		c->theme_count++;
	}
	return (fclose(f), DB_OK);
}

/**
 * @brief Parse one split theme row into a t_theme.
 *
 * Expects exactly four fields (id, name, cost, description). The description
 * may itself contain no '|' (the splitter would have cut it), which suits the
 * prose used here. Both strings are copied NUL-terminated within their bounds.
 *
 * @param f The split field pointers.
 * @param n The number of fields present.
 * @param out Destination theme row.
 * @return 0 on success, -1 if the row does not have four fields.
 */
static int	parse_theme_row(char **f, int n, t_theme *out)
{
	if (n != 4)
		return (-1);
	out->theme_id = (t_item_id)strtoul(f[0], NULL, 10);
	strncpy(out->name, f[1], DB_MAX_USERNAME - 1);
	out->name[DB_MAX_USERNAME - 1] = '\0';
	out->cost_points = (int64_t)strtoll(f[2], NULL, 10);
	strncpy(out->description, f[3], DB_THEME_DESC_LEN - 1);
	out->description[DB_THEME_DESC_LEN - 1] = '\0';
	return (0);
}
