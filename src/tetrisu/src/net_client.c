#include "tetrisu.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

/*
** The socket, and the one rule that shapes everything above it: a client's
** read never assumes the next message is the answer to its last request.
**
** tetrisd pushes STATE on its own clock, so a reply and a snapshot can cross
** on the wire. Every read therefore sorts what it got - a response is
** returned to whoever asked, a STATE is filed as the latest snapshot - and
** the game loop calls net_pump for the ones nobody asked for.
*/

// Static Functions
static int	dial(const char *host, int port);
static int	send_message(t_net_client *net, t_htttp_message *msg);
static int	receive_message(t_net_client *net, t_htttp_message *out,
				int timeout_ms);
static bool	take_state(t_net_client *net, const t_htttp_message *msg);
static void	take_chat(t_net_client *net, const t_htttp_message *msg);
static bool	addressed_to_this_room(const t_net_client *net, const char *path);
static void	take_player_id(t_net_client *net, const t_htttp_message *msg);
static void	take_reason(const t_htttp_message *msg, t_net_result *out);
static void	take_body(const t_htttp_message *msg, t_net_result *out);
static int	env_port(const char *value, int fallback);
static void	console_mute(int saved[2]);
static void	console_unmute(int saved[2]);

/**
 * @brief Reads where tetrisd is out of the environment .tetrishrc exports.
 *
 * No path is compiled in: the rc file is where this system writes down where
 * things are, and a client that hard-coded a certificate path could not be
 * pointed at a second server without rebuilding it.
 *
 * @param cfg Receives the resolved settings; defaults when nothing is set.
 */
void	net_config_load(t_net_config *cfg)
{
	const char	*value;

	memset(cfg, 0, sizeof(*cfg));
	value = getenv("TETRISU_HOST");
	if (value == NULL || value[0] == '\0')
		value = NET_DEFAULT_HOST;
	snprintf(cfg->host, sizeof(cfg->host), "%s", value);
	cfg->port = env_port(getenv("TETRISU_PORT"), NET_DEFAULT_PORT);
	value = getenv("TETRISU_CA_PATH");
	if (value == NULL || value[0] == '\0')
		value = NET_DEFAULT_CA_PATH;
	snprintf(cfg->ca_path, sizeof(cfg->ca_path), "%s", value);
}

/**
 * @brief Opens a session to tetrisd: TCP, then the libtetrissh handshake.
 *
 * The handshake is the one blocking thing the client does, and it happens
 * before there is a game to keep running, which is why it is allowed to be
 * blocking here when nothing else on this path is.
 *
 * It is also the one thing on this path that writes to the terminal. The
 * certificate report comes out of lib/libtetrissh/src/common.c, which is the
 * frozen course-provided helper and cannot be quietened where it is written
 * - and notcurses owns the screen those lines land on, so they scroll the
 * board out from under the player. The descriptors are therefore muted for
 * the length of the handshake and given straight back.
 *
 * Any client already open on this handle is closed first: net_connect blanked
 * the struct and leaked the socket, which is how a second sign-in left a
 * connection tetrisd still believed in.
 *
 * @param net Client to bring up; blanked first.
 * @param cfg Where the server is and which CA to trust.
 * @return 0 on success, -1 with net->error saying which step failed.
 */
int	net_connect(t_net_client *net, const t_net_config *cfg)
{
	int	saved[2];
	int	handshake;

	if (net->state != NET_OFFLINE)
		net_disconnect(net);
	memset(net, 0, sizeof(*net));
	net->fd = -1;
	net->fd = dial(cfg->host, cfg->port);
	if (net->fd < 0)
	{
		snprintf(net->error, sizeof(net->error), "no server");
		return (-1);
	}
	console_mute(saved);
	handshake = session_handshake_client(net->fd, &net->sess, cfg->ca_path);
	console_unmute(saved);
	if (handshake != 0)
	{
		close(net->fd);
		net->fd = -1;
		snprintf(net->error, sizeof(net->error), "handshake refused");
		return (-1);
	}
	net->state = NET_CONNECTED;
	return (0);
}

/**
 * @brief Closes the session and returns the client to offline.
 *
 * session_close wipes the keys and forgets the descriptor; it deliberately
 * does not close it, because the descriptor is the caller's (libtetrissh's
 * README says so, and tetrisd's own release path calls both). So the socket
 * is closed here. Leaving it open leaked a descriptor per reconnect, and -
 * far worse than the descriptor - sent no FIN, so tetrisd went on believing
 * in a connection nobody was reading and held the player's seat with it.
 *
 * @param net Client to close; safe on one that never connected.
 */
void	net_disconnect(t_net_client *net)
{
	if (net == NULL)
		return ;
	if (net->fd >= 0)
	{
		session_close(&net->sess);
		close(net->fd);
	}
	net->fd = -1;
	net->state = NET_OFFLINE;
	net->has_state = false;
	net_chat_reset(net);
}

/**
 * @brief The descriptor a caller polls to know a message is waiting.
 *
 * The game loop already waits on the terminal with a computed timeout; this
 * is what lets it wait on the server in the same call instead of choosing
 * between reading input and reading the network.
 *
 * @param net Client to ask.
 * @return The socket, or -1 when there is no session.
 */
int	net_fd(const t_net_client *net)
{
	if (net == NULL || net->state == NET_OFFLINE)
		return (-1);
	return (net->fd);
}

/**
 * @brief Sends one request and waits for the response that answers it.
 *
 * STATE pushes that arrive while waiting are filed rather than dropped, so a
 * snapshot never goes missing just because it crossed a request.
 *
 * @param net Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body text, or NULL.
 * @param out Receives the status and any refusal reason; may be NULL.
 * @return 0 when a response arrived, -1 on a transport failure.
 */
int	net_request(t_net_client *net, const char *method, const char *path,
		const char *body, t_net_result *out)
{
	t_htttp_message	msg;
	char			pid[32];
	int				rc;

	if (out != NULL)
		memset(out, 0, sizeof(*out));
	if (net == NULL || net->state == NET_OFFLINE)
		return (-1);
	htttp_message_init(&msg);
	rc = -1;
	if (htttp_message_make_request(&msg, method, path) == HTTTP_OK)
	{
		if (net->player_id != 0)
		{
			snprintf(pid, sizeof(pid), "%llu",
				(unsigned long long)net->player_id);
			htttp_message_set_header(&msg, "Player-Id", pid);
		}
		if (body != NULL && body[0] != '\0')
		{
			htttp_message_set_header(&msg, "Content-Type",
				HTTTP_CONTENT_TYPE_COMMAND);
			htttp_message_set_body(&msg, body, strlen(body));
		}
		rc = send_message(net, &msg);
	}
	htttp_message_free(&msg);
	while (rc == 0)
	{
		if (receive_message(net, &msg, NET_REPLY_TIMEOUT_MS) != 0)
			return (-1);
		if (msg.type == HTTTP_MESSAGE_RESPONSE)
		{
			take_player_id(net, &msg);
			if (out != NULL)
			{
				out->status = (int)msg.status_code;
				take_reason(&msg, out);
				take_body(&msg, out);
			}
			htttp_message_free(&msg);
			return (0);
		}
		take_state(net, &msg);
		take_chat(net, &msg);
		htttp_message_free(&msg);
	}
	return (rc);
}

/**
 * @brief Drains whatever the server has already sent, without waiting.
 *
 * Called once per loop turn after poll says the socket is readable. Draining
 * rather than reading one message is what keeps a burst of snapshots from
 * taking one loop turn each; keeping only the latest is what stops a client
 * that fell behind from rendering its way back through history.
 *
 * @param net Connected client.
 * @return 1 when a new snapshot arrived, 0 when nothing did, -1 on a closed
 *         or broken session.
 */
int	net_pump(t_net_client *net)
{
	t_htttp_message	msg;
	int				fresh;

	fresh = 0;
	if (net == NULL || net->state == NET_OFFLINE)
		return (-1);
	while (receive_message(net, &msg, 0) == 0)
	{
		if (take_state(net, &msg))
			fresh = 1;
		take_chat(net, &msg);
		htttp_message_free(&msg);
	}
	if (net->state == NET_OFFLINE)
		return (-1);
	return (fresh);
}

/**
 * @brief Opens a TCP connection to one host and port.
 *
 * TCP_NODELAY is set because every message this client sends is small and
 * latency-bearing: waiting to coalesce a keypress with the next one would
 * add exactly the delay a player would feel.
 *
 * @param host Hostname or address text.
 * @param port TCP port.
 * @return A connected descriptor, or -1.
 */
static int	dial(const char *host, int port)
{
	struct addrinfo	hints;
	struct addrinfo	*list;
	struct addrinfo	*it;
	char			service[16];
	int				fd;
	int				one;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(service, sizeof(service), "%d", port);
	if (getaddrinfo(host, service, &hints, &list) != 0)
		return (-1);
	fd = -1;
	it = list;
	while (it != NULL && fd < 0)
	{
		fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (fd >= 0 && connect(fd, it->ai_addr, it->ai_addrlen) != 0)
		{
			close(fd);
			fd = -1;
		}
		it = it->ai_next;
	}
	freeaddrinfo(list);
	one = 1;
	if (fd >= 0)
		(void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	return (fd);
}

/**
 * @brief Serialises one message and writes it as a sealed frame.
 *
 * @param net Connected client.
 * @param msg Message to send.
 * @return 0 on success, -1 on a serialise or write failure.
 */
static int	send_message(t_net_client *net, t_htttp_message *msg)
{
	unsigned char	*bytes;
	size_t			len;
	ssize_t			sent;

	bytes = NULL;
	len = 0;
	if (htttp_serialize(msg, &bytes, &len) != HTTTP_OK)
		return (-1);
	sent = session_send(&net->sess, bytes, len);
	free(bytes);
	if (sent <= 0)
	{
		net_disconnect(net);
		return (-1);
	}
	return (0);
}

/**
 * @brief Receives and parses the next message, waiting up to a timeout.
 *
 * A timeout of zero polls: it reports "nothing yet" rather than blocking,
 * which is what the drain wants.
 *
 * @param net Connected client.
 * @param out Receives the parsed message; the caller frees it.
 * @param timeout_ms How long to wait for the first byte.
 * @return 0 on success, -1 on timeout, close, or a parse failure.
 */
static int	receive_message(t_net_client *net, t_htttp_message *out,
			int timeout_ms)
{
	unsigned char	buf[TETRISSH_MAX_PLAINTEXT];
	struct pollfd	pfd;
	ssize_t			n;

	pfd.fd = net->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeout_ms) <= 0)
		return (-1);
	n = session_recv(&net->sess, buf, sizeof(buf));
	if (n <= 0)
	{
		net_disconnect(net);
		return (-1);
	}
	htttp_message_init(out);
	if (htttp_parse(buf, (size_t)n, out) != HTTTP_OK)
	{
		htttp_message_free(out);
		return (-1);
	}
	return (0);
}

/**
 * @brief Files a server-pushed STATE as the latest snapshot.
 *
 * Snapshots carry a sequence number and an older one is dropped, so a frame
 * that arrives out of order cannot walk the board backwards - which is the
 * acceptance check docs/tetrisu-local-to-tetrisd.md asks for.
 *
 * @param net Client whose snapshot is being replaced.
 * @param msg The message that arrived.
 * @return true when this was a newer snapshot, false otherwise.
 */
static bool	take_state(t_net_client *net, const t_htttp_message *msg)
{
	t_body_state	snap;

	if (msg->type != HTTTP_MESSAGE_REQUEST || msg->method == NULL
		|| strcmp(msg->method, "STATE") != 0 || msg->body == NULL)
		return (false);
	if (body_state_decode((const char *)msg->body, msg->body_len, &snap) != 0)
		return (false);
	if (net->has_state && snap.seq < net->last_seq)
		return (false);
	net->state_snapshot = snap;
	net->last_seq = snap.seq;
	net->has_state = true;
	return (true);
}

/**
 * @brief Files a server-pushed CHAT into the room's feed.
 *
 * Called from both readers. A chat line that crosses a reply is read here by
 * net_request, not by net_pump, and dropping it because the wrong function
 * happened to see it is the mistake `applied_seq` already documents for
 * snapshots.
 *
 * Unlike a snapshot a line is never superseded, so nothing is compared: every
 * message that decodes is kept, and the ring decides what falls off the end.
 *
 * What is compared is the room. A line the server sent while this client was
 * still seated elsewhere can arrive after it has joined the next room, and
 * filing it would put one room's conversation in another's panel.
 *
 * @param net Client whose feed receives the line.
 * @param msg The message that arrived.
 */
static void	take_chat(t_net_client *net, const t_htttp_message *msg)
{
	t_body_chat	line;

	if (msg->type != HTTTP_MESSAGE_REQUEST || msg->method == NULL
		|| strcmp(msg->method, "CHAT") != 0 || msg->body == NULL)
		return ;
	if (!addressed_to_this_room(net, msg->path))
		return ;
	if (body_chat_decode((const char *)msg->body, msg->body_len, &line) != 0)
		return ;
	net_chat_take(net, &line);
}

/**
 * @brief Checks that a pushed CHAT names the room this client is sitting in.
 *
 * @param net Client to ask.
 * @param path The pushed message's path.
 * @return true when the path is /room/<name> and names this client's room.
 */
static bool	addressed_to_this_room(const t_net_client *net, const char *path)
{
	size_t	prefix_len;

	if (path == NULL || net->room[0] == '\0')
		return (false);
	prefix_len = strlen(TETRISU_ROUTE_ROOM);
	if (strncmp(path, TETRISU_ROUTE_ROOM, prefix_len) != 0)
		return (false);
	return (strcmp(path + prefix_len, net->room) == 0);
}

/**
 * @brief Reads one `key value` line out of a server answer's body.
 *
 * Response bodies are the same plaintext line format requests use, so a
 * caller that wants one field asks for it rather than carrying a decoder.
 *
 * @param result The answer to read.
 * @param key Key to look for.
 * @param out Buffer receiving the value.
 * @param cap Size of out.
 * @return out on success, NULL when the key is absent or does not fit.
 */
const char	*net_result_field(const t_net_result *result, const char *key,
			char *out, size_t cap)
{
	const char	*line;
	const char	*end;
	size_t		key_len;

	if (result == NULL || key == NULL || out == NULL || cap == 0)
		return (NULL);
	key_len = strlen(key);
	line = result->body;
	while (*line != '\0')
	{
		end = strchr(line, '\n');
		if (end == NULL)
			end = line + strlen(line);
		if ((size_t)(end - line) > key_len + 1
			&& strncmp(line, key, key_len) == 0 && line[key_len] == ' ')
		{
			if ((size_t)(end - line) - key_len - 1 >= cap)
				return (NULL);
			memcpy(out, line + key_len + 1, (size_t)(end - line) - key_len - 1);
			out[(size_t)(end - line) - key_len - 1] = '\0';
			return (out);
		}
		line = end;
		while (*line == '\n')
			line++;
	}
	return (NULL);
}

/**
 * @brief Learns which player this connection is bound to.
 *
 * tetrisd puts Player-Id on every authenticated answer, so the client reads
 * it from there rather than from a body: one place to learn who it is means
 * it cannot end up with two ideas about that.
 *
 * @param net Client to bind.
 * @param msg The response that arrived.
 */
static void	take_player_id(t_net_client *net, const t_htttp_message *msg)
{
	const char			*value;
	unsigned long long	id;
	char				*end;

	if (net->player_id != 0)
		return ;
	value = htttp_message_get_header(msg, "Player-Id");
	if (value == NULL || value[0] == '\0')
		return ;
	errno = 0;
	id = strtoull(value, &end, 10);
	if (errno == 0 && end != value && *end == '\0' && id != 0)
		net->player_id = (uint64_t)id;
}

/**
 * @brief Reads the `reason` word out of a refusal body.
 *
 * @param msg The response.
 * @param out Result whose reason field is filled; left empty when there is
 *        none to read.
 */
static void	take_reason(const t_htttp_message *msg, t_net_result *out)
{
	const char	*text;
	const char	*end;
	size_t		len;

	if (msg->body == NULL || msg->body_len <= strlen("reason ")
		|| memcmp(msg->body, "reason ", strlen("reason ")) != 0)
		return ;
	text = (const char *)msg->body + strlen("reason ");
	len = msg->body_len - strlen("reason ");
	end = memchr(text, '\n', len);
	if (end != NULL)
		len = (size_t)(end - text);
	while (len > 0 && text[len - 1] == '\r')
		len--;
	if (len >= sizeof(out->reason))
		len = sizeof(out->reason) - 1;
	memcpy(out->reason, text, len);
	out->reason[len] = '\0';
}

/**
 * @brief Keeps a copy of an answer's body for the caller to read fields from.
 *
 * @param msg The response.
 * @param out Result whose body copy is filled; truncated rather than grown,
 *        because a status body that would not fit is not one this client
 *        knows how to want.
 */
static void	take_body(const t_htttp_message *msg, t_net_result *out)
{
	size_t	len;

	out->body[0] = '\0';
	if (msg->body == NULL || msg->body_len == 0)
		return ;
	len = msg->body_len;
	if (len >= sizeof(out->body))
		len = sizeof(out->body) - 1;
	memcpy(out->body, msg->body, len);
	out->body[len] = '\0';
}

/**
 * @brief Parses a port out of an environment value, refusing nonsense.
 *
 * @param value The text, or NULL.
 * @param fallback What to use when it is absent or out of range.
 * @return A usable TCP port.
 */
static int	env_port(const char *value, int fallback)
{
	long	port;
	char	*end;

	if (value == NULL || value[0] == '\0')
		return (fallback);
	errno = 0;
	port = strtol(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0'
		|| port < 1 || port > 65535)
		return (fallback);
	return ((int)port);
}

/**
 * @brief Points stdout and stderr somewhere harmless and remembers where
 *        they were.
 *
 * TETRISU_NET_LOG names a file to keep the muted output in, because a
 * handshake that fails silently is worse than one that scrolls the screen.
 * Without it the output is discarded: net->error is what the screen shows,
 * and it says the same thing.
 *
 * Both descriptors are restored or neither is muted - a half-applied
 * redirection would leave the caller with no way back.
 *
 * @param saved Receives the duplicated descriptors, or -1 when nothing was
 *        muted.
 */
static void	console_mute(int saved[2])
{
	const char	*log_path;
	int			sink;

	saved[0] = -1;
	saved[1] = -1;
	log_path = getenv("TETRISU_NET_LOG");
	if (log_path != NULL && log_path[0] != '\0')
		sink = open(log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	else
		sink = open("/dev/null", O_WRONLY | O_CLOEXEC);
	if (sink < 0)
		return ;
	fflush(stdout);
	fflush(stderr);
	saved[0] = dup(STDOUT_FILENO);
	saved[1] = dup(STDERR_FILENO);
	if (saved[0] < 0 || saved[1] < 0)
	{
		if (saved[0] >= 0)
			close(saved[0]);
		if (saved[1] >= 0)
			close(saved[1]);
		saved[0] = -1;
		saved[1] = -1;
	}
	else
	{
		(void)dup2(sink, STDOUT_FILENO);
		(void)dup2(sink, STDERR_FILENO);
	}
	close(sink);
}

/**
 * @brief Puts stdout and stderr back where console_mute found them.
 *
 * The flush happens while the sink is still installed, so anything the muted
 * code left in a stdio buffer goes to the sink rather than arriving on the
 * terminal one statement after it was unmuted.
 *
 * @param saved The descriptors console_mute duplicated.
 */
static void	console_unmute(int saved[2])
{
	if (saved[0] < 0 || saved[1] < 0)
		return ;
	fflush(stdout);
	fflush(stderr);
	(void)dup2(saved[0], STDOUT_FILENO);
	(void)dup2(saved[1], STDERR_FILENO);
	close(saved[0]);
	close(saved[1]);
	saved[0] = -1;
	saved[1] = -1;
}
