/* ************************************************************************** */
/*                                                                            */
/*   catalogue_chars.c — parse the character catalogue (§6)                    */
/*                                                                            */
/*   Reads characters.cfg line by line into the catalogue's character table.  */
/*   Each row is "id | name | abilities(hex) | cost_points"; abilities is a    */
/*   bitfield of which of the character's 4 ability levels are offered (bit i  */
/*   => level i+1). The ability effects live in the game engine, not here.     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static int	parse_char_row(char **f, int n, t_character *out);

/**
 * @brief Load every character row from <config_dir>/characters.cfg.
 *
 * Opens the file, then appends one t_character per significant line until the
 * table cap is reached. A missing file or a malformed row fails the whole load
 * (DB_IO_ERROR) so the catalogue is all-or-nothing rather than silently partial.
 *
 * @param config_dir Directory holding characters.cfg (from .tetrishrc).
 * @param c The catalogue whose characters[] / char_count are filled.
 * @return DB_OK on a clean load, DB_IO_ERROR on open/parse/overflow failure.
 */
t_db_result	catalogue_parse_chars(const char *config_dir, t_catalogue *c)
{
	FILE	*f;
	char	line[DB_CFG_LINE_MAX];
	char	*fields[8];
	int		n;

	f = catalogue_open(config_dir, DB_CHAR_CFG_NAME);
	if (!f)
		return (DB_IO_ERROR);
	while (catalogue_next_line(f, line, sizeof(line)))
	{
		if (c->char_count >= DB_MAX_CHARACTERS)
			return (fclose(f), DB_IO_ERROR);
		n = catalogue_split(line, fields, 8);
		if (parse_char_row(fields, n, &c->characters[c->char_count]) != 0)
			return (fclose(f), DB_IO_ERROR);
		c->char_count++;
	}
	return (fclose(f), DB_OK);
}

/**
 * @brief Parse one split character row into a t_character.
 *
 * Expects exactly four fields (id, name, abilities-hex, cost). The numeric
 * fields are read with the C library's base-aware/explicit conversions; the
 * name is copied NUL-terminated within DB_MAX_USERNAME.
 *
 * @param f The split field pointers.
 * @param n The number of fields present.
 * @param out Destination character row.
 * @return 0 on success, -1 if the row does not have four fields.
 */
static int	parse_char_row(char **f, int n, t_character *out)
{
	if (n != 4)
		return (-1);
	out->character_id = (t_item_id)strtoul(f[0], NULL, 10);
	strncpy(out->name, f[1], DB_MAX_USERNAME - 1);
	out->name[DB_MAX_USERNAME - 1] = '\0';
	out->abilities = (uint32_t)strtoul(f[2], NULL, 0);
	out->cost_points = (int64_t)strtoll(f[3], NULL, 10);
	return (0);
}
