#include "tetrisbrain.h"

// Modern Guideline base points, multiplied by the displayed level (1+).
static const int NORMAL_CLEAR_BASE[] = {0, 100, 300, 500, 800};
static const int MINI_SPIN_BASE[] = {100, 200, 400};
static const int FULL_SPIN_BASE[] = {400, 800, 1200, 1600};
static const int PERFECT_CLEAR_BASE[] = {0, 800, 1200, 1800, 2000};

// Tetris Worlds curve rounded to milliseconds. Level 19+ is 20G and is
// represented by 0 so the caller grounds the piece in a single tick.
static const int GRAVITY_MS[] = {
  1000, 793, 618, 473, 355, 262, 190, 135, 94,
  64, 43, 28, 18, 11, 7, 4, 2, 1
};

static int valid_level(int level) {
  return level < 1 ? 1 : level;
}

static uint64_t action_base(int lines_cleared, int level,
                            t_spin_type spin) {
  int base = 0;

  if (spin == T_SPIN_MINI && lines_cleared <= 2)
    base = MINI_SPIN_BASE[lines_cleared];
  else if (spin == T_SPIN_FULL && lines_cleared <= 3)
    base = FULL_SPIN_BASE[lines_cleared];
  else if (spin == T_SPIN_NONE)
    base = NORMAL_CLEAR_BASE[lines_cleared];
  return (uint64_t)base * (uint64_t)valid_level(level);
}

int score_on_clear(int lines_cleared, int level) {
  if (lines_cleared <= 0) return 0;
  if (lines_cleared > 4) lines_cleared = 4;

  return NORMAL_CLEAR_BASE[lines_cleared] * valid_level(level);
}

int level_from_lines(int total_lines) {
  if (total_lines < 0) total_lines = 0;
  return total_lines / 10 + 1;
}

int gravity_interval_ms(int level) {
  level = valid_level(level);
  if (level >= 19) return 0;
  return GRAVITY_MS[level - 1];
}

void score_state_init(t_score_state *state) {
  state->total = 0;
  state->combo = -1;
  state->back_to_back = false;
}

/* AI-assisted: applies one lock event atomically to caller-owned scoring
 * state, including combo, back-to-back, and additive perfect-clear bonuses. */
t_score_result score_apply_clear(t_score_state *state, int lines_cleared,
                                 int level, t_spin_type spin,
                                 bool perfect_clear) {
  t_score_result result = {0};
  bool was_back_to_back = state->back_to_back;

  if (lines_cleared < 0) lines_cleared = 0;
  if (lines_cleared > 4) lines_cleared = 4;
  level = valid_level(level);
  result.difficult = lines_cleared > 0 &&
                     (lines_cleared == 4 || spin != T_SPIN_NONE);
  result.action_points = action_base(lines_cleared, level, spin);
  if (result.difficult && was_back_to_back)
    result.action_points += result.action_points / 2;
  if (lines_cleared > 0) {
    state->combo++;
    result.combo_points = 50u * (uint64_t)state->combo * (uint64_t)level;
    state->back_to_back = result.difficult;
  } else {
    state->combo = -1;
  }
  if (perfect_clear && lines_cleared > 0) {
    int perfect_base = PERFECT_CLEAR_BASE[lines_cleared];
    if (lines_cleared == 4 && was_back_to_back) perfect_base = 3200;
    result.perfect_clear_points =
        (uint64_t)perfect_base * (uint64_t)level;
  }
  result.total_awarded = result.action_points + result.combo_points +
                         result.perfect_clear_points;
  state->total += result.total_awarded;
  return result;
}

uint64_t score_add_drop(t_score_state *state, int cells, bool hard_drop) {
  uint64_t points;

  if (cells <= 0) return 0;
  points = (uint64_t)cells * (hard_drop ? 2u : 1u);
  state->total += points;
  return points;
}
