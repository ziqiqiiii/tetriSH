/* ************************************************************************** */
/*                                                                            */
/*   test_access.c - the Access line and the connection id it is filed under  */
/*                                                                            */
/*   One log record stands for one complete HTTTP exchange. These cases pin   */
/*   the four things that make it worth reading: it names the connection,     */
/*   it names the player once one is bound, it says which way a gameplay      */
/*   verb was driven, and it exists even for an exchange the server refused   */
/*   before any handler ran.                                                  */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_accept_and_handshake_are_logged(void);
static void	test_signup_is_one_access_line(void);
static void	test_access_line_names_the_logged_in_player(void);
static void	test_gameplay_verbs_reach_a_daemon_at_info(void);
static void	test_move_names_the_direction(void);
static void	test_rotate_and_drop_name_their_word(void);
static void	test_hold_names_no_word(void);
static void	test_a_forged_word_never_reaches_the_log(void);
static void	test_unparsed_frame_still_writes_an_access_line(void);
static void	test_disconnect_carries_the_connection_id(void);

int	main(void)
{
	test_accept_and_handshake_are_logged();
	test_signup_is_one_access_line();
	test_access_line_names_the_logged_in_player();
	test_gameplay_verbs_reach_a_daemon_at_info();
	test_move_names_the_direction();
	test_rotate_and_drop_name_their_word();
	test_hold_names_no_word();
	test_a_forged_word_never_reaches_the_log();
	test_unparsed_frame_still_writes_an_access_line();
	test_disconnect_carries_the_connection_id();
	return (0);
}

static void	test_accept_and_handshake_are_logged(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(fx_log_wait(&fx, "conn 1 accepted", NULL, HC_TIMEOUT_MS) == 0);
	assert(fx_log_wait(&fx, "conn 1 handshake ok", NULL, HC_TIMEOUT_MS) == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_accept_and_handshake_are_logged\n");
}

static void	test_signup_is_one_access_line(void)
{
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(fx_log_wait(&fx, "conn 1 (anonymous) SIGNUP 201", &rec, HC_TIMEOUT_MS) == 0);
	assert(rec.level == COREIPC_LOG_INFO);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_signup_is_one_access_line\n");
}

static void	test_access_line_names_the_logged_in_player(void)
{
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** LOGIN binds the identity before it returns, so its own Access line is
	** already the player's rather than the anonymous one that preceded it.
	*/
	assert(fx_log_wait(&fx, "conn 1 amber LOGIN 200", &rec, HC_TIMEOUT_MS) == 0);
	assert(rec.level == COREIPC_LOG_INFO);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_access_line_names_the_logged_in_player\n");
}

static void	test_gameplay_verbs_reach_a_daemon_at_info(void)
{
	t_htttp_message	resp;
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	/*
	** The fixture runs at info, which is the level a daemon is configured with.
	** These verbs used to log at debug, so this is the case that would have
	** timed out: the record was written and then filtered away before the ring.
	*/
	assert(fx_start_logged(&fx, COREIPC_LOG_INFO) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** The move is refused - this player is in no room - and that is the point:
	** the line is written for the verb, not for whether the verb succeeded.
	*/
	assert(hc_request(&hc, "MOVE", "/room/S-01/player/1", "LEFT\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber MOVE LEFT", &rec, HC_TIMEOUT_MS) == 0);
	assert(rec.level == COREIPC_LOG_INFO);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_gameplay_verbs_reach_a_daemon_at_info\n");
}

static void	test_move_names_the_direction(void)
{
	t_htttp_message	resp;
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** Two MOVEs that differ only in their body used to log the same line, so
	** the record said a piece had been driven without saying which way.
	*/
	assert(hc_request(&hc, "MOVE", "/room/S-01/player/1", "LEFT\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber MOVE LEFT 409", &rec, HC_TIMEOUT_MS) == 0);
	assert(hc_request(&hc, "MOVE", "/room/S-01/player/1", "RIGHT\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber MOVE RIGHT 409", &rec, HC_TIMEOUT_MS) == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_move_names_the_direction\n");
}

static void	test_rotate_and_drop_name_their_word(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** The word is read off the body rather than from the handler, so every
	** verb that carries one reports it without a table of its own.
	*/
	assert(hc_request(&hc, "ROTATE", "/room/S-01/player/1", "CCW\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber ROTATE CCW 409", NULL, HC_TIMEOUT_MS) == 0);
	assert(hc_request(&hc, "DROP", "/room/S-01/player/1", "HARD\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber DROP HARD 409", NULL, HC_TIMEOUT_MS) == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_rotate_and_drop_name_their_word\n");
}

static void	test_hold_names_no_word(void)
{
	t_htttp_message	resp;
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** HOLD carries no body, so there is nothing to name and the line stays the
	** plain one - no stray separator where a word would have gone.
	*/
	assert(hc_request(&hc, "HOLD", "/room/S-01/player/1", NULL, &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber HOLD", &rec, HC_TIMEOUT_MS) == 0);
	assert(strcmp(rec.msg, "conn 1 amber HOLD 409") == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_hold_names_no_word\n");
}

static void	test_a_forged_word_never_reaches_the_log(void)
{
	t_htttp_message	resp;
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	/*
	** The body is the client's, so a word going into a log line is an
	** injection site: this one carries a newline and a second record's worth
	** of text behind it. A word that is not plain letters is dropped whole,
	** which is why the line below is the bare one and not a truncation.
	*/
	assert(hc_request(&hc, "MOVE", "/room/S-01/player/1", "L\nconn 1 amber LOGIN 200\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber MOVE", &rec, HC_TIMEOUT_MS) == 0);
	assert(strcmp(rec.msg, "conn 1 amber MOVE 409") == 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_forged_word_never_reaches_the_log\n");
}

static void	test_unparsed_frame_still_writes_an_access_line(void)
{
	t_log_record	rec;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	/*
	** A frame that never parsed has no method to report, so the line carries
	** "-" and stays at info: a client sending nonsense is worth seeing.
	*/
	assert(session_send(&hc.sess, (const unsigned char *)"@", 1) > 0);
	assert(fx_log_wait(&fx, "conn 1 (anonymous) - ", &rec, HC_TIMEOUT_MS) == 0);
	assert(rec.level == COREIPC_LOG_INFO);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_unparsed_frame_still_writes_an_access_line\n");
}

static void	test_disconnect_carries_the_connection_id(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start_logged(&fx, COREIPC_LOG_DEBUG) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	hc_close(&hc);
	assert(fx_log_wait(&fx, "conn 1 amber disconnected", NULL, HC_TIMEOUT_MS) == 0);
	fx_stop(&fx);
	printf("PASS test_disconnect_carries_the_connection_id\n");
}
