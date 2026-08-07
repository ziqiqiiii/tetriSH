#include "tetrisu.h"

// Static Functions
static room_action_t	handle_room_key(waiting_room_state_t *state,
							uint32_t key);
static room_action_t	handle_chat_key(waiting_room_state_t *state,
							uint32_t key);
static bool	append_compose(waiting_room_state_t *state, uint32_t key);
static bool	is_confirm_key(uint32_t key);
static bool	valid_slot(const app_room_view_model_t *room, int index);

/**
 * @brief Resets the waiting room to browsing, not typing and not counting down.
 *
 * @param state Waiting-room state to initialise.
 */
void	waiting_room_state_init(waiting_room_state_t *state)
{
	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	state->chatting = false;
	state->counting_down = false;
	state->countdown = 0;
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
 * @param key Key identifier from Notcurses.
 * @return The resolved action, or ROOM_ACTION_NONE when nothing else happened.
 */
room_action_t	waiting_room_handle_key(waiting_room_state_t *state,
	uint32_t key)
{
	if (state == NULL)
		return (ROOM_ACTION_NONE);
	if (key == NCKEY_RESIZE)
		return (ROOM_ACTION_NONE);
	if (state->chatting)
		return (handle_chat_key(state, key));
	return (handle_room_key(state, key));
}

/**
 * @brief Reports whether two waiting-room states would draw differently.
 *
 * @param before State captured before the keystroke.
 * @param after State after the keystroke.
 * @return true when a repaint is required.
 */
bool	waiting_room_state_view_changed(const waiting_room_state_t *before,
	const waiting_room_state_t *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->chatting != after->chatting
		|| before->counting_down != after->counting_down
		|| before->countdown != after->countdown
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value
		|| before->compose_length != after->compose_length
		|| strncmp(before->compose, after->compose,
			APP_ROOM_CHAT_TEXT_MAX) != 0);
}

/**
 * @brief Reports whether a held key may be folded into one repaint.
 *
 * Nothing on this screen repeats usefully: there is no cursor to hold down, and
 * folding typed characters would drop letters out of a chat message.
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
bool	waiting_room_action_leaves_screen(room_action_t action)
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
int	waiting_room_ready_count(const app_room_view_model_t *room)
{
	int	index;
	int	ready;

	if (room == NULL)
		return (0);
	ready = 0;
	index = 0;
	while (index < room->player_count && index < APP_ROOM_MAX_PLAYERS)
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
 * needs a strict majority, so a lobby of eight starts on five rather than
 * waiting for a straggler who has gone to make tea.
 *
 * @param room Room snapshot to measure.
 * @return The ready threshold, or 0 when the room is missing.
 */
int	waiting_room_required_ready(const app_room_view_model_t *room)
{
	if (room == NULL)
		return (0);
	if (room->mode == APP_GAME_MODE_DOUBLE)
		return (WAITING_ROOM_DOUBLE_PLAYERS);
	return (room->player_count / 2 + 1);
}

/**
 * @brief Reports whether the room satisfies every condition for starting.
 *
 * @param room Room snapshot to test.
 * @return true when a start would be accepted.
 */
bool	waiting_room_can_start(const app_room_view_model_t *room)
{
	if (room == NULL || room->state != APP_ROOM_STATE_WAITING)
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
 * @brief Reports whether the local player owns this room.
 *
 * @param room Room snapshot to read.
 * @return true when the local seat carries the owner flag.
 */
bool	waiting_room_local_is_owner(const app_room_view_model_t *room)
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
bool	waiting_room_local_ready(const app_room_view_model_t *room)
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
bool	waiting_room_toggle_ready(app_room_view_model_t *room)
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
room_feedback_t	waiting_room_start_blocker(const app_room_view_model_t *room)
{
	int	required_players;

	if (room == NULL)
		return (ROOM_FEEDBACK_NEED_PLAYERS);
	if (!waiting_room_local_is_owner(room))
		return (ROOM_FEEDBACK_NOT_OWNER);
	if (room->mode == APP_GAME_MODE_DOUBLE)
		required_players = WAITING_ROOM_DOUBLE_PLAYERS;
	else
		required_players = WAITING_ROOM_ROYALE_MIN_PLAYERS;
	if (room->player_count < required_players)
		return (ROOM_FEEDBACK_NEED_PLAYERS);
	if (waiting_room_ready_count(room) < waiting_room_required_ready(room))
		return (ROOM_FEEDBACK_NEED_READY);
	return (ROOM_FEEDBACK_NONE);
}

/**
 * @brief Arms the pre-match countdown.
 *
 * @param state Waiting-room state to update.
 * @return true when the countdown was not already running.
 */
bool	waiting_room_begin_countdown(waiting_room_state_t *state)
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
bool	waiting_room_cancel_countdown(waiting_room_state_t *state)
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
bool	waiting_room_tick(waiting_room_state_t *state)
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
bool	waiting_room_append_chat(app_room_view_model_t *room,
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
bool	waiting_room_send_chat(app_room_view_model_t *room,
	waiting_room_state_t *state)
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
const char	*waiting_room_status_text(const app_room_view_model_t *room,
	const waiting_room_state_t *state, char *out, size_t size)
{
	int	required_players;

	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (room == NULL)
		return (out);
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
	return (out);
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
const char	*waiting_room_slot_label(const app_room_view_model_t *room,
	int index, char *out, size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (room == NULL || index < 0 || index >= APP_ROOM_MAX_PLAYERS)
		return (out);
	if (index >= room->player_count)
	{
		snprintf(out, size, "%d. (empty)", index + 1);
		return (out);
	}
	if (room->players[index].owner)
		snprintf(out, size, "%d. %s (owner)", index + 1,
			room->players[index].username);
	else
		snprintf(out, size, "%d. %s", index + 1,
			room->players[index].username);
	return (out);
}

/**
 * @brief Returns the ready badge printed beside one seat.
 *
 * @param room Room snapshot to read.
 * @param index Seat index.
 * @return A static, bounded caption; "" for a seat nobody occupies.
 */
const char	*waiting_room_badge_text(const app_room_view_model_t *room,
	int index)
{
	if (room == NULL || index < 0 || index >= room->player_count
		|| index >= APP_ROOM_MAX_PLAYERS)
		return ("");
	if (room->players[index].ready)
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
const char	*waiting_room_feedback_text(const waiting_room_state_t *state,
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
		snprintf(out, size, "NOT ENOUGH PLAYERS YET");
	else if (state->feedback == ROOM_FEEDBACK_NEED_READY)
		snprintf(out, size, "WAITING FOR MORE PLAYERS TO READY UP");
	else if (state->feedback == ROOM_FEEDBACK_NOT_OWNER)
		snprintf(out, size, "ONLY THE ROOM OWNER CAN START");
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
	return (out);
}

/**
 * @brief Returns the navigation action that launches this room's match.
 *
 * @param room Room snapshot to read.
 * @return APP_NAV_START_DOUBLE or APP_NAV_START_BATTLE_ROYALE.
 */
app_nav_action_t	waiting_room_launch_action(
	const app_room_view_model_t *room)
{
	if (room != NULL && room->mode == APP_GAME_MODE_BATTLE_ROYALE)
		return (APP_NAV_START_BATTLE_ROYALE);
	return (APP_NAV_START_DOUBLE);
}

/**
 * @brief Reads one keystroke outside the composer, where letters are commands.
 */
static room_action_t	handle_room_key(waiting_room_state_t *state,
	uint32_t key)
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
	if (key == 's' || key == 'S')
		return (ROOM_ACTION_START);
	if (key == 'c' || key == 'C' || is_confirm_key(key))
	{
		state->chatting = true;
		state->feedback = ROOM_FEEDBACK_NONE;
		return (ROOM_ACTION_NONE);
	}
	return (ROOM_ACTION_NONE);
}

/**
 * @brief Reads one keystroke inside the composer, where letters are text.
 */
static room_action_t	handle_chat_key(waiting_room_state_t *state,
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
static bool	append_compose(waiting_room_state_t *state, uint32_t key)
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

static bool	valid_slot(const app_room_view_model_t *room, int index)
{
	if (room == NULL || index < 0 || index >= room->player_count
		|| index >= APP_ROOM_MAX_PLAYERS)
		return (false);
	return (true);
}
