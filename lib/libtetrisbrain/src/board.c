#include "tetrisbrain.h"
#include <string.h>

void board_init(t_board *b) { memset(b, 0, sizeof(*b)); }

bool board_in_bounds(int col, int row) {
  return col >= 0 && col < BOARD_WIDTH && row >= 0 && row < BOARD_HEIGHT;
}

t_cell board_get(const t_board *b, int col, int row) {
  if (!board_in_bounds(col, row)) {
    return (t_cell){CELL_FILLED, 0}; // out of bounds = solid wall
  }
  return b->cells[row][col];
}

void board_inject_garbage(t_board *b, int lines, int hole_col) {
  if (lines <= 0) return;
  if (lines > BOARD_HEIGHT) lines = BOARD_HEIGHT;

  // shift everything up by `lines` rows (rows 0..lines-1 fall off the top)
  for (int row = 0; row < BOARD_HEIGHT - lines; row++)
    memcpy(&b->cells[row][0], &b->cells[row + lines][0],
           sizeof(t_cell) * BOARD_WIDTH);

  // fill the bottom `lines` rows with garbage, leaving hole_col empty
  for (int row = BOARD_HEIGHT - lines; row < BOARD_HEIGHT; row++) {
    for (int col = 0; col < BOARD_WIDTH; col++) {
      if (col == hole_col)
        b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
      else
        b->cells[row][col] = (t_cell){CELL_GARBAGE, 0};
    }
  }
}

void board_set(t_board *b, int col, int row, t_cell cell) {
  if (board_in_bounds(col, row))
    b->cells[row][col] = cell;
}

void board_copy(t_board *dst, const t_board *src) {
  memcpy(dst, src, sizeof(t_board));
}
