#include "tetrisu.h"
#include "tetrisu_bot.h"

// Static Functions
static t_room_action	handle_room_key(t_waiting_room_state *state,
							const t_app_room_view_model *room, uint32_t key);
static t_room_action	handle_chat_key(t_waiting_room_state *state,
							uint32_t key);
static bool	append_compose(t_waiting_room_state *state, uint32_t key);
static bool	is_confirm_key(uint32_t key);
static bool	valid_slot(const t_app_room_view_model *room, int index);
static bool	valid_room_snapshot(const t_app_room_view_model *room);
static void	append_fighter(const t_waiting_room_state *state, char *out,
				size_t size);
static void	bot_feedback_text(t_room_feedback feedback, int value,
				char *out, size_t size);
static void	move_roster(t_waiting_room_state *state,
					const t_app_room_view_model *room, int delta);

/**
 * @brief Resets the waiting room to browsing, not typing and not counting down.
 *
 * @param state Waiting-room state to initialise.
 */
void	waiting_room_state_init(t_waiting_room_state *state)
{
	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	state->chatting = false;
	state->counting_down = false;
	state->countdown = 0;
	state->roster_cursor = 0;
	state->roster_offset = 0;
	state->feedback = ROOM_FEEDBACK_NONE;
	state->compose[0] = '\0';
	state->compose_length = 0;
}

/**
 * @brief Applies one keystroke to the waiting room.
 *
 * The screen has two input modes. While composing a chat message every
 * printable key is text and the single-letter commands are deliberately
 * unreachable, so a message containing "s" cannot start the match. Escape
 * leaves the composer rather than the room.
 *
 * @param state Waiting-room state to update.
 * @param room Current snapshot used to bound roster movement.
 * @param key Key identifier from Notcurses.
 * @return The resolved action, or ROOM_ACTION_NONE when nothing else happened.
 */
t_room_action	waiting_room_handle_key(t_waiting_room_state *state,
	const t_app_room_view_model *room, uint32_t key)
{
	if (state == NULL)
		return (ROOM_ACTION_NONE);
	if (key == NCKEY_RESIZE)
		return (ROOM_ACTION_NONE);
	if (state->chatting)
		return (handle_chat_key(state, key));
	return (handle_room_key(state, room, key));
}

/**
 * @brief Reports whether two waiting-room states would draw differently.
 *
 * @param before State captured before the keystroke.
 * @param after State after the keystroke.
 * @return true when a repaint is required.
 */
bool	waiting_room_state_view_changed(const t_waiting_room_state *before,
	const t_waiting_room_state *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->chatting != after->chatting
		|| before->counting_down != after->counting_down
		|| before->countdown != after->countdown
		|| before->roster_cursor != after->roster_cursor
		|| before->roster_offset != after->roster_offset
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value
		|| before->compose_length != after->compose_length
		|| strncmp(before->compose, after->compose,
			APP_ROOM_CHAT_TEXT_MAX) != 0);
}

/**
 * @brief Reports whether a held key may be folded into one repaint.
 *
 * This loop deliberately does not batch input. Roster scrolling is cheap, and
 * using one no-coalescing policy across its browsing and chat modes guarantees
 * that repeated printable characters are never dropped from a message.
 *
 * @param active_key The key currently being handled.
 * @param queued_key A key already waiting in the input queue.
 * @return false; the waiting room never coalesces input.
 */
bool	waiting_room_navigation_keys_coalesce(uint32_t active_key,
	uint32_t queued_key)
{
	(void)active_key;
	(void)queued_key;
	return (false);
}

/**
 * @brief Reports whether an action ends the waiting room's input loop.
 *
 * @param action Action returned by waiting_room_handle_key().
 * @return true when the screen is about to be left.
 */
bool	waiting_room_action_leaves_screen(t_room_action action)
{
	return (action == ROOM_ACTION_LEAVE || action == ROOM_ACTION_LAUNCH
		|| action == ROOM_ACTION_QUIT);
}

/**
 * @brief Counts the seated players who have marked themselves ready.
 *
 * @param room Room snapshot to scan.
 * @return The ready count, 0 when the room is empty or missing.
 */
int	waiting_room_ready_count(const t_app_room_view_model *room)
{
	int	index;
	int	players;
	int	ready;

	if (room == NULL)
		return (0);
	players = room->player_count;
	if (players < 0)
		return (0);
	if (players > waiting_room_slot_count(room))
		players = waiting_room_slot_count(room);
	ready = 0;
	index = 0;
	while (index < players)
	{
		if (room->players[index].ready)
			ready++;
		index++;
	}
	return (ready);
}

/**
 * @brief Returns how many ready players this room needs before it may start.
 *
 * Double needs both seats: a duel with one player is not a duel. Battle Royale
 * needs every occupied seat ready before its owner may start the match.
 *
 * @param room Room snapshot to measure.
 * @return The ready threshold, or 0 when the room is missing.
 */
int	waiting_room_required_ready(const t_app_room_view_model *room)
{
	if (room == NULL)
		return (0);
	if (room->mode == APP_GAME_MODE_DOUBLE)
		return (WAITING_ROOM_DOUBLE_PLAYERS);
	return (room->player_count);
}

/**
 * @brief Returns a room's safe logical slot count.
 *
 * Provider data is untrusted at this boundary. The result is always suitable
 * for indexing the fixed player array, even while an invalid snapshot is being
 * rendered with an explanatory status elsewhere.
 */
int	waiting_room_slot_count(const t_app_room_view_model *room)
{
	int	seats;

	if (room == NULL)
		return (0);
	seats = room->capacity > 0 ? room->capacity : room->player_count;
	if (seats < 0)
		return (0);
	if (seats > APP_ROOM_MAX_PLAYERS)
		return (APP_ROOM_MAX_PLAYERS);
	return (seats);
}

/**
 * @brief Returns how many roster rows fit the authored waiting-room panel.
 */
int	waiting_room_visible_slot_count(const t_app_room_view_model *room)
{
	int	seats;

	seats = waiting_room_slot_count(room);
	if (seats > WAITING_ROOM_VISIBLE_PLAYERS)
		return (WAITING_ROOM_VISIBLE_PLAYERS);
	return (seats);
}

/**
 * @brief Reports whether the room satisfies every condition for starting.
 *
 * @param room Room snapshot to test.
 * @return true when a start would be accepted.
 */
bool	waiting_room_can_start(const t_app_room_view_model *room)
{
	if (!valid_room_snapshot(room)
		|| (room->state != APP_ROOM_STATE_WAITING
			&& room->state != APP_ROOM_STATE_READY))
		return (false);
	if (room->mode == APP_GAME_MODE_DOUBLE)
		return (room->player_count >= WAITING_ROOM_DOUBLE_PLAYERS
			&& waiting_room_ready_count(room) >= WAITING_ROOM_DOUBLE_PLAYERS);
	if (room->player_count < WAITING_ROOM_ROYALE_MIN_PLAYERS)
		return (false);
	return (waiting_room_ready_count(room)
		>= waiting_room_required_ready(room));
}

/**
 * @brief Reports whether readiness alone should arm the countdown.
 *
 * Double is deliberately frictionless once both players are ready. Battle
 * Royale always waits for an explicit owner Start action.
 */
bool	waiting_room_auto_start_allowed(const t_app_room_view_model *room)
{
	return (room != NULL && room->mode == APP_GAME_MODE_DOUBLE
		&& waiting_room_can_start(room));
}

/**
 * @brief Reconciles the local room status with players and ready flags.
 *
 * Server-backed rooms will arrive already reconciled. The fixture provider is
 * mutable in-process, so ready toggles use this same transition rule locally.
 *
 * Only the two statuses this rule can express are its to move. SELECTING is
 * the third, and it was being recomputed away: a room the server had already
 * committed came back as READY, which is what the launch check reads as "not
 * under way" - so the screen stayed put and an S pressed in that window asked
 * a selecting room to start and was refused.
 *
 * @return true when WAITING/READY changed; false for stable or terminal rooms.
 */
bool	waiting_room_sync_state(t_app_room_view_model *room)
{
	t_app_room_state	next;
	int					minimum;

	if (!valid_room_snapshot(room) || room->state == APP_ROOM_STATE_IN_GAME
		|| room->state == APP_ROOM_STATE_SELECTING
		|| room->state == APP_ROOM_STATE_FINISHED)
		return (false);
	minimum = room->mode == APP_GAME_MODE_DOUBLE
		? WAITING_ROOM_DOUBLE_PLAYERS : WAITING_ROOM_ROYALE_MIN_PLAYERS;
	next = APP_ROOM_STATE_WAITING;
	if (room->player_count >= minimum
		&& waiting_room_ready_count(room) == room->player_count)
		next = APP_ROOM_STATE_READY;
	if (room->state == next)
		return (false);
	room->state = next;
	return (true);
}

/**
 * @brief Reports whether the local player owns this room.
 *
 * @param room Room snapshot to read.
 * @return true when the local seat carries the owner flag.
 */
bool	waiting_room_local_is_owner(const t_app_room_view_model *room)
{
	if (!valid_slot(room, room == NULL ? -1 : room->local_slot))
		return (false);
	return (room->players[room->local_slot].owner);
}

/**
 * @brief Reports whether the local player has marked themselves ready.
 *
 * @param room Room snapshot to read.
 * @return true when the local seat is ready.
 */
bool	waiting_room_local_ready(const t_app_room_view_model *room)
{
	if (!valid_slot(room, room == NULL ? -1 : room->local_slot))
		return (false);
	return (room->players[room->local_slot].ready);
}

/**
 * @brief Flips the local player's ready flag.
 *
 * The server will own this decision once it exists; until then the local model
 * is authoritative so the ready and countdown paths can be exercised.
 *
 * @param room Room snapshot to update.
 * @return true when a flag actually moved.
 */
bool	waiting_room_toggle_ready(t_app_room_view_model *room)
{
	if (!valid_slot(room, room == NULL ? -1 : room->local_slot))
		return (false);
	room->players[room->local_slot].ready
		= !room->players[room->local_slot].ready;
	return (true);
}

/**
 * @brief Explains why a start would be refused, if it would be.
 *
 * @param room Room snapshot to test.
 * @return ROOM_FEEDBACK_NONE when the start may proceed.
 */
t_room_feedback	waiting_room_start_blocker(const t_app_room_view_model *room)
{
	if (room == NULL)
		return (ROOM_FEEDBACK_NEED_PLAYERS);
	if (!valid_room_snapshot(room))
		return (ROOM_FEEDBACK_INVALID_ROOM);
	if (room->state != APP_ROOM_STATE_WAITING
		&& room->state != APP_ROOM_STATE_READY)
		return (ROOM_FEEDBACK_UNAVAILABLE);
	if (!waiting_room_local_is_owner(room))
		return (ROOM_FEEDBACK_NOT_OWNER);
	if (waiting_room_players_needed(room) > 0)
		return (ROOM_FEEDBACK_NEED_PLAYERS);
	if (waiting_room_ready_count(room) < waiting_room_required_ready(room))
		return (ROOM_FEEDBACK_NEED_READY);
	return (ROOM_FEEDBACK_NONE);
}

/**
 * @brief How many more players this room needs before it can be started.
 *
 * The number rather than the fact, because "not enough players" is a poor
 * thing to tell somebody who cannot see how many are missing - and in a Battle
 * Royale, where the minimum is four and a player with two laptops has two, it
 * is the number that says whether bots can close the gap.
 *
 * @param room Room snapshot to test.
 * @return How many more are needed, 0 when the room already has enough.
 */
int	waiting_room_players_needed(const t_app_room_view_model *room)
{
	int	required_players;

	if (room == NULL)
		return (0);
	if (room->mode == APP_GAME_MODE_DOUBLE)
		required_players = WAITING_ROOM_DOUBLE_PLAYERS;
	else
		required_players = WAITING_ROOM_ROYALE_MIN_PLAYERS;
	if (room->player_count >= required_players)
		return (0);
	return (required_players - room->player_count);
}

/**
 * @brief Arms the pre-match countdown.
 *
 * @param state Waiting-room state to update.
 * @return true when the countdown was not already running.
 */
bool	waiting_room_begin_countdown(t_waiting_room_state *state)
{
	if (state == NULL || state->counting_down)
		return (false);
	state->counting_down = true;
	state->countdown = WAITING_ROOM_COUNTDOWN_START;
	state->feedback = ROOM_FEEDBACK_NONE;
	return (true);
}

/**
 * @brief Disarms the countdown, used when the room stops being startable.
 *
 * @param state Waiting-room state to update.
 * @return true when a running countdown was stopped.
 */
bool	waiting_room_cancel_countdown(t_waiting_room_state *state)
{
	if (state == NULL || !state->counting_down)
		return (false);
	state->counting_down = false;
	state->countdown = 0;
	state->feedback = ROOM_FEEDBACK_CANCELLED;
	state->feedback_value = 0;
	return (true);
}

/**
 * @brief Advances the countdown by one second.
 *
 * @param state Waiting-room state to update.
 * @return true when the countdown reached zero and the match should launch.
 */
bool	waiting_room_tick(t_waiting_room_state *state)
{
	if (state == NULL || !state->counting_down)
		return (false);
	if (state->countdown > 0)
		state->countdown--;
	if (state->countdown > 0)
		return (false);
	state->counting_down = false;
	return (true);
}

/**
 * @brief Appends one line to the room transcript, dropping the oldest if full.
 *
 * The transcript is a fixed ring rather than a growing buffer because the
 * server will replay only a bounded tail anyway, and because a waiting room
 * that is left open must not grow without bound.
 *
 * @param room Room snapshot to update.
 * @param author Sender name; ignored for system lines.
 * @param text Message body.
 * @param system true for room events rather than player speech.
 * @return true when a line was stored.
 */
bool	waiting_room_append_chat(t_app_room_view_model *room,
	const char *author, const char *text, bool system)
{
	int	index;

	if (room == NULL || text == NULL || text[0] == '\0')
		return (false);
	if (room->chat_count >= APP_ROOM_CHAT_MAX)
	{
		index = 0;
		while (index + 1 < APP_ROOM_CHAT_MAX)
		{
			room->chat[index] = room->chat[index + 1];
			index++;
		}
		room->chat_count = APP_ROOM_CHAT_MAX - 1;
	}
	index = room->chat_count;
	memset(&room->chat[index], 0, sizeof(room->chat[index]));
	snprintf(room->chat[index].author, sizeof(room->chat[index].author), "%s",
		author == NULL ? "" : author);
	snprintf(room->chat[index].text, sizeof(room->chat[index].text), "%s",
		text);
	room->chat[index].system = system;
	room->chat_count++;
	return (true);
}

/**
 * @brief Posts the composed message and clears the composer.
 *
 * @param room Room snapshot to append to.
 * @param state Waiting-room state holding the composed text.
 * @return true when a message was posted.
 */
bool	waiting_room_send_chat(t_app_room_view_model *room,
	t_waiting_room_state *state)
{
	const char	*author;

	if (room == NULL || state == NULL)
		return (false);
	if (state->compose_length == 0)
	{
		state->feedback = ROOM_FEEDBACK_CHAT_EMPTY;
		state->feedback_value = 0;
		return (false);
	}
	author = "you";
	if (valid_slot(room, room->local_slot))
		author = room->players[room->local_slot].username;
	if (!waiting_room_append_chat(room, author, state->compose, false))
	{
		state->feedback = ROOM_FEEDBACK_CHAT_FULL;
		state->feedback_value = 0;
		return (false);
	}
	state->compose[0] = '\0';
	state->compose_length = 0;
	state->feedback = ROOM_FEEDBACK_CHAT_SENT;
	state->feedback_value = 0;
	return (true);
}

/**
 * @brief Formats the room's headline status line.
 *
 * The countdown takes priority over everything else because it is the only
 * line that is about to stop being true.
 *
 * @param room Room snapshot to describe.
 * @param state Waiting-room state holding the countdown.
 * @param out Destination buffer.
 * @param size Capacity of out.
 * @return out.
 */
const char	*waiting_room_status_text(const t_app_room_view_model *room,
	const t_waiting_room_state *state, char *out, size_t size)
{
	int	required_players;

	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (room == NULL)
		return (out);
	if (!valid_room_snapshot(room))
	{
		snprintf(out, size, "Invalid room player data");
		return (out);
	}
	if (room->state == APP_ROOM_STATE_IN_GAME)
	{
		snprintf(out, size, "Game in progress");
		return (out);
	}
	if (room->state == APP_ROOM_STATE_FINISHED)
	{
		snprintf(out, size, "Game over, recording the results");
		return (out);
	}
	if (state != NULL && state->counting_down)
	{
		snprintf(out, size, "Starting in %d...", state->countdown);
		return (out);
	}
	if (room->mode == APP_GAME_MODE_DOUBLE)
		required_players = WAITING_ROOM_DOUBLE_PLAYERS;
	else
		required_players = WAITING_ROOM_ROYALE_MIN_PLAYERS;
	if (room->player_count < required_players)
		snprintf(out, size, "Waiting for opponents (%d/%d)",
			room->player_count, required_players);
	else if (waiting_room_ready_count(room) < waiting_room_required_ready(room))
		snprintf(out, size, "Waiting for players to ready (%d/%d)",
			waiting_room_ready_count(room), waiting_room_required_ready(room));
	else
		snprintf(out, size, "Ready to start");
	append_fighter(state, out, size);
	return (out);
}

/**
 * @brief Appends the chosen fighter to a status line, when one is chosen.
 *
 * It lives on the status line rather than in a panel of its own because it is
 * a thing about this player's next match, exactly like the readiness the line
 * already reports - and because both renderers are handed this text, so saying
 * it here says it in both without either learning what a roster is.
 *
 * @param state Waiting-room state holding the name, possibly NULL.
 * @param out Status line to append to.
 * @param size Capacity of out.
 */
static void	append_fighter(const t_waiting_room_state *state, char *out,
	size_t size)
{
	size_t	used;

	if (state == NULL || state->character_name[0] == '\0')
		return ;
	used = strlen(out);
	if (used + 16 >= size)
		return ;
	snprintf(out + used, size - used, "  -  fighter %s",
		state->character_name);
}

/**
 * @brief Formats one seat of the slot list.
 *
 * @param room Room snapshot to read.
 * @param index Seat index, including seats past the current player count.
 * @param out Destination buffer.
 * @param size Capacity of out.
 * @return out, holding "N. (empty)" for a seat nobody occupies.
 */
const char	*waiting_room_slot_label(const t_app_room_view_model *room,
	int position, char *out, size_t size)
{
	int	seat;

	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (room == NULL || position < 0
		|| position >= waiting_room_slot_count(room))
		return (out);
	if (position >= room->player_count)
	{
		snprintf(out, size, "%d. (empty)", position + 1);
		return (out);
	}
	seat = waiting_room_seat_index(room, position);
	snprintf(out, size, "%d. %s%s", position + 1, room->players[seat].username,
		room->players[seat].owner ? " (owner)"
		: waiting_room_seat_is_bot(room, position) ? " (bot)" : "");
	return (out);
}

/**
 * @brief Which seat of the model the roster's Nth line is showing.
 *
 * The one place the roster's order is written down, so that the label, the
 * badge, the bot mark and the highlight cannot disagree about which player a
 * line belongs to.
 *
 * Occupied seats read newest first. A Battle Royale shows eight lines of
 * ninety-nine, so somebody who joins - or a bot that is added - lands out of
 * sight at the bottom and the room looks like it did nothing. Reversing puts
 * the arrival on the first line, where the person who caused it is looking.
 *
 * Only the occupied block is reversed, never the empty seats after it, so
 * "the first player_count positions are taken" stays true and every occupancy
 * test in the renderers is unaffected.
 *
 * Newest is taken to be the highest occupied seat, which is what the server's
 * ordering gives: members arrive in slot order and a joiner takes the lowest
 * free seat. After somebody leaves mid-room their seat is reused, so a player
 * who fills a gap appears where the gap was rather than at the top. The
 * snapshot carries no arrival time to do better with, and the case this is
 * for - a room filling up - is exactly the case it gets right.
 *
 * @param room Room snapshot to read.
 * @param position Line of the roster, from the top.
 * @return The index into room->players, or -1 when there is no such line.
 */
int	waiting_room_seat_index(const t_app_room_view_model *room, int position)
{
	int	occupied;

	if (room == NULL || position < 0)
		return (-1);
	occupied = room->player_count;
	if (occupied > APP_ROOM_MAX_PLAYERS)
		occupied = APP_ROOM_MAX_PLAYERS;
	if (position >= occupied)
		return (position);
	return (occupied - 1 - position);
}

/**
 * @brief Whether the seat on this roster line has declared itself ready.
 *
 * The renderers colour the badge before they print it, and reaching into
 * players[] to ask would be reaching past the ordering this file owns.
 *
 * @param room Room snapshot to read.
 * @param position Line of the roster, from the top.
 * @return true when that seat is occupied and ready.
 */
bool	waiting_room_seat_ready(const t_app_room_view_model *room, int position)
{
	int	seat;

	seat = waiting_room_seat_index(room, position);
	if (room == NULL || seat < 0 || position >= room->player_count
		|| seat >= APP_ROOM_MAX_PLAYERS)
		return (false);
	return (room->players[seat].ready);
}

/**
 * @brief The control legend, at whichever length the panel can print.
 *
 * Two keys were added to a line that already fitted exactly. The cell
 * renderer's narrowest supported panel is MP_COMPAT_MIN_COLS wide and
 * put_centered clips to two columns less than that, so the full legend went in
 * at 68 characters and took `[C] CHAT [L] LEAVE` off the end of it - losing
 * the key that gets a player *out* of the room in order to advertise the one
 * that fills it.
 *
 * So the length is chosen rather than assumed. The short form abbreviates
 * rather than dropping keys, because a legend that silently omits a control on
 * a small terminal is the same bug written more politely.
 *
 * The pixel renderer needs none of this - it scales the glyphs to the box -
 * and always asks for the full one.
 *
 * @param cols Columns the panel has, or 0 to ask for the full legend.
 * @return A static string, never NULL.
 */
const char	*waiting_room_legend(int cols)
{
	if (cols <= 0 || cols - 2 >= (int)sizeof(WAITING_ROOM_LEGEND) - 1)
		return (WAITING_ROOM_LEGEND);
	return (WAITING_ROOM_LEGEND_SHORT);
}

/**
 * @brief Is this seat one of the room's bots?
 *
 * Asked of the name, because the name is all the server says about it: a bot
 * is an ordinary client on an ordinary account, and the only thing that marks
 * the account is the reserved prefix no person may sign up with. That is what
 * makes this answerable from a room snapshot at all, and it is right for
 * *every* client in the room rather than only for the one that spawned them -
 * a player who joined somebody else's room sees the bots in it as bots.
 *
 * @param room Room snapshot to read.
 * @param position Line of the roster, from the top.
 * @return true when the seat is occupied by a bot account.
 */
bool	waiting_room_seat_is_bot(const t_app_room_view_model *room,
		int position)
{
	int	seat;

	seat = waiting_room_seat_index(room, position);
	if (room == NULL || seat < 0 || position >= room->player_count
		|| seat >= APP_ROOM_MAX_PLAYERS)
		return (false);
	return (strncmp(room->players[seat].username, BOT_ACCOUNT_PREFIX,
			sizeof(BOT_ACCOUNT_PREFIX) - 1) == 0);
}

/**
 * @brief The account sitting on one roster line, or NULL for an empty seat.
 *
 * The roster is what the player is pointing at and the seat array is what the
 * room is made of, and the two are not the same index - waiting_room_seat_index
 * is the map between them. Everything that acts on "the one I am pointing at"
 * has to go through here rather than indexing players[] with a screen row.
 *
 * @param room Room snapshot to read.
 * @param position Line of the roster, from the top.
 * @return The username, or NULL when that line has nobody on it.
 */
const char	*waiting_room_seat_username(const t_app_room_view_model *room,
		int position)
{
	int	seat;

	seat = waiting_room_seat_index(room, position);
	if (room == NULL || seat < 0 || position >= room->player_count
		|| seat >= APP_ROOM_MAX_PLAYERS)
		return (NULL);
	if (room->players[seat].username[0] == '\0')
		return (NULL);
	return (room->players[seat].username);
}

/**
 * @brief How many of this room's seats are held by bots right now.
 *
 * Asked of the roster rather than of the farm, because the two answer different
 * questions: the farm knows how many processes were started and the roster
 * knows how many of them actually got in. A bot that was spawned and refused is
 * in the first and not the second, which is exactly the gap this is here to
 * measure.
 *
 * @param room Room snapshot to scan.
 * @return The count, 0 when the room is empty or missing.
 */
int	waiting_room_bot_seat_count(const t_app_room_view_model *room)
{
	int	position;
	int	occupied;
	int	bots;

	if (room == NULL)
		return (0);
	occupied = room->player_count;
	if (occupied > APP_ROOM_MAX_PLAYERS)
		occupied = APP_ROOM_MAX_PLAYERS;
	bots = 0;
	position = 0;
	while (position < occupied)
	{
		if (waiting_room_seat_is_bot(room, position))
			bots++;
		position++;
	}
	return (bots);
}

/**
 * @brief How many seats this room still has nobody in.
 *
 * The number B has to respect. A Double room is two seats and the player is
 * one of them, so a second bot there is a process that is certain to be turned
 * away at the door - it forks, connects, completes a handshake, is refused the
 * room, and exits, while the screen says a bot was added. Counting the seats
 * first is how that key answers honestly instead.
 *
 * @param room Room snapshot to measure.
 * @return The free seat count, 0 when the room is full, invalid or missing.
 */
int	waiting_room_free_seats(const t_app_room_view_model *room)
{
	int	free_seats;

	if (!valid_room_snapshot(room))
		return (0);
	free_seats = waiting_room_slot_count(room) - room->player_count;
	if (free_seats < 0)
		return (0);
	return (free_seats);
}

/**
 * @brief Returns the ready badge printed beside one seat.
 *
 * @param room Room snapshot to read.
 * @param index Seat index.
 * @return A static, bounded caption; "" for a seat nobody occupies.
 */
const char	*waiting_room_badge_text(const t_app_room_view_model *room,
	int position)
{
	if (room == NULL || position < 0 || position >= room->player_count
		|| position >= APP_ROOM_MAX_PLAYERS)
		return ("");
	if (waiting_room_seat_ready(room, position))
		return ("ready");
	return ("not ready");
}

/**
 * @brief Formats the waiting room's inline result line.
 *
 * As on every other multiplayer surface this is a line inside a region rather
 * than a notification card, so no plane is ever raised over the screen.
 *
 * @param state Waiting-room state holding the result.
 * @param out Destination buffer.
 * @param size Capacity of out.
 * @return out, holding "" when there is nothing to report.
 */
const char	*waiting_room_feedback_text(const t_waiting_room_state *state,
	char *out, size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (state == NULL)
		return (out);
	if (state->feedback == ROOM_FEEDBACK_READY)
		snprintf(out, size, "YOU ARE READY");
	else if (state->feedback == ROOM_FEEDBACK_NOT_READY)
		snprintf(out, size, "YOU ARE NO LONGER READY");
	else if (state->feedback == ROOM_FEEDBACK_NEED_PLAYERS)
		snprintf(out, size, "NEED %d MORE PLAYER%s", state->feedback_value,
			state->feedback_value == 1 ? "" : "S");
	else if (state->feedback == ROOM_FEEDBACK_NEED_READY)
		snprintf(out, size, "WAITING FOR MORE PLAYERS TO READY UP");
	else if (state->feedback == ROOM_FEEDBACK_NOT_OWNER)
		snprintf(out, size, "ONLY THE ROOM OWNER CAN START");
	else if (state->feedback == ROOM_FEEDBACK_INVALID_ROOM)
		snprintf(out, size, "ROOM PLAYER DATA IS INVALID");
	else if (state->feedback == ROOM_FEEDBACK_UNAVAILABLE)
		snprintf(out, size, "ROOM IS NOT WAITING TO START");
	else if (state->feedback == ROOM_FEEDBACK_CANCELLED)
		snprintf(out, size, "COUNTDOWN CANCELLED");
	else if (state->feedback == ROOM_FEEDBACK_CHAT_SENT)
		snprintf(out, size, "MESSAGE SENT");
	else if (state->feedback == ROOM_FEEDBACK_CHAT_EMPTY)
		snprintf(out, size, "TYPE SOMETHING FIRST");
	else if (state->feedback == ROOM_FEEDBACK_CHAT_FULL)
		snprintf(out, size, "CHAT IS FULL");
	else if (state->feedback == ROOM_FEEDBACK_VOLUME)
		snprintf(out, size, "MUSIC VOLUME %d%%", state->feedback_value);
	else if (state->feedback == ROOM_FEEDBACK_NEED_PLAYERS_BOT)
		snprintf(out, size, "NEED %d MORE - PRESS B TO ADD A BOT",
			state->feedback_value);
	else
		bot_feedback_text(state->feedback, state->feedback_value, out, size);
	return (out);
}

/**
 * @brief Writes the line for whichever bot outcome the last key produced.
 *
 * Separate from the rest so that the chain above stays one `if` per feedback:
 * five more branches in it is the point at which a reader stops finding the
 * one they are looking for.
 *
 * @param feedback The outcome to describe.
 * @param out Buffer receiving the line.
 * @param size Size of out.
 */
static void	bot_feedback_text(t_room_feedback feedback, int value,
			char *out, size_t size)
{
	if (feedback == ROOM_FEEDBACK_BOT_ADDED)
		snprintf(out, size, "BOT ADDED");
	else if (feedback == ROOM_FEEDBACK_BOT_KICKED)
		snprintf(out, size, "BOT KICKED");
	else if (feedback == ROOM_FEEDBACK_BOT_LIMIT)
		snprintf(out, size, "NO MORE ROOM FOR BOTS");
	else if (feedback == ROOM_FEEDBACK_BOT_NONE)
		snprintf(out, size, "NO BOT TO KICK");
	else if (feedback == ROOM_FEEDBACK_BOT_MISSING)
		snprintf(out, size, "NO tetrisu-bot FOUND - REBUILD TETRISU");
	else if (feedback == ROOM_FEEDBACK_BOT_UNAVAILABLE)
		snprintf(out, size, "NO BOT COULD BE STARTED");
	else if (feedback == ROOM_FEEDBACK_BOT_LOST)
		snprintf(out, size, "A BOT STOPPED - SEE " BOT_LOG_NAME);
	else if (feedback == ROOM_FEEDBACK_BOT_NOT_MINE)
		snprintf(out, size, "THAT SEAT IS NOT ONE OF YOUR BOTS");
	else if (feedback == ROOM_FEEDBACK_BOT_NOT_READY)
		snprintf(out, size, "THAT BOT HAS NOT SIGNED IN YET");
	else if (feedback == ROOM_FEEDBACK_BOT_FILLED)
		snprintf(out, size, "FILLED WITH %d BOT%s", value,
			value == 1 ? "" : "S");
}

/**
 * @brief Returns the navigation action that launches this room's match.
 *
 * @param room Room snapshot to read.
 * @return APP_NAV_START_DOUBLE or APP_NAV_START_BATTLE_ROYALE.
 */
/**
 * @brief Answers whether the room has already committed to its next match.
 *
 * Two states mean it has: the boards are dealt, or the select window that
 * precedes them is open. Both are the server's doing and neither wants a
 * START from a client - which is the whole reason this is one question rather
 * than two comparisons repeated at every call site.
 *
 * @param room The room to ask.
 * @return true when the match is already being set up or played.
 */
bool	waiting_room_is_under_way(const t_app_room_view_model *room)
{
	if (room == NULL)
		return (false);
	return (room->state == APP_ROOM_STATE_IN_GAME
		|| room->state == APP_ROOM_STATE_SELECTING);
}

t_app_nav_action	waiting_room_launch_action(
	const t_app_room_view_model *room)
{
	if (room != NULL && room->mode == APP_GAME_MODE_BATTLE_ROYALE)
		return (APP_NAV_START_BATTLE_ROYALE);
	return (APP_NAV_START_DOUBLE);
}

/**
 * @brief Reads one keystroke outside the composer, where letters are commands.
 */
static t_room_action	handle_room_key(t_waiting_room_state *state,
	const t_app_room_view_model *room, uint32_t key)
{
	if (key == 'q' || key == 'Q')
		return (ROOM_ACTION_QUIT);
	if (key == NCKEY_ESC || key == 'l' || key == 'L')
		return (ROOM_ACTION_LEAVE);
	if (key == '+' || key == '=')
		return (ROOM_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (ROOM_ACTION_VOLUME_DOWN);
	if (key == 'r' || key == 'R')
		return (ROOM_ACTION_TOGGLE_READY);
	/*
	 * The fighter is chosen here because the match screen is too late: the
	 * room deals the boards as soon as every seat has declared, so by the
	 * time that screen opens the character it would ask about is already the
	 * one the server is resolving abilities against.
	 */
	if (key == NCKEY_LEFT)
		return (ROOM_ACTION_CHARACTER_PREV);
	if (key == NCKEY_RIGHT)
		return (ROOM_ACTION_CHARACTER_NEXT);
	if (key == 's' || key == 'S')
		return (ROOM_ACTION_START);
	/*
	 * B fills a seat and K empties one. Both are refused to anybody but the
	 * owner here rather than further in, so that a player who is not the owner
	 * is told why instead of watching nothing happen.
	 */
	if (key == 'b' || key == 'B')
		return (ROOM_ACTION_ADD_BOT);
	if (key == 'k' || key == 'K')
		return (ROOM_ACTION_KICK_BOT);
	/*
	 * F1 fills the room with bots past the four B offers. It is a stress tool
	 * and it is on a function key on purpose: nothing advertises it, and no
	 * letter a player might reach for starts fifty processes.
	 */
	if (key == NCKEY_F01)
		return (ROOM_ACTION_FILL_BOTS);
	if (key == NCKEY_UP)
		move_roster(state, room, -1);
	else if (key == NCKEY_DOWN)
		move_roster(state, room, 1);
	else if (key == NCKEY_PGUP)
		move_roster(state, room, -WAITING_ROOM_VISIBLE_PLAYERS);
	else if (key == NCKEY_PGDOWN)
		move_roster(state, room, WAITING_ROOM_VISIBLE_PLAYERS);
	if (key == 'c' || key == 'C' || is_confirm_key(key))
	{
		state->chatting = true;
		state->feedback = ROOM_FEEDBACK_NONE;
		return (ROOM_ACTION_NONE);
	}
	return (ROOM_ACTION_NONE);
}

/**
 * @brief Moves the roster pointer, and scrolls only as far as it has to.
 *
 * The pointer is what moves; the window follows it. It was the other way
 * round - the arrows moved the window and nothing was ever pointed at - which
 * is why K could only ever mean "the bot added last", and why kicking one of
 * several was impossible however carefully the player scrolled.
 *
 * The pointer is bounded by the seats, not the players: a room's empty seats
 * are drawn as rows and skipping over them would make the pointer jump.
 *
 * @param state Waiting-room state whose cursor and offset move.
 * @param room Current snapshot, for the seat and window counts.
 * @param delta How far to move, in rows.
 */
static void	move_roster(t_waiting_room_state *state,
	const t_app_room_view_model *room, int delta)
{
	int	seats;
	int	visible;

	seats = waiting_room_slot_count(room);
	visible = waiting_room_visible_slot_count(room);
	if (seats <= 0)
		return ;
	state->roster_cursor += delta;
	if (state->roster_cursor < 0)
		state->roster_cursor = 0;
	if (state->roster_cursor > seats - 1)
		state->roster_cursor = seats - 1;
	if (state->roster_cursor < state->roster_offset)
		state->roster_offset = state->roster_cursor;
	if (visible > 0 && state->roster_cursor > state->roster_offset + visible - 1)
		state->roster_offset = state->roster_cursor - visible + 1;
	if (state->roster_offset < 0)
		state->roster_offset = 0;
}

/**
 * @brief Reads one keystroke inside the composer, where letters are text.
 */
static t_room_action	handle_chat_key(t_waiting_room_state *state,
	uint32_t key)
{
	if (key == NCKEY_ESC)
	{
		state->chatting = false;
		state->feedback = ROOM_FEEDBACK_NONE;
		return (ROOM_ACTION_NONE);
	}
	if (is_confirm_key(key))
		return (ROOM_ACTION_SEND_CHAT);
	if (key == NCKEY_BACKSPACE || key == 127 || key == 8)
	{
		state->feedback = ROOM_FEEDBACK_NONE;
		if (state->compose_length > 0)
		{
			state->compose_length--;
			state->compose[state->compose_length] = '\0';
		}
		return (ROOM_ACTION_NONE);
	}
	/* Ctrl-U, the readline habit, clears the line in one keystroke. */
	if (key == 21)
	{
		state->feedback = ROOM_FEEDBACK_NONE;
		state->compose[0] = '\0';
		state->compose_length = 0;
		return (ROOM_ACTION_NONE);
	}
	(void)append_compose(state, key);
	return (ROOM_ACTION_NONE);
}

/**
 * @brief Appends one printable character to the message being composed.
 *
 * Anything outside printable ASCII - arrow keys, function keys, multi-byte
 * input - is dropped rather than written as a replacement character, because
 * the chat frame this will become carries plain bytes.
 */
static bool	append_compose(t_waiting_room_state *state, uint32_t key)
{
	if (key < 0x20 || key > 0x7e)
		return (false);
	if (state->compose_length >= APP_ROOM_CHAT_TEXT_MAX - 1)
		return (false);
	state->feedback = ROOM_FEEDBACK_NONE;
	state->compose[state->compose_length] = (char)key;
	state->compose_length++;
	state->compose[state->compose_length] = '\0';
	return (true);
}

static bool	is_confirm_key(uint32_t key)
{
	return (key == NCKEY_ENTER || key == '\n' || key == '\r');
}

static bool	valid_slot(const t_app_room_view_model *room, int index)
{
	if (room == NULL || index < 0 || index >= room->player_count
		|| index >= waiting_room_slot_count(room))
		return (false);
	return (true);
}

static bool	valid_room_snapshot(const t_app_room_view_model *room)
{
	if (room == NULL
		|| !multiplayer_room_capacity_valid(room->mode, room->capacity)
		|| room->player_count < 0 || room->player_count > room->capacity
		|| room->player_count > APP_ROOM_MAX_PLAYERS
		|| room->state < APP_ROOM_STATE_WAITING
		|| room->state > APP_ROOM_STATE_FINISHED)
		return (false);
	return (true);
}
