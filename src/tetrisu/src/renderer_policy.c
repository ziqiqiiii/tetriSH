#include "tetrisu.h"

/**
 * @brief Parses the public renderer-selection environment value.
 *
 * Unknown and absent values deliberately preserve automatic selection so a
 * typo cannot disable graphics support or prevent the client from starting.
 * The three explicit values map onto the three capability tiers, which lets a
 * tier be exercised on a terminal the probe would have classified differently.
 *
 * @param value Value of TETRISU_RENDERER, or NULL when unset.
 * @return The matching forced tier, or automatic selection.
 */
t_tetrisu_renderer_mode	tetrisu_renderer_mode_from_value(const char *value)
{
	if (value == NULL)
		return (TETRISU_RENDERER_AUTO);
	if (strcmp(value, "cell") == 0)
		return (TETRISU_RENDERER_CELL);
	if (strcmp(value, "stationary") == 0)
		return (TETRISU_RENDERER_STATIONARY);
	if (strcmp(value, "pixel") == 0)
		return (TETRISU_RENDERER_PIXEL);
	return (TETRISU_RENDERER_AUTO);
}

/**
 * @brief Reads the renderer tier the user asked for on this run.
 *
 * @return Forced tier from TETRISU_RENDERER, or automatic selection.
 */
t_tetrisu_renderer_mode	tetrisu_renderer_mode_requested(void)
{
	return (tetrisu_renderer_mode_from_value(getenv("TETRISU_RENDERER")));
}
