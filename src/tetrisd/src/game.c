#include "tetrisd.h"

// Static Functions
static void				spawn_next(t_game *g);
static void				lock_piece(t_game *g);
static t_sb_clear_label	clear_label(int lines, bool perfect);
static int				step_interval(const t_game *g);

/**
 * @brief Blanks a game slot back to "nobody is playing here".
 *
 * @param g Game to reset.
 */
void	game_reset(t_game *g)
{
	if (g == NULL)
		return ;
	memset(g, 0, sizeof(*g));
}

/**
 * @brief Starts a fresh game for one player: empty board, 7-bag, first piece.
 *
 * Every game gets its own bag seeded per player, so two players in the same
 * room see independent - and equally fair - piece sequences.
 *
 * @param g Game to start.
 * @param pid Player this game belongs to.
 * @param seed Seed for this game's 7-bag randomiser.
 */
void	game_start(t_game *g, t_player_id pid, uint32_t seed)
{
	int	i;

	if (g == NULL)
		return ;
	game_reset(g);
	board_init(&g->board);
	piece_bag_init(&g->bag, seed);
	score_state_init(&g->score);
	charge_state_init(&g->charge);
	effect_state_init(&g->effects);
	g->player_id = pid;
	g->lines = 0;
	g->level = level_from_lines(0);
	g->seq = 1;
	i = 0;
	while (i < BODY_NEXT_COUNT)
	{
		g->next[i] = (int)piece_bag_next(&g->bag);
		i++;
	}
	g->active = true;
	spawn_next(g);
}

/**
 * @brief Applies however much real time has passed to the falling piece.
 *
 * Elapsed time is accumulated against this player's own level interval rather
 * than assuming one tick equals one row, so a descheduled ticker catches up
 * instead of slowing the game down.
 *
 * @param g Game to advance.
 * @param elapsed_ms Milliseconds since this game was last advanced.
 * @return true when the board or piece changed and a snapshot is due.
 */
bool	game_gravity(t_game *g, int elapsed_ms)
{
	bool	changed;

	if (g == NULL || !g->active || elapsed_ms <= 0)
		return (false);
	changed = false;
	g->accum_ms += elapsed_ms;
	while (g->active && g->accum_ms >= step_interval(g))
	{
		g->accum_ms -= step_interval(g);
		if (gravity_tick(&g->board, &g->piece) == BRAIN_LOCKED)
			lock_piece(g);
		changed = true;
	}
	return (changed);
}

/**
 * @brief Moves the falling piece sideways.
 *
 * @param g Game to act on.
 * @param dcol -1 for left, +1 for right.
 * @return true when the piece moved, false when the move was blocked.
 */
bool	game_move(t_game *g, int dcol)
{
	if (g == NULL || !g->active)
		return (false);
	if (effect_controls_inverted(&g->effects))
		dcol = -dcol;
	return (piece_move(&g->board, &g->piece, dcol, 0) == BRAIN_OK);
}

/**
 * @brief Rotates the falling piece, wall kicks included.
 *
 * @param g Game to act on.
 * @param dir +1 for clockwise, -1 for counter-clockwise.
 * @return true when the piece rotated, false when every kick was blocked.
 */
bool	game_rotate(t_game *g, int dir)
{
	int	kick;

	if (g == NULL || !g->active || effect_rotation_blocked(&g->effects))
		return (false);
	kick = 0;
	return (piece_rotate_with_kick(&g->board, &g->piece, dir, &kick)
		== BRAIN_OK);
}

/**
 * @brief Drops the falling piece: one row on a soft drop, all the way on hard.
 *
 * A hard drop always locks, which is why it can never be "blocked" - it is
 * accepted even when the piece is already resting on the stack.
 *
 * @param g Game to act on.
 * @param hard true for a hard drop, false for a soft drop.
 * @return true when the drop was applied.
 */
bool	game_drop(t_game *g, bool hard)
{
	int	distance;

	if (g == NULL || !g->active || effect_fastdrop_blocked(&g->effects))
		return (false);
	if (hard)
	{
		distance = piece_drop_distance(&g->board, &g->piece);
		piece_hard_drop(&g->board, &g->piece);
		score_add_drop(&g->score, distance, true);
		lock_piece(g);
		return (true);
	}
	if (piece_soft_drop(&g->board, &g->piece) == BRAIN_OK)
	{
		score_add_drop(&g->score, 1, false);
		g->accum_ms = 0;
		return (true);
	}
	lock_piece(g);
	return (true);
}

/**
 * @brief Projects a game onto the wire-facing STATE body structure.
 *
 * This is the domain-to-wire boundary: everything the client renders comes
 * from here, and nothing about rooms, connections, or identity does - the
 * subject rides in the request path instead (ADR-0003).
 *
 * @param g Game to project.
 * @param out Snapshot to fill.
 */
void	game_snapshot(const t_game *g, t_sb_state *out)
{
	t_cell	cell;
	int		row;
	int		col;

	if (g == NULL || out == NULL)
		return ;
	memset(out, 0, sizeof(*out));
	out->seq = g->seq;
	out->phase = BODY_PHASE_ACTIVE;
	if (g->topped_out)
		out->phase = BODY_PHASE_TOP_OUT;
	else if (!g->active)
		out->phase = BODY_PHASE_PAUSED;
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			cell = board_get(&g->board, col, row);
			out->cells[row][col].type = (uint8_t)cell.type;
			out->cells[row][col].color = (uint8_t)(cell.color & 0x0F);
			col++;
		}
		row++;
	}
	out->piece.type = (int)g->piece.type;
	out->piece.rotation = g->piece.rotation;
	out->piece.col = g->piece.col;
	out->piece.row = g->piece.row;
	out->next[0] = g->next[0];
	out->next[1] = g->next[1];
	out->next[2] = g->next[2];
	out->score = g->score.total;
	out->lines = g->lines;
	out->level = g->level;
	out->combo = g->score.combo;
	if (out->combo < 0)
		out->combo = 0;
	out->back_to_back = g->score.back_to_back;
	out->charge = g->charge.charges;
	if (out->charge > BODY_CHARGE_MAX)
		out->charge = BODY_CHARGE_MAX;
	out->last_clear = g->last_clear;
}

/**
 * @brief Spawns the next piece from the bag, ending the game on a top-out.
 *
 * A piece that cannot be placed at spawn is the honest end of the game: the
 * stack has reached the ceiling, so the game is over rather than the piece
 * being nudged somewhere it does not belong.
 *
 * @param g Game to spawn into.
 */
static void	spawn_next(t_game *g)
{
	int	i;

	g->piece = piece_spawn((t_piece_type)g->next[0]);
	i = 0;
	while (i < BODY_NEXT_COUNT - 1)
	{
		g->next[i] = g->next[i + 1];
		i++;
	}
	g->next[BODY_NEXT_COUNT - 1] = (int)piece_bag_next(&g->bag);
	if (!piece_is_valid(&g->board, &g->piece))
	{
		g->topped_out = true;
		g->active = false;
	}
}

/**
 * @brief Locks the falling piece: stamp, clear lines, score, spawn the next.
 *
 * @param g Game whose piece has landed.
 */
static void	lock_piece(t_game *g)
{
	int		cleared;
	bool	perfect;

	piece_stamp(&g->board, &g->piece);
	cleared = board_clear_lines(&g->board);
	perfect = cleared > 0 && board_is_empty(&g->board);
	score_apply_clear(&g->score, cleared, g->level, T_SPIN_NONE, perfect);
	if (cleared > 0)
	{
		g->lines += cleared;
		g->level = level_from_lines(g->lines);
		charge_on_clear(&g->charge, cleared);
	}
	g->last_clear = clear_label(cleared, perfect);
	effect_on_piece_lock(&g->effects);
	g->accum_ms = 0;
	g->seq++;
	spawn_next(g);
}

/**
 * @brief Names a clear for the client's feedback line.
 *
 * @param lines Lines cleared by the lock, 0 for none.
 * @param perfect true when the clear emptied the board.
 * @return The matching wire label.
 */
static t_sb_clear_label	clear_label(int lines, bool perfect)
{
	if (perfect)
		return (BODY_CLEAR_PERFECT);
	if (lines == 1)
		return (BODY_CLEAR_SINGLE);
	if (lines == 2)
		return (BODY_CLEAR_DOUBLE);
	if (lines == 3)
		return (BODY_CLEAR_TRIPLE);
	if (lines >= 4)
		return (BODY_CLEAR_TETRIS);
	return (BODY_CLEAR_NONE);
}

/**
 * @brief The current gravity step for this player, never zero.
 *
 * The brain reports 0 ms at 20G; a zero step would spin the ticker, so one
 * millisecond is the floor.
 *
 * @param g Game whose level sets the interval.
 * @return Milliseconds between automatic drops, at least 1.
 */
static int	step_interval(const t_game *g)
{
	int	interval;

	interval = gravity_interval_ms(g->level);
	if (interval < 1)
		return (1);
	return (interval);
}
