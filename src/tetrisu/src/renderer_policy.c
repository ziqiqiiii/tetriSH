#include "tetrisu.h"

/**
 * @brief Parses the public renderer-selection environment value.
 *
 * Unknown and absent values deliberately preserve automatic selection so a
 * typo cannot disable graphics support or prevent the client from starting.
 *
 * @param value Value of TETRISU_RENDERER, or NULL when unset.
 * @return Forced cell mode only for the exact public "cell" value.
 */
tetrisu_renderer_mode_t	tetrisu_renderer_mode_from_value(const char *value)
{
	if (value != NULL && strcmp(value, "cell") == 0)
		return (TETRISU_RENDERER_CELL);
	return (TETRISU_RENDERER_AUTO);
}

/**
 * @brief Reports whether the user explicitly requested terminal-cell output.
 *
 * @return true for TETRISU_RENDERER=cell, otherwise false.
 */
bool	tetrisu_renderer_forced_cell(void)
{
	return (tetrisu_renderer_mode_from_value(getenv("TETRISU_RENDERER"))
		== TETRISU_RENDERER_CELL);
}
