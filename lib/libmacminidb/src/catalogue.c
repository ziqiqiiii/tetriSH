/* ************************************************************************** */
/*                                                                            */
/*   catalogue.c — read-only Character / Theme tables from config (§6)        */
/*                                                                            */
/*   Static data: loaded once at boot, never written to the log. Paths come   */
/*   from .tetrishrc via config_dir — no hard-coded paths (CLAUDE.md). The     */
/*   line/field parsing lives in catalogue_parse.c; the per-type row parsers   */
/*   in catalogue_chars.c / catalogue_themes.c. Lookups are linear over the    */
/*   few dozen rows.                                                           */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Load the character and theme catalogues from config_dir.
 *
 * Allocates a zeroed catalogue, then fills it from characters.cfg and
 * themes.cfg. Either file missing or malformed fails the whole load (the
 * catalogue is freed and NULL returned) so the db never boots with a partial
 * roster.
 *
 * @param config_dir Directory holding characters.cfg and themes.cfg.
 * @return The loaded catalogue, or NULL on alloc or parse failure.
 */
t_catalogue	*catalogue_load(const char *config_dir)
{
	t_catalogue	*c;

	if (!config_dir)
		return (NULL);
	c = calloc(1, sizeof(*c));
	if (!c)
		return (NULL);
	if (catalogue_parse_chars(config_dir, c) != DB_OK
		|| catalogue_parse_themes(config_dir, c) != DB_OK)
	{
		catalogue_free(c);
		return (NULL);
	}
	return (c);
}

/**
 * @brief Free the catalogue.
 *
 * The character and theme rows are stored inline, so only the handle itself is
 * freed. Safe to call with a NULL catalogue.
 *
 * @param c The catalogue to free (may be NULL).
 */
void	catalogue_free(t_catalogue *c)
{
	free(c);
}

/**
 * @brief Look up a character row by its id.
 *
 * Linear scan over the loaded rows — the roster is a few dozen entries, so an
 * index would not pay for itself.
 *
 * @param c The loaded catalogue (may be NULL).
 * @param id The character id to find.
 * @return A pointer to the matching row, or NULL if absent.
 */
const t_character	*catalogue_character(t_catalogue *c, t_item_id id)
{
	size_t	i;

	if (!c)
		return (NULL);
	i = 0;
	while (i < c->char_count)
	{
		if (c->characters[i].character_id == id)
			return (&c->characters[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Look up a theme row by its id.
 *
 * Linear scan over the loaded rows, as for characters.
 *
 * @param c The loaded catalogue (may be NULL).
 * @param id The theme id to find.
 * @return A pointer to the matching row, or NULL if absent.
 */
const t_theme	*catalogue_theme(t_catalogue *c, t_item_id id)
{
	size_t	i;

	if (!c)
		return (NULL);
	i = 0;
	while (i < c->theme_count)
	{
		if (c->themes[i].theme_id == id)
			return (&c->themes[i]);
		i++;
	}
	return (NULL);
}
