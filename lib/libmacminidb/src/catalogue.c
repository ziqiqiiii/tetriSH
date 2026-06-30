/* ************************************************************************** */
/*                                                                            */
/*   catalogue.c — read-only Character / Theme tables from config (§6)        */
/*                                                                            */
/*   Static data: loaded once at boot, never written to the log. Paths come   */
/*   from .tetrishrc via config_dir — no hard-coded paths (CLAUDE.md).        */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: define struct s_catalogue (character_t[] + theme_t[] + counts)
// TODO: static helpers — parse_characters / parse_themes, forward-declared up top
// TODO: catalogue_load / catalogue_free
// TODO: catalogue_character / catalogue_theme (linear lookup by id; ~tens of rows)
