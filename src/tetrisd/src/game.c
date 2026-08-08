#include "tetrisd.h"

// Static Functions
static void				spawn_next(t_game *g);
static void				lock_piece(t_game *g);
static t_body_clear_label	clear_label(int lines, bool perfect);
static int				step_interval(const t_game *g);
static bool				begin_clear(t_game *g);
static void				finish_clear(t_game *g);
static bool				advance_clear(t_game *g, int elapsed_ms);
static bool				apply_fall(t_game *g, int elapsed_ms);
static bool				apply_lockdown(t_game *g, int elapsed_ms);

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
	g->seed = seed;
	g->hold = BODY_HOLD_EMPTY;
	g->lines = 0;
	g->level = level_from_lines(0);
	g->seq = 0;
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
 * than assuming one tick equals one row, so a late tick catches up
 * instead of slowing the game down.
 *
 * A paused game returns before the accumulator is touched, so time spent
 * paused is not owed back to it - resuming must not drop the piece four rows
 * to make up for the pause.
 *
 * A game holding completed rows is not falling: gravity is the clear's timer
 * for as long as it runs, and the piece that comes next has not been dealt.
 * Every tick of it is a snapshot, so the client has the frames to animate it
 * with rather than one flash at each end.
 *
 * Falling and locking are two clocks, not one. A piece that has landed is
 * out of rows to fall but still has its lock delay to spend, so the same
 * elapsed milliseconds are charged against both.
 *
 * @param g Game to advance.
 * @param elapsed_ms Milliseconds since this game was last advanced.
 * @return true when the board or piece changed and a snapshot is due.
 */
bool	game_gravity(t_game *g, int elapsed_ms)
{
	bool	changed;

	if (g == NULL || !g->active || g->paused || elapsed_ms <= 0)
		return (false);
	if (g->clearing_count > 0)
		return (advance_clear(g, elapsed_ms));
	changed = apply_fall(g, elapsed_ms);
	return (apply_lockdown(g, elapsed_ms) || changed);
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
	bool	was_grounded;

	if (g == NULL || !g->active || g->paused || g->clearing_count > 0)
		return (false);
	if (effect_controls_inverted(&g->effects))
		dcol = -dcol;
	was_grounded = lockdown_grounded(&g->board, &g->piece);
	if (piece_move(&g->board, &g->piece, dcol, 0) != BRAIN_OK)
		return (false);
	lockdown_on_shift(&g->lockdown, was_grounded);
	return (true);
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
	bool	was_grounded;
	int		kick;

	if (g == NULL || !g->active || g->paused || g->clearing_count > 0
		|| effect_rotation_blocked(&g->effects))
		return (false);
	kick = 0;
	was_grounded = lockdown_grounded(&g->board, &g->piece);
	if (piece_rotate_with_kick(&g->board, &g->piece, dir, &kick) != BRAIN_OK)
		return (false);
	lockdown_on_shift(&g->lockdown, was_grounded);
	return (true);
}

/**
 * @brief Drops the falling piece: one row on a soft drop, all the way on hard.
 *
 * A hard drop always locks, which is why it can never be "blocked" - it is
 * accepted even when the piece is already resting on the stack. That is the
 * whole difference between the two under the Guideline: hard drop is the
 * input that says "and I am done with it", soft drop is only gravity in a
 * hurry. So a soft drop into the floor does nothing at all and the piece
 * keeps its lock delay, which is what leaves the player the half-second the
 * drop was pressed to reach.
 *
 * @param g Game to act on.
 * @param hard true for a hard drop, false for a soft drop.
 * @return true when the drop was applied.
 */
bool	game_drop(t_game *g, bool hard)
{
	int	distance;

	if (g == NULL || !g->active || g->paused || g->clearing_count > 0
		|| effect_fastdrop_blocked(&g->effects))
		return (false);
	if (hard)
	{
		distance = piece_drop_distance(&g->board, &g->piece);
		piece_hard_drop(&g->board, &g->piece);
		score_add_drop(&g->score, distance, true);
		lock_piece(g);
		return (true);
	}
	if (piece_soft_drop(&g->board, &g->piece) != BRAIN_OK)
		return (false);
	score_add_drop(&g->score, 1, false);
	g->accum_ms = 0;
	lockdown_on_fall(&g->lockdown, &g->piece);
	return (true);
}

/**
 * @brief Swaps the falling piece with the hold slot.
 *
 * The first hold of a game has nothing to swap with, so it takes the head of
 * the next queue instead - which is why the queue is refilled here and not
 * only on a lock. Either way the incoming piece is spawned fresh rather than
 * keeping the outgoing one's position, so holding cannot be used to teleport
 * a piece across the board.
 *
 * One hold per piece is the rule that makes this a swap and not a shuffle: a
 * player who could hold repeatedly would never have to place anything.
 *
 * @param g Game to act on.
 * @return true when the swap happened, false when it was refused.
 */
bool	game_hold(t_game *g)
{
	int	outgoing;

	if (g == NULL || !g->active || g->paused || g->hold_used
		|| g->clearing_count > 0)
		return (false);
	outgoing = (int)g->piece.type;
	g->hold_used = true;
	g->accum_ms = 0;
	if (!g->has_hold)
	{
		g->hold = outgoing;
		g->has_hold = true;
		spawn_next(g);
		return (true);
	}
	g->piece = piece_spawn((t_piece_type)g->hold);
	g->hold = outgoing;
	lockdown_init(&g->lockdown, &g->piece);
	if (!piece_is_valid(&g->board, &g->piece))
	{
		g->topped_out = true;
		g->active = false;
	}
	return (true);
}

/**
 * @brief Pauses or resumes a game.
 *
 * Pausing leaves `active` alone: a paused game is still being played, and a
 * room that read it as finished would record the score and evict the player.
 * Gravity is what stops, and it stops without owing the piece the time.
 *
 * @param g Game to act on.
 * @param paused true to pause, false to resume.
 * @return true when the state changed, false when it was already there.
 */
bool	game_pause(t_game *g, bool paused)
{
	if (g == NULL || !g->active || g->paused == paused)
		return (false);
	g->paused = paused;
	g->accum_ms = 0;
	return (true);
}

/**
 * @brief Deals the same player a fresh game in the same slot.
 *
 * The seed is advanced rather than reused, so a restart is a new game and not
 * a replay of the one just abandoned. The score of the abandoned game is not
 * recorded: restarting is the player choosing that it did not happen, and the
 * room records what a player finishes or forfeits, not what they discard.
 *
 * The snapshot sequence is the one thing carried across. It counts STATE
 * frames on a connection, not events in a game, and the client drops any
 * snapshot numbered below the last it saw - so a counter that went back to
 * zero made the whole of the new game's opening look like replayed frames,
 * and the board sat frozen until the count climbed past where the old game
 * had left it.
 *
 * @param g Game to restart.
 * @return true when a new game was dealt.
 */
bool	game_restart(t_game *g)
{
	t_player_id	pid;
	uint32_t	seed;
	uint64_t	seq;

	if (g == NULL || g->player_id == 0)
		return (false);
	pid = g->player_id;
	seed = g->seed * 1664525u + 1013904223u;
	seq = g->seq;
	game_start(g, pid, seed);
	g->seq = seq;
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
void	game_snapshot(const t_game *g, t_body_state *out)
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
	else if (g->paused)
		out->phase = BODY_PHASE_PAUSED;
	else if (g->clearing_count > 0)
		out->phase = BODY_PHASE_CLEARING;
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
	out->hold = BODY_HOLD_EMPTY;
	if (g->has_hold)
		out->hold = g->hold;
	out->hold_used = g->hold_used;
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
	out->last_ability = g->last_ability;
	out->last_clear = g->last_clear;
	out->clearing_count = g->clearing_count;
	out->clearing_ms = g->clearing_ms;
	row = 0;
	while (row < g->clearing_count && row < BODY_CLEARING_MAX)
	{
		out->clearing_rows[row] = g->clearing_rows[row];
		row++;
	}
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
	lockdown_init(&g->lockdown, &g->piece);
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
 * Thwack and Fry are applied here rather than inside board_clear_lines,
 * because they are status effects on a player and the brain's line clear is a
 * fact about a board. With Thwack active, non-crystal blocks fall out of the
 * rows above and whatever that completes clears too; Fry's three filled rows
 * burn off now, one piece after they went in (docs/themes.md, Wolf-man L4 and
 * Halloween L1). Both are read before effect_on_piece_lock, which is what
 * consumes the counters.
 *
 * Locking is also the one thing that gives the hold slot back - one hold per
 * piece, counted from the piece that just landed.
 *
 * @param g Game whose piece has landed.
 */
static void	lock_piece(t_game *g)
{
	piece_stamp(&g->board, &g->piece);
	g->hold_used = false;
	g->accum_ms = 0;
	if (begin_clear(g))
		return ;
	finish_clear(g);
}

/**
 * @brief Holds the rows the landed piece completed, if it completed any.
 *
 * Nothing is taken away and nothing is scored here. The rows stay on the
 * board for clear_duration_ms, which is what the client animates and what
 * makes it honest to animate: for that long the board really does still have
 * them, and there is no piece to move because none has been dealt.
 *
 * @param g Game whose piece has just landed.
 * @return true when a clear is now in progress.
 */
static bool	begin_clear(t_game *g)
{
	g->clearing_count = board_find_full_lines(&g->board, g->clearing_rows);
	if (g->clearing_count <= 0)
	{
		g->clearing_count = 0;
		return (false);
	}
	g->clearing_ms = 0;
	return (true);
}

/**
 * @brief Runs the held clear out and hands the board back.
 *
 * This is the whole of what a lock used to do inline: take the rows away,
 * apply the effects that ride on a clear, score it, and deal the next piece.
 * It is also reached with clearing_count 0 by a lock that completed nothing,
 * because a lock that clears nothing still scores nothing, still consumes its
 * effects, and still needs a piece.
 *
 * @param g Game whose clear is over.
 */
static void	finish_clear(t_game *g)
{
	int		cleared;
	bool	perfect;

	cleared = board_clear_lines(&g->board);
	if (cleared > 0 && effect_thwack_active(&g->effects))
		cleared += board_cascade_clear(&g->board);
	board_cut_bottom(&g->board, effect_fry_rows(&g->effects));
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
	g->clearing_count = 0;
	g->clearing_ms = 0;
	g->accum_ms = 0;
	spawn_next(g);
}

/**
 * @brief Charges elapsed time against a clear in progress.
 *
 * Every tick reports a change even when the millisecond count is all that
 * moved, because that count is the animation: a client sent only the first
 * and last frame would have nothing to interpolate and would flash.
 *
 * @param g Game holding completed rows.
 * @param elapsed_ms Milliseconds since the last tick.
 * @return true, always - a clear in progress always owes a snapshot.
 */
static bool	advance_clear(t_game *g, int elapsed_ms)
{
	int	duration_ms;

	duration_ms = clear_duration_ms(g->level);
	g->clearing_ms += elapsed_ms;
	if (g->clearing_ms >= duration_ms)
	{
		g->clearing_ms = duration_ms;
		finish_clear(g);
	}
	return (true);
}

/**
 * @brief Names a clear for the client's feedback line.
 *
 * @param lines Lines cleared by the lock, 0 for none.
 * @param perfect true when the clear emptied the board.
 * @return The matching wire label.
 */
static t_body_clear_label	clear_label(int lines, bool perfect)
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
 * The brain reports 0 ms at 20G; a zero step would spin the tick, so one
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

/**
 * @brief Drops the piece by however many rows the elapsed time bought.
 *
 * The accumulator is emptied the moment the piece lands rather than left to
 * grow: the seconds a piece spends resting are not owed to it, and a piece
 * nudged back off a ledge should fall on the next step and not instantly.
 *
 * @param g Game whose piece is falling.
 * @param elapsed_ms Milliseconds since the last tick.
 * @return true when the piece changed rows.
 */
static bool	apply_fall(t_game *g, int elapsed_ms)
{
	bool	changed;

	changed = false;
	g->accum_ms += elapsed_ms;
	while (g->accum_ms >= step_interval(g))
	{
		g->accum_ms -= step_interval(g);
		if (gravity_tick(&g->board, &g->piece) != BRAIN_OK)
		{
			g->accum_ms = 0;
			break ;
		}
		lockdown_on_fall(&g->lockdown, &g->piece);
		changed = true;
	}
	return (changed);
}

/**
 * @brief Charges the same elapsed time against a landed piece's lock delay.
 *
 * Only the expiry is a change worth a snapshot: while the delay runs the
 * board looks exactly as it did, so a client told about every tick of it
 * would be sent the same frame forty times.
 *
 * @param g Game whose piece may be resting.
 * @param elapsed_ms Milliseconds since the last tick.
 * @return true when the piece locked on this tick.
 */
static bool	apply_lockdown(t_game *g, int elapsed_ms)
{
	bool	grounded;

	grounded = lockdown_grounded(&g->board, &g->piece);
	if (!lockdown_tick(&g->lockdown, grounded, elapsed_ms))
		return (false);
	lock_piece(g);
	return (true);
}
