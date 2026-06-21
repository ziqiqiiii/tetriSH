#include "tetrisbrain.h"

typedef struct {
  int8_t dcol;
  int8_t drow;
} cell_offset_t;

// SHAPES[type][rotation][4 cells], offsets from piece_t.col/row.
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
static const piece_t SPAWN[7] = {
    [PIECE_I] = {PIECE_I, 3, -1, 0}, [PIECE_O] = {PIECE_O, 4, 0, 0},
    [PIECE_T] = {PIECE_T, 3, 0, 0},  [PIECE_S] = {PIECE_S, 3, 0, 0},
    [PIECE_Z] = {PIECE_Z, 3, 0, 0},  [PIECE_J] = {PIECE_J, 3, 0, 0},
    [PIECE_L] = {PIECE_L, 3, 0, 0},
};

piece_t piece_spawn(piece_type_t type) { return SPAWN[type]; }

// A corrupted piece_t (e.g. from a malformed network message) could carry a
// type/rotation outside the SHAPES/KICK_TRANSITION tables; guard every entry
// point that indexes those tables by p->type/p->rotation.
static bool piece_shape_in_range(const piece_t *p) {
  return p->type >= PIECE_I && p->type <= PIECE_L && p->rotation >= 0 &&
         p->rotation < 4;
}

bool piece_is_valid(const board_t *b, const piece_t *p) {
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
brain_result_t piece_rotate(const board_t *b, piece_t *p, int dir) {
  if (!piece_shape_in_range(p)) return BRAIN_BLOCKED;

  int to = (p->rotation + (dir == 1 ? 1 : 3)) % 4;

  // O piece's shape is identical in every rotation state: if the current
  // position is valid, it stays valid after relabeling the rotation.
  if (p->type == PIECE_O) {
    p->rotation = to;
    return BRAIN_OK;
  }

  int transition = KICK_TRANSITION[p->rotation][dir == 1 ? 0 : 1];
  const cell_offset_t *kicks =
      (p->type == PIECE_I) ? I_KICKS[transition] : JLSTZ_KICKS[transition];

  for (int i = 0; i < 5; i++) {
    piece_t cand = *p;
    cand.rotation = to;
    cand.col += kicks[i].dcol;
    cand.row += kicks[i].drow;
    if (piece_is_valid(b, &cand)) {
      *p = cand;
      return BRAIN_OK;
    }
  }
  return BRAIN_BLOCKED;
}

brain_result_t piece_move(const board_t *b, piece_t *p, int dcol, int drow) {
  piece_t moved = *p;
  moved.col += dcol;
  moved.row += drow;
  if (!piece_is_valid(b, &moved)) return BRAIN_BLOCKED;
  *p = moved;
  return BRAIN_OK;
}

void piece_stamp(board_t *b, const piece_t *p) {
  if (!piece_shape_in_range(p)) return;
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    board_set(b, col, row, (cell_t){CELL_FILLED, (uint8_t)p->type});
  }
}