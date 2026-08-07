#include "tetrisu.h"

/**
 * @brief Initializes a confirmation with the safe answer selected.
 */
void	confirmation_dialog_init(confirmation_dialog_t *dialog,
	confirmation_kind_t kind)
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
confirmation_result_t	confirmation_dialog_handle_key(
	confirmation_dialog_t *dialog, uint32_t key)
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

const char	*confirmation_title(confirmation_kind_t kind)
{
	if (kind == CONFIRM_LEAVE_ROOM)
		return ("LEAVE ROOM?");
	if (kind == CONFIRM_LEAVE_MATCH)
		return ("LEAVE MATCH?");
	return ("QUIT TETRISH?");
}

const char	*confirmation_body(confirmation_kind_t kind)
{
	if (kind == CONFIRM_LEAVE_ROOM)
		return ("Do you really want to leave this room?");
	if (kind == CONFIRM_LEAVE_MATCH)
		return ("Do you really want to leave this game?");
	return ("Do you really want to quit the game?");
}
