#include "tetrisu.h"

// Static Functions
static void	reset_piece_timers(t_solo_game *game);
static bool	apply_shift(t_solo_game *game, int direction);
static bool	piece_is_grounded(const t_solo_game *game);
static void	reset_lock_after_move(t_solo_game *game, bool was_grounded);
static bool	apply_rotation(t_solo_game *game, int direction);
static bool	apply_hold(t_solo_game *game);
static void	lock_active_piece(t_solo_game *game);
static bool	piece_touches_top(const t_piece *piece);
static void	begin_top_out_reveal(t_solo_game *game);
static void	remember_score_event(t_solo_game *game, int lines, t_spin_type spin, bool perfect_clear);
static void	activate_piece(t_solo_game *game, t_piece_type type);
static void	spawn_queued_piece(t_solo_game *game);
static bool	process_due_event(t_solo_game *game);
static void	finish_line_clear(t_solo_game *game);
static bool	advance_ability_feedback(t_solo_game *game, int elapsed_ms);
static bool	advance_personal_best(t_solo_game *game, int elapsed_ms);
static bool	advance_event_animations(t_solo_game *game, int elapsed_ms);
static bool	advance_danger_presentation(t_solo_game *game, int elapsed_ms);
static bool	advance_timed_animation(int *elapsed_ms, bool *active,
				int duration_ms, int step_ms);
static unsigned	pulse_fade_opacity(int elapsed_ms, int pulse_ms, int fade_ms);
static int	animation_wake_ms(int wake_ms, bool active, int remaining_ms);
static int	gameplay_next_wake_ms(const t_solo_game *game);
static bool	advance_top_out_reveal(t_solo_game *game, int *remaining_ms);
static int	min_int(int left, int right);
static bool	advance_clearing(t_solo_game *game, int *remaining_ms);
static bool	advance_active(t_solo_game *game, int *remaining_ms);

/**
 * @brief Initializes a new endless Solo game.
 *
 * The board, seven-bag, score state, timers, active piece, and three-piece
 *   preview are reset together.
 *
 * @param game Pointer to the Solo state to initialize.
 * @param seed Deterministic seed for the seven-bag generator.
 */
void	solo_game_init(t_solo_game *game, uint32_t seed)
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

/**
 * @brief Applies one player action to the active piece.
 *
 * Actions are ignored while paused or outside the active phase; hard drop
 *   locks immediately and awards drop points.
 *
 * @param game Pointer to the Solo state.
 * @param action Requested movement, rotation, or drop.
 * @return true when the action changed game state, otherwise false.
 */
bool	solo_game_apply_action(t_solo_game *game, t_solo_action action)
{
	int	distance;

	if (game->paused || game->countdown_active || game->phase != SOLO_ACTIVE)
		return (false);
	if (action == SOLO_MOVE_LEFT)
		return (apply_shift(game, -1));
	if (action == SOLO_MOVE_RIGHT)
		return (apply_shift(game, 1));
	if (action == SOLO_ROTATE_CW)
		return (apply_rotation(game, 1));
	if (action == SOLO_ROTATE_CCW)
		return (apply_rotation(game, -1));
	if (action == SOLO_HOLD)
		return (apply_hold(game));
	if (action == SOLO_SOFT_DROP)
	{
		if (piece_soft_drop(&game->board, &game->active) != BRAIN_OK)
			return (false);
		game->pending_events |= SOLO_EVENT_SOFT_DROP;
		score_add_drop(&game->scoring, 1, false);
		game->gravity_elapsed_ms = 0;
		game->lock_elapsed_ms = 0;
		game->last_action_was_rotation = false;
		return (true);
	}
	if (action != SOLO_HARD_DROP)
		return (false);
	distance = piece_drop_distance(&game->board, &game->active);
	piece_hard_drop(&game->board, &game->active);
	game->pending_events |= SOLO_EVENT_HARD_DROP;
	score_add_drop(&game->scoring, distance, true);
	if (distance > 0)
		game->last_action_was_rotation = false;
	lock_active_piece(game);
	return (true);
}

/**
 * @brief Advances Solo timers by a bounded elapsed interval.
 *
 * Due gravity, lock, clear, and top-out boundaries are consumed without
 *   skipping the visible top-out reveal.
 *
 * @param game Pointer to the Solo state.
 * @param elapsed_ms Elapsed monotonic time in milliseconds.
 * @return true when visible game state changed, otherwise false.
 */
bool	solo_game_update(t_solo_game *game, int elapsed_ms)
{
	int		remaining_ms;
	int		due_events;
	bool	changed;
	bool	started_in_countdown;
	bool	started_in_reveal;

	if (elapsed_ms < 0)
		return (false);
	remaining_ms = elapsed_ms;
	started_in_countdown = game->countdown_active;
	changed = solo_game_update_presentation(game, elapsed_ms);
	if (started_in_countdown)
		return (changed);
	if (game->paused)
		return (changed);
	if (game->phase == SOLO_GAME_OVER)
		return (changed);
	started_in_reveal = game->phase == SOLO_TOP_OUT_REVEAL;
	due_events = 0;
	while (due_events < 64 && process_due_event(game))
	{
		changed = true;
		due_events++;
	}
	/* Do not spend catch-up time on a reveal that began inside this update.
	 * Returning now guarantees the final board reaches the terminal once. */
	if (!started_in_reveal && game->phase == SOLO_TOP_OUT_REVEAL)
		return (true);
	while (remaining_ms > 0 && game->phase != SOLO_GAME_OVER)
	{
		if (game->phase == SOLO_TOP_OUT_REVEAL)
			changed = advance_top_out_reveal(game, &remaining_ms) || changed;
		else if (game->phase == SOLO_CLEARING)
			changed = advance_clearing(game, &remaining_ms) || changed;
		else
			changed = advance_active(game, &remaining_ms) || changed;
		if (!started_in_reveal && game->phase == SOLO_TOP_OUT_REVEAL)
			return (true);
		due_events = 0;
		while (due_events < 64 && process_due_event(game))
		{
			changed = true;
			due_events++;
		}
		if (!started_in_reveal && game->phase == SOLO_TOP_OUT_REVEAL)
			return (true);
	}
	return (changed);
}

/**
 * @brief Advances only the timers that belong to the client either way.
 *
 * The countdown, the ability feedback, the personal-best banner and the
 * danger fade are presentation: the server has no opinion about any of them
 * and never sends them. Online they are the *only* thing that ticks, which is
 * why they are reachable without the rules that follow them in
 * solo_game_update - an online game that skipped this would hold its 3-2-1 on
 * screen forever and never accept an input.
 *
 * @param game Pointer to the Solo state.
 * @param elapsed_ms Elapsed monotonic time in milliseconds.
 * @return true when a visible animation may have changed.
 */
bool	solo_game_update_presentation(t_solo_game *game, int elapsed_ms)
{
	bool	changed;

	if (game == NULL || elapsed_ms < 0)
		return (false);
	changed = advance_ability_feedback(game, elapsed_ms);
	changed = advance_personal_best(game, elapsed_ms) || changed;
	changed = advance_event_animations(game, elapsed_ms) || changed;
	changed = advance_danger_presentation(game, elapsed_ms) || changed;
	return (changed);
}

/**
 * @brief Updates the high-stack danger state with exit hysteresis.
 *
 * Settled blocks in rows 0–3 enter danger immediately. Danger remains active
 * until every settled block stays below row 4 for 1.5 seconds, avoiding rapid
 * music changes while the stack moves around the threshold.
 *
 * @param game Pointer to the Solo state.
 * @param elapsed_ms Elapsed active-play time in milliseconds.
 * @return true only when danger was entered or exited.
 */
bool	solo_game_update_danger(t_solo_game *game, int elapsed_ms)
{
	int	highest_row;
	int	row;
	int	col;

	if (game == NULL || elapsed_ms < 0)
		return (false);
	highest_row = BOARD_HEIGHT;
	row = 0;
	while (row < BOARD_HEIGHT && highest_row == BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (board_get(&game->board, col, row).type != CELL_EMPTY)
			{
				highest_row = row;
				break ;
			}
			col++;
		}
		row++;
	}
	if (highest_row <= SOLO_DANGER_ENTER_ROW)
	{
		game->danger_safe_elapsed_ms = 0;
		if (game->danger_active)
			return (false);
		game->danger_active = true;
		return (true);
	}
	if (!game->danger_active)
	{
		game->danger_safe_elapsed_ms = 0;
		return (false);
	}
	if (highest_row < SOLO_DANGER_EXIT_ROW)
	{
		game->danger_safe_elapsed_ms = 0;
		return (false);
	}
	if (game->paused)
		return (false);
	if (elapsed_ms > SOLO_DANGER_EXIT_HOLD_MS
		- game->danger_safe_elapsed_ms)
		game->danger_safe_elapsed_ms = SOLO_DANGER_EXIT_HOLD_MS;
	else
		game->danger_safe_elapsed_ms += elapsed_ms;
	if (game->danger_safe_elapsed_ms < SOLO_DANGER_EXIT_HOLD_MS)
		return (false);
	game->danger_safe_elapsed_ms = 0;
	game->danger_active = false;
	return (true);
}

/**
 * @brief Returns the current no-glow environment dim strength.
 *
 * The board renderer excludes the playfield from this tint, so the value only
 * darkens the surrounding authored environment.
 *
 * @param game Pointer to the Solo state.
 * @return Black-mix strength from 0 through SOLO_DANGER_DIM_MAX.
 */
unsigned	solo_game_danger_dim(const t_solo_game *game)
{
	if (game == NULL || game->danger_fade_elapsed_ms <= 0)
		return (0);
	if (game->danger_fade_elapsed_ms >= SOLO_DANGER_FADE_MS)
		return (SOLO_DANGER_DIM_MAX);
	return ((unsigned)(SOLO_DANGER_DIM_MAX
			* game->danger_fade_elapsed_ms / SOLO_DANGER_FADE_MS));
}

/**
 * @brief Returns and clears gameplay events accumulated since the last frame.
 *
 * @param game Pointer to the Solo state.
 * @return Bitmask of t_solo_event values.
 */
uint32_t	solo_game_take_events(t_solo_game *game)
{
	uint32_t	events;

	if (game == NULL)
		return (0);
	events = game->pending_events;
	game->pending_events = 0;
	return (events);
}

/**
 * @brief Copies the locally loaded best score into a fresh Solo game.
 *
 * @param game Pointer to the Solo state.
 * @param score Previously persisted best score.
 */
void	solo_game_set_personal_best(t_solo_game *game, uint64_t score)
{
	if (game != NULL)
		game->personal_best = score;
}

/**
 * @brief Finalizes one completed top-out score against the local best.
 *
 * The one-shot guard ensures the same game-over frame can be processed more
 *   than once without rewriting state or replaying its achievement cue.
 *
 * @param game Pointer to the Solo state.
 * @return true only when this completed game established a new best.
 */
bool	solo_game_finish_personal_best(t_solo_game *game)
{
	if (game == NULL || game->phase != SOLO_GAME_OVER
		|| game->personal_best_checked)
		return (false);
	game->personal_best_checked = true;
	if (game->scoring.total <= game->personal_best)
		return (false);
	game->personal_best = game->scoring.total;
	game->new_personal_best = true;
	game->personal_best_elapsed_ms = 0;
	game->pending_events |= SOLO_EVENT_PERSONAL_BEST;
	return (true);
}

/**
 * @brief Calculates the pulsing then fading best-score banner opacity.
 *
 * @param game Pointer to the Solo state.
 * @return Alpha from 0 through 255.
 */
unsigned	solo_game_personal_best_opacity(const t_solo_game *game)
{
	int	elapsed;
	int	phase;
	int	remaining;

	if (game == NULL || !game->new_personal_best)
		return (0);
	elapsed = game->personal_best_elapsed_ms;
	if (elapsed >= SOLO_PERSONAL_BEST_PULSE_MS)
	{
		remaining = SOLO_PERSONAL_BEST_PULSE_MS
			+ SOLO_PERSONAL_BEST_FADE_MS - elapsed;
		if (remaining <= 0)
			return (0);
		return ((unsigned)(255 * remaining / SOLO_PERSONAL_BEST_FADE_MS));
	}
	phase = elapsed % (SOLO_PERSONAL_BEST_PULSE_MS / 2);
	if (phase > SOLO_PERSONAL_BEST_PULSE_MS / 4)
		phase = SOLO_PERSONAL_BEST_PULSE_MS / 2 - phase;
	return ((unsigned)(255 - 60 * phase / (SOLO_PERSONAL_BEST_PULSE_MS / 4)));
}

/**
 * @brief Starts the four-step 3, 2, 1, GO presentation before Solo input.
 */
void	solo_game_start_countdown(t_solo_game *game)
{
	if (game == NULL || game->phase != SOLO_ACTIVE)
		return ;
	game->countdown_elapsed_ms = 0;
	game->countdown_active = true;
	game->pending_events |= SOLO_EVENT_COUNTDOWN_TICK;
	reset_piece_timers(game);
}

/**
 * @brief Returns 3, 2, 1, or 0 for GO while the countdown is visible.
 */
int	solo_game_countdown_value(const t_solo_game *game)
{
	int	stage;

	if (game == NULL || !game->countdown_active)
		return (-1);
	stage = game->countdown_elapsed_ms / SOLO_COUNTDOWN_STEP_MS;
	if (stage < 0 || stage >= SOLO_COUNTDOWN_STEPS)
		return (-1);
	if (stage == SOLO_COUNTDOWN_STEPS - 1)
		return (0);
	return (3 - stage);
}

/**
 * @brief Returns one countdown label's restrained hold-and-fade opacity.
 */
unsigned	solo_game_countdown_opacity(const t_solo_game *game)
{
	int	step_elapsed;
	int	fade_start;

	if (solo_game_countdown_value(game) < 0)
		return (0);
	step_elapsed = game->countdown_elapsed_ms % SOLO_COUNTDOWN_STEP_MS;
	fade_start = SOLO_COUNTDOWN_STEP_MS / 2;
	if (step_elapsed <= fade_start)
		return ((unsigned)(255 - 55 * step_elapsed / fade_start));
	return ((unsigned)(200 * (SOLO_COUNTDOWN_STEP_MS - step_elapsed)
			/ (SOLO_COUNTDOWN_STEP_MS - fade_start)));
}

/**
 * @brief Returns the active line-clear score-label pulse/fade opacity.
 */
unsigned	solo_game_score_event_opacity(const t_solo_game *game)
{
	if (game == NULL || !game->score_event_active)
		return (0);
	return (pulse_fade_opacity(game->score_event_elapsed_ms,
			SOLO_SCORE_EVENT_PULSE_MS, SOLO_SCORE_EVENT_FADE_MS));
}

/**
 * @brief Returns the newly affordable ability-marker pulse strength.
 */
unsigned	solo_game_ability_ready_opacity(const t_solo_game *game)
{
	if (game == NULL || !game->ability_ready_active)
		return (0);
	return (pulse_fade_opacity(game->ability_ready_elapsed_ms,
			SOLO_ABILITY_READY_PULSE_MS, SOLO_ABILITY_READY_FADE_MS));
}

/**
 * @brief Returns activation/rejection feedback pulse/fade opacity.
 */
unsigned	solo_game_ability_result_opacity(const t_solo_game *game)
{
	int	pulse_ms;

	if (game == NULL || game->ability_result == SOLO_ABILITY_RESULT_NONE)
		return (0);
	pulse_ms = SOLO_ABILITY_FEEDBACK_MS - SOLO_ABILITY_RESULT_FADE_MS;
	return (pulse_fade_opacity(game->ability_feedback_elapsed_ms,
			pulse_ms, SOLO_ABILITY_RESULT_FADE_MS));
}

/**
 * @brief Calculates the next state-timer deadline.
 *
 * The renderer can sleep until this value instead of polling gravity or lock
 *   state continuously.
 *
 * @param game Pointer to the Solo state.
 * @return Milliseconds until the next deadline, 0 when due, or -1 when no
 *   timer is active.
 */
int	solo_game_next_wake_ms(const t_solo_game *game)
{
	int	feedback_ms;
	int	personal_best_ms;
	int	wake_ms;

	wake_ms = -1;
	if (!game->paused)
		wake_ms = gameplay_next_wake_ms(game);
	if (game->ability_result == SOLO_ABILITY_RESULT_NONE)
		feedback_ms = -1;
	else
		feedback_ms = SOLO_ABILITY_FEEDBACK_MS
			- game->ability_feedback_elapsed_ms;
	wake_ms = animation_wake_ms(wake_ms, feedback_ms > 0, feedback_ms);
	personal_best_ms = SOLO_PERSONAL_BEST_PULSE_MS
		+ SOLO_PERSONAL_BEST_FADE_MS - game->personal_best_elapsed_ms;
	wake_ms = animation_wake_ms(wake_ms, game->new_personal_best,
			personal_best_ms);
	wake_ms = animation_wake_ms(wake_ms, game->score_event_active,
			SOLO_SCORE_EVENT_PULSE_MS + SOLO_SCORE_EVENT_FADE_MS
			- game->score_event_elapsed_ms);
	wake_ms = animation_wake_ms(wake_ms, game->ability_ready_active,
			SOLO_ABILITY_READY_PULSE_MS + SOLO_ABILITY_READY_FADE_MS
			- game->ability_ready_elapsed_ms);
	wake_ms = animation_wake_ms(wake_ms, game->countdown_active,
			SOLO_COUNTDOWN_STEP_MS * SOLO_COUNTDOWN_STEPS
			- game->countdown_elapsed_ms);
	if (game->danger_active)
		wake_ms = animation_wake_ms(wake_ms,
				game->danger_fade_elapsed_ms < SOLO_DANGER_FADE_MS,
				SOLO_DANGER_FADE_MS - game->danger_fade_elapsed_ms);
	else
		wake_ms = animation_wake_ms(wake_ms,
				game->danger_fade_elapsed_ms > 0,
				game->danger_fade_elapsed_ms);
	return (wake_ms);
}

/**
 * @brief Calculates only the gravity, lock, clear, or top-out deadline.
 *
 * Keeping this separate lets short ability feedback share the same sleeping
 * input loop without changing gameplay timer semantics.
 *
 * @param game Pointer to the current Solo state.
 * @return Milliseconds until the next gameplay deadline, or -1 when idle.
 */
static int	gameplay_next_wake_ms(const t_solo_game *game)
{
	int	clear_ms;
	int	gravity_ms;
	int	wake_ms;
	int	lock_ms;

	if (game->phase == SOLO_GAME_OVER)
		return (-1);
	if (game->phase == SOLO_TOP_OUT_REVEAL)
	{
		wake_ms = SOLO_TOP_OUT_REVEAL_MS - game->top_out_elapsed_ms;
		if (wake_ms < 0)
			return (0);
		return (wake_ms);
	}
	if (game->phase == SOLO_CLEARING)
	{
		clear_ms = solo_clear_duration_ms(game->level);
		if (game->clear_elapsed_ms < clear_ms / 2)
			return (clear_ms / 2 - game->clear_elapsed_ms);
		return (clear_ms - game->clear_elapsed_ms);
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

/**
 * @brief Returns level-aware line-clear animation duration.
 *
 * @param level Current Solo level.
 * @return Total two-frame animation duration in milliseconds.
 */
int	solo_clear_duration_ms(int level)
{
	return (clear_duration_ms(level));
}

/**
 * @brief Calculates the active piece's landing projection.
 *
 * The returned copy is translated by the current unobstructed hard-drop
 *   distance.
 *
 * @param game Pointer to the Solo state.
 * @return Ghost piece positioned at its landing row.
 */
t_piece	solo_game_ghost(const t_solo_game *game)
{
	t_piece	ghost;

	ghost = game->active;
	ghost.row += piece_drop_distance(&game->board, &ghost);
	return (ghost);
}

/**
 * @brief Checks whether a row participates in the current clear animation.
 *
 * Rows are reported only during the clearing phase and are matched against the
 *   saved clear list.
 *
 * @param game Pointer to the Solo state.
 * @param row Board row to query.
 * @return true when the row is clearing, otherwise false.
 */
bool	solo_game_row_is_clearing(const t_solo_game *game, int row)
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

/**
 * @brief Toggles pause during playable animation phases.
 *
 * Top-out reveal and game-over phases cannot be paused.
 *
 * @param game Pointer to the Solo state.
 */
void	solo_game_toggle_pause(t_solo_game *game)
{
	if (!game->countdown_active
		&& (game->phase == SOLO_ACTIVE || game->phase == SOLO_CLEARING))
	{
		game->paused = !game->paused;
		game->pending_events |= SOLO_EVENT_PAUSE;
	}
}

/**
 * @brief Resets per-piece gravity and lock bookkeeping.
 *
 * Every newly spawned piece receives fresh gravity, lock-delay, kick, and
 *   action state.
 *
 * @param game Pointer to the Solo state.
 */
static void	reset_piece_timers(t_solo_game *game)
{
	game->gravity_elapsed_ms = 0;
	game->lock_elapsed_ms = 0;
	game->lock_resets = 0;
	game->last_kick_index = -1;
	game->last_action_was_rotation = false;
}

/**
 * @brief Moves the active piece horizontally.
 *
 * A successful shift updates lock-delay bookkeeping and clears rotation
 *   provenance.
 *
 * @param game Pointer to the Solo state.
 * @param direction Horizontal delta, normally -1 or 1.
 * @return true when the piece moved, otherwise false.
 */
static bool	apply_shift(t_solo_game *game, int direction)
{
	bool	was_grounded;

	was_grounded = piece_is_grounded(game);
	if (piece_move(&game->board, &game->active, direction, 0) != BRAIN_OK)
		return (false);
	game->pending_events |= SOLO_EVENT_MOVE;
	reset_lock_after_move(game, was_grounded);
	game->last_action_was_rotation = false;
	return (true);
}

/**
 * @brief Checks whether the active piece can descend one row.
 *
 * A copied piece is probed so the live state remains unchanged.
 *
 * @param game Pointer to the Solo state.
 * @return true when downward movement is blocked, otherwise false.
 */
static bool	piece_is_grounded(const t_solo_game *game)
{
	t_piece	probe;

	probe = game->active;
	return (piece_move(&game->board, &probe, 0, 1) == BRAIN_BLOCKED);
}

/**
 * @brief Applies the Guideline lock-delay reset limit.
 *
 * A successful grounded move refreshes lock delay only while reset capacity
 *   remains.
 *
 * @param game Pointer to the Solo state.
 * @param was_grounded Whether the piece was grounded before the action.
 */
static void	reset_lock_after_move(t_solo_game *game, bool was_grounded)
{
	if (was_grounded && game->lock_resets < SOLO_LOCK_RESET_LIMIT)
	{
		game->lock_elapsed_ms = 0;
		game->lock_resets++;
	}
}

/**
 * @brief Rotates the active piece with SRS wall kicks.
 *
 * The accepted kick index is preserved for later T-spin classification.
 *
 * @param game Pointer to the Solo state.
 * @param direction Rotation direction, normally -1 or 1.
 * @return true when a kick candidate succeeded, otherwise false.
 */
static bool	apply_rotation(t_solo_game *game, int direction)
{
	bool	was_grounded;
	int		kick_index;

	was_grounded = piece_is_grounded(game);
	if (piece_rotate_with_kick(&game->board, &game->active, direction,
			&kick_index) != BRAIN_OK)
		return (false);
	game->pending_events |= SOLO_EVENT_ROTATE;
	reset_lock_after_move(game, was_grounded);
	game->last_action_was_rotation = true;
	game->last_kick_index = kick_index;
	return (true);
}

/**
 * @brief Stores or swaps the active tetromino once per spawned piece.
 *
 * A held tetromino always returns at its canonical spawn rotation and
 * position. The first hold consumes the preview head; later holds swap without
 * advancing the queue.
 *
 * @param game Pointer to the Solo state.
 * @return true when the hold changed state, otherwise false.
 */
static bool	apply_hold(t_solo_game *game)
{
	t_piece_type	outgoing;
	t_piece_type	incoming;

	if (game->hold_used)
		return (false);
	outgoing = game->active.type;
	if (game->has_hold)
	{
		incoming = game->hold;
		game->hold = outgoing;
		activate_piece(game, incoming);
	}
	else
	{
		game->hold = outgoing;
		game->has_hold = true;
		spawn_queued_piece(game);
	}
	game->hold_used = true;
	game->pending_events |= SOLO_EVENT_HOLD;
	return (true);
}

/**
 * @brief Locks the active piece and chooses its next phase.
 *
 * T-spin classification happens before stamping; top-out, clear animation, and
 *   normal spawn paths then diverge without sleeping.
 *
 * @param game Pointer to the Solo state.
 */
static void	lock_active_piece(t_solo_game *game)
{
	t_spin_type	spin;

	spin = T_SPIN_NONE;
	if (game->last_action_was_rotation)
		spin = piece_t_spin_type(&game->board, &game->active,
			game->last_kick_index);
	piece_stamp(&game->board, &game->active);
	game->pending_events |= SOLO_EVENT_LOCK;
	game->hold_used = false;
	if (piece_touches_top(&game->active))
	{
		begin_top_out_reveal(game);
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

/**
 * @brief Checks whether a locked piece occupies the visible top row.
 *
 * The local board has no hidden spawn rows, so row zero is the visible top-out
 *   boundary.
 *
 * @param piece Pointer to the locked piece.
 * @return true when any occupied cell is in row zero, otherwise false.
 */
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

/**
 * @brief Starts the noninteractive top-out reveal phase.
 *
 * The reveal timer is reset so the final settled board is presented before the
 *   game-over panel.
 *
 * @param game Pointer to the Solo state.
 */
static void	begin_top_out_reveal(t_solo_game *game)
{
	game->top_out_elapsed_ms = 0;
	game->phase = SOLO_TOP_OUT_REVEAL;
}

/**
 * @brief Applies and records the latest scoring event.
 *
 * The stored clear, spin, and perfect-clear values drive both persistent
 *   scoring and the HUD award label.
 *
 * @param game Pointer to the Solo state.
 * @param lines Number of lines cleared.
 * @param spin Classified T-spin type.
 * @param perfect_clear Whether no settled cells remain.
 */
static void	remember_score_event(t_solo_game *game, int lines,
	t_spin_type spin, bool perfect_clear)
{
	game->last_score = score_apply_clear(&game->scoring, lines, game->level,
		spin, perfect_clear);
	game->last_lines = lines;
	game->last_spin = spin;
	game->last_perfect_clear = perfect_clear;
}

/**
 * @brief Replaces the active tetromino with a canonical spawned piece.
 *
 * Movement bookkeeping is reset and a blocked spawn enters the same visible
 * top-out reveal used after a normal lock.
 *
 * @param game Pointer to the Solo state.
 * @param type Tetromino type to spawn.
 */
static void	activate_piece(t_solo_game *game, t_piece_type type)
{
	game->active = piece_spawn(type);
	reset_piece_timers(game);
	game->phase = SOLO_ACTIVE;
	if (!piece_is_valid(&game->board, &game->active))
	{
		/* A blocked spawn keeps the final settled board visible too. */
		begin_top_out_reveal(game);
	}
}

/**
 * @brief Promotes the preview head and appends one bag piece.
 *
 * Queue advancement is shared by normal locks and the first HOLD. Hold
 * availability is reset by locking, not here, so first HOLD remains consumed.
 *
 * @param game Pointer to the Solo state.
 */
static void	spawn_queued_piece(t_solo_game *game)
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
	activate_piece(game, type);
}

/**
 * @brief Processes one timer boundary that is already due.
 *
 * Zero-elapsed updates can still drain due boundaries, preventing a busy loop
 *   around equal millisecond samples.
 *
 * @param game Pointer to the Solo state.
 * @return true when one due event was processed, otherwise false.
 */
static bool	process_due_event(t_solo_game *game)
{
	int	clear_ms;
	int	gravity_ms;

	if (game->phase == SOLO_TOP_OUT_REVEAL)
	{
		if (game->top_out_elapsed_ms >= SOLO_TOP_OUT_REVEAL_MS)
		{
			game->phase = SOLO_GAME_OVER;
			return (true);
		}
		return (false);
	}
	if (game->phase == SOLO_CLEARING)
	{
		clear_ms = solo_clear_duration_ms(game->level);
		if (game->clear_elapsed_ms >= clear_ms)
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

/**
 * @brief Commits a completed line-clear animation.
 *
 * Rows collapse, score and level advance, crystal charge fills, and the next
 *   piece spawns.
 *
 * @param game Pointer to the Solo state.
 */
static void	finish_line_clear(t_solo_game *game)
{
	bool	perfect_clear;
	int		previous_charge;
	int		previous_level;
	int		ability;
	int		crystals_earned;

	board_clear_lines(&game->board);
	perfect_clear = board_is_empty(&game->board);
	remember_score_event(game, game->clear_count, game->pending_spin,
		perfect_clear);
	if (perfect_clear)
	{
		game->pending_events |= SOLO_EVENT_PERFECT_CLEAR;
		game->score_event_active = true;
		game->score_event_elapsed_ms = 0;
	}
	else if (game->clear_count == 1)
	{
		game->pending_events |= SOLO_EVENT_SINGLE;
		game->score_event_active = true;
		game->score_event_elapsed_ms = 0;
	}
	else if (game->clear_count == 2)
	{
		game->pending_events |= SOLO_EVENT_DOUBLE;
		game->score_event_active = true;
		game->score_event_elapsed_ms = 0;
	}
	else if (game->clear_count == 3)
	{
		game->pending_events |= SOLO_EVENT_TRIPLE;
		game->score_event_active = true;
		game->score_event_elapsed_ms = 0;
	}
	else if (game->clear_count == 4)
	{
		game->pending_events |= SOLO_EVENT_TETRIS;
		game->score_event_active = true;
		game->score_event_elapsed_ms = 0;
	}
	previous_level = game->level;
	previous_charge = game->crystal_charge;
	game->total_lines += game->clear_count;
	game->level = level_from_lines(game->total_lines);
	if (game->level > previous_level)
		game->pending_events |= SOLO_EVENT_LEVEL_UP;
	game->crystal_line_progress += game->clear_count;
	crystals_earned = game->crystal_line_progress
		/ SOLO_CRYSTAL_LINES_PER_CHARGE;
	game->crystal_line_progress %= SOLO_CRYSTAL_LINES_PER_CHARGE;
	game->crystal_charge += crystals_earned;
	if (game->crystal_charge >= SOLO_CRYSTAL_CAPACITY)
	{
		game->crystal_charge = SOLO_CRYSTAL_CAPACITY;
		game->crystal_line_progress = 0;
	}
	ability = SOLO_ABILITY_MIRURUN;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		if (previous_charge < solo_ability_cost((t_solo_ability)ability)
			&& game->crystal_charge
				>= solo_ability_cost((t_solo_ability)ability))
		{
			game->pending_events |= SOLO_EVENT_ABILITY_READY;
			game->ability_ready_active = true;
			game->ability_ready_elapsed_ms = 0;
			game->ready_ability = (t_solo_ability)ability;
			break ;
		}
		ability++;
	}
	game->clear_count = 0;
	game->clear_elapsed_ms = 0;
	spawn_queued_piece(game);
}

/**
 * @brief Advances one pulsing and fading ability response.
 *
 * Gameplay and feedback consume the same elapsed wall-clock interval.
 *
 * @param game Pointer to the Solo state.
 * @param elapsed_ms Elapsed monotonic milliseconds.
 * @return true when visible feedback expired, otherwise false.
 */
static bool	advance_ability_feedback(t_solo_game *game, int elapsed_ms)
{
	if (game->ability_result == SOLO_ABILITY_RESULT_NONE)
		return (false);
	if (elapsed_ms < SOLO_ABILITY_FEEDBACK_MS
		- game->ability_feedback_elapsed_ms)
	{
		game->ability_feedback_elapsed_ms += elapsed_ms;
		return (elapsed_ms > 0);
	}
	game->ability_feedback_elapsed_ms = 0;
	game->ability_result = SOLO_ABILITY_RESULT_NONE;
	game->last_ability = SOLO_ABILITY_NONE;
	return (true);
}

/**
 * @brief Advances the short new-best pulse and fade on the shared loop clock.
 *
 * @param game Pointer to the Solo state.
 * @param elapsed_ms Elapsed monotonic milliseconds.
 * @return true while the visible banner opacity may have changed.
 */
static bool	advance_personal_best(t_solo_game *game, int elapsed_ms)
{
	int	total_ms;

	if (!game->new_personal_best)
		return (false);
	total_ms = SOLO_PERSONAL_BEST_PULSE_MS + SOLO_PERSONAL_BEST_FADE_MS;
	if (game->personal_best_elapsed_ms >= total_ms)
		return (false);
	game->personal_best_elapsed_ms += elapsed_ms;
	if (game->personal_best_elapsed_ms > total_ms)
		game->personal_best_elapsed_ms = total_ms;
	return (elapsed_ms > 0);
}

/**
 * @brief Advances bounded score, ready, and countdown animation timers.
 */
static bool	advance_event_animations(t_solo_game *game, int elapsed_ms)
{
	int		before_stage;
	int		after_stage;
	bool	changed;

	changed = advance_timed_animation(&game->score_event_elapsed_ms,
			&game->score_event_active,
			SOLO_SCORE_EVENT_PULSE_MS + SOLO_SCORE_EVENT_FADE_MS, elapsed_ms);
	changed = advance_timed_animation(&game->ability_ready_elapsed_ms,
			&game->ability_ready_active,
			SOLO_ABILITY_READY_PULSE_MS + SOLO_ABILITY_READY_FADE_MS,
			elapsed_ms) || changed;
	if (!game->countdown_active)
		return (changed);
	before_stage = game->countdown_elapsed_ms / SOLO_COUNTDOWN_STEP_MS;
	game->countdown_elapsed_ms += elapsed_ms;
	if (game->countdown_elapsed_ms
		> SOLO_COUNTDOWN_STEP_MS * SOLO_COUNTDOWN_STEPS)
		game->countdown_elapsed_ms
			= SOLO_COUNTDOWN_STEP_MS * SOLO_COUNTDOWN_STEPS;
	after_stage = game->countdown_elapsed_ms / SOLO_COUNTDOWN_STEP_MS;
	if (before_stage < 1 && after_stage >= 1)
		game->pending_events |= SOLO_EVENT_COUNTDOWN_TICK;
	if (before_stage < 2 && after_stage >= 2)
		game->pending_events |= SOLO_EVENT_COUNTDOWN_TICK;
	if (before_stage < SOLO_COUNTDOWN_STEPS - 1
		&& after_stage >= SOLO_COUNTDOWN_STEPS - 1)
		game->pending_events |= SOLO_EVENT_COUNTDOWN_GO;
	if (after_stage >= SOLO_COUNTDOWN_STEPS)
		game->countdown_active = false;
	return (elapsed_ms > 0 || changed);
}

/**
 * @brief Fades the environment dim level toward the current danger state.
 */
static bool	advance_danger_presentation(t_solo_game *game, int elapsed_ms)
{
	int	before;

	if (elapsed_ms <= 0)
		return (false);
	before = game->danger_fade_elapsed_ms;
	if (game->danger_active)
	{
		game->danger_fade_elapsed_ms += elapsed_ms;
		if (game->danger_fade_elapsed_ms > SOLO_DANGER_FADE_MS)
			game->danger_fade_elapsed_ms = SOLO_DANGER_FADE_MS;
	}
	else
	{
		game->danger_fade_elapsed_ms -= elapsed_ms;
		if (game->danger_fade_elapsed_ms < 0)
			game->danger_fade_elapsed_ms = 0;
	}
	return (before != game->danger_fade_elapsed_ms);
}

/**
 * @brief Advances one duration-capped animation and clears it at expiry.
 */
static bool	advance_timed_animation(int *elapsed_ms, bool *active,
	int duration_ms, int step_ms)
{
	if (!*active || step_ms <= 0)
		return (false);
	*elapsed_ms += step_ms;
	if (*elapsed_ms >= duration_ms)
	{
		*elapsed_ms = duration_ms;
		*active = false;
	}
	return (true);
}

/**
 * @brief Produces a small brightness pulse followed by a linear fade.
 */
static unsigned	pulse_fade_opacity(int elapsed_ms, int pulse_ms, int fade_ms)
{
	int	phase;
	int	remaining;

	if (elapsed_ms >= pulse_ms)
	{
		remaining = pulse_ms + fade_ms - elapsed_ms;
		if (remaining <= 0)
			return (0);
		return ((unsigned)(255 * remaining / fade_ms));
	}
	phase = elapsed_ms % (pulse_ms / 2);
	if (phase > pulse_ms / 4)
		phase = pulse_ms / 2 - phase;
	return ((unsigned)(255 - 48 * phase / (pulse_ms / 4)));
}

/**
 * @brief Merges one active animation's 30 FPS deadline into the current wake.
 */
static int	animation_wake_ms(int wake_ms, bool active, int remaining_ms)
{
	int	frame_ms;

	if (!active || remaining_ms <= 0)
		return (wake_ms);
	frame_ms = SOLO_EVENT_ANIMATION_FRAME_MS;
	if (remaining_ms < frame_ms)
		frame_ms = remaining_ms;
	if (wake_ms < 0 || frame_ms < wake_ms)
		return (frame_ms);
	return (wake_ms);
}

/**
 * @brief Consumes time in the final-board reveal phase.
 *
 * The static reveal has one deterministic deadline before the game-over
 *   overlay appears.
 *
 * @param game Pointer to the Solo state.
 * @param remaining_ms In/out unconsumed elapsed milliseconds.
 * @return true when the reveal deadline is reached, otherwise false.
 */
static bool	advance_top_out_reveal(t_solo_game *game, int *remaining_ms)
{
	int	step;

	step = min_int(*remaining_ms,
		SOLO_TOP_OUT_REVEAL_MS - game->top_out_elapsed_ms);
	if (step < 0)
		step = 0;
	game->top_out_elapsed_ms += step;
	*remaining_ms -= step;
	return (game->top_out_elapsed_ms >= SOLO_TOP_OUT_REVEAL_MS);
}

/**
 * @brief Returns the smaller of two integers.
 *
 * This local helper keeps timer-boundary calculations explicit.
 *
 * @param left First value.
 * @param right Second value.
 * @return The smaller input value.
 */
static int	min_int(int left, int right)
{
	if (left < right)
		return (left);
	return (right);
}

/**
 * @brief Consumes time until the next clear-animation boundary.
 *
 * The midpoint swaps animation tiles and the endpoint collapses rows and
 *   spawns.
 *
 * @param game Pointer to the Solo state.
 * @param remaining_ms In/out unconsumed elapsed milliseconds.
 * @return true when a visual boundary was reached, otherwise false.
 */
static bool	advance_clearing(t_solo_game *game, int *remaining_ms)
{
	int	duration_ms;
	int	target;
	int	step;

	duration_ms = solo_clear_duration_ms(game->level);
	target = duration_ms;
	if (game->clear_elapsed_ms < duration_ms / 2)
		target = duration_ms / 2;
	step = min_int(*remaining_ms, target - game->clear_elapsed_ms);
	if (step < 0)
		step = 0;
	game->clear_elapsed_ms += step;
	*remaining_ms -= step;
	if (game->clear_elapsed_ms >= duration_ms)
	{
		finish_line_clear(game);
		return (true);
	}
	return (game->clear_elapsed_ms == duration_ms / 2);
}

/**
 * @brief Consumes elapsed time while gameplay is active.
 *
 * Time advances to one gravity or lock boundary at a time so newly landed
 *   pieces receive the full lock delay.
 *
 * @param game Pointer to the Solo state.
 * @param remaining_ms In/out unconsumed elapsed milliseconds.
 * @return true when visible state changed, otherwise false.
 */
static bool	advance_active(t_solo_game *game, int *remaining_ms)
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
