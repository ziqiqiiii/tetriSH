#include "tetrisbrain.h"
#include <string.h>

// Tetris Battle Gaiden character specials. Cooldowns/charges/targeting live
// in tetrisd/ability.c (server-side, stateful) - these are just the pure
// t_board transforms each special applies.

// Wolfman: cut off the top N rows of his own stack. Rows n..H-1 shift up to
// 0..H-n-1; the vacated bottom n rows become empty.
void board_cut_top(t_board *b, int n) {
  if (n <= 0) return;
  if (n > BOARD_HEIGHT) n = BOARD_HEIGHT;

  for (int row = 0; row < BOARD_HEIGHT - n; row++)
    memcpy(&b->cells[row][0], &b->cells[row + n][0],
           sizeof(t_cell) * BOARD_WIDTH);

  for (int row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++)
    memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);
}

// Mirurun: smash down and remove the bottom N lines. Rows 0..H-n-1 shift
// down to n..H-1; the vacated top n rows become empty. Walk bottom-up so the
// copy never overwrites a row it still needs to read.
void board_cut_bottom(t_board *b, int n) {
  if (n <= 0) return;
  if (n > BOARD_HEIGHT) n = BOARD_HEIGHT;

  for (int row = BOARD_HEIGHT - 1; row >= n; row--)
    memcpy(&b->cells[row][0], &b->cells[row - n][0],
           sizeof(t_cell) * BOARD_WIDTH);

  for (int row = 0; row < n; row++)
    memset(&b->cells[row][0], 0, sizeof(t_cell) * BOARD_WIDTH);
}

/* AI-assisted: Wolfman lvl4 - drop every suspended block straight down,
 * column by column, until it lands on the floor or another block. Compacts
 * non-empty cells to the bottom of each column, preserving their relative
 * order, and zeroes whatever's left above. */
void board_apply_gravity(t_board *b) {
  for (int col = 0; col < BOARD_WIDTH; col++) {
    int write = BOARD_HEIGHT - 1;
    for (int row = BOARD_HEIGHT - 1; row >= 0; row--) {
      if (b->cells[row][col].type != CELL_EMPTY) {
        b->cells[write][col] = b->cells[row][col];
        write--;
      }
    }
    for (int row = write; row >= 0; row--)
      b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
  }
}

// Halloween "Dark": invert every cell - empty space becomes garbage, any
// occupied cell becomes empty.
void board_invert(t_board *b) {
  for (int row = 0; row < BOARD_HEIGHT; row++) {
    for (int col = 0; col < BOARD_WIDTH; col++) {
      t_cell *c = &b->cells[row][col];
      *c = (c->type == CELL_EMPTY) ? (t_cell){CELL_GARBAGE, 0}
                                    : (t_cell){CELL_EMPTY, 0};
    }
  }
}

/* AI-assisted: Halloween "Burn" - overwrite the bottom N rows in place
 * (no shift) with garbage, leaving hole_col empty in each row, same pattern
 * as board_inject_garbage's row fill. If hole_col is outside the board no
 * column matches it, so the rows come out fully solid - board_clear_lines
 * sees them as full on the next tick and "burns" them for points. */
void board_fill_rows(t_board *b, int n, int hole_col) {
  if (n <= 0) return;
  if (n > BOARD_HEIGHT) n = BOARD_HEIGHT;

  for (int row = BOARD_HEIGHT - n; row < BOARD_HEIGHT; row++) {
    for (int col = 0; col < BOARD_WIDTH; col++) {
      if (col == hole_col)
        b->cells[row][col] = (t_cell){CELL_EMPTY, 0};
      else
        b->cells[row][col] = (t_cell){CELL_GARBAGE, 0};
    }
  }
}

// Halloween "Bomb": clear an arbitrary scattered set of cells. cols/rows are
// parallel arrays of length count. board_set bounds-checks, so a malformed
// ABILITY frame with out-of-range targets is silently ignored.
void board_clear_cells(t_board *b, int cols[], int rows[], int count) {
  for (int i = 0; i < count; i++)
    board_set(b, cols[i], rows[i], (t_cell){CELL_EMPTY, 0});
}

// Princess: laser blast clears columns [start_col, end_col] inclusive across
// every row, with no compaction. Out-of-range bounds are clamped to the
// board so a bad target can't write OOB. Cells are zeroed with memset (not a
// compound-literal assignment) so the cleared cells - padding included - are
// byte-identical to a board_init'd board, which callers compare against.
void board_delete_columns(t_board *b, int start_col, int end_col) {
  if (start_col < 0) start_col = 0;
  if (end_col >= BOARD_WIDTH) end_col = BOARD_WIDTH - 1;

  for (int row = 0; row < BOARD_HEIGHT; row++)
    for (int col = start_col; col <= end_col; col++)
      memset(&b->cells[row][col], 0, sizeof(t_cell));
}
