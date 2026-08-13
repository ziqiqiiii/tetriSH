#include "tetrisd.h"

// Static Functions
static void				spawn_next(t_game *g);
static void				lock_piece(t_game *g);
static t_body_clear_label	clear_label(int lines, bool perfect);
static int				step_interval(const t_game *g);
static bool				begin_clear(t_game *g);
static void				finish_clear(t_game *g);
static bool				advance_clear(t_game *g, int *remaining_ms);
static bool				advance_active(t_game *g, int *remaining_ms);
static void				drain_garbage(t_game *g);
static void				inject_rows(t_game *g, int lines);
static int				next_garbage_hole(t_game *g);
static void				age_server_effects(t_game *g);
static void				drain_abilities(t_game *g);
static void				apply_bomb(t_game *g);

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
	g->garbage_seq = seed ^ 0x9e3779b9u;
	g->garbage_hole = -1;
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
 * Falling, clearing and locking share one budget, so a late tick catches up
 * across all three instead of charging itself twice. A paused game is left
 * alone: resuming must not drop the piece to pay for the pause.
 *
 * @param g Game to advance.
 * @param elapsed_ms Milliseconds since this game was last advanced.
 * @return true when the board or piece changed and a snapshot is due.
 */
bool	game_gravity(t_game *g, int elapsed_ms)
{
	bool	changed;
	int		remaining_ms;

	if (g == NULL || !g->active || g->paused || elapsed_ms <= 0)
		return (false);
	changed = false;
	remaining_ms = elapsed_ms;
	while (remaining_ms > 0 && g->active && !g->paused)
	{
		if (g->clearing_count > 0)
			changed = advance_clear(g, &remaining_ms) || changed;
		else
			changed = advance_active(g, &remaining_ms) || changed;
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
 * A hard drop always locks, so it is never blocked; a soft drop into the floor
 * does nothing and the piece keeps its lock delay.
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
 * The first hold takes the head of the next queue instead. Either way the
 * incoming piece spawns fresh, so a hold cannot teleport a piece, and one hold
 * per piece is what keeps this a swap rather than a shuffle.
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
 * The seed is advanced, so a restart is a new game and not a replay, and the
 * abandoned score is not recorded. The snapshot sequence is the one thing
 * carried across: it counts STATE frames on a connection, and a client drops
 * any snapshot below the last it saw.
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
 * Everything the client renders comes from here; nothing about rooms,
 * connections or identity does.
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
	out->pending = g->pending_garbage + g->pending_ability_garbage;
	out->effect_paralysis = g->effects.no_rotate_pieces;
	out->effect_inversion = g->effects.inverted_pieces;
	out->effect_nue = g->effects.no_fastdrop_pieces;
	out->effect_thwack = g->effects.thwack_pieces;
	out->effect_fry = g->effects.fry_rows;
	out->effect_dark = g->dark_pieces;
	out->effect_pals = g->pals_pieces;
	out->effect_mirror = g->effects.mirror_armed;
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
 * Thwack and Fry are applied here rather than inside board_clear_lines: they
 * are status effects on a player, and both must be read before
 * effect_on_piece_lock consumes their counters. Locking is also what gives the
 * hold slot back.
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
 * Nothing is taken away and nothing is scored here: the rows stay on the board
 * for clear_duration_ms, which is what the client animates.
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
 * Also reached with clearing_count 0 by a lock that completed nothing, which
 * still consumes its effects and still needs a piece.
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
	/*
	 * Fry's second half. The rows it filled burn off this floor here - that
	 * part always worked - and the count is taken before effect_on_piece_lock
	 * consumes it, because they are owed to somebody. room.c passes them on;
	 * a game does not know it has a Target.
	 */
	g->fry_owed += effect_fry_rows(&g->effects);
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
	if (cleared > 0)
		g->cleared_owed += cleared;
	effect_on_piece_lock(&g->effects);
	age_server_effects(g);
	g->clearing_count = 0;
	g->clearing_ms = 0;
	g->accum_ms = 0;
	drain_abilities(g);
	drain_garbage(g);
	spawn_next(g);
}

/**
 * @brief Applies the abilities that were aimed at this player.
 *
 * Runs before the garbage and before the next piece, so a board Sirtet
 * inverted takes its rows on top of the inversion. The queue is emptied
 * whether or not each entry did anything - an effect that cannot be applied is
 * spent, not held.
 *
 * @param g Game whose queue is being emptied.
 */
static void	drain_abilities(t_game *g)
{
	int	index;

	index = 0;
	while (index < g->pending_count)
	{
		if (g->pending[index].kind == PENDING_EFFECT)
		{
			effect_apply(&g->effects,
				(t_status_effect)g->pending[index].argument);
			/*
			 * Dark carries no counter of its own, so its length is set where
			 * it lands rather than where it was sent - the ageing below runs
			 * on the holder's locks, and arming it any earlier would spend a
			 * piece of it on the lock that delivered it.
			 */
			if (g->pending[index].argument == (int)EFFECT_DARK)
				g->dark_pieces = TETRISD_DARK_PIECES;
		}
		else if (g->pending[index].kind == PENDING_BOMB)
			apply_bomb(g);
		else if (g->pending[index].kind == PENDING_SIRTET)
			board_invert(&g->board);
		index++;
	}
	g->pending_count = 0;
}

/**
 * @brief Halloween L4 (Bomb): destroys scattered cells on this board.
 *
 * The cells walk from the game's own counters, the same trick garbage's hole
 * column uses, because libtetrisbrain owns no RNG on purpose.
 *
 * @param g Game whose board is bombed.
 */
static void	apply_bomb(t_game *g)
{
	int			cols[TETRISD_BOMB_CELLS];
	int			rows[TETRISD_BOMB_CELLS];
	uint32_t	walk;
	int			index;

	walk = (uint32_t)g->seq + g->garbage_seq * 31u + 17u;
	index = 0;
	while (index < TETRISD_BOMB_CELLS)
	{
		walk = walk * 1664525u + 1013904223u;
		cols[index] = (int)((walk >> 16) % (uint32_t)BOARD_WIDTH);
		rows[index] = (int)((walk >> 8) % (uint32_t)BOARD_HEIGHT);
		index++;
	}
	g->garbage_seq++;
	board_clear_cells(&g->board, cols, rows, TETRISD_BOMB_CELLS);
}

/**
 * @brief Queues one ability against this player, to land at their next lock.
 *
 * Only ability_ctrl.c calls this. A full queue drops its oldest entry rather
 * than refusing the newest, which is the one somebody just spent charge on. A
 * game that is over takes nothing.
 *
 * @param g Game the ability is aimed at.
 * @param kind Which transform it is.
 * @param argument The effect for PENDING_EFFECT, otherwise unused.
 */
void	game_queue_ability(t_game *g, t_pending_kind kind, int argument)
{
	int	index;

	if (g == NULL || kind == PENDING_NONE || !g->active || g->topped_out)
		return ;
	if (g->pending_count >= TD_MAX_PENDING)
	{
		index = 1;
		while (index < TD_MAX_PENDING)
		{
			g->pending[index - 1] = g->pending[index];
			index++;
		}
		g->pending_count = TD_MAX_PENDING - 1;
	}
	g->pending[g->pending_count].kind = kind;
	g->pending[g->pending_count].argument = argument;
	g->pending_count++;
}

/**
 * @brief Lands whatever garbage was owed, now that the board is nobody's.
 *
 * Its two neighbours are the whole reason this is a separate step: after the
 * clear resolves, so no clear is cancelled by somebody else's gift, and before
 * spawn_next, so the rows are part of the board the next piece is validated
 * against. The hole walks one column per row, from a counter this game owns.
 *
 * @param g Game whose queue is being emptied.
 */
static void	drain_garbage(t_game *g)
{
	int	ordinary;

	ordinary = g->pending_garbage;
	/*
	 * The landing is what credits an attacker, not the sending. Rows queued
	 * against a player who clears them away first were never a knockout, and
	 * rows that arrive after their sender has left the room still are one.
	 * Pals is deliberately not an exception: it turns the rows around, but
	 * they landed, and the flag it feeds is "somebody is attacking you".
	 */
	if (ordinary > 0 || g->pending_ability_garbage > 0)
	{
		g->landed_from = g->garbage_from;
		g->garbage_from = 0;
	}
	g->pending_garbage = 0;
	/*
	 * Pals turns the ordinary kind upside down: rows that would have raised
	 * the stack take the same number off the floor instead. Ability garbage is
	 * excluded by the ability text, which is the whole reason the two are
	 * counted separately - Pentaris lands on a player holding Pals exactly as
	 * it lands on one who is not.
	 */
	if (ordinary > 0 && g->effects.pals)
	{
		board_cut_bottom(&g->board, ordinary);
		ordinary = 0;
	}
	inject_rows(g, ordinary);
	inject_rows(g, g->pending_ability_garbage);
	g->pending_ability_garbage = 0;
}

/**
 * @brief Puts n garbage rows on the board, one at a time so the hole moves.
 *
 * @param g Game to raise.
 * @param lines How many rows; zero or fewer is a no-op.
 */
static void	inject_rows(t_game *g, int lines)
{
	if (lines > BOARD_HEIGHT)
		lines = BOARD_HEIGHT;
	while (lines > 0)
	{
		board_inject_garbage(&g->board, 1, next_garbage_hole(g));
		lines--;
	}
}

/**
 * @brief Draws the column the next garbage row leaves open.
 *
 * It was `garbage_seq % BOARD_WIDTH` off a counter incremented once per row,
 * which is a staircase and not a draw: every run of garbage left its holes on
 * columns 0, 1, 2, 3 in order, and a player taking ten rows got a diagonal
 * straight across the board. "The hole walks instead of stacking" was the
 * right worry answered by the wrong arithmetic - walking by exactly one column
 * is the most legible pattern there is, and the only reason it went unnoticed
 * is that it takes a Battle Royale's worth of garbage to see the shape.
 *
 * So the column is drawn, from the same LCG apply_bomb uses, and only the
 * column used last is excluded - which is what the original worry was actually
 * about. Drawing over BOARD_WIDTH - 1 and stepping past the previous hole
 * keeps every remaining column equally likely; a rejection loop would have
 * been the same distribution with a branch that can spin.
 *
 * The state is the game's own and seeded from the game's own seed, so a match
 * replayed from one seed lands the same rows in the same places, which is what
 * lets a test assert where a hole went at all. No randomness enters
 * libtetrisbrain: the column arrives there as an argument.
 *
 * @param g Game whose queue is being drained.
 * @return A column in [0, BOARD_WIDTH), never the one drawn immediately before.
 */
static int	next_garbage_hole(t_game *g)
{
	int	column;

	g->garbage_seq = g->garbage_seq * 1664525u + 1013904223u;
	if (g->garbage_hole < 0 || BOARD_WIDTH < 2)
		column = (int)((g->garbage_seq >> 16) % (uint32_t)BOARD_WIDTH);
	else
	{
		column = (int)((g->garbage_seq >> 16) % (uint32_t)(BOARD_WIDTH - 1));
		if (column >= g->garbage_hole)
			column++;
	}
	g->garbage_hole = column;
	return (column);
}

/**
 * @brief Ages the two effects libtetrisbrain leaves for the server to end.
 *
 * Dark and Pals have no counter of their own, so this is where "for a limited
 * time" is given a length - counted in the holder's own pieces.
 *
 * @param g Game whose server-timed effects are aged by one lock.
 */
static void	age_server_effects(t_game *g)
{
	if (g->dark_pieces > 0)
	{
		g->dark_pieces--;
		if (g->dark_pieces == 0)
			effect_clear(&g->effects, EFFECT_DARK);
	}
	if (g->pals_pieces > 0)
	{
		g->pals_pieces--;
		if (g->pals_pieces == 0)
			effect_clear(&g->effects, EFFECT_PALS);
	}
}

/**
 * @brief Queues garbage rows against this player, to land at their next lock.
 *
 * Only room.c calls this: a game does not know it has an opponent. A game that
 * is over takes nothing.
 *
 * The sender is carried rather than used. A board does not know who it is
 * playing and does not start now: the id is held until the lock that lands
 * the rows and handed back to room.c there, which is the only module that
 * knows what a player is.
 *
 * @param g Game the rows are owed to.
 * @param lines How many rows; zero or fewer is a no-op.
 * @param from The player who sent them.
 */
void	game_queue_garbage(t_game *g, int lines, t_player_id from)
{
	if (g == NULL || lines <= 0 || !g->active || g->topped_out)
		return ;
	g->garbage_from = from;
	g->pending_garbage += lines;
	if (g->pending_garbage > BOARD_HEIGHT)
		g->pending_garbage = BOARD_HEIGHT;
}

/**
 * @brief Queues garbage an ability sent, which Pals does not absorb.
 *
 * Counted apart from the ordinary kind because the ability text excludes
 * ability garbage from Pals; a single counter could not tell Pentaris from a
 * tetris.
 *
 * @param g Game the rows are owed to.
 * @param lines How many rows; zero or fewer is a no-op.
 * @param from The player who sent them.
 */
void	game_queue_ability_garbage(t_game *g, int lines, t_player_id from)
{
	if (g == NULL || lines <= 0 || !g->active || g->topped_out)
		return ;
	g->garbage_from = from;
	g->pending_ability_garbage += lines;
	if (g->pending_ability_garbage > BOARD_HEIGHT)
		g->pending_ability_garbage = BOARD_HEIGHT;
}

/**
 * @brief Takes the id of whoever last landed rows on this board.
 *
 * Taken rather than read: it is a one-shot report of something that happened
 * at the last lock, and a second reader would file the same attack twice.
 *
 * @param g Game to take from.
 * @return The player who sent the rows that landed, or 0 when none did.
 */
t_player_id	game_take_attacker(t_game *g)
{
	t_player_id	from;

	if (g == NULL)
		return (0);
	from = g->landed_from;
	g->landed_from = 0;
	return (from);
}

/**
 * @brief Takes the Fry rows this player burned and has not passed on.
 *
 * @param g Game to take from.
 * @return Rows burned since the last call.
 */
int	game_take_fry(t_game *g)
{
	int	rows;

	if (g == NULL)
		return (0);
	rows = g->fry_owed;
	g->fry_owed = 0;
	return (rows);
}

/**
 * @brief Takes the lines this game has cleared and not yet been credited for.
 *
 * Reading clears the count, so a clear is charged to the Target exactly once
 * however many ticks pass before anybody asks.
 *
 * @param g Game to take from.
 * @return Lines cleared since the last call.
 */
int	game_take_cleared(t_game *g)
{
	int	cleared;

	if (g == NULL)
		return (0);
	cleared = g->cleared_owed;
	g->cleared_owed = 0;
	return (cleared);
}

/**
 * @brief Charges elapsed time against a clear in progress.
 *
 * Every tick reports a change even when only the millisecond count moved,
 * because that count is what the client animates.
 *
 * @param g Game holding completed rows.
 * @param remaining_ms Time still unspent; reduced by the clear's portion.
 * @return true, always - a clear in progress always owes a snapshot.
 */
static bool	advance_clear(t_game *g, int *remaining_ms)
{
	int	duration_ms;
	int	step_ms;

	duration_ms = clear_duration_ms(g->level);
	step_ms = duration_ms - g->clearing_ms;
	if (step_ms > *remaining_ms)
		step_ms = *remaining_ms;
	if (step_ms < 0)
		step_ms = 0;
	g->clearing_ms += step_ms;
	*remaining_ms -= step_ms;
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
 * @brief Advances one falling or lock-down boundary from the time budget.
 *
 * Only the milliseconds needed to reach the next event are consumed; the
 * caller loops with whatever remains.
 *
 * @param g Game whose active piece is advancing.
 * @param remaining_ms Time still unspent; reduced by this step.
 * @return true when the piece moved or locked.
 */
static bool	advance_active(t_game *g, int *remaining_ms)
{
	int	step_ms;
	int	until_event_ms;

	if (lockdown_grounded(&g->board, &g->piece))
	{
		until_event_ms = LOCKDOWN_DELAY_MS - g->lockdown.elapsed_ms;
		if (until_event_ms < 0)
			until_event_ms = 0;
		step_ms = until_event_ms;
		if (step_ms > *remaining_ms)
			step_ms = *remaining_ms;
		*remaining_ms -= step_ms;
		if (!lockdown_tick(&g->lockdown, true, step_ms))
			return (false);
		lock_piece(g);
		return (true);
	}
	until_event_ms = step_interval(g) - g->accum_ms;
	if (until_event_ms < 0)
		until_event_ms = 0;
	step_ms = until_event_ms;
	if (step_ms > *remaining_ms)
		step_ms = *remaining_ms;
	g->accum_ms += step_ms;
	*remaining_ms -= step_ms;
	if (g->accum_ms < step_interval(g))
		return (false);
	g->accum_ms -= step_interval(g);
	if (gravity_tick(&g->board, &g->piece) != BRAIN_OK)
	{
		g->accum_ms = 0;
		return (false);
	}
	lockdown_on_fall(&g->lockdown, &g->piece);
	return (true);
}
