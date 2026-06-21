// tests/test_lineclear.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

static void fill_row(board_t *b, int row, cell_type_t type, uint8_t color) {
  for (int c = 0; c < BOARD_WIDTH; c++)
    board_set(b, c, row, (cell_t){type, color});
}

void test_no_full_lines_returns_zero(void) {
  board_t b;
  board_init(&b);
  board_set(&b, 0, 19, (cell_t){CELL_FILLED, 1});

  assert(board_clear_lines(&b) == 0);
  assert(board_get(&b, 0, 19).type == CELL_FILLED);
  assert(board_get(&b, 1, 19).type == CELL_EMPTY);

  printf("PASS test_no_full_lines_returns_zero\n");
}

void test_single_full_line_cleared(void) {
  board_t b;
  board_init(&b);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 3, 18, (cell_t){CELL_FILLED, 7}); // marker, row above

  assert(board_clear_lines(&b) == 1);

  // marker shifts down into row 19
  assert(board_get(&b, 3, 19).type == CELL_FILLED);
  assert(board_get(&b, 3, 19).color == 7);
  assert(board_get(&b, 0, 19).type == CELL_EMPTY);

  // top row is now empty
  for (int c = 0; c < BOARD_WIDTH; c++)
    assert(board_get(&b, c, 0).type == CELL_EMPTY);

  printf("PASS test_single_full_line_cleared\n");
}

void test_partial_line_not_cleared(void) {
  board_t b;
  board_init(&b);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 5, 19, (cell_t){CELL_EMPTY, 0}); // leave a gap

  assert(board_clear_lines(&b) == 0);
  assert(board_get(&b, 5, 19).type == CELL_EMPTY);
  assert(board_get(&b, 0, 19).type == CELL_FILLED);

  printf("PASS test_partial_line_not_cleared\n");
}

void test_two_lines_cleared(void) {
  board_t b;
  board_init(&b);
  fill_row(&b, 18, CELL_GARBAGE, 0);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 2, 17, (cell_t){CELL_FILLED, 9}); // marker, two rows above

  assert(board_clear_lines(&b) == 2);

  // marker shifts down two rows into row 19
  assert(board_get(&b, 2, 19).type == CELL_FILLED);
  assert(board_get(&b, 2, 19).color == 9);
  assert(board_get(&b, 0, 19).type == CELL_EMPTY);

  // top two rows are now empty
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      assert(board_get(&b, c, r).type == CELL_EMPTY);

  printf("PASS test_two_lines_cleared\n");
}

int main(void) {
  test_no_full_lines_returns_zero();
  test_single_full_line_cleared();
  test_partial_line_not_cleared();
  test_two_lines_cleared();
  return 0;
}
