#include "tetrisu.h"

// Static Functions
static bool	is_confirm_key(uint32_t key);
static bool	is_previous_key(uint32_t key);
static bool	is_next_key(uint32_t key);

/**
 * @brief Resets the multiplayer mode picker to its default card.
 *
 * Double is the default because it is the mode that can be played with one
 * friend and no lobby population, so it is the one a first run is most likely
 * to want.
 *
 * @param state Picker state to initialise.
 */
void	mp_mode_state_init(t_mp_mode_state *state)
{
	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	state->focus = MP_MODE_FOCUS_DOUBLE;
	state->feedback = MP_MODE_FEEDBACK_NONE;
	state->feedback_value = 0;
}

/**
 * @brief Applies one keystroke to the mode picker.
 *
 * The two cards sit side by side but read as a list, so both axes move focus:
 * Left/Up step back and Right/Down step forward. The digits pick a card
 * directly, matching the bracketed numbers the cards are labelled with.
 *
 * @param state Picker state to update.
 * @param key Key identifier from Notcurses.
 * @return The resolved action, or MP_MODE_ACTION_NONE when focus only moved.
 */
t_mp_mode_action	mp_mode_handle_key(t_mp_mode_state *state, uint32_t key)
{
	if (state == NULL)
		return (MP_MODE_ACTION_NONE);
	if (key == 'q' || key == 'Q')
		return (MP_MODE_ACTION_QUIT);
	if (key == NCKEY_ESC || key == 'b' || key == 'B')
		return (MP_MODE_ACTION_BACK);
	if (key == '+' || key == '=')
		return (MP_MODE_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (MP_MODE_ACTION_VOLUME_DOWN);
	if (key == '1' || key == '2')
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->focus = key == '1'
			? MP_MODE_FOCUS_DOUBLE : MP_MODE_FOCUS_BATTLE_ROYALE;
		return (MP_MODE_ACTION_SELECT);
	}
	if (is_confirm_key(key))
		return (MP_MODE_ACTION_SELECT);
	/* Any movement retires the last result: it described the old cursor. */
	if (is_previous_key(key))
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->focus = MP_MODE_FOCUS_DOUBLE;
	}
	else if (is_next_key(key))
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->focus = MP_MODE_FOCUS_BATTLE_ROYALE;
	}
	return (MP_MODE_ACTION_NONE);
}

/**
 * @brief Reports whether two picker states would draw differently.
 *
 * @param before State captured before the keystroke.
 * @param after State after the keystroke.
 * @return true when a repaint is required.
 */
bool	mp_mode_state_view_changed(const t_mp_mode_state *before,
	const t_mp_mode_state *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->focus != after->focus
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value);
}

/**
 * @brief Reports whether a held key may be folded into one repaint.
 *
 * Only identical movement keys coalesce. Folding a different key would act on
 * it before its target state had been drawn, and folding opposite arrows would
 * swallow a real focus change.
 *
 * @param active_key The key currently being handled.
 * @param queued_key A key already waiting in the input queue.
 * @return true when the queued key may be folded into the active repaint.
 */
bool	mp_mode_navigation_keys_coalesce(uint32_t active_key,
	uint32_t queued_key)
{
	if (active_key != queued_key)
		return (false);
	return (active_key == NCKEY_LEFT || active_key == NCKEY_RIGHT
		|| active_key == NCKEY_UP || active_key == NCKEY_DOWN);
}

/**
 * @brief Reports whether an action ends the mode picker's input loop.
 *
 * @param action Action returned by mp_mode_handle_key().
 * @return true when the screen is about to be left.
 */
bool	mp_mode_action_leaves_screen(t_mp_mode_action action)
{
	return (action == MP_MODE_ACTION_SELECT
		|| action == MP_MODE_ACTION_BACK
		|| action == MP_MODE_ACTION_QUIT);
}

/**
 * @brief Returns the game mode the focused card stands for.
 *
 * @param state Picker state to read.
 * @return APP_GAME_MODE_DOUBLE or APP_GAME_MODE_BATTLE_ROYALE.
 */
t_app_game_mode	mp_mode_focused_mode(const t_mp_mode_state *state)
{
	if (state == NULL || state->focus == MP_MODE_FOCUS_DOUBLE)
		return (APP_GAME_MODE_DOUBLE);
	return (APP_GAME_MODE_BATTLE_ROYALE);
}

/**
 * @brief Returns the display name printed on one mode card.
 *
 * @param index Card index, 0 for Double and 1 for Battle Royale.
 * @return A static, bounded caption.
 */
const char	*mp_mode_card_name(int index)
{
	static const char	*names[MP_MODE_CARD_COUNT] = {
		"[1] DOUBLE",
		"[2] BATTLE ROYALE"
	};

	if (index < 0 || index >= MP_MODE_CARD_COUNT)
		return ("");
	return (names[index]);
}

/**
 * @brief Returns the player-count line printed on one mode card.
 *
 * @param index Card index, 0 for Double and 1 for Battle Royale.
 * @return A static, bounded caption.
 */
const char	*mp_mode_card_players(int index)
{
	static const char	*players[MP_MODE_CARD_COUNT] = {
		"2 players",
		"4 - 99 players"
	};

	if (index < 0 || index >= MP_MODE_CARD_COUNT)
		return ("");
	return (players[index]);
}

/**
 * @brief Validates the server-facing capacity contract for one game mode.
 *
 * Double always has exactly two slots. Battle Royale rooms may choose any
 * capacity from the four-player start threshold through the protocol maximum.
 * Keeping the rule here gives providers, lobby guards, and room policy one
 * authoritative boundary.
 */
bool	multiplayer_room_capacity_valid(t_app_game_mode mode, int capacity)
{
	if (mode == APP_GAME_MODE_DOUBLE)
		return (capacity == WAITING_ROOM_DOUBLE_PLAYERS);
	if (mode == APP_GAME_MODE_BATTLE_ROYALE)
		return (capacity >= WAITING_ROOM_ROYALE_MIN_PLAYERS
			&& capacity <= APP_ROOM_MAX_PLAYERS);
	return (false);
}

/**
 * @brief Returns one body line of a mode card's description.
 *
 * Kept as data rather than formatted at the call site so the bitmap and the
 * compatibility renderers print exactly the same copy.
 *
 * @param index Card index, 0 for Double and 1 for Battle Royale.
 * @param line Body line index, 0 or 1.
 * @return A static, bounded caption, or "" when either index is out of range.
 */
const char	*mp_mode_card_line(int index, int line)
{
	static const char	*body[MP_MODE_CARD_COUNT][2] = {
		{
			"One opponent, one board each.",
			"Starts when both players are ready."
		},
		{
			"Last stack standing wins the arena.",
			"Needs four players before it can start."
		}
	};

	if (index < 0 || index >= MP_MODE_CARD_COUNT || line < 0 || line >= 2)
		return ("");
	return (body[index][line]);
}

/**
 * @brief Resets the create-room panel to the mode the lobby was filtered by.
 *
 * Carrying the lobby's filter across means the option already highlighted is
 * almost always the one wanted, so creating a room is Enter rather than
 * Down-then-Enter.
 *
 * @param state Panel state to initialise.
 * @param mode Mode to pre-select; anything else falls back to Double.
 */
void	create_room_state_init(t_create_room_state *state, t_app_game_mode mode)
{
	if (state == NULL)
		return ;
	memset(state, 0, sizeof(*state));
	if (mode == APP_GAME_MODE_BATTLE_ROYALE)
		state->mode = APP_GAME_MODE_BATTLE_ROYALE;
	else
		state->mode = APP_GAME_MODE_DOUBLE;
	state->feedback = MP_MODE_FEEDBACK_NONE;
	state->feedback_value = 0;
}

/**
 * @brief Applies one keystroke to the create-room panel.
 *
 * @param state Panel state to update.
 * @param key Key identifier from Notcurses.
 * @return The resolved action, or CREATE_ROOM_ACTION_NONE when focus moved.
 */
t_create_room_action	create_room_handle_key(t_create_room_state *state,
	uint32_t key)
{
	if (state == NULL)
		return (CREATE_ROOM_ACTION_NONE);
	if (key == 'q' || key == 'Q')
		return (CREATE_ROOM_ACTION_QUIT);
	if (key == NCKEY_ESC)
		return (CREATE_ROOM_ACTION_CANCEL);
	if (key == '+' || key == '=')
		return (CREATE_ROOM_ACTION_VOLUME_UP);
	if (key == '-' || key == '_')
		return (CREATE_ROOM_ACTION_VOLUME_DOWN);
	if (key == '1' || key == '2')
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->mode = key == '1'
			? APP_GAME_MODE_DOUBLE : APP_GAME_MODE_BATTLE_ROYALE;
		return (CREATE_ROOM_ACTION_NONE);
	}
	if (is_confirm_key(key))
		return (CREATE_ROOM_ACTION_CREATE);
	if (is_previous_key(key))
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->mode = APP_GAME_MODE_DOUBLE;
	}
	else if (is_next_key(key))
	{
		state->feedback = MP_MODE_FEEDBACK_NONE;
		state->mode = APP_GAME_MODE_BATTLE_ROYALE;
	}
	return (CREATE_ROOM_ACTION_NONE);
}

/**
 * @brief Reports whether two panel states would draw differently.
 *
 * @param before State captured before the keystroke.
 * @param after State after the keystroke.
 * @return true when a repaint is required.
 */
bool	create_room_state_view_changed(const t_create_room_state *before,
	const t_create_room_state *after)
{
	if (before == NULL || after == NULL)
		return (false);
	return (before->mode != after->mode
		|| before->feedback != after->feedback
		|| before->feedback_value != after->feedback_value);
}

/**
 * @brief Reports whether an action ends the create-room panel's input loop.
 *
 * @param action Action returned by create_room_handle_key().
 * @return true when the screen is about to be left.
 */
bool	create_room_action_leaves_screen(t_create_room_action action)
{
	return (action == CREATE_ROOM_ACTION_CREATE
		|| action == CREATE_ROOM_ACTION_CANCEL
		|| action == CREATE_ROOM_ACTION_QUIT);
}

/**
 * @brief Returns the highlighted option row of the create-room panel.
 *
 * @param state Panel state to read.
 * @return 0 for Double, 1 for Battle Royale.
 */
int	create_room_focused_index(const t_create_room_state *state)
{
	if (state != NULL && state->mode == APP_GAME_MODE_BATTLE_ROYALE)
		return (1);
	return (0);
}

/**
 * @brief Formats the inline result line shared by both mode surfaces.
 *
 * Neither surface raises a notification card. A card is a plane of its own
 * raised over the screen, and raising or dropping one damages the cells it
 * covers, which makes a stationary protocol retransmit the full-screen bitmap
 * underneath - over every region plane above it.
 *
 * @param feedback Result to describe.
 * @param value Percentage carried by the volume result.
 * @param out Destination buffer.
 * @param size Capacity of out.
 * @return out, holding "" when there is nothing to report.
 */
const char	*mp_feedback_text(t_mp_mode_feedback feedback, int value,
	char *out, size_t size)
{
	if (out == NULL || size == 0)
		return ("");
	out[0] = '\0';
	if (feedback == MP_MODE_FEEDBACK_VOLUME)
		snprintf(out, size, "MUSIC VOLUME %d%%", value);
	return (out);
}

static bool	is_confirm_key(uint32_t key)
{
	return (key == NCKEY_ENTER || key == '\n' || key == '\r' || key == ' ');
}

static bool	is_previous_key(uint32_t key)
{
	return (key == NCKEY_LEFT || key == NCKEY_UP);
}

static bool	is_next_key(uint32_t key)
{
	return (key == NCKEY_RIGHT || key == NCKEY_DOWN);
}
