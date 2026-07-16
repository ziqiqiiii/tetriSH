#include "tetrisu.h"

static bool	piece_is_grounded(const solo_game_t *game)
{
	t_piece	probe;

	probe = game->active;
	return (piece_move(&game->board, &probe, 0, 1) == BRAIN_BLOCKED);
}

static void	reset_piece_timers(solo_game_t *game)
{
	game->gravity_elapsed_ms = 0;
	game->lock_elapsed_ms = 0;
	game->lock_resets = 0;
	game->last_kick_index = -1;
	game->last_action_was_rotation = false;
}

/* AI-assisted: this 20-row local board has no hidden spawn rows, so locking
 * any occupied cell into row 0 is the Solo mode's immediate top-out rule. */
static bool	piece_touches_top(const t_piece *piece)
{
	int	cols[4];
	int	rows[4];
	int	index;

	if (!piece_cells(piece, cols, rows))
		return (false);
	index = 0;
	while (index < 4)
	{
		if (rows[index] == 0)
			return (true);
		index++;
	}
	return (false);
}

static void	spawn_queued_piece(solo_game_t *game)
{
	t_piece_type	type;
	int				index;

	type = game->next[0];
	index = 0;
	while (index < SOLO_NEXT_COUNT - 1)
	{
		game->next[index] = game->next[index + 1];
		index++;
	}
	game->next[SOLO_NEXT_COUNT - 1] = piece_bag_next(&game->bag);
	game->active = piece_spawn(type);
	reset_piece_timers(game);
	game->phase = SOLO_ACTIVE;
	if (!piece_is_valid(&game->board, &game->active))
		game->phase = SOLO_GAME_OVER;
}

static void	remember_score_event(solo_game_t *game, int lines,
	t_spin_type spin, bool perfect_clear)
{
	game->last_score = score_apply_clear(&game->scoring, lines, game->level,
		spin, perfect_clear);
	game->last_lines = lines;
	game->last_spin = spin;
	game->last_perfect_clear = perfect_clear;
}

static void	finish_line_clear(solo_game_t *game)
{
	bool	perfect_clear;

	board_clear_lines(&game->board);
	perfect_clear = board_is_empty(&game->board);
	remember_score_event(game, game->clear_count, game->pending_spin,
		perfect_clear);
	game->total_lines += game->clear_count;
	game->level = level_from_lines(game->total_lines);
	game->crystal_charge += game->clear_count;
	if (game->crystal_charge > SOLO_CRYSTAL_CAPACITY)
		game->crystal_charge = SOLO_CRYSTAL_CAPACITY;
	game->clear_count = 0;
	game->clear_elapsed_ms = 0;
	spawn_queued_piece(game);
}

/* AI-assisted: locks, classifies, and stages the clear without sleeping;
 * render_solo flashes the saved rows while update() advances the 200 ms phase. */
static void	lock_active_piece(solo_game_t *game)
{
	t_spin_type	spin;

	spin = T_SPIN_NONE;
	if (game->last_action_was_rotation)
		spin = piece_t_spin_type(&game->board, &game->active,
			game->last_kick_index);
	piece_stamp(&game->board, &game->active);
	if (piece_touches_top(&game->active))
	{
		game->phase = SOLO_GAME_OVER;
		return ;
	}
	game->clear_count = board_find_full_lines(&game->board, game->clear_rows);
	if (game->clear_count > 0)
	{
		game->pending_spin = spin;
		game->clear_elapsed_ms = 0;
		game->phase = SOLO_CLEARING;
		return ;
	}
	remember_score_event(game, 0, spin, false);
	spawn_queued_piece(game);
}

void	solo_game_init(solo_game_t *game, uint32_t seed)
{
	int	index;

	memset(game, 0, sizeof(*game));
	board_init(&game->board);
	piece_bag_init(&game->bag, seed);
	score_state_init(&game->scoring);
	game->level = 1;
	game->active = piece_spawn(piece_bag_next(&game->bag));
	index = 0;
	while (index < SOLO_NEXT_COUNT)
	{
		game->next[index] = piece_bag_next(&game->bag);
		index++;
	}
	reset_piece_timers(game);
	game->phase = SOLO_ACTIVE;
}

static void	reset_lock_after_move(solo_game_t *game, bool was_grounded)
{
	if (was_grounded && game->lock_resets < SOLO_LOCK_RESET_LIMIT)
	{
		game->lock_elapsed_ms = 0;
		game->lock_resets++;
	}
}

static bool	apply_shift(solo_game_t *game, int direction)
{
	bool	was_grounded;

	was_grounded = piece_is_grounded(game);
	if (piece_move(&game->board, &game->active, direction, 0) != BRAIN_OK)
		return (false);
	reset_lock_after_move(game, was_grounded);
	game->last_action_was_rotation = false;
	return (true);
}

static bool	apply_rotation(solo_game_t *game, int direction)
{
	bool	was_grounded;
	int		kick_index;

	was_grounded = piece_is_grounded(game);
	if (piece_rotate_with_kick(&game->board, &game->active, direction,
			&kick_index) != BRAIN_OK)
		return (false);
	reset_lock_after_move(game, was_grounded);
	game->last_action_was_rotation = true;
	game->last_kick_index = kick_index;
	return (true);
}

bool	solo_game_apply_action(solo_game_t *game, solo_action_t action)
{
	int	distance;

	if (game->paused || game->phase != SOLO_ACTIVE)
		return (false);
	if (action == SOLO_MOVE_LEFT)
		return (apply_shift(game, -1));
	if (action == SOLO_MOVE_RIGHT)
		return (apply_shift(game, 1));
	if (action == SOLO_ROTATE_CW)
		return (apply_rotation(game, 1));
	if (action == SOLO_ROTATE_CCW)
		return (apply_rotation(game, -1));
	if (action == SOLO_SOFT_DROP)
	{
		if (piece_soft_drop(&game->board, &game->active) != BRAIN_OK)
			return (false);
		score_add_drop(&game->scoring, 1, false);
		game->lock_elapsed_ms = 0;
		game->last_action_was_rotation = false;
		return (true);
	}
	if (action != SOLO_HARD_DROP)
		return (false);
	distance = piece_drop_distance(&game->board, &game->active);
	piece_hard_drop(&game->board, &game->active);
	score_add_drop(&game->scoring, distance, true);
	if (distance > 0)
		game->last_action_was_rotation = false;
	lock_active_piece(game);
	return (true);
}

static int	min_int(int left, int right)
{
	if (left < right)
		return (left);
	return (right);
}

/* AI-assisted: advances to one timer boundary at a time. Time spent falling
 * is never charged to lock delay, so a piece that lands on this update still
 * receives the full Guideline lock window. */
static bool	advance_active(solo_game_t *game, int *remaining_ms)
{
	int	gravity_ms;
	int	to_gravity;
	int	to_lock;
	int	step;
	bool	grounded;
	bool	changed;
	bool	event;

	changed = false;
	gravity_ms = gravity_interval_ms(game->level);
	if (gravity_ms == 0 && !piece_is_grounded(game))
	{
		piece_hard_drop(&game->board, &game->active);
		changed = true;
	}
	while (*remaining_ms > 0 && game->phase == SOLO_ACTIVE)
	{
		grounded = piece_is_grounded(game);
		to_gravity = INT32_MAX;
		if (!grounded && gravity_ms > 0)
			to_gravity = gravity_ms - game->gravity_elapsed_ms;
		to_lock = INT32_MAX;
		if (grounded)
			to_lock = SOLO_LOCK_DELAY_MS - game->lock_elapsed_ms;
		step = min_int(*remaining_ms, min_int(to_gravity, to_lock));
		if (step < 0)
			step = 0;
		if (!grounded)
			game->gravity_elapsed_ms += step;
		if (grounded)
			game->lock_elapsed_ms += step;
		else
			game->lock_elapsed_ms = 0;
		*remaining_ms -= step;
		event = false;
		if (grounded && game->lock_elapsed_ms >= SOLO_LOCK_DELAY_MS)
		{
			lock_active_piece(game);
			changed = true;
			event = true;
		}
		if (game->phase == SOLO_ACTIVE && !grounded && gravity_ms > 0
			&& game->gravity_elapsed_ms >= gravity_ms)
		{
			game->gravity_elapsed_ms -= gravity_ms;
			if (gravity_tick(&game->board, &game->active) == BRAIN_OK)
			{
				game->lock_elapsed_ms = 0;
				changed = true;
			}
			event = true;
		}
		if (step == 0 && !event)
			break ;
	}
	return (changed);
}

static bool	advance_clearing(solo_game_t *game, int *remaining_ms)
{
	int	target;
	int	step;

	target = SOLO_CLEAR_ANIMATION_MS;
	if (game->clear_elapsed_ms < SOLO_CLEAR_ANIMATION_MS / 2)
		target = SOLO_CLEAR_ANIMATION_MS / 2;
	step = min_int(*remaining_ms, target - game->clear_elapsed_ms);
	if (step < 0)
		step = 0;
	game->clear_elapsed_ms += step;
	*remaining_ms -= step;
	if (game->clear_elapsed_ms >= SOLO_CLEAR_ANIMATION_MS)
	{
		finish_line_clear(game);
		return (true);
	}
	return (game->clear_elapsed_ms == SOLO_CLEAR_ANIMATION_MS / 2);
}

/* AI-assisted: a monotonic deadline can expire within the same integer
 * millisecond as the previous sample. Consume already-due timer boundaries
 * even when update() receives zero elapsed time, preventing a busy loop. */
static bool	process_due_event(solo_game_t *game)
{
	int	gravity_ms;

	if (game->phase == SOLO_CLEARING)
	{
		if (game->clear_elapsed_ms >= SOLO_CLEAR_ANIMATION_MS)
		{
			finish_line_clear(game);
			return (true);
		}
		return (false);
	}
	if (game->phase != SOLO_ACTIVE)
		return (false);
	if (piece_is_grounded(game)
		&& game->lock_elapsed_ms >= SOLO_LOCK_DELAY_MS)
	{
		lock_active_piece(game);
		return (true);
	}
	if (piece_is_grounded(game))
		return (false);
	gravity_ms = gravity_interval_ms(game->level);
	if (gravity_ms == 0)
	{
		piece_hard_drop(&game->board, &game->active);
		return (true);
	}
	if (gravity_ms > 0 && game->gravity_elapsed_ms >= gravity_ms)
	{
		game->gravity_elapsed_ms -= gravity_ms;
		if (gravity_tick(&game->board, &game->active) == BRAIN_OK)
			game->lock_elapsed_ms = 0;
		return (true);
	}
	return (false);
}

bool	solo_game_update(solo_game_t *game, int elapsed_ms)
{
	int		remaining_ms;
	int		due_events;
	bool	changed;

	if (elapsed_ms < 0 || game->paused || game->phase == SOLO_GAME_OVER)
		return (false);
	remaining_ms = elapsed_ms;
	changed = false;
	due_events = 0;
	while (due_events < 64 && process_due_event(game))
	{
		changed = true;
		due_events++;
	}
	while (remaining_ms > 0 && game->phase != SOLO_GAME_OVER)
	{
		if (game->phase == SOLO_CLEARING)
			changed = advance_clearing(game, &remaining_ms) || changed;
		else
			changed = advance_active(game, &remaining_ms) || changed;
		due_events = 0;
		while (due_events < 64 && process_due_event(game))
		{
			changed = true;
			due_events++;
		}
	}
	return (changed);
}

int	solo_game_next_wake_ms(const solo_game_t *game)
{
	int	gravity_ms;
	int	wake_ms;
	int	lock_ms;

	if (game->paused || game->phase == SOLO_GAME_OVER)
		return (-1);
	if (game->phase == SOLO_CLEARING)
	{
		if (game->clear_elapsed_ms < SOLO_CLEAR_ANIMATION_MS / 2)
			return (SOLO_CLEAR_ANIMATION_MS / 2 - game->clear_elapsed_ms);
		return (SOLO_CLEAR_ANIMATION_MS - game->clear_elapsed_ms);
	}
	if (piece_is_grounded(game))
	{
		lock_ms = SOLO_LOCK_DELAY_MS - game->lock_elapsed_ms;
		if (lock_ms < 0)
			return (0);
		return (lock_ms);
	}
	gravity_ms = gravity_interval_ms(game->level);
	if (gravity_ms == 0)
		return (0);
	wake_ms = INT32_MAX;
	if (gravity_ms > 0)
		wake_ms = gravity_ms - game->gravity_elapsed_ms;
	if (wake_ms == INT32_MAX)
		return (-1);
	if (wake_ms < 0)
		return (0);
	return (wake_ms);
}

t_piece	solo_game_ghost(const solo_game_t *game)
{
	t_piece	ghost;

	ghost = game->active;
	ghost.row += piece_drop_distance(&game->board, &ghost);
	return (ghost);
}

bool	solo_game_row_is_clearing(const solo_game_t *game, int row)
{
	int	index;

	if (game->phase != SOLO_CLEARING)
		return (false);
	index = 0;
	while (index < game->clear_count)
	{
		if (game->clear_rows[index] == row)
			return (true);
		index++;
	}
	return (false);
}

void	solo_game_toggle_pause(solo_game_t *game)
{
	if (game->phase != SOLO_GAME_OVER)
		game->paused = !game->paused;
}
