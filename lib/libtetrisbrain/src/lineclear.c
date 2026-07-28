#include "tetrisbrain.h"
#include <string.h>

int board_find_full_lines(const t_board *b,
                          int rows[BRAIN_MAX_CLEAR_LINES]) {
  int count = 0;

  for (int row = BOARD_HEIGHT - 1; row >= 0; row--) {
    bool full = true;
    for (int col = 0; col < BOARD_WIDTH; col++) {
      if (b->cells[row][col].type == CELL_EMPTY) {
        full = false;
        break;
      }
    }
    if (full && count < BRAIN_MAX_CLEAR_LINES) rows[count++] = row;
  }
  return count;
}

bool board_is_empty(const t_board *b) {
  for (int row = 0; row < BOARD_HEIGHT; row++)
    for (int col = 0; col < BOARD_WIDTH; col++)
      if (b->cells[row][col].type != CELL_EMPTY) return false;
  return true;
}

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
