// tests/test_lineclear.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

static void fill_row(t_board *b, int row, t_cell_type type, uint8_t color) {
  for (int c = 0; c < BOARD_WIDTH; c++)
    board_set(b, c, row, (t_cell){type, color});
}

void test_no_full_lines_returns_zero(void) {
  t_board b;
  board_init(&b);
  board_set(&b, 0, 19, (t_cell){CELL_FILLED, 1});

  assert(board_clear_lines(&b) == 0);
  assert(board_get(&b, 0, 19).type == CELL_FILLED);
  assert(board_get(&b, 1, 19).type == CELL_EMPTY);

  printf("PASS test_no_full_lines_returns_zero\n");
}

void test_single_full_line_cleared(void) {
  t_board b;
  board_init(&b);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 3, 18, (t_cell){CELL_FILLED, 7}); // marker, row above

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
  t_board b;
  board_init(&b);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 5, 19, (t_cell){CELL_EMPTY, 0}); // leave a gap

  assert(board_clear_lines(&b) == 0);
  assert(board_get(&b, 5, 19).type == CELL_EMPTY);
  assert(board_get(&b, 0, 19).type == CELL_FILLED);

  printf("PASS test_partial_line_not_cleared\n");
}

void test_two_lines_cleared(void) {
  t_board b;
  board_init(&b);
  fill_row(&b, 18, CELL_GARBAGE, 0);
  fill_row(&b, 19, CELL_FILLED, 1);
  board_set(&b, 2, 17, (t_cell){CELL_FILLED, 9}); // marker, two rows above

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

void test_find_full_lines_and_empty_board(void) {
  t_board b;
  int rows[BRAIN_MAX_CLEAR_LINES] = {-1, -1, -1, -1};

  board_init(&b);
  assert(board_is_empty(&b));
  fill_row(&b, 17, CELL_FILLED, 1);
  fill_row(&b, 19, CELL_FILLED, 1);
  assert(!board_is_empty(&b));
  assert(board_find_full_lines(&b, rows) == 2);
  assert(rows[0] == 19 && rows[1] == 17);
  assert(board_clear_lines(&b) == 2);
  assert(board_is_empty(&b));
  printf("PASS test_find_full_lines_and_empty_board\n");
}

int main(void) {
  test_no_full_lines_returns_zero();
  test_single_full_line_cleared();
  test_partial_line_not_cleared();
  test_two_lines_cleared();
  test_find_full_lines_and_empty_board();
  return 0;
}
