#include "tetrisbrain.h"

/* Static Functions */
static int	valid_level(int level);
static uint64_t	action_base(int lines_cleared, int level, t_spin_type spin);

/* Modern Guideline base points, multiplied by the displayed level (1+). */
static const int	NORMAL_CLEAR_BASE[] = {0, 100, 300, 500, 800};
static const int	MINI_SPIN_BASE[] = {100, 200, 400};
static const int	FULL_SPIN_BASE[] = {400, 800, 1200, 1600};
static const int	PERFECT_CLEAR_BASE[] = {0, 800, 1200, 1800, 2000};

/* Tetris Worlds curve rounded to milliseconds. Level 19+ is 20G and is
 * represented by 0 so the caller grounds the piece in a single tick. */
static const int	GRAVITY_MS[] = {
	1000, 793, 618, 473, 355, 262, 190, 135, 94,
	64, 43, 28, 18, 11, 7, 4, 2, 1
};

/**
 * @brief Scores a line clear with no T-spin using the Modern Guideline table.
 *
 * @param lines_cleared Number of lines cleared by the placement.
 * @param level Current level used to scale the base score.
 * @return Points awarded for the clear, or 0 if lines_cleared is <= 0.
 */
int	score_on_clear(int lines_cleared, int level)
{
	if (lines_cleared <= 0)
		return (0);
	if (lines_cleared > 4)
		lines_cleared = 4;
	return (NORMAL_CLEAR_BASE[lines_cleared] * valid_level(level));
}

/**
 * @brief Derives the current level from total lines cleared (1 + one per
 * 10 lines).
 *
 * @param total_lines Total lines cleared so far this game.
 * @return The player's current level (1 and up).
 */
int	level_from_lines(int total_lines)
{
	if (total_lines < 0)
		total_lines = 0;
	return (total_lines / 10 + 1);
}

/**
 * @brief Looks up the automatic gravity interval, in milliseconds, for a
 * level (the Tetris Worlds curve).
 *
 * @param level Current level.
 * @return Milliseconds between automatic gravity ticks, or 0 at 20G
 * (level 19+, grounding the piece in a single tick).
 */
int	gravity_interval_ms(int level)
{
	level = valid_level(level);
	if (level >= 19)
		return (0);
	return (GRAVITY_MS[level - 1]);
}

/**
 * @brief Initialises caller-owned scoring state to its starting values.
 *
 * @param state Scoring state to initialise.
 */
void	score_state_init(t_score_state *state)
{
	state->total = 0;
	state->combo = -1;
	state->back_to_back = false;
}

/**
 * @brief Applies one piece-lock event to caller-owned scoring state: action
 * points, combo, back-to-back, and perfect-clear bonuses.
 *
 * A back-to-back Tetris perfect clear scores a fixed 3200 instead of the
 * PERFECT_CLEAR_BASE table value.
 *
 * @param state Scoring state updated in place (total, combo, back_to_back).
 * @param lines_cleared Number of lines cleared by this lock.
 * @param level Current level used to scale every point component.
 * @param spin T-spin classification of the piece that locked.
 * @param perfect_clear True if the clear emptied the entire board.
 * @return The point breakdown and total awarded by this event.
 */
t_score_result	score_apply_clear(t_score_state *state, int lines_cleared,
	int level, t_spin_type spin, bool perfect_clear)
{
	t_score_result	result;
	bool			was_back_to_back;

	result = (t_score_result){0};
	was_back_to_back = state->back_to_back;
	if (lines_cleared < 0)
		lines_cleared = 0;
	if (lines_cleared > 4)
		lines_cleared = 4;
	level = valid_level(level);
	result.difficult = lines_cleared > 0
		&& (lines_cleared == 4 || spin != T_SPIN_NONE);
	result.action_points = action_base(lines_cleared, level, spin);
	if (result.difficult && was_back_to_back)
		result.action_points += result.action_points / 2;
	if (lines_cleared > 0)
	{
		state->combo++;
		result.combo_points = 50u * (uint64_t)state->combo * (uint64_t)level;
		state->back_to_back = result.difficult;
	}
	else
		state->combo = -1;
	if (perfect_clear && lines_cleared > 0)
	{
		int	perfect_base;

		perfect_base = PERFECT_CLEAR_BASE[lines_cleared];
		if (lines_cleared == 4 && was_back_to_back)
			perfect_base = 3200;
		result.perfect_clear_points = (uint64_t)perfect_base * (uint64_t)level;
	}
	result.total_awarded = result.action_points + result.combo_points
		+ result.perfect_clear_points;
	state->total += result.total_awarded;
	return (result);
}

/**
 * @brief Awards points for a soft or hard drop and adds them to the total.
 *
 * @param state Scoring state whose total is incremented in place.
 * @param cells Number of cells the piece travelled this drop.
 * @param hard_drop True for a hard drop (2 points/cell), false for soft
 * (1 point/cell).
 * @return Points awarded for this drop (already added to state->total).
 */
uint64_t	score_add_drop(t_score_state *state, int cells, bool hard_drop)
{
	uint64_t	points;

	if (cells <= 0)
		return (0);
	points = (uint64_t)cells * (hard_drop ? 2u : 1u);
	state->total += points;
	return (points);
}

/* Clamps a level to the game's valid minimum of 1; every score/gravity
 * lookup that scales by level routes through this first. */
static int	valid_level(int level)
{
	return (level < 1 ? 1 : level);
}

/* Looks up the base points for one clear: the T-spin mini/full tables when
 * spin indicates a T-spin, the normal-clear table otherwise, scaled by the
 * (already-clamped) level. Falls back to 0 base points for a T-spin/line
 * count combination outside the tables (e.g. T_SPIN_MINI with 3 lines). */
static uint64_t	action_base(int lines_cleared, int level, t_spin_type spin)
{
	int	base;

	base = 0;
	if (spin == T_SPIN_MINI && lines_cleared <= 2)
		base = MINI_SPIN_BASE[lines_cleared];
	else if (spin == T_SPIN_FULL && lines_cleared <= 3)
		base = FULL_SPIN_BASE[lines_cleared];
	else if (spin == T_SPIN_NONE)
		base = NORMAL_CLEAR_BASE[lines_cleared];
	return ((uint64_t)base * (uint64_t)valid_level(level));
}
