#include "tetrisu.h"

/*
** Solo, played against tetrisd instead of against a copy of the rules.
**
** The authority boundary is docs/tetrisu-local-to-tetrisd.md: every board
** mutation belongs to the server, and this file serialises actions one way
** and decodes snapshots the other. It never calls solo_game_apply_action or
** solo_game_update - the moment it did, the client would have an opinion
** about the board, and the two would drift.
**
** The view model is still t_solo_game, which is not a shortcut. Everything
** STATE carries is a field the Solo renderer already reads; everything the
** renderer needs that STATE does not carry - the clear animation, the
** countdown, the personal best, the danger tint, the ability popover - is
** presentation, which the same document says stays here. So the snapshot is
** applied into the struct and the renderer is untouched.
**
** What is deliberately *not* applied is anything the client made up. A
** snapshot is the whole of the board, the piece, the queue, the hold slot and
** the score; local timers keep running the animations around them.
*/

// Static Functions
static bool	action_request(t_net_client *net, t_solo_action action,
				const char **method, const char **body);
static int	refused(t_net_result *out, const t_net_result *result);
static void	apply_cells(t_solo_game *game, const t_body_state *snap);
static void	apply_counters(t_solo_game *game, const t_body_state *snap);
static void	apply_phase(t_solo_game *game, const t_body_state *snap);
static void	apply_clearing(t_solo_game *game, const t_body_state *snap);
static void	apply_effects(t_solo_game *game, const t_body_state *snap);
static void	apply_clear_label(t_solo_game *game, const t_body_state *snap,
				uint64_t previous_score);
static t_solo_ability_result	verdict_of(const char *reason);

/**
 * @brief Takes a Single room, starts it, and remembers where to send inputs.
 *
 * The room is created rather than joined by name: the lobby names its own
 * rooms (S-01, D-02), so a client asks the collection for one and is told
 * which it got.
 *
 * The two steps are one operation, so a failed START gives the seat back.
 * Keeping it left the player in a room they were not playing in and could
 * not see: the authority reports itself offline, so its close does nothing,
 * and every later JOIN is answered 409 already-in-room - Solo silently
 * playing the local rules for the rest of the session.
 *
 * @param net Authenticated client.
 * @param out Receives the server's answer to whichever step refused.
 * @return 0 when a game is running, -1 otherwise.
 */
int	net_solo_start(t_net_client *net, t_net_result *out)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	if (net == NULL || net->state < NET_AUTHED)
		return (-1);
	if (net_request(net, "JOIN", TETRISU_ROUTE_ROOMS, "mode single\n",
			&result) != 0)
		return (-1);
	if (result.status != 201
		|| net_result_field(&result, "room", net->room,
			sizeof(net->room)) == NULL)
		return (refused(out, &result));
	net->state = NET_IN_ROOM;
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, net->room);
	if (net_request(net, "START", path, NULL, &result) != 0)
	{
		net_solo_leave(net);
		return (-1);
	}
	if (result.status != 200)
	{
		net_solo_leave(net);
		return (refused(out, &result));
	}
	snprintf(net->play_path, sizeof(net->play_path), "%s%s/player/%llu",
		TETRISU_ROUTE_ROOM, net->room,
		(unsigned long long)net->player_id);
	net->state = NET_IN_GAME;
	if (out != NULL)
		*out = result;
	return (0);
}

/**
 * @brief Gives up the room, ending the game the server is running.
 *
 * The state is only wound back to signed-in when the session survived the
 * request: a LEAVE that times out closes the connection on its way out, and
 * stamping NET_AUTHED over that would leave the client claiming it is signed
 * in on a socket that no longer exists.
 *
 * @param net Client in a room.
 */
void	net_solo_leave(t_net_client *net)
{
	char	path[NET_PATH_MAX];

	if (net == NULL || net->state < NET_IN_ROOM || net->room[0] == '\0')
		return ;
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, net->room);
	(void)net_request(net, "LEAVE", path, NULL, NULL);
	if (net->state != NET_OFFLINE)
		net->state = NET_AUTHED;
	net->room[0] = '\0';
	net->play_path[0] = '\0';
	net->has_state = false;
	net->last_seq = 0;
	net->applied_seq = 0;
	net_chat_reset(net);
}

/**
 * @brief Sends one player action as the request that expresses it.
 *
 * Nothing is applied locally, not even optimistically. The server answers
 * whether it stood, and the snapshot that follows is what the player sees -
 * a client that moved its own piece first would have to decide what to do
 * when the server disagreed, and there is no honest answer to that.
 *
 * @param net Client with a game running.
 * @param action The action the player asked for.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure or an action
 *         with no request behind it.
 */
int	net_solo_action(t_net_client *net, t_solo_action action,
		t_net_result *out)
{
	const char	*method;
	const char	*body;

	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	if (!action_request(net, action, &method, &body))
		return (-1);
	return (net_request(net, method, net->play_path, body, out));
}

/**
 * @brief Sends one player action without waiting to be told it stood.
 *
 * This is the path the game loop uses, and net_solo_action is the path a
 * caller uses when the verdict is the thing it wants - a test asserting that
 * a second HOLD is refused, for instance. The request on the wire is the same
 * either way.
 *
 * Waiting is what this drops, and the reason it can be dropped is the
 * authority boundary: the board changes when the snapshot that follows says it
 * did, so the response to a MOVE carries nothing the next snapshot does not.
 * What waiting cost was a turn of the render loop per keypress - at 40 ms
 * round trip a key repeating every 33 ms issues faster than replies come back,
 * and both boards sit still for each one.
 *
 * @param net Client with a game running.
 * @param action The action the player asked for.
 * @return 0 when the request went out or was dropped under a back-off, -1 on
 *         a transport failure or an action with no request behind it.
 */
int	net_solo_send_action(t_net_client *net, t_solo_action action)
{
	const char	*method;
	const char	*body;

	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	if (!action_request(net, action, &method, &body))
		return (-1);
	return (net_send(net, method, net->play_path, body));
}

/**
 * @brief PAUSE or RESUME the server-side game.
 *
 * @param net Client with a game running.
 * @param paused true to pause, false to resume.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_solo_pause(t_net_client *net, bool paused, t_net_result *out)
{
	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	if (paused)
		return (net_request(net, "PAUSE", net->play_path, "PAUSE\n", out));
	return (net_request(net, "PAUSE", net->play_path, "RESUME\n", out));
}

/**
 * @brief Asks for a fresh game in the same room.
 *
 * @param net Client with a game running.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_solo_restart(t_net_client *net, t_net_result *out)
{
	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	return (net_request(net, "RESTART", net->play_path, NULL, out));
}

/**
 * @brief Spends charge on one Gaiden ability level.
 *
 * The level goes on the wire, never the ability: which four a level selects
 * from is decided by the character the account has equipped, and the server
 * reads that itself.
 *
 * @param net Client with a game running.
 * @param ability Ability level the player selected.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_solo_ability(t_net_client *net, t_solo_ability ability,
		t_net_result *out)
{
	char	body[64];

	if (net == NULL || net->state != NET_IN_GAME
		|| ability < SOLO_ABILITY_MIRURUN || ability > SOLO_ABILITY_SIRTET)
		return (-1);
	snprintf(body, sizeof(body), "level %d\n", (int)ability);
	return (net_request(net, "ABILITY", net->play_path, body, out));
}

/**
 * @brief Turns the server's answer to an ABILITY into the HUD's feedback.
 *
 * The meter already knows how to say "not enough charge" and "that would not
 * fit"; what it lacked was somebody to tell it, and the only one who knows is
 * the server. The reason word is the whole of the mapping - a status alone
 * would collapse four different refusals into one.
 *
 * @param result The server's answer.
 * @param ability The level that was asked for.
 * @param game View model whose feedback slot is written.
 */
void	net_solo_ability_feedback(const t_net_result *result,
		t_solo_ability ability, t_solo_game *game)
{
	if (result == NULL || game == NULL)
		return ;
	game->last_ability = ability;
	game->ability_feedback_elapsed_ms = 0;
	if (result->status == 200)
	{
		game->ability_result = SOLO_ABILITY_RESULT_ACTIVATED;
		game->pending_events |= SOLO_EVENT_ABILITY_ACTIVATED;
		return ;
	}
	game->ability_result = verdict_of(result->reason);
	game->pending_events |= SOLO_EVENT_ABILITY_REJECTED;
}

/**
 * @brief Replaces the view model with the server's latest snapshot.
 *
 * Everything the snapshot carries is overwritten wholesale rather than
 * merged: a partial apply would leave the client holding a board from one
 * frame and a piece from another, which is a state the server was never in.
 *
 * Animation and presentation fields are left alone, because the server does
 * not have them and the renderer still needs them to run.
 *
 * @param net Client holding a snapshot.
 * @param game View model to overwrite.
 * @return true when a snapshot was applied, false when none has arrived.
 */
bool	net_solo_apply(t_net_client *net, t_solo_game *game)
{
	if (net == NULL || game == NULL || !net->has_state)
		return (false);
	net_state_apply(&net->state_snapshot, game);
	net->applied_seq = net->state_snapshot.seq;
	return (true);
}

/**
 * @brief Writes one decoded snapshot onto one view model.
 *
 * Split out of net_solo_apply so a match can use it for the board the player
 * is steering. Both modes render the same t_solo_game and the server projects
 * the same fields into it, so the mapping between them is one thing - and a
 * second copy of it would be a second place for the two to drift.
 *
 * The snapshot is passed rather than the client, because a match reads one
 * frame and applies parts of it to several view models.
 *
 * @param snap Snapshot to read.
 * @param game View model to overwrite.
 */
void	net_state_apply(const t_body_state *snap, t_solo_game *game)
{
	uint64_t	previous_score;

	if (snap == NULL || game == NULL)
		return ;
	previous_score = game->scoring.total;
	apply_cells(game, snap);
	game->active = piece_spawn((t_piece_type)snap->piece.type);
	game->active.rotation = snap->piece.rotation;
	game->active.col = snap->piece.col;
	game->active.row = snap->piece.row;
	game->next[0] = (t_piece_type)snap->next[0];
	game->next[1] = (t_piece_type)snap->next[1];
	game->next[2] = (t_piece_type)snap->next[2];
	game->has_hold = snap->hold != BODY_HOLD_EMPTY;
	if (game->has_hold)
		game->hold = (t_piece_type)snap->hold;
	game->hold_used = snap->hold_used;
	apply_counters(game, snap);
	apply_clearing(game, snap);
	apply_clear_label(game, snap, previous_score);
	apply_effects(game, snap);
	apply_phase(game, snap);
}

/**
 * @brief Copies the server's effect counts onto the view model.
 *
 * Copied and not interpreted. Whether a rotation is allowed is the server's
 * answer and always was; this is the client learning enough to say why it was
 * refused, and enough for the renderer to black a field out - which is the one
 * effect nothing but a renderer can carry out.
 *
 * @param game View model being filled.
 * @param snap Snapshot to read.
 */
static void	apply_effects(t_solo_game *game, const t_body_state *snap)
{
	game->effects.paralysis = snap->effect_paralysis;
	game->effects.inversion = snap->effect_inversion;
	game->effects.nue = snap->effect_nue;
	game->effects.thwack = snap->effect_thwack;
	game->effects.fry = snap->effect_fry;
	game->effects.dark = snap->effect_dark;
	game->effects.pals = snap->effect_pals;
	game->effects.mirror = snap->effect_mirror;
}

/**
 * @brief Reports whether a filed snapshot has not been applied yet.
 *
 * The socket has two readers: net_pump, and net_request while it waits for a
 * reply. Both file what they find, so "did net_pump see one?" is not the same
 * question as "is there one the view model has not been built from?" - and it
 * was the wrong one to ask. During play almost every STATE crosses an input
 * request, so the board fell behind the server for as long as the player kept
 * their hands on the keys.
 *
 * @param net Client to ask.
 * @return true when the latest snapshot is newer than the last applied.
 */
bool	net_solo_pending(const t_net_client *net)
{
	if (net == NULL || !net->has_state)
		return (false);
	return (net->state_snapshot.seq != net->applied_seq);
}

/**
 * @brief Names the request one player action is spelled as.
 *
 * @param net Client the action is for (unused beyond validation).
 * @param action The action to translate.
 * @param method Receives the HTTTP method.
 * @param body Receives the body text, or NULL for a method that takes none.
 * @return true when the action has a request, false otherwise.
 */
static bool	action_request(t_net_client *net, t_solo_action action,
			const char **method, const char **body)
{
	(void)net;
	*method = NULL;
	*body = NULL;
	if (action == SOLO_MOVE_LEFT || action == SOLO_MOVE_RIGHT)
		*method = "MOVE";
	else if (action == SOLO_ROTATE_CW || action == SOLO_ROTATE_CCW)
		*method = "ROTATE";
	else if (action == SOLO_SOFT_DROP || action == SOLO_HARD_DROP)
		*method = "DROP";
	else if (action == SOLO_HOLD)
		*method = "HOLD";
	else
		return (false);
	if (action == SOLO_MOVE_LEFT)
		*body = "LEFT\n";
	else if (action == SOLO_MOVE_RIGHT)
		*body = "RIGHT\n";
	else if (action == SOLO_ROTATE_CW)
		*body = "CW\n";
	else if (action == SOLO_ROTATE_CCW)
		*body = "CCW\n";
	else if (action == SOLO_SOFT_DROP)
		*body = "SOFT\n";
	else if (action == SOLO_HARD_DROP)
		*body = "HARD\n";
	return (true);
}

/**
 * @brief Reports a refusal to the caller and leaves the client where it was.
 *
 * @param out Receives the server's answer; may be NULL.
 * @param result The answer that refused.
 * @return Always -1, so a caller can return it directly.
 */
static int	refused(t_net_result *out, const t_net_result *result)
{
	if (out != NULL)
		*out = *result;
	return (-1);
}

/**
 * @brief Copies the settled board out of a snapshot.
 *
 * @param game View model to write.
 * @param snap Snapshot to read.
 */
static void	apply_cells(t_solo_game *game, const t_body_state *snap)
{
	t_cell	cell;
	int		row;
	int		col;

	memset(&cell, 0, sizeof(cell));
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			cell.type = (t_cell_type)snap->cells[row][col].type;
			cell.color = snap->cells[row][col].color;
			board_set(&game->board, col, row, cell);
			col++;
		}
		row++;
	}
}

/**
 * @brief Copies the score, counters and charge out of a snapshot.
 *
 * @param game View model to write.
 * @param snap Snapshot to read.
 */
static void	apply_counters(t_solo_game *game, const t_body_state *snap)
{
	game->scoring.total = snap->score;
	game->scoring.combo = snap->combo;
	game->scoring.back_to_back = snap->back_to_back;
	game->total_lines = snap->lines;
	game->level = snap->level;
	if (game->level < 1)
		game->level = 1;
	game->crystal_charge = snap->charge;
	if (snap->last_ability.level >= SOLO_ABILITY_MIRURUN
		&& snap->last_ability.level <= SOLO_ABILITY_SIRTET)
	{
		game->last_ability = (t_solo_ability)snap->last_ability.level;
		game->ability_result = SOLO_ABILITY_RESULT_UNAVAILABLE;
		if (snap->last_ability.accepted)
			game->ability_result = SOLO_ABILITY_RESULT_ACTIVATED;
	}
}

/**
 * @brief Copies the clear in progress, rows and all.
 *
 * The renderer draws the animation from `clear_rows`, `clear_count` and
 * `clear_elapsed_ms`, and online all three come from here - the offset is the
 * server's, not a timer of the client's. That is what keeps the flash on rows
 * the board still has: the same snapshot carries both, so they cannot
 * disagree.
 *
 * @param game View model to write.
 * @param snap Snapshot to read.
 */
static void	apply_clearing(t_solo_game *game, const t_body_state *snap)
{
	int	i;

	game->clear_count = snap->clearing_count;
	if (game->clear_count > BRAIN_MAX_CLEAR_LINES)
		game->clear_count = BRAIN_MAX_CLEAR_LINES;
	if (game->clear_count < 0)
		game->clear_count = 0;
	game->clear_elapsed_ms = snap->clearing_ms;
	i = 0;
	while (i < game->clear_count)
	{
		game->clear_rows[i] = snap->clearing_rows[i];
		i++;
	}
}

/**
 * @brief Fires the score banner when the server reports a fresh clear.
 *
 * The label is the server's; the points it awarded are not on the wire, so
 * they are read as the jump in the total the same snapshot carries. The
 * banner is what says TETRIS rather than SINGLE, and it was never shown
 * online because nothing looked at `last_clear`.
 *
 * The trigger is the score moving, not the label being non-none: the label
 * stays set until the next lock, so a snapshot mid-animation would re-fire
 * the banner sixteen times over one clear.
 *
 * @param game View model to write.
 * @param snap Snapshot to read.
 * @param previous_score The total before this snapshot was applied.
 */
static void	apply_clear_label(t_solo_game *game, const t_body_state *snap,
			uint64_t previous_score)
{
	if (snap->last_clear == BODY_CLEAR_NONE
		|| snap->score <= previous_score)
		return ;
	memset(&game->last_score, 0, sizeof(game->last_score));
	game->last_score.total_awarded = snap->score - previous_score;
	game->last_perfect_clear = snap->last_clear == BODY_CLEAR_PERFECT;
	game->last_lines = 0;
	if (snap->last_clear == BODY_CLEAR_SINGLE)
		game->last_lines = 1;
	else if (snap->last_clear == BODY_CLEAR_DOUBLE)
		game->last_lines = 2;
	else if (snap->last_clear == BODY_CLEAR_TRIPLE)
		game->last_lines = 3;
	else if (snap->last_clear == BODY_CLEAR_TETRIS)
		game->last_lines = 4;
	game->score_event_active = true;
	game->score_event_elapsed_ms = 0;
}

/**
 * @brief Maps the server's phase onto the renderer's.
 *
 * The clearing phase is the server's now, and so is the offset through it -
 * the client stopped keeping a timer of its own the moment tetrisd started
 * holding the rows (docs/bugs/the_line_clear_never_reached_the_client.md).
 * Pause is the server's for the same reason: showing it before the server
 * agreed would be a lie.
 *
 * @param game View model to write.
 * @param snap Snapshot to read.
 */
static void	apply_phase(t_solo_game *game, const t_body_state *snap)
{
	game->paused = snap->phase == BODY_PHASE_PAUSED;
	if (snap->phase == BODY_PHASE_TOP_OUT)
	{
		if (game->phase != SOLO_GAME_OVER)
			game->phase = SOLO_GAME_OVER;
		return ;
	}
	if (snap->phase == BODY_PHASE_CLEARING)
	{
		game->phase = SOLO_CLEARING;
		return ;
	}
	if (game->phase != SOLO_ACTIVE)
		game->phase = SOLO_ACTIVE;
}

/**
 * @brief Turns a refusal reason into the feedback the meter already shows.
 *
 * @param reason The server's reason word, or NULL.
 * @return The matching result for the HUD.
 */
static t_solo_ability_result	verdict_of(const char *reason)
{
	if (reason == NULL || reason[0] == '\0')
		return (SOLO_ABILITY_RESULT_INVALID);
	if (strcmp(reason, "no-charge") == 0)
		return (SOLO_ABILITY_RESULT_NO_CHARGE);
	if (strcmp(reason, "ability-blocked") == 0)
		return (SOLO_ABILITY_RESULT_BLOCKED);
	if (strcmp(reason, "no-target") == 0
		|| strcmp(reason, "ability-unavailable") == 0)
		return (SOLO_ABILITY_RESULT_UNAVAILABLE);
	return (SOLO_ABILITY_RESULT_INVALID);
}
