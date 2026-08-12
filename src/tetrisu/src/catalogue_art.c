#include "tetrisu.h"

// Static Variables
/*
 * Every table here is keyed by the server catalogue id. Names can be renamed
 * server-side and no other end of the client has ever heard of the numeric
 * ids, so neither field is a safe cross-end key; the catalogue id is the one
 * thing both ends agree on and it never changes. The ids also carry gaps
 * (theme 5 was cut), so each lookup scans for a matching id and nothing here
 * is ever indexed by position.
 */
static const uint32_t	g_character_ids[APP_CATALOGUE_CHARACTER_ROWS] = {
	1U, 2U, 3U, 4U
};

static const char		*g_character_slugs[APP_CATALOGUE_CHARACTER_ROWS] = {
	"halloween", "mirurun", "princess", "wolfman"
};

static const char		*g_character_ability_names[
	APP_CATALOGUE_CHARACTER_ROWS][APP_CHARACTER_ABILITY_COUNT] = {
	{"Fry", "Dark", "Vampire", "Bomb"},
	{"Mirurun", "Inversion", "Pentaris", "Sirtet"},
	{"Sol", "Mirror", "Paralysis", "Copy"},
	{"Cut", "Nue", "Pals", "Thwack"}
};

static const char		*g_character_ability_descriptions[
	APP_CATALOGUE_CHARACTER_ROWS][APP_CHARACTER_ABILITY_COUNT] = {
	{
		"Fills bottom 3 rows, then clears/sends them after next piece.",
		"Blacks out opponent field except near active piece.",
		"Steals opponent crystals.",
		"Destroys random opponent-field blocks."
	},
	{
		"Removes player bottom 4 rows, not sent.",
		"Inverts opponent controls next 3 pieces.",
		"Sends 5 garbage lines.",
		"Inverts filled/empty normal cells in all occupied opponent rows."
	},
	{
		"Clears 3 adjacent player-field columns, aimable, 3-second auto-fire.",
		"Steals opponent's next crystal power.",
		"Prevents opponent rotating next 3 pieces.",
		"Replaces player field with copy of opponent's."
	},
	{
		"Clears player's top 4 rows.",
		"Prevents opponent fast-dropping next 4 pieces.",
		"Incoming normal garbage lowers player stack briefly (power-raised lines excluded).",
		"For next 4 pieces, non-crystal blocks cascade after line clears."
	}
};

static const uint32_t	g_theme_ids[APP_CATALOGUE_THEME_ROWS] = {
	1U, 2U, 3U, 4U, 6U, 7U, 8U
};

static const char		*g_theme_slugs[APP_CATALOGUE_THEME_ROWS] = {
	"classic", "design_ai_university", "snowman", "haaland", "clauding",
	"al_merqaedes", "nuclear_ghandi"
};

/*
 * Bitmap planes are looked up by the same id the slugs above use, so the two
 * columns always share a row. The preview macros are the only place the
 * artwork paths are written; no row here hardcodes one.
 */
static const char		*g_theme_preview_paths[APP_CATALOGUE_THEME_ROWS] = {
	SETTINGS_THEME_CLASSIC_PREVIEW_PATH,
	SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH,
	SETTINGS_THEME_SNOWMAN_PREVIEW_PATH,
	SETTINGS_THEME_HAALAND_PREVIEW_PATH,
	SETTINGS_THEME_CLAUDING_PREVIEW_PATH,
	SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH,
	SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH
};

static const char		*g_theme_directories[APP_CATALOGUE_THEME_ROWS] = {
	"default_theme",
	"design_ai_university_theme",
	"do_you_want_a_snowman_theme",
	"haaland_theme",
	"clauding_theme",
	"al_merqaedes_f1_team_theme",
	"nuclear_ghandi_theme"
};

// Static Functions
static int	character_row_of(uint32_t item_id);
static int	theme_row_of(uint32_t item_id);

/**
 * @brief Resolves a character catalogue id to its local display slug.
 *
 * @param item_id The catalogue id tetrisd sells the character under.
 * @return The slug, or NULL if the table does not know the id.
 */
const char	*catalogue_character_slug(uint32_t item_id)
{
	int	row;

	row = character_row_of(item_id);
	if (row < 0)
		return (NULL);
	return (g_character_slugs[row]);
}

/**
 * @brief Writes the four crystal powers the catalogue lists for one id.
 *
 * Every level the table knows is copied into the caller's array; an id the
 * table does not know zeroes all entries so the caller never reads stale
 * text.
 *
 * @param item_id The catalogue id of the character.
 * @param out The row-major array the four powers are written into.
 */
void	catalogue_character_abilities(uint32_t item_id,
			t_app_character_ability_view_model *out)
{
	int	level;
	int	row;

	if (out == NULL)
		return ;
	memset(out, 0, sizeof(*out) * (size_t)APP_CHARACTER_ABILITY_COUNT);
	row = character_row_of(item_id);
	if (row < 0)
		return ;
	level = 0;
	while (level < APP_CHARACTER_ABILITY_COUNT)
	{
		snprintf(out[level].name, sizeof(out[level].name), "%s",
			g_character_ability_names[row][level]);
		snprintf(out[level].description, sizeof(out[level].description), "%s",
			g_character_ability_descriptions[row][level]);
		level++;
	}
}

/**
 * @brief Resolves a theme catalogue id to its local display slug.
 *
 * @param item_id The catalogue id tetrisd sells the theme under.
 * @return The slug, or NULL if the table does not know the id.
 */
const char	*catalogue_theme_slug(uint32_t item_id)
{
	int	row;

	row = theme_row_of(item_id);
	if (row < 0)
		return (NULL);
	return (g_theme_slugs[row]);
}

/**
 * @brief Resolves a theme catalogue id to its settings preview bitmap.
 *
 * @param item_id The catalogue id of the theme.
 * @return The preview asset path, or NULL if the table does not know the id.
 */
const char	*catalogue_theme_preview(uint32_t item_id)
{
	int	row;

	row = theme_row_of(item_id);
	if (row < 0)
		return (NULL);
	return (g_theme_preview_paths[row]);
}

/**
 * @brief Resolves a theme catalogue id to its artwork directory.
 *
 * @param item_id The catalogue id of the theme.
 * @return The directory holding the theme's assets, or NULL on an unknown id.
 */
const char	*catalogue_theme_directory(uint32_t item_id)
{
	int	row;

	row = theme_row_of(item_id);
	if (row < 0)
		return (NULL);
	return (g_theme_directories[row]);
}

/**
 * @brief Finds which table row a character catalogue id occupies.
 *
 * A scan rather than an index: the id is the server's label for the item, not
 * its position here, and the two only look interchangeable while the roster
 * happens to be numbered from one with no gaps.
 *
 * @param item_id The catalogue id to look for.
 * @return The row index, or -1 when no row carries that id.
 */
static int	character_row_of(uint32_t item_id)
{
	int	row;

	row = 0;
	while (row < APP_CATALOGUE_CHARACTER_ROWS)
	{
		if (g_character_ids[row] == item_id)
			return (row);
		row++;
	}
	return (-1);
}

/**
 * @brief Finds which table row a theme catalogue id occupies.
 *
 * The theme ids are the ones that make the scan load-bearing rather than
 * merely correct: id 5 was cut, so row 4 holds id 6 and everything after it
 * is off by one from its own id.
 *
 * @param item_id The catalogue id to look for.
 * @return The row index, or -1 when no row carries that id.
 */
static int	theme_row_of(uint32_t item_id)
{
	int	row;

	row = 0;
	while (row < APP_CATALOGUE_THEME_ROWS)
	{
		if (g_theme_ids[row] == item_id)
			return (row);
		row++;
	}
	return (-1);
}
