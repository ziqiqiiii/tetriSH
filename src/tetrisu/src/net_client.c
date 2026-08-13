#include "tetrisu.h"
#include "tetrisu_bot.h"

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
static int	connect_within(int fd, const struct sockaddr *addr,
				socklen_t len, int timeout_ms);
static void	socket_deadline(int fd, int timeout_ms);
static int	send_message(t_net_client *net, t_htttp_message *msg);
static int	dispatch_request(t_net_client *net, const char *method,
				const char *path, const char *body);
static int	receive_message(t_net_client *net, t_htttp_message *out,
				int timeout_ms);
static void	note_throttle(t_net_client *net, const t_htttp_message *msg);
static uint64_t	net_now_ms(void);
static void	take_chat(t_net_client *net, const t_htttp_message *msg);
static bool	addressed_to_this_room(const t_net_client *net, const char *path);
static bool	addressed_to_this_game(const t_net_client *net, const char *path);
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
	socket_deadline(net->fd, NET_HANDSHAKE_TIMEOUT_MS);
	console_mute(saved);
	handshake = session_handshake_client(net->fd, &net->sess, cfg->ca_path);
	console_unmute(saved);
	socket_deadline(net->fd, 0);
	if (handshake != 0)
	{
		close(net->fd);
		net->fd = -1;
		snprintf(net->error, sizeof(net->error), "handshake refused");
		return (-1);
	}
	net->state = NET_CONNECTED;
	/*
	** Here rather than at the screens, so that no path can open a session
	** without the bots learning where it went. A player types the address into
	** SERVER ID and it lives in this cfg and nowhere else - not in the
	** environment a forked bot inherits - which is why four bots once spawned
	** looking for a server on the player's own laptop.
	*/
	bot_farm_remember_server(cfg->host, cfg->port, cfg->ca_path);
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
	net->last_seq = 0;
	net->applied_seq = 0;
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
	int				rc;

	if (out != NULL)
		memset(out, 0, sizeof(*out));
	if (net == NULL || net->state == NET_OFFLINE)
		return (-1);
	rc = dispatch_request(net, method, path, body);
	while (rc == 0)
	{
		if (receive_message(net, &msg, NET_REPLY_TIMEOUT_MS) != 0)
		{
			net_disconnect(net);
			return (-1);
		}
		if (msg.type == HTTTP_MESSAGE_RESPONSE)
		{
			take_player_id(net, &msg);
			note_throttle(net, &msg);
			if (out != NULL)
			{
				out->status = (int)msg.status_code;
				take_reason(&msg, out);
				take_body(&msg, out);
			}
			htttp_message_free(&msg);
			return (0);
		}
		net_state_take(net, &msg);
		take_chat(net, &msg);
		htttp_message_free(&msg);
	}
	return (rc);
}

/**
 * @brief Sends one request and does not wait for the answer.
 *
 * The response is left for net_pump, which reads it with everything else the
 * server pushed. Nothing here inspects it, because the callers of this path -
 * the gameplay inputs - act on the snapshot rather than on the verdict.
 *
 * While the server is asking for a back-off the request is dropped instead of
 * sent. That is the point of noticing a 429 at all: a client that keeps
 * sending through one spends the same wire and gets the same refusal, and the
 * board looks the same to the player either way.
 *
 * @param net Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body text, or NULL.
 * @return 0 when the request went out or was deliberately dropped, -1 on a
 *         transport failure.
 */
int	net_send(t_net_client *net, const char *method, const char *path,
		const char *body)
{
	if (net == NULL || net->state == NET_OFFLINE)
		return (-1);
	if (net_throttled(net))
		return (0);
	return (dispatch_request(net, method, path, body));
}

/**
 * @brief Reports whether the server has asked this client to slow down.
 *
 * @param net Client to ask.
 * @return true while a Retry-After the server sent is still running.
 */
bool	net_throttled(const t_net_client *net)
{
	if (net == NULL || net->throttle_until_ms == 0)
		return (false);
	return (net_now_ms() < net->throttle_until_ms);
}

/**
 * @brief Drains whatever the server has already sent, without waiting.
 *
 * Called once per loop turn after poll says the socket is readable. Draining
 * rather than reading one message is what keeps a burst of snapshots from
 * taking one loop turn each; keeping only the latest is what stops a client
 * that fell behind from rendering its way back through history.
 *
 * This is also where the answers to net_send's requests land. All but one of
 * them is dropped on the floor, which is what asking for no answer means; the
 * one is a 429, because a back-off nobody reads is a back-off that never
 * happens.
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
		if (net_state_take(net, &msg))
			fresh = 1;
		take_chat(net, &msg);
		note_throttle(net, &msg);
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
		if (fd >= 0 && connect_within(fd, it->ai_addr, it->ai_addrlen,
				NET_CONNECT_TIMEOUT_MS) != 0)
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
 * @brief Connects with a deadline of our own instead of the kernel's.
 *
 * A blocking connect() waits out the TCP SYN timeout, measured at 75 seconds
 * on macOS, and this runs on the render loop - so a peer that drops the SYN
 * rather than refusing it freezes the whole client on "CHECKING SERVER..."
 * instead of reporting anything. Dropping the SYN is not the exotic case: it
 * is what a host firewall does by default and what hotspot client isolation
 * does to everything.
 *
 * The socket goes non-blocking for the attempt only, poll() bounds the wait,
 * and SO_ERROR carries the verdict - a connect that is still in flight when
 * poll returns is a failure, not something to wait longer for. Blocking is put
 * back before returning, because the handshake above reads normally.
 *
 * @param fd Socket to connect.
 * @param addr Address to reach.
 * @param len Size of addr.
 * @param timeout_ms How long to allow.
 * @return 0 once connected, -1 on error or timeout.
 */
static int	connect_within(int fd, const struct sockaddr *addr,
			socklen_t len, int timeout_ms)
{
	struct pollfd	pfd;
	int				flags;
	int				err;
	socklen_t		errlen;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return (-1);
	if (connect(fd, addr, len) != 0 && errno != EINPROGRESS)
		return (-1);
	pfd.fd = fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeout_ms) != 1)
		return (-1);
	err = 0;
	errlen = sizeof(err);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) != 0 || err != 0)
		return (-1);
	return (fcntl(fd, F_SETFL, flags));
}

/**
 * @brief Puts a receive and send deadline on a socket, or takes it off.
 *
 * session_handshake_client reads and writes straight through the descriptor
 * with no deadline of its own, so a peer that accepts the connection and then
 * says nothing hangs the client for good - not for 75 seconds, but forever.
 * That is what a wrong port looks like, or a right port with something else
 * behind it, or a published container port whose daemon has not finished
 * booting.
 *
 * Taken off again once the handshake is through: every read after it is
 * poll-gated by receive_message, and a timeout left on the socket would turn a
 * message that merely arrives slowly into a dropped session. A zero timeval is
 * how the kernel is told there is no deadline.
 *
 * @param fd Socket to bound.
 * @param timeout_ms Deadline in milliseconds; 0 removes it.
 */
static void	socket_deadline(int fd, int timeout_ms)
{
	struct timeval	tv;

	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	(void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
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
 * @brief Builds one request with this client's identity on it and writes it.
 *
 * Both send paths share this, which is the whole difference between them: what
 * goes out is byte-identical whether or not anybody intends to wait for the
 * answer, so the choice to wait is the caller's alone and never the server's
 * to notice.
 *
 * @param net Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body text, or NULL.
 * @return 0 when the frame was written, -1 on a build or write failure.
 */
static int	dispatch_request(t_net_client *net, const char *method,
		const char *path, const char *body)
{
	t_htttp_message	msg;
	char			pid[32];
	int				rc;

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
	return (rc);
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
 * @return true when the snapshot was accepted, false otherwise.
 */
bool	net_state_take(t_net_client *net, const t_htttp_message *msg)
{
	t_body_state	snap;

	if (net == NULL || msg == NULL || msg->type != HTTTP_MESSAGE_REQUEST
		|| msg->method == NULL
		|| strcmp(msg->method, "STATE") != 0 || msg->body == NULL)
		return (false);
	if (!addressed_to_this_game(net, msg->path))
		return (false);
	if (body_state_decode((const char *)msg->body, msg->body_len, &snap) != 0)
		return (false);
	if (net->has_state && snap.seq < net->last_seq)
		return (false);
	net->state_snapshot = snap;
	net->last_seq = snap.seq;
	net->has_state = true;
	/*
	 * A snapshot addressed to this client's own play path is the server
	 * saying it is running a game for this player, and that is the only
	 * honest definition of "in game" this side has. START is not one: in a
	 * match only the owner sends it, so the player who merely joined would
	 * sit at IN_ROOM for the whole game and have every input refused here,
	 * before it ever reached the socket - which a game loop reads as a lost
	 * session rather than as a rule it broke.
	 *
	 * It belongs in this function rather than in either mode's apply, because
	 * both readers of the socket land here and the fact is about the session,
	 * not about whichever view model happens to consume the frame.
	 */
	if (net->state == NET_IN_ROOM)
		net->state = NET_IN_GAME;
	return (true);
}

/**
 * @brief Checks that a pushed STATE belongs to the game this client renders.
 *
 * Room names are reusable, so the whole player path is compared rather than
 * only the room segment. An empty play path means START has not yet established
 * a game, or LEAVE has already torn it down, and no snapshot is accepted.
 *
 * @param net Client to ask.
 * @param path The pushed message's path.
 * @return true when the path is the client's current game path.
 */
static bool	addressed_to_this_game(const t_net_client *net, const char *path)
{
	if (net == NULL || path == NULL || net->play_path[0] == '\0')
		return (false);
	return (strcmp(path, net->play_path) == 0);
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

/**
 * @brief Records a back-off the server asked for.
 *
 * Retry-After is read rather than assumed, because it is the server's number:
 * dispatch.c sends 1 today, and a client that had hard-coded that would be
 * wrong the moment it changed. A malformed or absent value falls back to one
 * second, which is the same answer as guessing but says so.
 *
 * @param net Client whose back-off is being set.
 * @param msg The message just read; anything but a 429 response is ignored.
 */
static void	note_throttle(t_net_client *net, const t_htttp_message *msg)
{
	const char	*retry;
	long		seconds;

	if (msg->type != HTTTP_MESSAGE_RESPONSE || msg->status_code != 429u)
		return ;
	retry = htttp_message_get_header(msg, "Retry-After");
	seconds = 1;
	if (retry != NULL)
		seconds = strtol(retry, NULL, 10);
	if (seconds < 1)
		seconds = 1;
	if (seconds > 30)
		seconds = 30;
	net->throttle_until_ms = net_now_ms() + (uint64_t)seconds * 1000u;
	net->throttled++;
}

/**
 * @brief The monotonic millisecond clock the back-off is measured on.
 *
 * Monotonic and not wall-clock: a back-off is a duration, and a clock that can
 * step backwards would leave one running until the system time caught up.
 *
 * @return Milliseconds since an unspecified fixed point.
 */
static uint64_t	net_now_ms(void)
{
	struct timespec	now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return (0);
	return ((uint64_t)now.tv_sec * 1000u + (uint64_t)(now.tv_nsec / 1000000));
}
