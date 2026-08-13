#include "tetrisu.h"

/**
 * @brief Initializes a confirmation with the safe answer selected.
 */
void	confirmation_dialog_init(t_confirmation_dialog *dialog,
	t_confirmation_kind kind)
{
	if (dialog == NULL)
		return ;
	memset(dialog, 0, sizeof(*dialog));
	dialog->visible = true;
	dialog->kind = kind;
	dialog->focus = CONFIRM_FOCUS_NO;
}

/**
 * @brief Applies one key without performing the confirmed action itself.
 */
t_confirmation_result	confirmation_dialog_handle_key(
	t_confirmation_dialog *dialog, uint32_t key)
{
	if (dialog == NULL || !dialog->visible)
		return (CONFIRM_RESULT_NONE);
	if (key == NCKEY_ESC || key == 'n' || key == 'N')
		return (CONFIRM_RESULT_NO);
	if (key == 'y' || key == 'Y')
		return (CONFIRM_RESULT_YES);
	if (key == NCKEY_LEFT || key == NCKEY_RIGHT || key == NCKEY_TAB
		|| key == '\t')
	{
		if (dialog->focus == CONFIRM_FOCUS_NO)
			dialog->focus = CONFIRM_FOCUS_YES;
		else
			dialog->focus = CONFIRM_FOCUS_NO;
		return (CONFIRM_RESULT_NONE);
	}
	if (key == NCKEY_ENTER || key == '\n' || key == '\r')
	{
		if (dialog->focus == CONFIRM_FOCUS_YES)
			return (CONFIRM_RESULT_YES);
		return (CONFIRM_RESULT_NO);
	}
	return (CONFIRM_RESULT_NONE);
}

const char	*confirmation_title(t_confirmation_kind kind)
{
	if (kind == CONFIRM_LEAVE_ROOM)
		return ("LEAVE ROOM?");
	if (kind == CONFIRM_LEAVE_MATCH)
		return ("LEAVE MATCH?");
	if (kind == CONFIRM_CONNECTION_LOST)
		return ("CONNECTION LOST");
	return ("QUIT TETRISH?");
}

const char	*confirmation_body(t_confirmation_kind kind)
{
	if (kind == CONFIRM_LEAVE_ROOM)
		return ("Do you really want to leave this room?");
	if (kind == CONFIRM_LEAVE_MATCH)
		return ("Do you really want to leave this game?");
	if (kind == CONFIRM_CONNECTION_LOST)
		return ("The server stopped answering.");
	return ("Do you really want to quit the game?");
}

/**
 * @brief The safe answer's label - the one Escape gives and focus starts on.
 *
 * @param kind Which question is being asked.
 * @return A label to draw in the left-hand button.
 */
const char	*confirmation_no_label(t_confirmation_kind kind)
{
	if (kind == CONFIRM_CONNECTION_LOST)
		return ("PLAY OFFLINE");
	return ("NO");
}

/**
 * @brief The other answer's label.
 *
 * @param kind Which question is being asked.
 * @return A label to draw in the right-hand button.
 */
const char	*confirmation_yes_label(t_confirmation_kind kind)
{
	if (kind == CONFIRM_CONNECTION_LOST)
		return ("SIGN IN AGAIN");
	return ("YES");
}

/**
 * @brief The line of controls under the answers.
 *
 * A dropped connection has no cancel: there is no state to go back to, and
 * offering one would be offering to carry on talking to a server that is not
 * there. Escape still picks the safe answer, which is why it is named as
 * that answer rather than as a way out of the question.
 *
 * @param kind Which question is being asked.
 * @return The hint line for that question.
 */
const char	*confirmation_hint(t_confirmation_kind kind)
{
	if (kind == CONFIRM_CONNECTION_LOST)
		return ("ESC PLAY OFFLINE   LEFT/RIGHT SELECT   ENTER CONFIRM");
	return ("ESC/N CANCEL   LEFT/RIGHT SELECT   ENTER CONFIRM");
}
