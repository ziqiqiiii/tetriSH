#include "tetrisu.h"

/*
** Double, played against tetrisd instead of against a fixture.
**
** The Solo sibling of this file is net_solo.c and the rule is the same one:
** every board mutation belongs to the server, this file serialises actions
** one way and decodes snapshots the other, and nothing here ever writes a
** board of its own.
**
** What is different is that one snapshot now describes more than one board.
** The opponent rides inside the recipient's own frame because tetrisd holds a
** single STATE mailbox slot per client - a second push would free the first
** before it was written - and because one message makes the two boards the
** same instant by construction. So a match reads one frame and applies parts
** of it to several view models, which is why the shared mapping takes a
** snapshot rather than a client.
**
** The countdown and the result come from the same frame. Neither is something
** the client may decide: two players have to start together, and the winner's
** board looks exactly like a board still being played.
*/

// Static Functions
static void	apply_countdown(t_mp_match_state *state,
				const t_body_state *snap);
static void	apply_result(t_mp_match_state *state, const t_body_state *snap);
static void	apply_opponents(t_mp_match_state *state,
				const t_body_state *snap);
static void	apply_opponent_game(t_solo_game *game,
				const t_body_opponent *opponent);
static void	apply_opponent_card(t_mp_match_state *state, int index,
				const t_body_opponent *opponent);

/**
 * @brief Remembers which room's game this client is about to render.
 *
 * The play path is what every pushed STATE is matched against, and it used to
 * be written only by net_solo_start - so a player who joined a room somebody
 * else started never had one, and rejected every snapshot the server sent
 * them. Binding it when the seat is taken also makes the first snapshot the
 * signal that the match has begun, which is more prompt than waiting for a
 * poll of the room to say so.
 *
 * The name is copied out before either field is written, because the caller
 * that matters most passes net->room itself - a refresh re-binds the path for
 * the room the client is already in. Writing straight through would be
 * snprintf copying a buffer onto itself, which is undefined and which glibc
 * resolves by leaving it empty: the room name vanished and every later
 * request addressed `/room/`.
 *
 * @param net Client seated in a room.
 * @param room The room's name; may alias net->room.
 * @return 0 when the path was bound, -1 when there is nothing to bind it to.
 */
int	net_match_join(t_net_client *net, const char *room)
{
	char	name[NET_ROOM_MAX];

	if (net == NULL || room == NULL || room[0] == '\0'
		|| net->player_id == 0)
		return (-1);
	snprintf(name, sizeof(name), "%s", room);
	snprintf(net->room, sizeof(net->room), "%s", name);
	snprintf(net->play_path, sizeof(net->play_path), "%s%s/player/%llu",
		TETRISU_ROUTE_ROOM, name, (unsigned long long)net->player_id);
	return (0);
}

/**
 * @brief Replaces the match view model with the server's latest snapshot.
 *
 * @param net Client holding a snapshot.
 * @param state Match model to overwrite.
 * @return true when a snapshot was applied, false when none has arrived.
 */
bool	net_match_apply(t_net_client *net, t_mp_match_state *state)
{
	const t_body_state	*snap;

	if (net == NULL || state == NULL || !net->has_state)
		return (false);
	snap = &net->state_snapshot;
	net_state_apply(snap, &state->local_game);
	/*
	 * Rows owed to this player, straight from the server. It is a count and
	 * not a board change - the rows land at the next lock - so there is
	 * nothing here to reconcile against local_game, only something to say.
	 */
	state->incoming_garbage = snap->pending;
	apply_countdown(state, snap);
	apply_opponents(state, snap);
	apply_result(state, snap);
	net->applied_seq = snap->seq;
	return (true);
}

/**
 * @brief Sends one player action as the request that expresses it.
 *
 * @param net Client with a match running.
 * @param action The action the player asked for.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_match_action(t_net_client *net, t_solo_action action,
		t_net_result *out)
{
	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	return (net_solo_action(net, action, out));
}

/**
 * @brief Sends one player action without waiting to be told it stood.
 *
 * The match loop's path. A match is where the wait it drops is worst: two
 * boards stop for it rather than one, and the one that stops is the one being
 * steered while the other keeps arriving on its own clock.
 *
 * @param net Client with a match running.
 * @param action The action the player asked for.
 * @return 0 when the request went out or was dropped under a back-off, -1 on
 *         a transport failure.
 */
int	net_match_send_action(t_net_client *net, t_solo_action action)
{
	if (net == NULL || net->state != NET_IN_GAME)
		return (-1);
	return (net_solo_send_action(net, action));
}

/**
 * @brief Spends charge on one Gaiden ability level.
 *
 * @param net Client with a match running.
 * @param ability Ability level the player selected.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_match_ability(t_net_client *net, t_solo_ability ability,
		t_net_result *out)
{
	return (net_solo_ability(net, ability, out));
}

/**
 * @brief Drives the 3-2-1 from the room's clock rather than from a local one.
 *
 * The server counts down in milliseconds remaining and the renderer counts up
 * in milliseconds elapsed, so the one is turned into the other here. The two
 * totals need not agree and deliberately are not shared: a client that had to
 * be told the server's countdown length in order to draw it would have a
 * second copy of a number only the server acts on. Any remainder beyond what
 * the animation covers simply holds the first digit a little longer.
 *
 * @param state Match model whose local board is being held.
 * @param snap Snapshot carrying the room's countdown.
 */
static void	apply_countdown(t_mp_match_state *state, const t_body_state *snap)
{
	int	total_ms;
	int	remaining_ms;

	total_ms = SOLO_COUNTDOWN_STEP_MS * SOLO_COUNTDOWN_STEPS;
	remaining_ms = snap->countdown_ms;
	if (remaining_ms > total_ms)
		remaining_ms = total_ms;
	if (remaining_ms < 0)
		remaining_ms = 0;
	if (snap->countdown_ms > 0)
	{
		if (!state->local_game.countdown_active)
			state->local_game.pending_events |= SOLO_EVENT_COUNTDOWN_TICK;
		state->local_game.countdown_active = true;
		state->local_game.countdown_elapsed_ms = total_ms - remaining_ms;
		return ;
	}
	if (state->local_game.countdown_active)
	{
		state->local_game.countdown_active = false;
		state->local_game.countdown_elapsed_ms = total_ms;
		state->local_game.pending_events |= SOLO_EVENT_COUNTDOWN_GO;
	}
}

/**
 * @brief Ends the match when the server says how it ended.
 *
 * Nothing about the board says this: the winner's is active with a piece on
 * it, exactly like a board mid-match. So the verdict arrives as its own field
 * and is the only thing that finishes a match here - a local top-out no longer
 * decides anything, because a player can top out and still be waiting on
 * somebody else to do the same.
 *
 * @param state Match model to finish.
 * @param snap Snapshot carrying the verdict.
 */
static void	apply_result(t_mp_match_state *state, const t_body_state *snap)
{
	if (snap->result == BODY_RESULT_NONE
		|| state->phase == MP_MATCH_FINISHED)
		return ;
	mp_match_finish(state, snap->result == BODY_RESULT_WON, snap->rank);
}

/**
 * @brief Puts the other players' boards where the renderer reads them.
 *
 * Double draws one whole opponent board and Battle Royale draws a grid of
 * cards, so the same projection feeds two shapes: the first opponent fills
 * the full view model the split screen renders, and every opponent fills a
 * card. Doing both costs one board copy and means neither renderer had to
 * change.
 *
 * The cards are cleared first, because a snapshot says who is in the room now
 * and writing only the seats it names cannot unsay the ones it does not. A
 * player who disconnects mid-match is forfeited by the server and their game
 * goes with them, so the very next frame carries one fewer opponent - and the
 * card left standing showed them still sitting there alive until the verdict
 * arrived behind it.
 *
 * @param state Match model to write.
 * @param snap Snapshot carrying the opponents.
 */
static void	apply_opponents(t_mp_match_state *state, const t_body_state *snap)
{
	size_t	index;

	memset(state->opponents, 0, sizeof(state->opponents));
	if (snap->opponent_count == 0)
	{
		solo_game_init(&state->opponent_game, 0);
		state->opponent_name[0] = '\0';
		state->opponent_charge = 0;
		state->opponent_character = 0;
	}
	index = 0;
	while (index < snap->opponent_count
		&& index < (size_t)(APP_ROOM_MAX_PLAYERS - 1))
	{
		if (index == 0)
		{
			apply_opponent_game(&state->opponent_game, &snap->opponents[0]);
			snprintf(state->opponent_name, sizeof(state->opponent_name), "%s",
				snap->opponents[0].username);
			/*
			 * The two facts the other half of the screen is drawn from. They
			 * used to be a hardcoded 6 and nothing, because the server did
			 * not send them - so the rival's meter was a constant and there
			 * was no fighter opposite to put a face to.
			 */
			state->opponent_charge = snap->opponents[0].charge;
			state->opponent_character = snap->opponents[0].character;
		}
		apply_opponent_card(state, (int)index, &snap->opponents[index]);
		index++;
	}
	state->players_alive = 1;
	if (state->local_game.phase == SOLO_GAME_OVER)
		state->players_alive = 0;
	index = 0;
	while (index < snap->opponent_count)
	{
		if (snap->opponents[index].alive)
			state->players_alive++;
		index++;
	}
}

/**
 * @brief Fills the full opponent view model the Double screen renders.
 *
 * The piece is spawned and then placed rather than copied field by field,
 * because the renderer needs the shape a type implies and the snapshot
 * carries only where it is. Everything the projection deliberately withholds -
 * the next queue, the hold slot, the charge - is simply left as it was: those
 * are the parts of an opponent's game that are none of this player's business.
 *
 * @param game View model to overwrite.
 * @param opponent The projection to read.
 */
static void	apply_opponent_game(t_solo_game *game,
		const t_body_opponent *opponent)
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
			cell.type = (t_cell_type)opponent->cells[row][col].type;
			cell.color = opponent->cells[row][col].color;
			board_set(&game->board, col, row, cell);
			col++;
		}
		row++;
	}
	game->active = piece_spawn((t_piece_type)opponent->piece.type);
	game->active.rotation = opponent->piece.rotation;
	game->active.col = opponent->piece.col;
	game->active.row = opponent->piece.row;
	game->scoring.total = opponent->score;
	game->total_lines = opponent->lines;
	game->phase = SOLO_ACTIVE;
	if (opponent->phase == BODY_PHASE_TOP_OUT)
		game->phase = SOLO_GAME_OVER;
	else if (opponent->phase == BODY_PHASE_CLEARING)
		game->phase = SOLO_CLEARING;
}

/**
 * @brief Fills one card of the Battle Royale grid.
 *
 * @param state Match model holding the cards.
 * @param index Which card to write.
 * @param opponent The projection to read.
 */
static void	apply_opponent_card(t_mp_match_state *state, int index,
		const t_body_opponent *opponent)
{
	t_cell	cell;
	int		row;
	int		col;

	memset(&cell, 0, sizeof(cell));
	state->opponents[index].present = true;
	state->opponents[index].alive = opponent->alive;
	state->opponents[index].garbage_pending = opponent->pending;
	snprintf(state->opponents[index].name,
		sizeof(state->opponents[index].name), "%s", opponent->username);
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			cell.type = (t_cell_type)opponent->cells[row][col].type;
			cell.color = opponent->cells[row][col].color;
			board_set(&state->opponents[index].board, col, row, cell);
			col++;
		}
		row++;
	}
}
