#ifndef TETRISBRAIN_H
#define TETRISBRAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20

// what's in a cell
typedef enum {
  CELL_EMPTY = 0,
  CELL_FILLED = 1,
  CELL_GARBAGE = 2, // for garbage lines (different color in tetrisu)
} cell_type_t;

typedef struct {
  cell_type_t type;
  uint8_t color; // piece color index (0-6), 0 if empty
} cell_t;

typedef struct {
  cell_t cells[BOARD_HEIGHT][BOARD_WIDTH];
} board_t;

// 7 tetromino types
typedef enum {
  PIECE_I,
  PIECE_O,
  PIECE_T,
  PIECE_S,
  PIECE_Z,
  PIECE_J,
  PIECE_L
} piece_type_t;

typedef struct {
  piece_type_t type;
  int col;      // leftmost column of bounding box
  int row;      // topmost row of bounding box
  int rotation; // 0-3
} piece_t;

// return codes
typedef enum {
  BRAIN_OK,
  BRAIN_BLOCKED,   // move rejected (wall, floor, other piece)
  BRAIN_LOCKED,    // piece can't fall, should be stored
  BRAIN_GAME_OVER, // piece locked at top
  BRAIN_CLEARED,   // lines were cleared this tick
} brain_result_t;

// ---- board.c ----
void board_init(board_t *b);
cell_t board_get(const board_t *b, int col, int row);
void board_set(board_t *b, int col, int row, cell_t cell);
bool board_in_bounds(int col, int row);
void board_inject_garbage(board_t *b, int lines, int hole_col);
void board_copy(board_t *dst, const board_t *src);

// ---- pieces.c ----
piece_t piece_spawn(piece_type_t type);
bool piece_is_valid(const board_t *b, const piece_t *p);
brain_result_t piece_move(const board_t *b, piece_t *p, int dcol, int drow);
brain_result_t piece_rotate(const board_t *b, piece_t *p,
                            int dir);           // +1 CW, -1 CCW
void piece_stamp(board_t *b, const piece_t *p); // lock piece into board

// ---- gravity.c ----
brain_result_t gravity_tick(const board_t *b, piece_t *p);
brain_result_t piece_soft_drop(const board_t *b, piece_t *p);
void piece_hard_drop(const board_t *b, piece_t *p); // moves p in place

// ---- lineclear.c ----
int board_clear_lines(board_t *b); // returns lines cleared (0-4)

// ---- scoring.c ----
int score_on_clear(int lines_cleared, int level);
int level_from_lines(int total_lines);
int gravity_interval_ms(int level); // ms per tick

// ---- abilities.c ----
void board_cut_top(board_t *b, int n);
void board_cut_bottom(board_t *b, int n);
void board_apply_gravity(board_t *b);
void board_invert(board_t *b);
void board_fill_rows(board_t *b, int n, int hole_col);
void board_clear_cells(board_t *b, int cols[], int rows[], int count);
void board_delete_columns(board_t *b, int start_col, int end_col);

#endif
