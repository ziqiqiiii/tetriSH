#include "tetrisbrain.h"

// Static Functions
static bool	piece_shape_in_range(const t_piece *p);
static bool	corner_occupied(const t_board *b, int col, int row);

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
static const t_cell_offset	SHAPES[7][4][4] = {
	[PIECE_I] = {
		{{0, 1}, {1, 1}, {2, 1}, {3, 1}}, // 0 (spawn)
		{{2, 0}, {2, 1}, {2, 2}, {2, 3}}, // R (CW from spawn)
		{{0, 2}, {1, 2}, {2, 2}, {3, 2}}, // 2 (180 from spawn)
		{{1, 0}, {1, 1}, {1, 2}, {1, 3}}, // L (CCW from spawn)
	},
	[PIECE_O] = {
		{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // 0 (spawn)
		{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // R - identical to 0
		{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // 2 - identical to 0
		{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // L - identical to 0
	},
	[PIECE_T] = {
		{{1, 0}, {0, 1}, {1, 1}, {2, 1}}, // 0 (spawn)
		{{1, 0}, {1, 1}, {2, 1}, {1, 2}}, // R (CW from spawn)
		{{0, 1}, {1, 1}, {2, 1}, {1, 2}}, // 2 (180 from spawn)
		{{1, 0}, {0, 1}, {1, 1}, {1, 2}}, // L (CCW from spawn)
	},
	[PIECE_S] = {
		{{1, 0}, {2, 0}, {0, 1}, {1, 1}}, // 0 (spawn)
		{{1, 0}, {1, 1}, {2, 1}, {2, 2}}, // R (CW from spawn)
		{{1, 1}, {2, 1}, {0, 2}, {1, 2}}, // 2 (180 from spawn)
		{{0, 0}, {0, 1}, {1, 1}, {1, 2}}, // L (CCW from spawn)
	},
	[PIECE_Z] = {
		{{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // 0 (spawn)
		{{2, 0}, {1, 1}, {2, 1}, {1, 2}}, // R (CW from spawn)
		{{0, 1}, {1, 1}, {1, 2}, {2, 2}}, // 2 (180 from spawn)
		{{1, 0}, {0, 1}, {1, 1}, {0, 2}}, // L (CCW from spawn)
	},
	[PIECE_J] = {
		{{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // 0 (spawn)
		{{1, 0}, {2, 0}, {1, 1}, {1, 2}}, // R (CW from spawn)
		{{0, 1}, {1, 1}, {2, 1}, {2, 2}}, // 2 (180 from spawn)
		{{1, 0}, {1, 1}, {0, 2}, {1, 2}}, // L (CCW from spawn)
	},
	[PIECE_L] = {
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
static const t_piece	SPAWN[7] = {
	[PIECE_I] = {PIECE_I, 3, -1, 0},
	[PIECE_O] = {PIECE_O, 4, 0, 0},
	[PIECE_T] = {PIECE_T, 3, 0, 0},
	[PIECE_S] = {PIECE_S, 3, 0, 0},
	[PIECE_Z] = {PIECE_Z, 3, 0, 0},
	[PIECE_J] = {PIECE_J, 3, 0, 0},
	[PIECE_L] = {PIECE_L, 3, 0, 0},
};

/**
 * @brief Returns the spawn-state piece for a given tetromino type.
 *
 * @param type The tetromino type to spawn.
 * @return The spawn-state t_piece for that type (PIECE_I's spawn entry if
 *         type is out of range).
 */
t_piece	piece_spawn(t_piece_type type)
{
	if (type < PIECE_I || type > PIECE_L)
		return (SPAWN[PIECE_I]);
	return (SPAWN[type]);
}

// Guards every entry point that indexes SHAPES/KICK_TRANSITION by
// p->type/p->rotation: a corrupted t_piece (e.g. from a malformed network
// message) could carry a type/rotation outside those tables.
static bool	piece_shape_in_range(const t_piece *p)
{
	return (p->type >= PIECE_I && p->type <= PIECE_L
		&& p->rotation >= 0 && p->rotation < 4);
}

/**
 * @brief Computes the four absolute board cells a piece currently occupies.
 *
 * @param p The piece to read.
 * @param cols Output array of 4 column coordinates.
 * @param rows Output array of 4 row coordinates.
 * @return true on success, false if p->type/p->rotation is out of range
 *         (cols/rows are left untouched in that case).
 */
bool	piece_cells(const t_piece *p, int cols[4], int rows[4])
{
	int	i;

	if (!piece_shape_in_range(p))
		return (false);
	for (i = 0; i < 4; i++)
	{
		cols[i] = p->col + SHAPES[p->type][p->rotation][i].dcol;
		rows[i] = p->row + SHAPES[p->type][p->rotation][i].drow;
	}
	return (true);
}

/**
 * @brief Checks whether a piece's current position/rotation is legal (all
 * four occupied cells empty; out-of-bounds reads as filled).
 *
 * @param b The board to check against.
 * @param p The piece to validate.
 * @return true if the piece fits, false if it overlaps a filled cell, a
 *         wall, the floor, or if p->type/p->rotation is out of range.
 */
bool	piece_is_valid(const t_board *b, const t_piece *p)
{
	int	i;
	int	col;
	int	row;

	if (!piece_shape_in_range(p))
		return (false);
	for (i = 0; i < 4; i++)
	{
		col = p->col + SHAPES[p->type][p->rotation][i].dcol;
		row = p->row + SHAPES[p->type][p->rotation][i].drow;
		if (board_get(b, col, row).type != CELL_EMPTY)
			return (false);
	}
	return (true);
}

// Wall kicks: when a rotation's naive shape would overlap a wall, floor, or
// existing block, SRS retries with a small set of (dcol,drow) offsets applied
// to the *new* (post-rotation) shape, in order, until one fits. Test 0 is
// always (0,0) - the unkicked rotation. Offsets are already converted to this
// project's row-down convention (drow = -dy_guideline).
//
// Transition indices: 0=0->R 1=R->0 2=R->2 3=2->R 4=2->L 5=L->2 6=L->0 7=0->L
// KICK_TRANSITION[from_rotation][dir_idx], dir_idx: 0=CW(+1), 1=CCW(-1),
// maps the piece's current rotation + turn direction to one of the 8 rows
// above.
static const int8_t	KICK_TRANSITION[4][2] = {
	{0, 7}, // from 0
	{2, 1}, // from R
	{4, 3}, // from 2
	{6, 5}, // from L
};

// Kick table for J, L, S, T, Z (all share the same 3x3-bounding-box kicks).
// Each A->B row is the elementwise negation of its reverse B->A row, so
// undoing a kicked rotation always lands back where it started.
static const t_cell_offset	JLSTZ_KICKS[8][5] = {
	{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},  // 0->R
	{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},    // R->0
	{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},    // R->2
	{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},  // 2->R
	{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},     // 2->L
	{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, // L->2
	{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, // L->0
	{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},     // 0->L
};

// Kick table for the I piece. Its 4x1 bounding box needs larger, asymmetric
// offsets compared to JLSTZ, so it gets its own table (no special-case for
// O is needed here - see piece_rotate_with_kick below).
static const t_cell_offset	I_KICKS[8][5] = {
	{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},  // 0->R
	{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},  // R->0
	{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},  // R->2
	{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},  // 2->R
	{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},  // 2->L
	{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},  // L->2
	{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},  // L->0
	{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},  // 0->L
};

/**
 * @brief Rotates a piece via SRS, retrying kick test 0-4 from the JLSTZ/I
 * kick table until one fits. O always succeeds by relabeling p->rotation,
 * since its shape is identical in every state.
 *
 * @param b The board to validate candidate positions against.
 * @param p The piece to rotate, updated in place on success.
 * @param dir Rotation direction: +1 rotates clockwise, -1 rotates
 *        counter-clockwise.
 * @param kick_index Optional output: the index (0-4) of the kick test that
 *        succeeded, or -1 if the rotation was rejected outright (pass NULL
 *        to ignore).
 * @return BRAIN_OK if a valid rotated position was found and applied,
 *         BRAIN_BLOCKED if dir/p->type/p->rotation is invalid or every
 *         kick test fails.
 */
t_brain_result	piece_rotate_with_kick(const t_board *b, t_piece *p,
	int dir, int *kick_index)
{
	int					to;
	int					transition;
	const t_cell_offset	*kicks;
	int					i;
	t_piece				cand;

	if (kick_index != NULL)
		*kick_index = -1;
	if (!piece_shape_in_range(p) || (dir != 1 && dir != -1))
		return (BRAIN_BLOCKED);
	to = (p->rotation + (dir == 1 ? 1 : 3)) % 4;
	if (p->type == PIECE_O)
	{
		p->rotation = to;
		if (kick_index != NULL)
			*kick_index = 0;
		return (BRAIN_OK);
	}
	transition = KICK_TRANSITION[p->rotation][dir == 1 ? 0 : 1];
	if (p->type == PIECE_I)
		kicks = I_KICKS[transition];
	else
		kicks = JLSTZ_KICKS[transition];
	for (i = 0; i < 5; i++)
	{
		cand = *p;
		cand.rotation = to;
		cand.col += kicks[i].dcol;
		cand.row += kicks[i].drow;
		if (piece_is_valid(b, &cand))
		{
			*p = cand;
			if (kick_index != NULL)
				*kick_index = i;
			return (BRAIN_OK);
		}
	}
	return (BRAIN_BLOCKED);
}

/**
 * @brief Rotates a piece without reporting which kick test succeeded.
 *
 * Thin wrapper over piece_rotate_with_kick with kick_index discarded.
 *
 * @param b The board to validate candidate positions against.
 * @param p The piece to rotate, updated in place on success.
 * @param dir Rotation direction: +1 rotates clockwise, -1 rotates
 *        counter-clockwise.
 * @return BRAIN_OK if the rotation succeeded, BRAIN_BLOCKED otherwise.
 */
t_brain_result	piece_rotate(const t_board *b, t_piece *p, int dir)
{
	return (piece_rotate_with_kick(b, p, dir, NULL));
}

// Returns whether the board cell at (col,row) is occupied (non-empty); used
// by piece_t_spin_type to probe the four corners around a T piece's center.
static bool	corner_occupied(const t_board *b, int col, int row)
{
	return (board_get(b, col, row).type != CELL_EMPTY);
}

/**
 * @brief Classifies a just-rotated T piece via the Guideline three-corner
 * rule: full when both front corners are occupied or kick_index is 4,
 * mini when 3+ corners are occupied otherwise, none below 3.
 *
 * @param b The board to probe for occupied corners.
 * @param p The just-rotated piece to classify (must be PIECE_T).
 * @param kick_index The kick test index that produced this rotation (see
 *        piece_rotate_with_kick); index 4 forces T_SPIN_FULL.
 * @return T_SPIN_FULL, T_SPIN_MINI, or T_SPIN_NONE if fewer than three
 *         corners are occupied or p is not a valid T piece.
 */
t_spin_type	piece_t_spin_type(const t_board *b, const t_piece *p,
	int kick_index)
{
	bool	corners[4];
	int		occupied;
	int		front;
	int		center_col;
	int		center_row;

	if (!piece_shape_in_range(p) || p->type != PIECE_T)
		return (T_SPIN_NONE);
	center_col = p->col + 1;
	center_row = p->row + 1;
	corners[0] = corner_occupied(b, center_col - 1, center_row - 1);
	corners[1] = corner_occupied(b, center_col + 1, center_row - 1);
	corners[2] = corner_occupied(b, center_col - 1, center_row + 1);
	corners[3] = corner_occupied(b, center_col + 1, center_row + 1);
	occupied = (int)corners[0] + (int)corners[1]
		+ (int)corners[2] + (int)corners[3];
	if (occupied < 3)
		return (T_SPIN_NONE);
	if (p->rotation == 0)
		front = (int)corners[0] + (int)corners[1];
	else if (p->rotation == 1)
		front = (int)corners[1] + (int)corners[3];
	else if (p->rotation == 2)
		front = (int)corners[2] + (int)corners[3];
	else
		front = (int)corners[0] + (int)corners[2];
	if (front == 2 || kick_index == 4)
		return (T_SPIN_FULL);
	return (T_SPIN_MINI);
}

/**
 * @brief Attempts to move a piece by a column/row offset.
 *
 * @param b The board to validate the moved position against.
 * @param p The piece to move, updated in place on success.
 * @param dcol Column offset to apply.
 * @param drow Row offset to apply.
 * @return BRAIN_OK if the moved position is valid and was applied,
 *         BRAIN_BLOCKED if it would overlap a wall, the floor, or a
 *         filled cell (p is left unchanged).
 */
t_brain_result	piece_move(const t_board *b, t_piece *p, int dcol, int drow)
{
	t_piece	moved;

	moved = *p;
	moved.col += dcol;
	moved.row += drow;
	if (!piece_is_valid(b, &moved))
		return (BRAIN_BLOCKED);
	*p = moved;
	return (BRAIN_OK);
}

/**
 * @brief Writes a piece's four occupied cells onto the board as CELL_FILLED,
 * colored by its type. Does nothing if p->type/p->rotation is out of range.
 *
 * @param b The board to write into.
 * @param p The piece to stamp.
 */
void	piece_stamp(t_board *b, const t_piece *p)
{
	int	i;
	int	col;
	int	row;

	if (!piece_shape_in_range(p))
		return ;
	for (i = 0; i < 4; i++)
	{
		col = p->col + SHAPES[p->type][p->rotation][i].dcol;
		row = p->row + SHAPES[p->type][p->rotation][i].drow;
		board_set(b, col, row, (t_cell){CELL_FILLED, (uint8_t)p->type});
	}
}
