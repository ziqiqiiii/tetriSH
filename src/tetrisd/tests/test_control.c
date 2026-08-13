/* ************************************************************************** */
/*                                                                            */
/*   test_control.c - the Control channel and its four read-only routes       */
/*                                                                            */
/*   UC-22, UC-25, UC-26 and UC-27, asked the way tetrisctl asks them: a       */
/*   plaintext HTTTP request behind a 4-byte length prefix on an AF_UNIX       */
/*   socket, with no session, no handshake and nothing naming a Player.       */
/*   What is under test is that the answers come off the reactor whole -       */
/*   so the suite drives a real server and reads real bodies back.            */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>
#include <sys/stat.h>

// Static Functions
static void	test_status_answers_health(void);
static void	test_rooms_lists_what_the_lobby_lists(void);
static void	test_players_lists_connections_named_and_not(void);
static void	test_dropped_answers_the_counter(void);
static void	test_unknown_method_and_path_are_refused(void);
static void	test_socket_is_private_to_its_owner(void);
static void	test_the_channel_survives_a_rude_administrator(void);

static int	ctl_open(t_fixture *fx);
static int	ctl_ask(t_fixture *fx, const char *method, const char *path, t_htttp_message *out);
static int	ctl_send(int fd, const char *method, const char *path);
static int	ctl_read(int fd, t_htttp_message *out);

int	main(void)
{
	test_status_answers_health();
	test_rooms_lists_what_the_lobby_lists();
	test_players_lists_connections_named_and_not();
	test_dropped_answers_the_counter();
	test_unknown_method_and_path_are_refused();
	test_socket_is_private_to_its_owner();
	test_the_channel_survives_a_rude_administrator();
	return (0);
}

static void	test_status_answers_health(void)
{
	t_htttp_message	resp;
	t_body_health	health;
	t_fixture		fx;
	t_harness		hc;

	assert(fx_start(&fx) == 0);
	assert(ctl_ask(&fx, "STATUS", "/admin", &resp) == 0);
	assert(resp.status_code == 200u);
	assert(body_health_decode((const char *)resp.body, resp.body_len,
			&health) == 0);
	assert(health.pid == (int64_t)getpid());
	assert(health.connections == 0);
	assert(health.rooms == 0);
	assert(health.tick_ms == fx.cfg.tick_ms); // the configured one, not a rate
	htttp_message_free(&resp);
	assert(hc_connect(&hc, &fx) == 0);
	assert(ctl_ask(&fx, "STATUS", "/admin", &resp) == 0);
	assert(body_health_decode((const char *)resp.body, resp.body_len,
			&health) == 0);
	assert(health.connections == 1); // the registry, read on the reactor
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_status_answers_health\n");
}

static void	test_rooms_lists_what_the_lobby_lists(void)
{
	t_htttp_message	resp;
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	t_fixture		fx;
	t_harness		hc;
	char			room[ROOM_NAME_MAX];
	size_t			count;

	assert(fx_start(&fx) == 0);
	assert(ctl_ask(&fx, "ROOMS", "/admin", &resp) == 0);
	assert(resp.status_code == 200u);
	assert(resp.body_len == 0); // no open rooms is an empty body, not an error
	htttp_message_free(&resp);
	assert(hc_connect(&hc, &fx) == 0);
	assert(hc_signup(&hc, "amber", "pw") == 201);
	assert(hc_login(&hc, "amber", "pw") == 200);
	assert(hc_join_new(&hc, "single", room, sizeof(room)) == 201);
	assert(ctl_ask(&fx, "ROOMS", "/admin", &resp) == 0);
	assert(body_rooms_decode((const char *)resp.body, resp.body_len, rows,
			LOBBY_MAX_ROOMS, &count) == 0);
	assert(count == 1);
	assert(strcmp(rows[0].name, room) == 0);
	assert(rows[0].mode == BODY_MODE_SINGLE);
	assert(strcmp(rows[0].owner, "amber") == 0);
	htttp_message_free(&resp);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_rooms_lists_what_the_lobby_lists\n");
}

static void	test_players_lists_connections_named_and_not(void)
{
	t_htttp_message		resp;
	t_body_player_row	rows[BODY_PLAYERS_MAX];
	t_fixture			fx;
	t_harness			named;
	t_harness			anon;
	char				room[ROOM_NAME_MAX];
	size_t				count;

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&named, &fx) == 0);
	assert(hc_signup(&named, "amber", "pw") == 201);
	assert(hc_login(&named, "amber", "pw") == 200);
	assert(hc_join_new(&named, "single", room, sizeof(room)) == 201);
	assert(hc_connect(&anon, &fx) == 0);
	/*
	** SIGNUP rather than nothing, so the assertion is not a race. hc_connect
	** returns as soon as the client's own handshake is done, but the server
	** adopts that connection off the handshake pool on a later reactor pass -
	** and a connection the pool still owns is deliberately not listed. One
	** round trip is what proves it has been adopted. It stays anonymous:
	** signing up creates an account, logging in is what names a connection.
	*/
	assert(hc_signup(&anon, "bramble", "pw") == 201);
	assert(ctl_ask(&fx, "PLAYERS", "/admin", &resp) == 0);
	assert(resp.status_code == 200u);
	assert(body_players_decode((const char *)resp.body, resp.body_len, rows,
			BODY_PLAYERS_MAX, &count) == 0);
	assert(count == 2); // a Client that never logged in is still a connection
	assert(rows[0].authenticated == true);
	assert(strcmp(rows[0].username, "amber") == 0);
	assert(strcmp(rows[0].room, room) == 0);
	assert(rows[1].authenticated == false);
	assert(rows[1].username[0] == '\0');
	assert(rows[1].room[0] == '\0');
	assert(rows[0].connection != rows[1].connection);
	htttp_message_free(&resp);
	hc_close(&named);
	hc_close(&anon);
	fx_stop(&fx);
	printf("PASS test_players_lists_connections_named_and_not\n");
}

static void	test_dropped_answers_the_counter(void)
{
	t_htttp_message	resp;
	t_body_dropped	dropped;
	t_fixture		fx;

	assert(fx_start(&fx) == 0);
	assert(ctl_ask(&fx, "DROPPED", "/admin", &resp) == 0);
	assert(resp.status_code == 200u);
	/* a healthy server still answers, and answers zero rather than nothing */
	assert(body_dropped_decode((const char *)resp.body, resp.body_len,
			&dropped) == 0);
	assert(dropped.dropped == logger_dropped_count(&fx.srv->log));
	htttp_message_free(&resp);
	fx_stop(&fx);
	printf("PASS test_dropped_answers_the_counter\n");
}

static void	test_unknown_method_and_path_are_refused(void)
{
	t_htttp_message	resp;
	t_fixture		fx;

	assert(fx_start(&fx) == 0);
	assert(ctl_ask(&fx, "DANCE", "/admin", &resp) == 0);
	assert(resp.status_code == 404u);
	htttp_message_free(&resp);
	/* a known verb on a path this channel does not serve */
	assert(ctl_ask(&fx, "STATUS", "/admin/player/amber", &resp) == 0);
	assert(resp.status_code == 404u);
	htttp_message_free(&resp);
	/* KICK and SHUTDOWN are not built yet, and are refused rather than half-served */
	assert(ctl_ask(&fx, "SHUTDOWN", "/admin", &resp) == 0);
	assert(resp.status_code == 404u);
	htttp_message_free(&resp);
	assert(ctl_ask(&fx, "STATUS", "/admin", &resp) == 0);
	assert(resp.status_code == 200u); // the channel still works after a refusal
	htttp_message_free(&resp);
	fx_stop(&fx);
	printf("PASS test_unknown_method_and_path_are_refused\n");
}

static void	test_socket_is_private_to_its_owner(void)
{
	struct stat	st;
	t_fixture	fx;

	assert(fx_start(&fx) == 0);
	assert(stat(fx.cfg.control_path, &st) == 0);
	/* reachability is the credential, so the mode is the whole of authz */
	assert((st.st_mode & 0777) == 0600);
	fx_stop(&fx);
	/* closing takes it off the filesystem: a path left behind accepts nothing */
	assert(stat(fx.cfg.control_path, &st) != 0);
	printf("PASS test_socket_is_private_to_its_owner\n");
}

static void	test_the_channel_survives_a_rude_administrator(void)
{
	t_htttp_message	resp;
	t_fixture		fx;
	unsigned char	prefix[4];
	int				rude;

	assert(fx_start(&fx) == 0);
	/* a length no frame could carry ends that connection and only that one */
	rude = ctl_open(&fx);
	assert(rude >= 0);
	buffer_put_u32(prefix, 0xffffffffu);
	assert(write(rude, prefix, sizeof(prefix)) == (ssize_t)sizeof(prefix));
	close(rude);
	/* and one that hangs up mid-request */
	rude = ctl_open(&fx);
	assert(rude >= 0);
	buffer_put_u32(prefix, 64u);
	assert(write(rude, prefix, sizeof(prefix)) == (ssize_t)sizeof(prefix));
	close(rude);
	assert(ctl_ask(&fx, "STATUS", "/admin", &resp) == 0);
	assert(resp.status_code == 200u);
	htttp_message_free(&resp);
	fx_stop(&fx);
	printf("PASS test_the_channel_survives_a_rude_administrator\n");
}

/**
 * @brief Connects one administrator to the fixture's control socket.
 *
 * @param fx Fixture naming the socket.
 * @return The connected descriptor, or -1.
 */
static int	ctl_open(t_fixture *fx)
{
	return (unixsock_stream_connect(fx->cfg.control_path));
}

/**
 * @brief Opens a connection, sends one request, reads one response, closes.
 *
 * That is exactly what tetrisctl does, which is why the connection ceiling is
 * a ceiling rather than a budget.
 *
 * @param fx Fixture naming the socket.
 * @param method Method to send.
 * @param path Path to send.
 * @param out Receives the parsed response; caller frees it.
 * @return 0 on success, -1 on failure.
 */
static int	ctl_ask(t_fixture *fx, const char *method, const char *path, t_htttp_message *out)
{
	int	fd;
	int	rc;

	fd = ctl_open(fx);
	if (fd < 0)
		return (-1);
	rc = ctl_send(fd, method, path);
	if (rc == 0)
		rc = ctl_read(fd, out);
	close(fd);
	return (rc);
}

/**
 * @brief Serialises one request and writes it behind its length prefix.
 *
 * @param fd Connected control descriptor.
 * @param method Method to send.
 * @param path Path to send.
 * @return 0 on success, -1 on failure.
 */
static int	ctl_send(int fd, const char *method, const char *path)
{
	t_htttp_message	req;
	unsigned char	prefix[4];
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	htttp_message_init(&req);
	if (htttp_message_make_request(&req, method, path) != HTTTP_OK
		|| htttp_message_set_header(&req, "Host", "tetrish.local") != HTTTP_OK
		|| htttp_message_set_header(&req, "Client", "tetrisctl") != HTTTP_OK
		|| htttp_serialize(&req, &bytes, &len) != HTTTP_OK)
		return (htttp_message_free(&req), -1);
	htttp_message_free(&req);
	buffer_put_u32(prefix, (uint32_t)len);
	rc = 0;
	if (unixsock_send_all(fd, prefix, sizeof(prefix)) != 0
		|| unixsock_send_all(fd, bytes, len) != 0)
		rc = -1;
	free(bytes);
	return (rc);
}

/**
 * @brief Reads one length-prefixed response frame and parses it.
 *
 * @param fd Connected control descriptor.
 * @param out Receives the parsed response; caller frees it.
 * @return 0 on success, -1 on failure.
 */
static int	ctl_read(int fd, t_htttp_message *out)
{
	unsigned char	prefix[4];
	unsigned char	*bytes;
	uint32_t		len;
	int				rc;

	htttp_message_init(out);
	if (unixsock_recv_all(fd, prefix, sizeof(prefix)) != 0)
		return (-1);
	len = buffer_get_u32(prefix);
	if (len == 0 || len > TETRISD_CONTROL_FRAME_MAX)
		return (-1);
	bytes = malloc(len);
	if (bytes == NULL)
		return (-1);
	rc = 0;
	if (unixsock_recv_all(fd, bytes, len) != 0
		|| htttp_parse(bytes, len, out) != HTTTP_OK)
		rc = -1;
	free(bytes);
	return (rc);
}
