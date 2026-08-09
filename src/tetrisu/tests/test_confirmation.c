#include "tetrisu.h"

static void	test_safe_default_and_explicit_yes(void);
static void	test_shortcuts_and_copy(void);

int	main(void)
{
	test_safe_default_and_explicit_yes();
	test_shortcuts_and_copy();
	return (0);
}

static void	test_safe_default_and_explicit_yes(void)
{
	t_confirmation_dialog	dialog;

	confirmation_dialog_init(&dialog, CONFIRM_QUIT_APP);
	assert(dialog.visible);
	assert(dialog.focus == CONFIRM_FOCUS_NO);
	assert(confirmation_dialog_handle_key(&dialog, NCKEY_ENTER)
		== CONFIRM_RESULT_NO);
	assert(confirmation_dialog_handle_key(&dialog, NCKEY_RIGHT)
		== CONFIRM_RESULT_NONE);
	assert(dialog.focus == CONFIRM_FOCUS_YES);
	assert(confirmation_dialog_handle_key(&dialog, '\r')
		== CONFIRM_RESULT_YES);
	printf("PASS test_safe_default_and_explicit_yes\n");
}

static void	test_shortcuts_and_copy(void)
{
	t_confirmation_dialog	dialog;

	confirmation_dialog_init(&dialog, CONFIRM_LEAVE_ROOM);
	assert(confirmation_dialog_handle_key(&dialog, NCKEY_ESC)
		== CONFIRM_RESULT_NO);
	assert(confirmation_dialog_handle_key(&dialog, 'n') == CONFIRM_RESULT_NO);
	assert(confirmation_dialog_handle_key(&dialog, 'Y') == CONFIRM_RESULT_YES);
	assert(strstr(confirmation_title(CONFIRM_LEAVE_ROOM), "ROOM") != NULL);
	assert(strstr(confirmation_body(CONFIRM_LEAVE_MATCH), "game") != NULL);
	dialog.visible = false;
	assert(confirmation_dialog_handle_key(&dialog, 'y')
		== CONFIRM_RESULT_NONE);
	printf("PASS test_shortcuts_and_copy\n");
}
