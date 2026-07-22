// Unit tests for catalogue.c / catalogue_chars.c / catalogue_themes.c — load
// the read-only character & theme tables from config/ and look rows up by id.
// The catalogue API is internal, so this suite pulls in internal.h directly.
// Tests run from the library root (make -C lib/libmacminidb test), so the real
// config/ directory is reachable by that relative path.
#include "internal.h"
#include <assert.h>

#define CFG_DIR	"config"

// The bundled config loads with the expected roster sizes.
void	test_loads_full_roster(void)
{
	t_catalogue	*c;

	c = catalogue_load(CFG_DIR);
	assert(c != NULL);
	assert(c->char_count == 4);
	assert(c->theme_count == 8);
	catalogue_free(c);
	printf("PASS test_loads_full_roster\n");
}

// Halloween is id 1 and, like every character, costs a flat 10 points and
// offers all four ability levels (bitfield 0xF), matching characters.cfg.
void	test_character_lookup(void)
{
	t_catalogue			*c;
	const t_character	*halloween;
	const t_character	*wolfman;

	c = catalogue_load(CFG_DIR);
	assert(c != NULL);
	halloween = catalogue_character(c, 1);
	assert(halloween != NULL);
	assert(strcmp(halloween->name, "Halloween") == 0);
	assert(halloween->cost_points == 10);
	assert(halloween->abilities == 0xF);
	wolfman = catalogue_character(c, 4);
	assert(wolfman != NULL && strcmp(wolfman->name, "Wolf-man") == 0);
	assert(wolfman->cost_points == 10);
	assert(catalogue_character(c, 999) == NULL);
	catalogue_free(c);
	printf("PASS test_character_lookup\n");
}

// Themes carry an id, a short name, a wallet-points cost, and a description.
// id 1 (Default) is the free starting theme; paid themes carry a positive cost.
void	test_theme_lookup(void)
{
	t_catalogue		*c;
	const t_theme	*def;
	const t_theme	*cena;

	c = catalogue_load(CFG_DIR);
	assert(c != NULL);
	def = catalogue_theme(c, 1);
	assert(def != NULL);
	assert(strcmp(def->name, "Default") == 0);
	assert(def->cost_points == 0);
	assert(strlen(def->description) > 0);
	cena = catalogue_theme(c, 5);
	assert(cena != NULL && strcmp(cena->name, "John Cena") == 0);
	assert(cena->cost_points == 30);
	assert(catalogue_theme(c, 999) == NULL);
	catalogue_free(c);
	printf("PASS test_theme_lookup\n");
}

// A missing config directory fails the load cleanly (no crash, NULL result),
// and NULL inputs are handled defensively.
void	test_missing_dir_and_null(void)
{
	assert(catalogue_load("no/such/dir") == NULL);
	assert(catalogue_load(NULL) == NULL);
	assert(catalogue_character(NULL, 1) == NULL);
	assert(catalogue_theme(NULL, 1) == NULL);
	catalogue_free(NULL);
	printf("PASS test_missing_dir_and_null\n");
}

int	main(void)
{
	test_loads_full_roster();
	test_character_lookup();
	test_theme_lookup();
	test_missing_dir_and_null();
	return (0);
}
