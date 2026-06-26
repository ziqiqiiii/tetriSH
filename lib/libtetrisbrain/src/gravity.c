#include "tetrisbrain.h"

brain_result_t gravity_tick(const board_t *b, piece_t *p) {
  if (piece_move(b, p, 0, 1) == BRAIN_OK) return BRAIN_OK;
  return BRAIN_LOCKED;
}

// Soft drop is gravity_tick triggered by the player instead of the ticker:
// same fall-by-one / lock-on-landing rule. Caller awards the soft-drop
// score bonus on BRAIN_OK (scoring.c), separately from this result.
brain_result_t piece_soft_drop(const board_t *b, piece_t *p) {
  return gravity_tick(b, p);
}

void piece_hard_drop(const board_t *b, piece_t *p) {
  while (piece_move(b, p, 0, 1) == BRAIN_OK) {}
}
