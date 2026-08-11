#include "tetrisu.h"

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
 * @return true when WAITING/READY changed; false for stable or terminal rooms.
 */
bool	waiting_room_sync_state(t_app_room_view_model *room)
{
	t_app_room_state	next;
	int					minimum;

	if (!valid_room_snapshot(room) || room->state == APP_ROOM_STATE_IN_GAME
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
	int	required_players;

	if (room == NULL)
		return (ROOM_FEEDBACK_NEED_PLAYERS);
	if (!valid_room_snapshot(room))
		return (ROOM_FEEDBACK_INVALID_ROOM);
	if (room->state != APP_ROOM_STATE_WAITING
		&& room->state != APP_ROOM_STATE_READY)
		return (ROOM_FEEDBACK_UNAVAILABLE);
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
	int index, char *out, size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (room == NULL || index < 0 || index >= waiting_room_slot_count(room))
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
const char	*waiting_room_badge_text(const t_app_room_view_model *room,
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
		snprintf(out, size, "NOT ENOUGH PLAYERS YET");
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
	return (out);
}

/**
 * @brief Returns the navigation action that launches this room's match.
 *
 * @param room Room snapshot to read.
 * @return APP_NAV_START_DOUBLE or APP_NAV_START_BATTLE_ROYALE.
 */
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

static void	move_roster(t_waiting_room_state *state,
	const t_app_room_view_model *room, int delta)
{
	int	maximum;

	maximum = waiting_room_slot_count(room)
		- waiting_room_visible_slot_count(room);
	if (maximum < 0)
		maximum = 0;
	state->roster_offset += delta;
	if (state->roster_offset < 0)
		state->roster_offset = 0;
	if (state->roster_offset > maximum)
		state->roster_offset = maximum;
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
