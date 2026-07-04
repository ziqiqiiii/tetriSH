#include "tetrisbrain.h"
#include <string.h>

// Compact rows from the bottom up: full rows are skipped (not copied), every
// other row is copied down into the next free `write` slot. Whatever's left
// above `write` after the scan gets zeroed (CELL_EMPTY).
int board_clear_lines(t_board *b) {
  int cleared = 0;
  int write = BOARD_HEIGHT - 1;

  for (int read = BOARD_HEIGHT - 1; read >= 0; read--) {
    bool full = true;
    for (int col = 0; col < BOARD_WIDTH; col++) {
      if (b->cells[read][col].type == CELL_EMPTY) {
        full = false;
        break;
      }
    }
    if (full) {
      cleared++;
      continue;
    }
    if (write != read)
      memcpy(&b->cells[write][0], &b->cells[read][0],
             sizeof(t_cell) * BOARD_WIDTH);
    write--;
  }

  for (int row = write; row >= 0; row--)
    memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);

  return cleared;
}
