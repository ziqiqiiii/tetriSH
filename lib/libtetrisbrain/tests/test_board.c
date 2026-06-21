// tests/test_board.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

void test_init_empty(void) {
  board_t b;
  board_init(&b);
  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      assert(board_get(&b, c, r).type == CELL_EMPTY);
  printf("PASS test_init_empty\n");
}

void test_out_of_bounds_is_solid(void) {
  board_t b;
  board_init(&b);
  assert(board_get(&b, -1, 0).type == CELL_FILLED);           // left wall
  assert(board_get(&b, BOARD_WIDTH, 0).type == CELL_FILLED);  // right wall
  assert(board_get(&b, 0, -1).type == CELL_FILLED);           // ceiling
  assert(board_get(&b, 0, BOARD_HEIGHT).type == CELL_FILLED); // floor
  printf("PASS test_out_of_bounds_is_solid\n");
}

void test_inject_garbage_bottom_rows(void) {
  board_t b;
  board_init(&b);

  board_inject_garbage(&b, 2, 3); // 2 garbage lines, hole at col 3

  // bottom 2 rows should be garbage except at the hole column
  for (int col = 0; col < BOARD_WIDTH; col++) {
    if (col == 3) {
      assert(board_get(&b, col, BOARD_HEIGHT - 1).type == CELL_EMPTY);
      assert(board_get(&b, col, BOARD_HEIGHT - 2).type == CELL_EMPTY);
    } else {
      assert(board_get(&b, col, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
      assert(board_get(&b, col, BOARD_HEIGHT - 2).type == CELL_GARBAGE);
    }
  }
  printf("PASS test_inject_garbage_bottom_rows\n");
}

void test_inject_garbage_shifts_existing_blocks(void) {
  board_t b;
  board_init(&b);

  // place a block at the very bottom row, col 0
  board_set(&b, 0, BOARD_HEIGHT - 1, (cell_t){CELL_FILLED, 1});

  // inject 1 garbage line
  board_inject_garbage(&b, 1, 5);

  // the block should have shifted up by 1
  assert(board_get(&b, 0, BOARD_HEIGHT - 2).type == CELL_FILLED);
  // its old position should now be garbage
  assert(board_get(&b, 0, BOARD_HEIGHT - 1).type == CELL_GARBAGE);
  printf("PASS test_inject_garbage_shifts_existing_blocks\n");
}

void test_inject_garbage_hole_is_empty(void) {
  board_t b;
  board_init(&b);

  // inject 3 lines with hole at col 7
  board_inject_garbage(&b, 3, 7);

  // all 3 garbage rows should have hole at col 7
  for (int i = 1; i <= 3; i++)
    assert(board_get(&b, 7, BOARD_HEIGHT - i).type == CELL_EMPTY);

  printf("PASS test_inject_garbage_hole_is_empty\n");
}

void test_inject_garbage_zero_lines_is_noop(void) {
  board_t b, before;
  board_init(&b);
  board_set(&b, 4, 5, (cell_t){CELL_FILLED, 2});
  board_copy(&before, &b);

  board_inject_garbage(&b, 0, 3);

  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++) {
      assert(board_get(&b, c, r).type == board_get(&before, c, r).type);
      assert(board_get(&b, c, r).color == board_get(&before, c, r).color);
    }
  printf("PASS test_inject_garbage_zero_lines_is_noop\n");
}

void test_inject_garbage_negative_lines_is_noop(void) {
  board_t b, before;
  board_init(&b);
  board_set(&b, 4, 5, (cell_t){CELL_FILLED, 2});
  board_copy(&before, &b);

  board_inject_garbage(&b, -3, 3);

  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++) {
      assert(board_get(&b, c, r).type == board_get(&before, c, r).type);
      assert(board_get(&b, c, r).color == board_get(&before, c, r).color);
    }
  printf("PASS test_inject_garbage_negative_lines_is_noop\n");
}

void test_inject_garbage_more_than_height_caps(void) {
  board_t b;
  board_init(&b);
  board_set(&b, 4, 5, (cell_t){CELL_FILLED, 2}); // pre-existing block

  board_inject_garbage(&b, BOARD_HEIGHT + 5, 3); // request more lines than exist

  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++) {
      if (c == 3)
        assert(board_get(&b, c, r).type == CELL_EMPTY);
      else
        assert(board_get(&b, c, r).type == CELL_GARBAGE);
    }
  printf("PASS test_inject_garbage_more_than_height_caps\n");
}

int main(void) {
  test_init_empty();
  test_out_of_bounds_is_solid();
  test_inject_garbage_bottom_rows();
  test_inject_garbage_shifts_existing_blocks();
  test_inject_garbage_hole_is_empty();
  test_inject_garbage_zero_lines_is_noop();
  test_inject_garbage_negative_lines_is_noop();
  test_inject_garbage_more_than_height_caps();
  return 0;
}
