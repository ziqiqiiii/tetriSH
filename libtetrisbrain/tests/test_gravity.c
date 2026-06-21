// tests/test_gravity.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

void test_gravity_tick_moves_down(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_T); // col=3,row=0

  assert(gravity_tick(&b, &p) == BRAIN_OK);
  assert(p.row == 1);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_gravity_tick_moves_down\n");
}

void test_gravity_tick_locks_at_floor(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_I); // col=3,row=-1, occupies row 0

  // drop to the floor (row=18, occupies row 19)
  for (int i = 0; i < 19; i++)
    assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
  assert(p.row == 18);

  piece_t before = p;
  assert(gravity_tick(&b, &p) == BRAIN_LOCKED);
  assert(p.row == before.row && p.col == before.col &&
         p.rotation == before.rotation);

  printf("PASS test_gravity_tick_locks_at_floor\n");
}

void test_soft_drop_moves_then_locks(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_O); // col=4,row=0, occupies rows 0-1

  assert(piece_soft_drop(&b, &p) == BRAIN_OK);
  assert(p.row == 1);

  // O piece rests at row=18 (occupies rows 18-19)
  for (int i = 0; i < 17; i++)
    assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
  assert(p.row == 18);

  piece_t before = p;
  assert(piece_soft_drop(&b, &p) == BRAIN_LOCKED);
  assert(p.row == before.row);

  printf("PASS test_soft_drop_moves_then_locks\n");
}

void test_hard_drop_moves_to_floor(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_I); // col=3,row=-1

  piece_hard_drop(&b, &p);
  assert(p.row == 18);
  assert(p.col == 3);
  assert(piece_move(&b, &p, 0, 1) == BRAIN_BLOCKED); // resting on floor

  printf("PASS test_hard_drop_moves_to_floor\n");
}

int main(void) {
  test_gravity_tick_moves_down();
  test_gravity_tick_locks_at_floor();
  test_soft_drop_moves_then_locks();
  test_hard_drop_moves_to_floor();
  return 0;
}
