#include "tetrisbrain.h"

typedef struct {
  int8_t dcol;
  int8_t drow;
} cell_offset_t;

// SHAPES[type][rotation][4 cells], offsets from t_piece.col/row.
// Coordinates are row-down (matches board.c), converted from the SRS
// guideline's y-up tables via drow = -dy.
//
// rotation indices follow the SRS naming: 0 = spawn state, 1 = R (one CW
// turn from spawn), 2 = 180 degrees from spawn, 3 = L (one CCW turn from
// spawn / one CW turn from 2). Each row below is labeled with its state.
//
// Example (PIECE_T, rotation 0): {{1,0},{0,1},{1,1},{2,1}} plots as
//   row0:  .  X  .
//   row1:  X  X  X
// SPAWN[PIECE_T] = {col=3, row=0}, so the 4 board cells are
// (4,0) (3,1) (4,1) (5,1) - see test_stamp_writes_cells.
static const cell_offset_t SHAPES[7][4][4] = {
    [PIECE_I] =
        {
            {{0, 1}, {1, 1}, {2, 1}, {3, 1}}, // 0 (spawn)
            {{2, 0}, {2, 1}, {2, 2}, {2, 3}}, // R (CW from spawn)
            {{0, 2}, {1, 2}, {2, 2}, {3, 2}}, // 2 (180 from spawn)
            {{1, 0}, {1, 1}, {1, 2}, {1, 3}}, // L (CCW from spawn)
        },
    [PIECE_O] =
        {
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // 0 (spawn)
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // R - identical to 0
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // 2 - identical to 0
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // L - identical to 0
        },
    [PIECE_T] =
        {
            {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, // 0 (spawn)
            {{1, 0}, {1, 1}, {2, 1}, {1, 2}}, // R (CW from spawn)
            {{0, 1}, {1, 1}, {2, 1}, {1, 2}}, // 2 (180 from spawn)
            {{1, 0}, {0, 1}, {1, 1}, {1, 2}}, // L (CCW from spawn)
        },
    [PIECE_S] =
        {
            {{1, 0}, {2, 0}, {0, 1}, {1, 1}}, // 0 (spawn)
            {{1, 0}, {1, 1}, {2, 1}, {2, 2}}, // R (CW from spawn)
            {{1, 1}, {2, 1}, {0, 2}, {1, 2}}, // 2 (180 from spawn)
            {{0, 0}, {0, 1}, {1, 1}, {1, 2}}, // L (CCW from spawn)
        },
    [PIECE_Z] =
        {
            {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // 0 (spawn)
            {{2, 0}, {1, 1}, {2, 1}, {1, 2}}, // R (CW from spawn)
            {{0, 1}, {1, 1}, {1, 2}, {2, 2}}, // 2 (180 from spawn)
            {{1, 0}, {0, 1}, {1, 1}, {0, 2}}, // L (CCW from spawn)
        },
    [PIECE_J] =
        {
            {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // 0 (spawn)
            {{1, 0}, {2, 0}, {1, 1}, {1, 2}}, // R (CW from spawn)
            {{0, 1}, {1, 1}, {2, 1}, {2, 2}}, // 2 (180 from spawn)
            {{1, 0}, {1, 1}, {0, 2}, {1, 2}}, // L (CCW from spawn)
        },
    [PIECE_L] =
        {
            {{2, 0}, {0, 1}, {1, 1}, {2, 1}}, // 0 (spawn)
            {{1, 0}, {1, 1}, {1, 2}, {2, 2}}, // R (CW from spawn)
            {{0, 1}, {1, 1}, {2, 1}, {0, 2}}, // 2 (180 from spawn)
            {{0, 0}, {1, 0}, {1, 1}, {1, 2}}, // L (CCW from spawn)
        },
};

// Spawn anchors (rotation 0): centered, occupied cells land on row 0 (I,
// JLSTZ) or rows 0-1 (O). I piece uses row=-1 because its spawn shape's
// occupied cells are at local row 1, not row 0.
// {Type, Column, Row, Rotation}
static const t_piece SPAWN[7] = {
    [PIECE_I] = {PIECE_I, 3, -1, 0}, [PIECE_O] = {PIECE_O, 4, 0, 0},
    [PIECE_T] = {PIECE_T, 3, 0, 0},  [PIECE_S] = {PIECE_S, 3, 0, 0},
    [PIECE_Z] = {PIECE_Z, 3, 0, 0},  [PIECE_J] = {PIECE_J, 3, 0, 0},
    [PIECE_L] = {PIECE_L, 3, 0, 0},
};

t_piece piece_spawn(t_piece_type type) {
  if (type < PIECE_I || type > PIECE_L) return SPAWN[PIECE_I];
  return SPAWN[type];
}

// A corrupted t_piece (e.g. from a malformed network message) could carry a
// type/rotation outside the SHAPES/KICK_TRANSITION tables; guard every entry
// point that indexes those tables by p->type/p->rotation.
static bool piece_shape_in_range(const t_piece *p) {
  return p->type >= PIECE_I && p->type <= PIECE_L && p->rotation >= 0 &&
         p->rotation < 4;
}

bool piece_cells(const t_piece *p, int cols[4], int rows[4]) {
  if (!piece_shape_in_range(p)) return false;
  for (int i = 0; i < 4; i++) {
    cols[i] = p->col + SHAPES[p->type][p->rotation][i].dcol;
    rows[i] = p->row + SHAPES[p->type][p->rotation][i].drow;
  }
  return true;
}

bool piece_is_valid(const t_board *b, const t_piece *p) {
  if (!piece_shape_in_range(p)) return false;
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    if (board_get(b, col, row).type != CELL_EMPTY) return false;
  }
  return true;
}

// Wall kicks: when a rotation's naive shape would overlap a wall, floor, or
// existing block, SRS retries with a small set of (dcol,drow) offsets applied
// to the *new* (post-rotation) shape, in order, until one fits. Test 0 is
// always (0,0) — the unkicked rotation. Offsets are already converted to this
// project's row-down convention (drow = -dy_guideline).
//
// Transition indices: 0=0->R 1=R->0 2=R->2 3=2->R 4=2->L 5=L->2 6=L->0 7=0->L
// KICK_TRANSITION[from_rotation][dir_idx], dir_idx: 0=CW(+1), 1=CCW(-1),
// maps the piece's current rotation + turn direction to one of the 8 rows
// above.
static const int8_t KICK_TRANSITION[4][2] = {
    {0, 7}, // from 0
    {2, 1}, // from R
    {4, 3}, // from 2
    {6, 5}, // from L
};

// Kick table for J, L, S, T, Z (all share the same 3x3-bounding-box kicks).
// Each A->B row is the elementwise negation of its reverse B->A row, so
// undoing a kicked rotation always lands back where it started.
static const cell_offset_t JLSTZ_KICKS[8][5] = {
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   // 0->R
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // R->0
    {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // R->2
    {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   // 2->R
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      // 2->L
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},  // L->2
    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},  // L->0
    {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      // 0->L
};

// Kick table for the I piece. Its 4x1 bounding box needs larger, asymmetric
// offsets compared to JLSTZ, so it gets its own table (no special-case for
// O is needed here — see piece_rotate below).
static const cell_offset_t I_KICKS[8][5] = {
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},    // 0->R
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},    // R->0
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},    // R->2
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},    // 2->R
    {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},    // 2->L
    {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},    // L->2
    {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},    // L->0
    {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},    // 0->L
};

// dir: +1 rotates clockwise, -1 rotates counter-clockwise. Adding 3 (instead
// of subtracting 1) keeps the result non-negative before the % 4.
t_brain_result piece_rotate_with_kick(const t_board *b, t_piece *p, int dir,
                                     int *kick_index) {
  if (kick_index != NULL) *kick_index = -1;
  if (!piece_shape_in_range(p) || (dir != 1 && dir != -1))
    return BRAIN_BLOCKED;

  int to = (p->rotation + (dir == 1 ? 1 : 3)) % 4;

  // O piece's shape is identical in every rotation state: if the current
  // position is valid, it stays valid after relabeling the rotation.
  if (p->type == PIECE_O) {
    p->rotation = to;
    if (kick_index != NULL) *kick_index = 0;
    return BRAIN_OK;
  }

  int transition = KICK_TRANSITION[p->rotation][dir == 1 ? 0 : 1];
  const cell_offset_t *kicks =
      (p->type == PIECE_I) ? I_KICKS[transition] : JLSTZ_KICKS[transition];

  for (int i = 0; i < 5; i++) {
    t_piece cand = *p;
    cand.rotation = to;
    cand.col += kicks[i].dcol;
    cand.row += kicks[i].drow;
    if (piece_is_valid(b, &cand)) {
      *p = cand;
      if (kick_index != NULL) *kick_index = i;
      return BRAIN_OK;
    }
  }
  return BRAIN_BLOCKED;
}

t_brain_result piece_rotate(const t_board *b, t_piece *p, int dir) {
  return piece_rotate_with_kick(b, p, dir, NULL);
}

static bool corner_occupied(const t_board *b, int col, int row) {
  return board_get(b, col, row).type != CELL_EMPTY;
}

/* AI-assisted: classifies a rotated T with the Guideline three-corner rule;
 * the fifth SRS kick upgrades a mini because that kick enables full triples. */
t_spin_type piece_t_spin_type(const t_board *b, const t_piece *p,
                              int kick_index) {
  bool corners[4];
  int occupied;
  int front;
  int center_col;
  int center_row;

  if (!piece_shape_in_range(p) || p->type != PIECE_T) return T_SPIN_NONE;
  center_col = p->col + 1;
  center_row = p->row + 1;
  corners[0] = corner_occupied(b, center_col - 1, center_row - 1);
  corners[1] = corner_occupied(b, center_col + 1, center_row - 1);
  corners[2] = corner_occupied(b, center_col - 1, center_row + 1);
  corners[3] = corner_occupied(b, center_col + 1, center_row + 1);
  occupied = (int)corners[0] + (int)corners[1] +
             (int)corners[2] + (int)corners[3];
  if (occupied < 3) return T_SPIN_NONE;
  if (p->rotation == 0) front = (int)corners[0] + (int)corners[1];
  else if (p->rotation == 1) front = (int)corners[1] + (int)corners[3];
  else if (p->rotation == 2) front = (int)corners[2] + (int)corners[3];
  else front = (int)corners[0] + (int)corners[2];
  if (front == 2 || kick_index == 4) return T_SPIN_FULL;
  return T_SPIN_MINI;
}

t_brain_result piece_move(const t_board *b, t_piece *p, int dcol, int drow) {
  t_piece moved = *p;
  moved.col += dcol;
  moved.row += drow;
  if (!piece_is_valid(b, &moved)) return BRAIN_BLOCKED;
  *p = moved;
  return BRAIN_OK;
}

void piece_stamp(t_board *b, const t_piece *p) {
  if (!piece_shape_in_range(p)) return;
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    board_set(b, col, row, (t_cell){CELL_FILLED, (uint8_t)p->type});
  }
}
