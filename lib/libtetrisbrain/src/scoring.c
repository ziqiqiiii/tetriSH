#include "tetrisbrain.h"

// NES-style base points per line clear, indexed by lines_cleared (0-4).
static const int LINE_CLEAR_BASE[] = {0, 40, 100, 300, 1200};

#define GRAVITY_BASE_MS 1000
#define GRAVITY_STEP_MS 50
#define GRAVITY_MIN_MS 100

int score_on_clear(int lines_cleared, int level) {
  if (lines_cleared <= 0) return 0;
  if (lines_cleared > 4) lines_cleared = 4; // board_clear_lines caps at 4
  if (level < 0) level = 0;

  return LINE_CLEAR_BASE[lines_cleared] * (level + 1);
}

int level_from_lines(int total_lines) {
  if (total_lines < 0) total_lines = 0;
  return total_lines / 10;
}

// Linear ramp from GRAVITY_BASE_MS down to GRAVITY_MIN_MS, GRAVITY_STEP_MS
// per level; clamped at the floor so the ticker interval never hits zero.
int gravity_interval_ms(int level) {
  if (level < 0) level = 0;

  int ms = GRAVITY_BASE_MS - level * GRAVITY_STEP_MS;
  if (ms < GRAVITY_MIN_MS) ms = GRAVITY_MIN_MS;
  return ms;
}