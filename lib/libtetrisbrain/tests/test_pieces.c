// tests/test_pieces.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

void test_spawn_all_types_valid(void) {
  t_piece_type types[] = {PIECE_I, PIECE_O, PIECE_T, PIECE_S,
                           PIECE_Z, PIECE_J, PIECE_L};
  for (int i = 0; i < 7; i++) {
    t_board b;
    board_init(&b);
    t_piece p = piece_spawn(types[i]);
    assert(p.type == types[i]);
    assert(p.rotation == 0);
    assert(piece_is_valid(&b, &p));

    piece_stamp(&b, &p);
    int filled = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++)
      for (int c = 0; c < BOARD_WIDTH; c++)
        if (board_get(&b, c, r).type == CELL_FILLED) filled++;
    assert(filled == 4);
  }
  printf("PASS test_spawn_all_types_valid\n");
}

void test_stamp_writes_cells(void) {
  t_board b;
  board_init(&b);
  // T spawn: col=3,row=0, shape0 offsets (1,0)(0,1)(1,1)(2,1)
  // -> abs cells (4,0) (3,1) (4,1) (5,1)
  t_piece p = piece_spawn(PIECE_T);
  piece_stamp(&b, &p);

  assert(board_get(&b, 4, 0).type == CELL_FILLED);
  assert(board_get(&b, 4, 0).color == (uint8_t)PIECE_T);
  assert(board_get(&b, 3, 1).type == CELL_FILLED);
  assert(board_get(&b, 4, 1).type == CELL_FILLED);
  assert(board_get(&b, 5, 1).type == CELL_FILLED);

  assert(board_get(&b, 0, 0).type == CELL_EMPTY);
  assert(board_get(&b, 3, 0).type == CELL_EMPTY); // not part of T spawn shape

  printf("PASS test_stamp_writes_cells\n");
}

void test_move_blocked_by_walls_and_floor(void) {
  t_board b;
  board_init(&b);
  t_piece p = piece_spawn(PIECE_I); // col=3,row=-1, occupies cols 3-6, row 0

  for (int i = 0; i < 3; i++)
    assert(piece_move(&b, &p, -1, 0) == BRAIN_OK);
  assert(p.col == 0);
  assert(piece_move(&b, &p, -1, 0) == BRAIN_BLOCKED);
  assert(p.col == 0); // unchanged on block

  for (int i = 0; i < 6; i++)
    assert(piece_move(&b, &p, 1, 0) == BRAIN_OK);
  assert(p.col == 6);
  assert(piece_move(&b, &p, 1, 0) == BRAIN_BLOCKED);
  assert(p.col == 6);

  for (int i = 0; i < 19; i++)
    assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
  assert(p.row == 18);
  assert(piece_move(&b, &p, 0, 1) == BRAIN_BLOCKED);
  assert(p.row == 18);

  printf("PASS test_move_blocked_by_walls_and_floor\n");
}

void test_rotate_basic_updates_rotation(void) {
  t_board b;
  board_init(&b);
  t_piece p = piece_spawn(PIECE_T); // col=3,row=0,rotation=0

  assert(piece_rotate(&b, &p, 1) == BRAIN_OK); // 0->R, no kick needed
  assert(p.rotation == 1);
  assert(p.col == 3 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_rotate_basic_updates_rotation\n");
}

void test_jlstz_wall_kick(void) {
  t_board b;
  board_init(&b);
  // T piece, R orientation, flush against the left wall.
  t_piece p = {PIECE_T, -1, 0, 1};
  assert(piece_is_valid(&b, &p));

  // R->2: naive (0,0) kick puts a cell at col -1 (invalid).
  // JLSTZ_KICKS[R->2] test2 = (+1,0) shifts the piece right by 1.
  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 2);
  assert(p.col == 0 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_jlstz_wall_kick\n");
}

void test_i_piece_wall_kick(void) {
  t_board b;
  board_init(&b);
  // I piece, L orientation (single column at col+1), flush against left wall.
  t_piece p = {PIECE_I, -1, 0, 3};
  assert(piece_is_valid(&b, &p));

  // L->0: naive (0,0) kick puts a cell at col -1 (invalid).
  // I_KICKS[L->0] test2 = (+1,0) shifts the piece right by 1.
  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 0);
  assert(p.col == 0 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_i_piece_wall_kick\n");
}

void test_rotation_reports_srs_kick_index(void) {
  t_board b;
  t_piece p;
  int kick_index = -1;

  board_init(&b);
  p = (t_piece){PIECE_T, -1, 0, 1};
  assert(piece_rotate_with_kick(&b, &p, 1, &kick_index) == BRAIN_OK);
  assert(kick_index == 1);
  printf("PASS test_rotation_reports_srs_kick_index\n");
}

void test_t_spin_three_corner_classification(void) {
  t_board b;
  t_piece p = {PIECE_T, 3, 5, 0};

  board_init(&b);
  board_set(&b, 3, 5, (t_cell){CELL_FILLED, 0});
  board_set(&b, 5, 5, (t_cell){CELL_FILLED, 0});
  board_set(&b, 3, 7, (t_cell){CELL_FILLED, 0});
  assert(piece_t_spin_type(&b, &p, 0) == T_SPIN_FULL);
  board_init(&b);
  board_set(&b, 3, 5, (t_cell){CELL_FILLED, 0});
  board_set(&b, 3, 7, (t_cell){CELL_FILLED, 0});
  board_set(&b, 5, 7, (t_cell){CELL_FILLED, 0});
  assert(piece_t_spin_type(&b, &p, 0) == T_SPIN_MINI);
  assert(piece_t_spin_type(&b, &p, 4) == T_SPIN_FULL);
  printf("PASS test_t_spin_three_corner_classification\n");
}

void test_rotate_blocked_returns_blocked(void) {
  t_board b;
  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      board_set(&b, c, r, (t_cell){CELL_FILLED, 0});

  // Carve out exactly the T spawn shape's 4 cells: (4,0)(3,1)(4,1)(5,1).
  board_set(&b, 4, 0, (t_cell){CELL_EMPTY, 0});
  board_set(&b, 3, 1, (t_cell){CELL_EMPTY, 0});
  board_set(&b, 4, 1, (t_cell){CELL_EMPTY, 0});
  board_set(&b, 5, 1, (t_cell){CELL_EMPTY, 0});

  t_piece p = piece_spawn(PIECE_T); // col=3,row=0,rotation=0
  assert(piece_is_valid(&b, &p));

  t_piece before = p;
  assert(piece_rotate(&b, &p, 1) == BRAIN_BLOCKED);
  assert(p.type == before.type && p.col == before.col &&
         p.row == before.row && p.rotation == before.rotation);

  printf("PASS test_rotate_blocked_returns_blocked\n");
}

void test_o_piece_rotate_is_noop(void) {
  t_board b;
  board_init(&b);
  t_piece p = piece_spawn(PIECE_O); // col=4,row=0,rotation=0

  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 1);
  assert(p.col == 4 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  assert(piece_rotate(&b, &p, -1) == BRAIN_OK);
  assert(p.rotation == 0);
  assert(p.col == 4 && p.row == 0);

  printf("PASS test_o_piece_rotate_is_noop\n");
}

void test_invalid_type_rejected_safely(void) {
  t_board b;
  board_init(&b);
  t_piece p = {(t_piece_type)99, 3, 0, 0}; // corrupted type, out of SHAPES range

  assert(!piece_is_valid(&b, &p));

  piece_stamp(&b, &p); // must not read OOB / must not touch the board
  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      assert(board_get(&b, c, r).type == CELL_EMPTY);

  t_piece before = p;
  assert(piece_rotate(&b, &p, 1) == BRAIN_BLOCKED);
  assert(p.type == before.type && p.col == before.col &&
         p.row == before.row && p.rotation == before.rotation);

  printf("PASS test_invalid_type_rejected_safely\n");
}

void test_invalid_rotation_rejected_safely(void) {
  t_board b;
  board_init(&b);
  t_piece p = {PIECE_T, 3, 0, 99}; // corrupted rotation, out of SHAPES range

  assert(!piece_is_valid(&b, &p));

  piece_stamp(&b, &p); // must not read OOB / must not touch the board
  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      assert(board_get(&b, c, r).type == CELL_EMPTY);

  t_piece before = p;
  assert(piece_rotate(&b, &p, 1) == BRAIN_BLOCKED);
  assert(p.type == before.type && p.col == before.col &&
         p.row == before.row && p.rotation == before.rotation);

  printf("PASS test_invalid_rotation_rejected_safely\n");
}

int main(void) {
  test_spawn_all_types_valid();
  test_stamp_writes_cells();
  test_move_blocked_by_walls_and_floor();
  test_rotate_basic_updates_rotation();
  test_jlstz_wall_kick();
  test_i_piece_wall_kick();
  test_rotation_reports_srs_kick_index();
  test_t_spin_three_corner_classification();
  test_rotate_blocked_returns_blocked();
  test_o_piece_rotate_is_noop();
  test_invalid_type_rejected_safely();
  test_invalid_rotation_rejected_safely();
  return 0;
}
