/* ************************************************************************** */
/*                                                                            */
/*   test_access.c - the Access line and the connection id it is filed under  */
/*                                                                            */
/*   One log record stands for one complete HTTTP exchange. These cases pin   */
/*   the three things that makes it worth reading: it names the connection,   */
/*   it names the player once one is bound, and it exists even for an         */
/*   exchange the server refused before any handler ran.                      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_accept_and_handshake_are_logged(void);
static void	test_signup_is_one_access_line(void);
static void	test_access_line_names_the_logged_in_player(void);
static void	test_gameplay_verbs_log_at_debug(void);
static void	test_unparsed_frame_still_writes_an_access_line(void);
static void	test_disconnect_carries_the_connection_id(void);

int	main(void)
{
	test_accept_and_handshake_are_logged();
	test_signup_is_one_access_line();
	test_access_line_names_the_logged_in_player();
	test_gameplay_verbs_log_at_debug();
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

static void	test_gameplay_verbs_log_at_debug(void)
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
	** The move is refused - this player is in no room - and that is the point:
	** the level is decided by the verb, not by whether the verb succeeded.
	*/
	assert(hc_request(&hc, "MOVE", "/room/S-01/player/1", "LEFT\n", &resp) == 0);
	htttp_message_free(&resp);
	assert(fx_log_wait(&fx, "conn 1 amber MOVE", &rec, HC_TIMEOUT_MS) == 0);
	assert(rec.level == COREIPC_LOG_DEBUG);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_gameplay_verbs_log_at_debug\n");
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
