#include "tetrisbrain.h"

t_brain_result gravity_tick(const t_board *b, t_piece *p) {
  if (piece_move(b, p, 0, 1) == BRAIN_OK) return BRAIN_OK;
  return BRAIN_LOCKED;
}

// Soft drop is gravity_tick triggered by the player instead of the ticker:
// same fall-by-one / lock-on-landing rule. Caller awards the soft-drop
// score bonus on BRAIN_OK (scoring.c), separately from this result.
t_brain_result piece_soft_drop(const t_board *b, t_piece *p) {
  return gravity_tick(b, p);
}

void piece_hard_drop(const t_board *b, t_piece *p) {
  while (piece_move(b, p, 0, 1) == BRAIN_OK) {}
}

int piece_drop_distance(const t_board *b, const t_piece *p) {
  t_piece projected = *p;
  int distance = 0;

  while (piece_move(b, &projected, 0, 1) == BRAIN_OK) distance++;
  return distance;
}
