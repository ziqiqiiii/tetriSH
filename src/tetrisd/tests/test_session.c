/* ************************************************************************** */
/*                                                                            */
/*   test_session.c - handshake, accounts, and connection-owned identity      */
/*                                                                            */
/*   Everything here is observed the way a client observes it: over a real    */
/*   secure session, as HTTTP responses. Identity is the theme - who the      */
/*   server thinks you are, and what it refuses when you claim otherwise.     */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_handshake_then_signup_and_login(void);
static void	test_signup_rejects_a_taken_username(void);
static void	test_login_refuses_wrong_credentials(void);
static void	test_authenticated_route_needs_a_player_id(void);
static void	test_forged_player_id_is_refused(void);
static void	test_unknown_method_and_malformed_frame(void);
static void	test_accounts_survive_a_restart(void);
static void	test_a_second_login_displaces_the_first(void);
static void	test_displacement_releases_the_old_connection_s_slot(void);
static void	test_a_body_without_a_content_type_is_refused(void);
static void	test_boot_refuses_an_unusable_private_key(void);
static void	test_handshakes_outlive_the_certificate_files(void);

static int	raw_request(t_harness *hc, const char *method, const char *path,
				const char *pid, const char *body);

int	main(void)
{
	test_handshake_then_signup_and_login();
	test_signup_rejects_a_taken_username();
	test_login_refuses_wrong_credentials();
	test_authenticated_route_needs_a_player_id();
	test_forged_player_id_is_refused();
	test_unknown_method_and_malformed_frame();
	test_accounts_survive_a_restart();
	test_a_second_login_displaces_the_first();
	test_displacement_releases_the_old_connection_s_slot();
	test_a_body_without_a_content_type_is_refused();
	test_boot_refuses_an_unusable_private_key();
	test_handshakes_outlive_the_certificate_files();
	return (0);
}

/*
** config_validate only proves the two files can be read. Parsing them is what
** proves they are usable, and that now happens once at boot, so a key that is
** readable but is not a key fails the person who started the daemon rather
** than the first player who tries to connect.
*/
static void	test_boot_refuses_an_unusable_private_key(void)
{
	t_fixture	fx;
	t_config	broken;
	t_server	*srv;

	assert(fx_start(&fx) == 0);
	broken = fx.cfg;
	snprintf(broken.key_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", fx.cfg.cert_path);
	srv = NULL;
	assert(server_start(&broken, &srv) == -1);
	assert(srv == NULL);
	fx_stop(&fx);
	printf("PASS test_boot_refuses_an_unusable_private_key\n");
}

/*
** The certificate and key are read once, not once per connection, so removing
** both files out from under a running server changes nothing a client can
** observe. This is what makes the handshake safe to run on a bounded worker
** pool: no connection is waiting on the disk (ADR-0008).
*/
static void	test_handshakes_outlive_the_certificate_files(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(remove(fx.cfg.cert_path) == 0);
	assert(remove(fx.cfg.key_path) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_handshakes_outlive_the_certificate_files\n");
}

/*
** A player has at most one connection, so logging in again takes the identity
** back rather than being refused: a client that died without closing its
** socket must not lock its own account out until TCP notices.
*/
static void	test_a_second_login_displaces_the_first(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		first;
	t_harness		second;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&first, &fx) == 0);
	assert(hc_signup(&first, "amber", "hunter2") == 201);
	assert(hc_login(&first, "amber", "hunter2") == 200);
	assert(hc_connect(&second, &fx) == 0);
	assert(hc_login(&second, "amber", "hunter2") == 200);
	assert(second.player_id == first.player_id);
	assert(hc_request(&first, "LIST", TETRISD_ROUTE_ROOMS, NULL, &resp) == -1);
	assert(hc_request(&second, "LIST", TETRISD_ROUTE_ROOMS, NULL, &resp) == 0);
	assert(resp.status_code == 200);
	htttp_message_free(&resp);
	hc_close(&second);
	hc_close(&first);
	fx_stop(&fx);
	printf("PASS test_a_second_login_displaces_the_first\n");
}

/*
** Displacement completes before the new connection is bound, so the forfeit
** the old one runs on its way out cannot reach into the room the new one
** goes on to join. The returning player finds a lobby they can act in.
*/
static void	test_displacement_releases_the_old_connection_s_slot(void)
{
	t_fixture	fx;
	t_harness	first;
	t_harness	second;
	char		room[ROOM_NAME_MAX];
	char		again[ROOM_NAME_MAX];

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&first, &fx) == 0);
	assert(hc_signup(&first, "amber", "hunter2") == 201);
	assert(hc_login(&first, "amber", "hunter2") == 200);
	assert(hc_join_new(&first, "single", room, sizeof(room)) == 201);
	assert(hc_connect(&second, &fx) == 0);
	assert(hc_login(&second, "amber", "hunter2") == 200);
	assert(hc_join_new(&second, "single", again, sizeof(again)) == 201);
	assert(strcmp(room, again) != 0);
	hc_close(&second);
	hc_close(&first);
	fx_stop(&fx);
	printf("PASS test_displacement_releases_the_old_connection_s_slot\n");
}

static void	test_handshake_then_signup_and_login(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	assert(hc.player_id != 0);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_handshake_then_signup_and_login\n");
}

static void	test_signup_rejects_a_taken_username(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_signup(&hc, "amber", "different") == 409);
	assert(hc_signup(&hc, "", "nothing") == 400);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_signup_rejects_a_taken_username\n");
}

static void	test_login_refuses_wrong_credentials(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "wrong") == 401);
	assert(hc_login(&hc, "ghost", "hunter2") == 401);
	assert(hc.authed == false);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_login_refuses_wrong_credentials\n");
}

static void	test_authenticated_route_needs_a_player_id(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(raw_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, NULL, NULL) == 401);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(raw_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, "1", NULL) == 401);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	assert(raw_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, NULL, NULL) == 401);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_authenticated_route_needs_a_player_id\n");
}

static void	test_forged_player_id_is_refused(void)
{
	t_fixture	fx;
	t_harness	amber;
	t_harness	mallory;
	char		claimed[32];

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&amber, &fx) == 0);
	assert(hc_signup(&amber, "amber", "hunter2") == 201);
	assert(hc_login(&amber, "amber", "hunter2") == 200);
	assert(hc_connect(&mallory, &fx) == 0);
	assert(hc_signup(&mallory, "mallory", "hunter2") == 201);
	assert(hc_login(&mallory, "mallory", "hunter2") == 200);
	snprintf(claimed, sizeof(claimed), "%llu",
		(unsigned long long)amber.player_id);
	assert(raw_request(&mallory, "LIST", TETRISD_ROUTE_ROOMS, claimed, NULL) == 401);
	hc_close(&mallory);
	hc_close(&amber);
	fx_stop(&fx);
	printf("PASS test_forged_player_id_is_refused\n");
}

static void	test_unknown_method_and_malformed_frame(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	assert(raw_request(&hc, "TELEPORT", "/room/S-01", NULL, NULL) == 501);
	assert(session_send(&hc.sess, "GARBAGE\r\n\r\n", 11) == 11);
	assert(raw_request(&hc, "LIST", TETRISD_ROUTE_ROOMS, NULL, NULL) == 400);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_unknown_method_and_malformed_frame\n");
}

static void	test_accounts_survive_a_restart(void)
{
	t_fixture	fx;
	t_harness	hc;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	hc_close(&hc);
	server_stop(fx.srv);
	fx.srv = NULL;
	assert(server_start(&fx.cfg, &fx.srv) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_login(&hc, "amber", "hunter2") == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_accounts_survive_a_restart\n");
}

/**
 * @brief Sends a request with a caller-chosen Player-Id, bypassing hc_request.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param pid Player-Id header value, or NULL to omit it.
 * @param body Request body, or NULL.
 * @return The response status code, or -1 when the exchange failed.
 */
/*
** Content-Type is a required header on any request carrying a body, so a
** body that does not say what it is gets refused rather than guessed at.
** Sent by hand because the harness always sets the header correctly.
*/
static void	test_a_body_without_a_content_type_is_refused(void)
{
	t_htttp_message	req;
	t_htttp_message	resp;
	t_fixture		fx;
	t_harness		hc;
	unsigned char	*bytes;
	size_t			len;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&hc, &fx) == 0);
	htttp_message_init(&req);
	assert(htttp_message_make_request(&req, "SIGNUP", TETRISD_ROUTE_ACCOUNT)
		== HTTTP_OK);
	assert(htttp_message_set_body(&req, "username amber\npassword hunter2\n",
			31) == HTTTP_OK);
	assert(htttp_serialize(&req, &bytes, &len) == HTTTP_OK);
	assert(session_send(&hc.sess, bytes, len) >= 0);
	free(bytes);
	htttp_message_free(&req);
	assert(hc_recv(&hc, &resp, HC_TIMEOUT_MS) == 0);
	assert(resp.status_code == 400);
	htttp_message_free(&resp);
	assert(hc_signup(&hc, "amber", "hunter2") == 201);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_body_without_a_content_type_is_refused\n");
}

static int	raw_request(t_harness *hc, const char *method, const char *path,
			const char *pid, const char *body)
{
	t_harness		saved;
	t_htttp_message	resp;
	int				status;

	saved = *hc;
	hc->authed = pid != NULL;
	if (pid != NULL)
		hc->player_id = (t_player_id)strtoull(pid, NULL, 10);
	status = -1;
	if (hc_request(hc, method, path, body, &resp) == 0)
	{
		status = (int)resp.status_code;
		htttp_message_free(&resp);
	}
	hc->authed = saved.authed;
	hc->player_id = saved.player_id;
	return (status);
}
