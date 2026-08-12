/* ************************************************************************** */
/*                                                                            */
/*   harness.c - the headless client and server fixture the suite drives      */
/*                                                                            */
/*   No test reaches into tetrisd's internals: they connect over TCP, run     */
/*   the real handshake, and send real HTTTP. What a test can observe is      */
/*   exactly what a client can observe.                                       */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

// Static Functions
static int	generate_certs(t_fixture *fx);
static int	connect_tcp(int port);
static int	send_message(t_harness *hc, t_htttp_message *msg);
static int	body_field(const t_htttp_message *msg, const char *key, char *out, size_t cap);
static t_item_id	owned_character(t_fixture *fx, t_player_id pid);

/**
 * @brief Starts a throwaway server: own port, own data directory, own certs.
 *
 * @param fx Fixture to fill.
 * @return 0 on success, -1 when the server would not start.
 */
int	fx_start(t_fixture *fx)
{
	char	path[192];

	memset(fx, 0, sizeof(*fx));
	snprintf(fx->dir, sizeof(fx->dir), "tests/tmp/srvXXXXXX");
	if (daemon_mkdir_p("tests/tmp") != 0 || mkdtemp(fx->dir) == NULL)
		return (-1);
	if (generate_certs(fx) != 0)
		return (-1);
	config_defaults(&fx->cfg);
	fx->cfg.port = 0;
	fx->cfg.log_level = COREIPC_LOG_ERROR;
	snprintf(path, sizeof(path), "%s/data", fx->dir);
	snprintf(fx->cfg.data_dir, TETRISD_FILESYSTEM_PATH_MAX, "%s", path);
	snprintf(fx->cfg.config_dir, TETRISD_FILESYSTEM_PATH_MAX, "%s", "../../lib/libmacminidb/config");
	snprintf(path, sizeof(path), "%s/certs/server.crt", fx->dir);
	snprintf(fx->cfg.cert_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", path);
	snprintf(path, sizeof(path), "%s/certs/server.key", fx->dir);
	snprintf(fx->cfg.key_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", path);
	snprintf(fx->cfg.ca_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", fx->ca_path);
	snprintf(path, sizeof(path), "%s/log.sock", fx->dir);
	snprintf(fx->cfg.log_ipc, TETRISD_FILESYSTEM_PATH_MAX, "%s", path);
	return (server_start(&fx->cfg, &fx->srv));
}

/**
 * @brief Stops the fixture's server and removes its directory.
 *
 * @param fx Fixture to tear down.
 */
void	fx_stop(t_fixture *fx)
{
	char	cmd[256];

	if (fx->srv != NULL)
		server_stop(fx->srv);
	fx->srv = NULL;
	if (fx->dir[0] != '\0')
	{
		snprintf(cmd, sizeof(cmd), "rm -rf %s", fx->dir);
		if (system(cmd) != 0)
			fprintf(stderr, "harness: could not remove %s\n", fx->dir);
	}
}

/**
 * @brief Connects one headless client and completes the secure handshake.
 *
 * @param hc Client to connect.
 * @param fx Fixture naming the port and the CA to verify against.
 * @return 0 on success, -1 on failure.
 */
int	hc_connect(t_harness *hc, const t_fixture *fx)
{
	memset(hc, 0, sizeof(*hc));
	hc->fd = connect_tcp(server_port(fx->srv));
	if (hc->fd < 0)
		return (-1);
	if (session_handshake_client(hc->fd, &hc->sess, fx->ca_path) != 0)
	{
		close(hc->fd);
		hc->fd = -1;
		return (-1);
	}
	return (0);
}

/**
 * @brief Closes a client connection.
 *
 * @param hc Client to close.
 */
void	hc_close(t_harness *hc)
{
	if (hc->fd >= 0)
	{
		session_close(&hc->sess);
		close(hc->fd);
	}
	hc->fd = -1;
}

/**
 * @brief Sends one request and returns the matching response.
 *
 * STATE pushes that arrive while waiting are kept as the latest snapshot
 * rather than dropped: a client's read loop cannot assume one request means
 * the next message is its request_reply, and a renderer would route them onward.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body text, or NULL.
 * @param out Receives the response; the caller frees it.
 * @return 0 on success, -1 on a send, receive, or parse failure.
 */
int	hc_request(t_harness *hc, const char *method, const char *path, const char *body, t_htttp_message *out)
{
	t_htttp_message	req;
	char			pid[32];
	int				rc;

	htttp_message_init(&req);
	rc = -1;
	if (htttp_message_make_request(&req, method, path) == HTTTP_OK)
	{
		if (hc->authed)
		{
			snprintf(pid, sizeof(pid), "%llu", (unsigned long long)hc->player_id);
			htttp_message_set_header(&req, "Player-Id", pid);
		}
		if (body != NULL)
		{
			htttp_message_set_header(&req, "Content-Type", HTTTP_CONTENT_TYPE_COMMAND);
			htttp_message_set_body(&req, body, strlen(body));
		}
		rc = send_message(hc, &req);
	}
	htttp_message_free(&req);
	while (rc == 0)
	{
		if (hc_recv(hc, out, HC_TIMEOUT_MS) != 0)
			return (-1);
		if (out->type == HTTTP_MESSAGE_RESPONSE)
			return (0);
		if (body_state_decode((const char *)out->body, out->body_len, &hc->last_state) == 0)
			hc->has_state = true;
		htttp_message_free(out);
	}
	return (rc);
}

/**
 * @brief Receives the next message of any kind, waiting up to a timeout.
 *
 * @param hc Connected client.
 * @param out Receives the parsed message; the caller frees it.
 * @param timeout_ms How long to wait for the first byte.
 * @return 0 on success, -1 on timeout, close, or a parse failure.
 */
int	hc_recv(t_harness *hc, t_htttp_message *out, int timeout_ms)
{
	unsigned char	buf[TETRISSH_MAX_PLAINTEXT];
	struct pollfd	pfd;
	ssize_t			n;

	pfd.fd = hc->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeout_ms) <= 0)
		return (-1);
	n = session_recv(&hc->sess, buf, sizeof(buf));
	if (n <= 0)
		return (-1);
	htttp_message_init(out);
	if (htttp_parse(buf, (size_t)n, out) != HTTTP_OK)
	{
		htttp_message_free(out);
		return (-1);
	}
	return (0);
}

/**
 * @brief Waits for the next STATE push and decodes its snapshot.
 *
 * @param hc Connected client.
 * @param out Receives the decoded snapshot.
 * @param timeout_ms How long to wait.
 * @return 0 on success, -1 on timeout or a body that would not decode.
 */
int	hc_wait_state(t_harness *hc, t_body_state *out, int timeout_ms)
{
	t_htttp_message	msg;
	int				rc;

	while (hc_recv(hc, &msg, timeout_ms) == 0)
	{
		if (msg.type == HTTTP_MESSAGE_REQUEST && msg.method != NULL && strcmp(msg.method, "STATE") == 0)
		{
			rc = body_state_decode((const char *)msg.body, msg.body_len, out);
			htttp_message_free(&msg);
			return (rc);
		}
		htttp_message_free(&msg);
	}
	return (-1);
}

/**
 * @brief Waits for the next pushed CHAT and decodes the feed line in it.
 *
 * Anything else that arrives first is discarded, exactly as hc_wait_state
 * discards chat: a test that is waiting for one kind of push has already
 * asserted whatever it cared about in the others.
 *
 * @param hc Connected client.
 * @param out Receives the decoded message.
 * @param timeout_ms How long to wait.
 * @return 0 on success, -1 on timeout or a body that would not decode.
 */
int	hc_wait_chat(t_harness *hc, t_body_chat *out, int timeout_ms)
{
	t_htttp_message	msg;
	int				rc;

	while (hc_recv(hc, &msg, timeout_ms) == 0)
	{
		if (msg.type == HTTTP_MESSAGE_REQUEST && msg.method != NULL
			&& strcmp(msg.method, "CHAT") == 0)
		{
			rc = body_chat_decode((const char *)msg.body, msg.body_len, out);
			htttp_message_free(&msg);
			return (rc);
		}
		htttp_message_free(&msg);
	}
	return (-1);
}

/**
 * @brief Registers an account.
 *
 * @param hc Connected client.
 * @param username Account name.
 * @param password Account password.
 * @return The response status code, or -1 when the exchange failed.
 */
int	hc_signup(t_harness *hc, const char *username, const char *password)
{
	t_htttp_message	resp;
	char			body[256];
	int				status;

	snprintf(body, sizeof(body), "username %s\npassword %s\n", username, password);
	if (hc_request(hc, "SIGNUP", TETRISD_ROUTE_ACCOUNT, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Logs in, remembering the player id the server bound to the session.
 *
 * @param hc Connected client.
 * @param username Account name.
 * @param password Account password.
 * @return The response status code, or -1 when the exchange failed.
 */
int	hc_login(t_harness *hc, const char *username, const char *password)
{
	t_htttp_message	resp;
	char			body[256];
	char			value[64];
	int				status;

	snprintf(body, sizeof(body), "username %s\npassword %s\n", username, password);
	if (hc_request(hc, "LOGIN", TETRISD_ROUTE_SESSION, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	if (status == 200 && body_field(&resp, "player-id", value, sizeof(value)) == 0)
	{
		hc->player_id = (t_player_id)strtoull(value, NULL, 10);
		hc->authed = true;
	}
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Creates a room of the given mode and reports its assigned name.
 *
 * @param hc Connected client.
 * @param mode Mode word (single, double, br).
 * @param room_out Receives the room name the lobby assigned.
 * @param cap Size of room_out.
 * @return The response status code, or -1 when the exchange failed.
 */
int	hc_join_new(t_harness *hc, const char *mode, char *room_out, size_t cap)
{
	t_htttp_message	resp;
	char			body[64];
	int				status;

	snprintf(body, sizeof(body), "mode %s\n", mode);
	if (hc_request(hc, "JOIN", TETRISD_ROUTE_ROOMS, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	if (room_out != NULL && cap > 0)
	{
		room_out[0] = '\0';
		body_field(&resp, "room", room_out, cap);
	}
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Declares ready and locks a fighter in, in one request.
 *
 * A room with an opponent in it no longer deals itself from READY or from
 * START - both open the character-select window instead, and the boards are
 * dealt when every seat has named a fighter. So a test that wants a match is
 * a test that has to choose one, and this is that step: every suite that
 * plays a Double game needs it, and none of them care which fighter it picks.
 *
 * The id is read out of the store rather than written down here, because
 * hard-coding one would be a guess about config/characters.cfg. A player who
 * owns none sends a bare declaration, which is still a ready seat - it just
 * is not a locked one, and the room will wait out its clock.
 *
 * @param hc Connected, authenticated client that is seated in the room.
 * @param fx Running fixture, whose store is reopened read-only.
 * @param path Room route the declaration is sent to.
 * @return The response status, or -1 when the request failed.
 */
int	hc_lock_in(t_harness *hc, t_fixture *fx, const char *path)
{
	t_htttp_message	resp;
	t_item_id		pick;
	char			body[64];
	int				status;

	pick = owned_character(fx, hc->player_id);
	if (pick == 0)
		snprintf(body, sizeof(body), "ready 1\n");
	else
		snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
			(unsigned)pick);
	if (hc_request(hc, "READY", path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief A character in the catalogue that this player already owns.
 *
 * @param fx Running fixture, whose store is reopened read-only.
 * @param pid The player to ask about.
 * @return An owned character id, or 0 when the player owns none.
 */
static t_item_id	owned_character(t_fixture *fx, t_player_id pid)
{
	t_character	roster[BODY_CATALOGUE_MAX];
	t_db		*db;
	t_item_id	found;
	size_t		count;
	size_t		i;

	if (db_open(fx->cfg.data_dir, fx->cfg.config_dir, &db) != DB_OK)
		return (0);
	count = 0;
	if (db_characters(db, roster, BODY_CATALOGUE_MAX, &count) != DB_OK)
		count = 0;
	found = 0;
	i = 0;
	while (i < count && found == 0)
	{
		if (db_player_owns_character(db, pid, roster[i].character_id)
			== DB_TRUE)
			found = roster[i].character_id;
		i++;
	}
	db_close(db);
	return (found);
}

/**
 * @brief Generates a throwaway CA and server certificate for one fixture.
 *
 * @param fx Fixture whose directory receives certs/.
 * @return 0 on success, -1 when openssl failed.
 */
static int	generate_certs(t_fixture *fx)
{
	char	cmd[512];

	snprintf(fx->ca_path, sizeof(fx->ca_path), "%s/certs/ca.crt", fx->dir);
	snprintf(cmd, sizeof(cmd), "sh ../../lib/libtetrissh/scripts/generate_test_certs.sh %s/certs", fx->dir);
	if (system(cmd) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Opens a TCP connection to a port on the loopback interface.
 *
 * @param port Port to connect to.
 * @return The connected descriptor, or -1 on failure.
 */
static int	connect_tcp(int port)
{
	struct sockaddr_in	addr;
	int					fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return (-1);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

/**
 * @brief Serialises and sends one message over the session.
 *
 * @param hc Connected client.
 * @param msg Message to send.
 * @return 0 on success, -1 on failure.
 */
static int	send_message(t_harness *hc, t_htttp_message *msg)
{
	unsigned char	*bytes;
	size_t			len;
	ssize_t			sent;

	if (htttp_serialize(msg, &bytes, &len) != HTTTP_OK)
		return (-1);
	sent = session_send(&hc->sess, bytes, len);
	free(bytes);
	if (sent < 0)
		return (-1);
	return (0);
}

/**
 * @brief Reads one `key value` line out of a message body.
 *
 * @param msg Message holding the body.
 * @param key Key to look for.
 * @param out Buffer receiving the value.
 * @param cap Size of out.
 * @return 0 when found, -1 otherwise.
 */
static int	body_field(const t_htttp_message *msg, const char *key, char *out,
			size_t cap)
{
	char		text[TETRISD_BODY_MAX_BYTES];
	const char	*line;
	size_t		key_len;

	if (msg->body == NULL || msg->body_len == 0 || msg->body_len >= sizeof(text))
		return (-1);
	memcpy(text, msg->body, msg->body_len);
	text[msg->body_len] = '\0';
	key_len = strlen(key);
	line = text;
	while (line != NULL && *line != '\0')
	{
		if (strncmp(line, key, key_len) == 0 && line[key_len] == ' ')
		{
			line += key_len + 1;
			snprintf(out, cap, "%.*s", (int)strcspn(line, "\n"), line);
			return (0);
		}
		line = strchr(line, '\n');
		if (line != NULL)
			line++;
	}
	return (-1);
}
