#include "tetrisbrain.h"

// Static Variables
// NES-style base points per line clear, indexed by lines_cleared (0-4).
static const int	LINE_CLEAR_BASE[] = {0, 40, 100, 300, 1200};

/**
 * @brief Computes the score awarded for clearing a number of lines.
 *
 * Applies NES-style base points scaled by the current level. Inputs are
 * clamped so out-of-range line counts or negative levels are handled safely.
 *
 * @param lines_cleared Number of lines cleared this tick (0-4).
 * @param level Current level, used as a scoring multiplier.
 * @return The points awarded, or 0 when no lines were cleared.
 */
int	score_on_clear(int lines_cleared, int level)
{
	if (lines_cleared <= 0)
		return (0);
	if (lines_cleared > 4)
		lines_cleared = 4;
	if (level < 0)
		level = 0;
	return (LINE_CLEAR_BASE[lines_cleared] * (level + 1));
}

/**
 * @brief Derives the current level from the total lines cleared.
 *
 * @param total_lines Total lines cleared so far; negative values clamp to 0.
 * @return The level, one per ten lines cleared.
 */
int	level_from_lines(int total_lines)
{
	if (total_lines < 0)
		total_lines = 0;
	return (total_lines / 10);
}

/**
 * @brief Computes the gravity tick interval for a level.
 *
 * Linear ramp from GRAVITY_BASE_MS down to GRAVITY_MIN_MS, GRAVITY_STEP_MS
 * per level, clamped at the floor so the interval never reaches zero.
 *
 * @param level Current level; negative values clamp to 0.
 * @return The milliseconds between gravity ticks at this level.
 */
int	gravity_interval_ms(int level)
{
	int	ms;

	if (level < 0)
		level = 0;
	ms = GRAVITY_BASE_MS - level * GRAVITY_STEP_MS;
	if (ms < GRAVITY_MIN_MS)
		ms = GRAVITY_MIN_MS;
	return (ms);
}

