#include "tetrisu.h"

// Static Functions
static lobby_action_t	handle_rooms_key(lobby_state_t *state, uint32_t key);
static lobby_action_t	handle_join_key(lobby_state_t *state, uint32_t key);
static void	move_selection(lobby_state_t *state, int delta);
static void	clamp_selection(lobby_state_t *state);
static void	cycle_filter(lobby_state_t *state);
static bool	append_room_id(lobby_state_t *state, uint32_t key);
static bool	is_confirm_key(uint32_t key);
static bool	room_matches(const app_room_summary_view_model_t *room,
				app_game_mode_t filter);

/**
 * @brief Prepares the lobby for the mode the player picked.
 *
 * The filter arrives from the mode picker, so a player who chose Double lands
 * on a list of duel rooms. It is a view filter and not a restriction: M cycles
 * to Battle Royale or to every room without leaving the screen.
 *
 * @param state Lobby state to initialise.
 * @param filter Mode to list first; APP_GAME_MODE_NONE lists everything.
 * @param lobby Room list used to clamp the initial selection.
 */
void	lobby_state_init(lobby_state_t *state, app_game_mode_t filter,
	const app_lobby_view_model_t *lobby)
{
	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	state->section = LOBBY_SECTION_ROOMS;
	state->feedback = LOBBY_FEEDBACK_NONE;
	state->filter = filter;
	state->selected = 0;
	state->room_id[0] = '\0';
	state->room_id_length = 0;
	lobby_state_sync(state, lobby);
}

/**
 * @brief Re-reads the visible room count and clamps the cursor onto it.
 *
 * Called after every refresh: a room that filled or started while the browser
 * was open changes the list length under the cursor.
 *
 * @param state Lobby state to update.
 * @param lobby Current room list.
 */
void	lobby_state_sync(lobby_state_t *state,
	const app_lobby_view_model_t *lobby)
{
	if (state == NULL)
		return ;
	state->visible_count = lobby_visible_count(lobby, state->filter);
	clamp_selection(state);
}

/**
 * @brief Applies one keystroke to the lobby.
 *
 * The screen has two sections and they read input differently. In the room
 * table single letters are commands; in the join field every printable key is
 * text, so the commands are deliberately unreachable there and Escape steps
 * back to the table rather than off the screen.
 *
 * @param state Lobby state to update.
 * @param key Key identifier from Notcurses.
 * @return The resolved action, or LOBBY_ACTION_NONE when only focus moved.
 */
lobby_action_t	lobby_handle_key(lobby_state_t *state, uint32_t key)
{
	if (state == NULL)
		return (LOBBY_ACTION_NONE);
	if (key == NCKEY_RESIZE)
		return (LOBBY_ACTION_NONE);
	if (state->section == LOBBY_SECTION_JOIN)
		return (handle_join_key(state, key));
	return (handle_rooms_key(state, key));
}

/**
 * @brief Reports whether two lobby states would draw differently.
 *
 * @param before State captured before the keystroke.
 * @param after State after the keystroke.
 * @return true when a repaint is required.
 */
bool	lobby_state_view_changed(const lobby_state_t *before,
	const lobby_state_t *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->section != after->section
		|| before->selected != after->selected
		|| before->visible_count != after->visible_count
		|| before->filter != after->filter
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value
		|| before->room_id_length != after->room_id_length
		|| strncmp(before->room_id, after->room_id, LOBBY_ROOM_ID_MAX) != 0);
}

/**
 * @brief Reports whether a held key may be folded into one repaint.
 *
 * Typed characters never coalesce: two identical letters in a room id are two
 * distinct edits, not a repeat of one.
 *
 * @param active_key The key currently being handled.
 * @param queued_key A key already waiting in the input queue.
 * @return true when the queued key may be folded into the active repaint.
 */
bool	lobby_navigation_keys_coalesce(uint32_t active_key, uint32_t queued_key)
{
	if (active_key != queued_key)
		return (false);
	return (active_key == NCKEY_UP || active_key == NCKEY_DOWN
		|| active_key == NCKEY_PGUP || active_key == NCKEY_PGDOWN);
}

/**
 * @brief Reports whether an action ends the lobby's input loop.
 *
 * @param action Action returned by lobby_handle_key().
 * @return true when the screen is about to be left.
 */
bool	lobby_action_leaves_screen(lobby_action_t action)
{
	return (action == LOBBY_ACTION_JOIN || action == LOBBY_ACTION_JOIN_BY_ID
		|| action == LOBBY_ACTION_CREATE || action == LOBBY_ACTION_BACK
		|| action == LOBBY_ACTION_QUIT);
}

/**
 * @brief Counts the rooms one filter admits.
 *
 * @param lobby Room list to scan.
 * @param filter Mode to keep; APP_GAME_MODE_NONE keeps everything.
 * @return The number of listed rooms, never more than the model holds.
 */
int	lobby_visible_count(const app_lobby_view_model_t *lobby,
	app_game_mode_t filter)
{
	int	index;
	int	count;

	if (lobby == NULL)
		return (0);
	count = 0;
	index = 0;
	while (index < lobby->count && index < APP_LOBBY_MAX_ROOMS)
	{
		if (room_matches(&lobby->rooms[index], filter))
			count++;
		index++;
	}
	return (count);
}

/**
 * @brief Returns the nth room the filter admits.
 *
 * @param lobby Room list to scan.
 * @param filter Mode to keep; APP_GAME_MODE_NONE keeps everything.
 * @param index Position within the filtered list.
 * @return The room, or NULL when the index is past the end.
 */
const app_room_summary_view_model_t	*lobby_visible_room(
	const app_lobby_view_model_t *lobby, app_game_mode_t filter, int index)
{
	int	scan;
	int	seen;

	if (lobby == NULL || index < 0)
		return (NULL);
	seen = 0;
	scan = 0;
	while (scan < lobby->count && scan < APP_LOBBY_MAX_ROOMS)
	{
		if (room_matches(&lobby->rooms[scan], filter))
		{
			if (seen == index)
				return (&lobby->rooms[scan]);
			seen++;
		}
		scan++;
	}
	return (NULL);
}

/**
 * @brief Returns the room under the cursor.
 *
 * @param lobby Room list to scan.
 * @param state Lobby state holding the filter and the cursor.
 * @return The selected room, or NULL when the filtered list is empty.
 */
const app_room_summary_view_model_t	*lobby_selected_room(
	const app_lobby_view_model_t *lobby, const lobby_state_t *state)
{
	if (state == NULL)
		return (NULL);
	return (lobby_visible_room(lobby, state->filter, state->selected));
}

/**
 * @brief Finds a room by id, ignoring the list filter and letter case.
 *
 * Joining by id is how a friend shares a room, so a filter the player happens
 * to have set must not hide the room they were invited to.
 *
 * @param lobby Room list to scan.
 * @param id Identifier to match.
 * @return The room, or NULL when no room carries that id.
 */
const app_room_summary_view_model_t	*lobby_room_by_id(
	const app_lobby_view_model_t *lobby, const char *id)
{
	int		index;
	size_t	position;

	if (lobby == NULL || id == NULL || id[0] == '\0')
		return (NULL);
	index = 0;
	while (index < lobby->count && index < APP_LOBBY_MAX_ROOMS)
	{
		position = 0;
		/*
		 * Bounded by the field the id was typed into: the loop stops at the
		 * first difference, and a match at the terminator returns, so neither
		 * buffer is ever read past its own end.
		 */
		while (position < LOBBY_ROOM_ID_MAX
			&& tolower((unsigned char)lobby->rooms[index].id[position])
			== tolower((unsigned char)id[position]))
		{
			if (id[position] == '\0')
				return (&lobby->rooms[index]);
			position++;
		}
		index++;
	}
	return (NULL);
}

/**
 * @brief Explains why a room cannot be joined, if it cannot.
 *
 * @param room Room the player is trying to enter.
 * @return LOBBY_FEEDBACK_NONE when the join may proceed.
 */
lobby_feedback_t	lobby_join_blocker(
	const app_room_summary_view_model_t *room)
{
	if (room == NULL)
		return (LOBBY_FEEDBACK_EMPTY_LIST);
	if (room->state == APP_ROOM_STATE_IN_GAME)
		return (LOBBY_FEEDBACK_IN_GAME);
	if (room->capacity > 0 && room->players >= room->capacity)
		return (LOBBY_FEEDBACK_FULL);
	return (LOBBY_FEEDBACK_NONE);
}

/**
 * @brief Returns the two-letter mode tag used in the room table.
 *
 * @param mode Mode to label.
 * @return "D", "BR", or "-".
 */
const char	*lobby_mode_tag(app_game_mode_t mode)
{
	if (mode == APP_GAME_MODE_DOUBLE)
		return ("D");
	if (mode == APP_GAME_MODE_BATTLE_ROYALE)
		return ("BR");
	return ("-");
}

/**
 * @brief Returns the state word used in the room table.
 *
 * @param state Room state to label.
 * @return "WAITING" or "IN-GAME".
 */
const char	*lobby_state_tag(app_room_state_t state)
{
	if (state == APP_ROOM_STATE_IN_GAME)
		return ("IN-GAME");
	return ("WAITING");
}

/**
 * @brief Returns the heading that names the current list filter.
 *
 * @param filter Mode being listed.
 * @return A static, bounded caption.
 */
const char	*lobby_filter_name(app_game_mode_t filter)
{
	if (filter == APP_GAME_MODE_DOUBLE)
		return ("DOUBLE ROOMS");
	if (filter == APP_GAME_MODE_BATTLE_ROYALE)
		return ("BATTLE ROYALE ROOMS");
	return ("OPEN ROOMS");
}

/**
 * @brief Records the result of the last lobby action.
 *
 * @param state Lobby state to update.
 * @param feedback Result to show.
 * @param value Percentage carried by the volume result.
 */
void	lobby_set_feedback(lobby_state_t *state, lobby_feedback_t feedback,
	int value)
{
	if (state == NULL)
		return ;
	state->feedback = feedback;
	state->feedback_value = value;
}

/**
 * @brief Formats the lobby's inline result line.
 *
 * The line lives inside the status region rather than on a notification card,
 * so no plane is ever raised over this screen. See mp_feedback_text() for why
 * that matters on a stationary bitmap protocol.
 *
 * @param state Lobby state holding the result.
 * @param out Destination buffer.
 * @param size Capacity of out.
 * @return out, holding "" when there is nothing to report.
 */
const char	*lobby_feedback_text(const lobby_state_t *state, char *out,
	size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (state == NULL)
		return (out);
	if (state->feedback == LOBBY_FEEDBACK_REFRESHED)
		snprintf(out, size, "ROOM LIST REFRESHED - %d SHOWN",
			state->visible_count);
	else if (state->feedback == LOBBY_FEEDBACK_FULL)
		snprintf(out, size, "THAT ROOM IS FULL");
	else if (state->feedback == LOBBY_FEEDBACK_IN_GAME)
		snprintf(out, size, "THAT ROOM IS ALREADY IN GAME");
	else if (state->feedback == LOBBY_FEEDBACK_EMPTY_LIST)
		snprintf(out, size, "NO ROOMS HERE - PRESS C TO CREATE ONE");
	else if (state->feedback == LOBBY_FEEDBACK_EMPTY_ID)
		snprintf(out, size, "TYPE A ROOM ID FIRST");
	else if (state->feedback == LOBBY_FEEDBACK_UNKNOWN_ID)
		snprintf(out, size, "NO ROOM CALLED %s", state->room_id);
	else if (state->feedback == LOBBY_FEEDBACK_FILTER)
		snprintf(out, size, "SHOWING %s - %d ROOMS",
			lobby_filter_name(state->filter), state->visible_count);
	else if (state->feedback == LOBBY_FEEDBACK_VOLUME)
		snprintf(out, size, "MUSIC VOLUME %d%%", state->feedback_value);
	return (out);
}

/**
 * @brief Reads one keystroke in the room table, where letters are commands.
 */
static lobby_action_t	handle_rooms_key(lobby_state_t *state, uint32_t key)
{
	if (key == 'q' || key == 'Q')
		return (LOBBY_ACTION_QUIT);
	if (key == NCKEY_ESC || key == 'b' || key == 'B')
		return (LOBBY_ACTION_BACK);
	if (key == '+' || key == '=')
		return (LOBBY_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (LOBBY_ACTION_VOLUME_DOWN);
	if (key == 'c' || key == 'C')
		return (LOBBY_ACTION_CREATE);
	if (key == 'r' || key == 'R')
		return (LOBBY_ACTION_REFRESH);
	if (is_confirm_key(key))
		return (LOBBY_ACTION_JOIN);
	/* Any movement retires the last result: it described the old cursor. */
	if (key == NCKEY_UP)
		move_selection(state, -1);
	else if (key == NCKEY_DOWN)
		move_selection(state, 1);
	else if (key == NCKEY_PGUP || key == NCKEY_HOME)
		move_selection(state, -state->visible_count);
	else if (key == NCKEY_PGDOWN || key == NCKEY_END)
		move_selection(state, state->visible_count);
	else if (key == NCKEY_RIGHT || key == NCKEY_TAB || key == '\t')
	{
		state->feedback = LOBBY_FEEDBACK_NONE;
		state->section = LOBBY_SECTION_JOIN;
	}
	else if (key == 'm' || key == 'M')
		cycle_filter(state);
	return (LOBBY_ACTION_NONE);
}

/**
 * @brief Reads one keystroke in the join field, where letters are text.
 */
static lobby_action_t	handle_join_key(lobby_state_t *state, uint32_t key)
{
	if (key == NCKEY_ESC || key == NCKEY_LEFT || key == NCKEY_TAB
		|| key == '\t')
	{
		state->feedback = LOBBY_FEEDBACK_NONE;
		state->section = LOBBY_SECTION_ROOMS;
		return (LOBBY_ACTION_NONE);
	}
	if (is_confirm_key(key))
		return (LOBBY_ACTION_JOIN_BY_ID);
	if (key == NCKEY_BACKSPACE || key == 127 || key == 8)
	{
		state->feedback = LOBBY_FEEDBACK_NONE;
		if (state->room_id_length > 0)
		{
			state->room_id_length--;
			state->room_id[state->room_id_length] = '\0';
		}
		return (LOBBY_ACTION_NONE);
	}
	/* Ctrl-U, the readline habit, clears the field in one keystroke. */
	if (key == 21)
	{
		state->feedback = LOBBY_FEEDBACK_NONE;
		state->room_id[0] = '\0';
		state->room_id_length = 0;
		return (LOBBY_ACTION_NONE);
	}
	(void)append_room_id(state, key);
	return (LOBBY_ACTION_NONE);
}

/**
 * @brief Moves the room cursor, clamping rather than wrapping.
 *
 * Clamping keeps a held arrow key from cycling the list back under the finger,
 * which reads as the cursor jumping rather than as reaching the end.
 */
static void	move_selection(lobby_state_t *state, int delta)
{
	state->feedback = LOBBY_FEEDBACK_NONE;
	if (state->visible_count <= 0)
	{
		state->selected = 0;
		return ;
	}
	state->selected += delta;
	clamp_selection(state);
}

static void	clamp_selection(lobby_state_t *state)
{
	if (state->visible_count <= 0)
	{
		state->selected = 0;
		return ;
	}
	if (state->selected < 0)
		state->selected = 0;
	if (state->selected >= state->visible_count)
		state->selected = state->visible_count - 1;
}

/**
 * @brief Steps the list filter through Double, Battle Royale and everything.
 */
static void	cycle_filter(lobby_state_t *state)
{
	if (state->filter == APP_GAME_MODE_DOUBLE)
		state->filter = APP_GAME_MODE_BATTLE_ROYALE;
	else if (state->filter == APP_GAME_MODE_BATTLE_ROYALE)
		state->filter = APP_GAME_MODE_NONE;
	else
		state->filter = APP_GAME_MODE_DOUBLE;
	state->selected = 0;
	state->feedback = LOBBY_FEEDBACK_FILTER;
	state->feedback_value = 0;
}

/**
 * @brief Appends one printable character to the room id being typed.
 *
 * Room ids are ASCII by construction, so anything outside the printable range
 * - arrow keys, function keys, multi-byte input - is dropped rather than
 * written as a replacement character the server would reject.
 */
static bool	append_room_id(lobby_state_t *state, uint32_t key)
{
	if (key < 0x20 || key > 0x7e)
		return (false);
	if (state->room_id_length >= LOBBY_ROOM_ID_MAX - 1)
		return (false);
	state->feedback = LOBBY_FEEDBACK_NONE;
	state->room_id[state->room_id_length] = (char)key;
	state->room_id_length++;
	state->room_id[state->room_id_length] = '\0';
	return (true);
}

static bool	is_confirm_key(uint32_t key)
{
	return (key == NCKEY_ENTER || key == '\n' || key == '\r');
}

static bool	room_matches(const app_room_summary_view_model_t *room,
	app_game_mode_t filter)
{
	if (filter == APP_GAME_MODE_NONE)
		return (true);
	return (room->mode == filter);
}
