#include "tetrisu.h"

static void	test_safe_default_and_explicit_yes(void);
static void	test_shortcuts_and_copy(void);
static void	test_a_lost_connection_offers_the_two_ways_out(void);

int	main(void)
{
	test_safe_default_and_explicit_yes();
	test_shortcuts_and_copy();
	test_a_lost_connection_offers_the_two_ways_out();
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

/*
** A dropped connection is not a confirmation of anything the player did, but
** it is the same two-answer modal - so the answers have to read as the two
** ways out rather than as yes and no, and the safe one has to be the one that
** takes nothing away. Escape picks playing offline, because there is no
** cancelling a connection that is already gone.
*/
static void	test_a_lost_connection_offers_the_two_ways_out(void)
{
	t_confirmation_dialog	dialog;

	confirmation_dialog_init(&dialog, CONFIRM_CONNECTION_LOST);
	assert(dialog.focus == CONFIRM_FOCUS_NO);
	assert(confirmation_dialog_handle_key(&dialog, NCKEY_ESC)
		== CONFIRM_RESULT_NO);
	assert(strstr(confirmation_title(CONFIRM_CONNECTION_LOST), "LOST")
		!= NULL);
	assert(strstr(confirmation_no_label(CONFIRM_CONNECTION_LOST), "OFFLINE")
		!= NULL);
	assert(strstr(confirmation_yes_label(CONFIRM_CONNECTION_LOST), "SIGN IN")
		!= NULL);
	/* the hint names the safe answer, because there is nothing to cancel */
	assert(strstr(confirmation_hint(CONFIRM_CONNECTION_LOST), "CANCEL")
		== NULL);
	assert(strcmp(confirmation_no_label(CONFIRM_QUIT_APP), "NO") == 0);
	assert(strcmp(confirmation_yes_label(CONFIRM_LEAVE_ROOM), "YES") == 0);
	printf("PASS test_a_lost_connection_offers_the_two_ways_out\n");
}
