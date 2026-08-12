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
static void	apply_arena(t_mp_match_state *state, const t_body_state *snap,
				uint64_t local);
static void	apply_arena_card(t_mp_match_state *state,
				const t_body_arena_slot *card, uint64_t local, bool *seated);
static void	forget_empty_seats(t_mp_match_state *state, const bool *seated);
static void	count_the_arena(t_mp_match_state *state);
static void	apply_arena_mask(t_board *board,
				const t_body_arena_slot *card);

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
	apply_arena(state, snap, net->player_id);
	/*
	 * The room's own head count, straight from the frame. It used to be
	 * derived by counting the opponents the frame carried, which in a Battle
	 * Royale is at most one - so a forty-player match read ALIVE 2/40. It
	 * cannot be counted from the arena either, because most frames carry no
	 * arena at all and the number would drop to zero between pushes.
	 */
	if (snap->players > 0)
	{
		state->players_total = snap->players;
		state->players_alive = snap->alive;
	}
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
 * @brief Tells the server which kind of rival to aim this player's garbage at.
 *
 * It is send-and-report rather than send-and-wait: a refused mode leaves the
 * previous one standing and says so, and what confirms a mode took is the next
 * arena, where the cards the mode singles out come back marked.
 *
 * The mode travels as a word rather than as the enum's number. Both ends share
 * the enum, but a body that spelled it as an integer would make a reordering
 * of the four a silent change of meaning on the wire.
 *
 * @param net Client with a match running.
 * @param mode The mode the player selected.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when the server answered, -1 on a transport failure.
 */
int	net_match_set_target(t_net_client *net, t_target_mode mode,
		t_net_result *out)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			body[32];

	if (net == NULL || net->state != NET_IN_GAME || net->room[0] == '\0')
		return (-1);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, net->room);
	snprintf(body, sizeof(body), "mode %s\n", net_target_mode_word(mode));
	memset(&result, 0, sizeof(result));
	if (net_request(net, "TARGET", path, body, &result) != 0)
		return (-1);
	if (out != NULL)
		*out = result;
	return (0);
}

/**
 * @brief Names one targeting mode the way the wire spells it.
 *
 * @param mode The mode.
 * @return The word for it; "random" for anything unrecognised, which is the
 *         mode every other one falls back to anyway.
 */
const char	*net_target_mode_word(t_target_mode mode)
{
	if (mode == TARGET_KO)
		return ("ko");
	if (mode == TARGET_ATTACKERS)
		return ("attackers");
	if (mode == TARGET_BADGES)
		return ("badges");
	return ("random");
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

	/*
	 * Double's cards are cleared and rewritten from every frame, because every
	 * Double frame carries the rival. A Battle Royale's are not: its cards come
	 * from the arena, which rides a slower clock, so most frames say nothing
	 * about them - and clearing on those would blank the whole screen between
	 * pushes, which is the exact failure the codec's `arena absent` exists to
	 * avoid. There the clearing belongs to apply_arena, which does it only when
	 * a push has arrived to replace them.
	 */
	if (state->mode != APP_GAME_MODE_BATTLE_ROYALE)
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

/**
 * @brief Replaces the arena from a push, or leaves it alone when there is none.
 *
 * A push is the complete roster, so the cards are cleared and rewritten: a seat
 * the push does not mention is a seat nobody is in, and that is the only way a
 * player who left stops being drawn. An eliminated player is still in the push,
 * with their alive bit clear - absence means "gone from the room", never
 * "knocked out".
 *
 * A frame carrying no arena is not an empty arena. Most frames carry none,
 * because the arena rides a slower clock than the board does, and clearing on
 * those would blink the whole screen between pushes. `arena_present` is the
 * difference and it is why the codec sends `absent` rather than a count of 0.
 *
 * The clearing is of the seats the push did not mention, and not of the whole
 * array, because a card is allowed to arrive without a board: a dead board
 * never changes again, so the server sends its mask on every fifth push only
 * and the client keeps the one it holds in between. Blanking every card first
 * and rewriting the ones the push describes throws that away four pushes in
 * five, and a knocked-out player's thumbnail blinks for the rest of the match.
 *
 * @param state Match model to write.
 * @param snap Snapshot that may carry an arena.
 * @param local This client's player id, so its own card can be marked.
 */
static void	apply_arena(t_mp_match_state *state, const t_body_state *snap,
		uint64_t local)
{
	bool	seated[APP_ROOM_MAX_PLAYERS];
	size_t	index;

	if (!snap->arena_present)
		return ;
	memset(seated, 0, sizeof(seated));
	index = 0;
	while (index < snap->arena_count && index < BODY_ARENA_MAX)
	{
		apply_arena_card(state, &snap->arena[index], local, seated);
		index++;
	}
	forget_empty_seats(state, seated);
	count_the_arena(state);
}

/**
 * @brief Reads the two HUD numbers that are properties of the whole arena.
 *
 * Both were invented before the server sent them: the knockout count was never
 * written by any line in the client, and the attacker count was set once to
 * the constant 2 by the fixture and never moved. They are read here rather
 * than per card because each is a fact about the room - how many rivals this
 * player has buried, and how many of them are currently burying them.
 *
 * @param state Match model whose arena has just been replaced.
 */
static void	count_the_arena(t_mp_match_state *state)
{
	int	attackers;
	int	slot;

	attackers = 0;
	slot = 0;
	while (slot < APP_ROOM_MAX_PLAYERS)
	{
		if (state->opponents[slot].present)
		{
			if (state->opponents[slot].local)
				state->ko_count = state->opponents[slot].ko;
			else if (state->opponents[slot].targeting_local)
				attackers++;
		}
		slot++;
	}
	state->incoming_attackers = attackers;
}

/**
 * @brief Blanks every seat the push did not carry a card for.
 *
 * A push is the whole roster, so a seat missing from it is a seat nobody is
 * in - a player who left the room rather than one who was knocked out, whose
 * card is still sent with its alive bit clear.
 *
 * @param state Match model holding the cards.
 * @param seated Which seats the push described.
 */
static void	forget_empty_seats(t_mp_match_state *state, const bool *seated)
{
	int	slot;

	slot = 0;
	while (slot < APP_ROOM_MAX_PLAYERS)
	{
		if (!seated[slot])
			memset(&state->opponents[slot], 0,
				sizeof(state->opponents[slot]));
		slot++;
	}
}

/**
 * @brief Files one card under the seat it belongs to.
 *
 * The slot is the index, not the order the card arrived in. Two things depend
 * on it: a card keeps its place on screen across pushes, so nobody slides
 * sideways when a player above them is knocked out; and the slot is what every
 * later feature names - a target, an attacker, a line on the knockout feed.
 *
 * @param state Match model holding the cards.
 * @param card The card to file.
 * @param local The player id this frame was built for.
 * @param seated Records that this seat was in the push.
 */
static void	apply_arena_card(t_mp_match_state *state,
		const t_body_arena_slot *card, uint64_t local, bool *seated)
{
	int	slot;

	slot = card->slot;
	if (slot < 0 || slot >= APP_ROOM_MAX_PLAYERS)
		return ;
	seated[slot] = true;
	state->opponents[slot].present = true;
	state->opponents[slot].alive = (card->flags & BODY_ARENA_ALIVE) != 0;
	state->opponents[slot].targeting_local
		= (card->flags & BODY_ARENA_ATTACKING_YOU) != 0;
	state->opponents[slot].targeted_by_local
		= (card->flags & BODY_ARENA_TARGETED_BY_YOU) != 0;
	state->opponents[slot].local = (local != 0 && card->player_id == local);
	state->opponents[slot].garbage_pending = card->pending;
	state->opponents[slot].ko = card->ko;
	state->opponents[slot].rank = card->rank;
	state->opponents[slot].lines = card->lines;
	/*
	 * The seat, which a room numbers from 1 - not the seat plus one. Until the
	 * arena carries a username, the seat is the only name a card has, and a
	 * card that said P2 while the roster said seat 1 was two names for one
	 * player.
	 */
	snprintf(state->opponents[slot].name,
		sizeof(state->opponents[slot].name), "P%d", card->slot);
	/*
	 * A card without a mask is a dead board that has not changed since the
	 * last one carrying one, so the board already held is still correct and is
	 * deliberately left standing.
	 */
	if (card->mask_valid)
		apply_arena_mask(&state->opponents[slot].board, card);
}

/**
 * @brief Unpacks a card's occupancy mask onto a board.
 *
 * Every filled cell gets the same colour, because that is all the wire carries:
 * a card is a few terminal cells wide, so the colour and the piece type a full
 * board spends two nibbles a cell on are information this can neither show nor
 * afford.
 *
 * @param board Board to overwrite.
 * @param card The card to read.
 */
static void	apply_arena_mask(t_board *board, const t_body_arena_slot *card)
{
	t_cell	cell;
	int		row;
	int		col;

	board_init(board);
	memset(&cell, 0, sizeof(cell));
	cell.type = CELL_FILLED;
	cell.color = MP_ARENA_CARD_COLOR;
	row = 0;
	while (row < BOARD_HEIGHT && row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BOARD_WIDTH && col < BODY_BOARD_COLS)
		{
			if ((card->mask[row][col / 8] >> (col % 8)) & 1u)
				board_set(board, col, row, cell);
			col++;
		}
		row++;
	}
}
