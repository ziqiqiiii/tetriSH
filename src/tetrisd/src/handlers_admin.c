#include "tetrisd.h"

// Static Functions
static unsigned int	route(t_control_connection *conn, const t_htttp_message *msg, size_t *omitted);
static unsigned int	status_handler(t_control_connection *conn);
static unsigned int	rooms_handler(t_control_connection *conn, size_t *omitted);
static unsigned int	players_handler(t_control_connection *conn, size_t *omitted);
static unsigned int	dropped_handler(t_control_connection *conn);
static size_t		collect_players(t_server *srv, t_body_player_row *rows, size_t cap, size_t *omitted);
static void			access_line(t_control_connection *conn, const t_htttp_message *msg, unsigned int status);
static unsigned int	kick_handler(t_control_connection *conn, const t_htttp_message *msg);

/*
** The four read-only admin routes, every one of them answered on the reactor.
**
** That is the whole reason they need no lock. ROOMS reads the lobby, PLAYERS
** walks the connection registry and STATUS counts both - and the reactor is
** the single owner of all three, so each of these is an ordinary read of
** memory this thread already owns rather than a snapshot somebody had to
** coordinate.
**
** None of them changes anything. A body is built into the control channel's
** own scratch buffer, which is why they may be this direct: there is one
** control buffer and requests on it serialise, so no two of these are ever
** part-way through at once.
*/

/**
 * @brief Parses one admin frame, answers it, and logs the exchange.
 *
 * @param conn Connection the request arrived on.
 * @param frame The plaintext HTTTP request bytes.
 * @param len Length of frame.
 */
void	control_handle_frame(t_control_connection *conn, const unsigned char *frame, size_t len)
{
	t_htttp_message	msg;
	unsigned int	status;
	size_t			omitted;

	htttp_message_init(&msg);
	omitted = 0;
	if (htttp_parse(frame, len, &msg) != HTTTP_OK
		|| htttp_validate(&msg, 0) != HTTTP_OK
		|| msg.type != HTTTP_MESSAGE_REQUEST)
	{
		control_reply(conn, 400u, NULL, 0, 0);
		logger_emit(&conn->srv->log, COREIPC_LOG_WARNING,
			"admin (admin) sent an unparseable request -> 400");
		return (htttp_message_free(&msg));
	}
	status = route(conn, &msg, &omitted);
	if (status != 200u)
		control_reply(conn, status, NULL, 0, 0);
	else
		control_reply(conn, status, conn->srv->control.body,
			conn->srv->control.body_len, omitted);
	access_line(conn, &msg, status);
	htttp_message_free(&msg);
}

/**
 * @brief Picks the handler for one admin request.
 *
 * An unknown method and a known method on the wrong path are both 404: the
 * Control channel serves exactly one route per verb, so there is nothing else
 * either could have meant.
 *
 * @param conn Connection the request arrived on.
 * @param msg The parsed request.
 * @param omitted Receives rows a capped listing left out.
 * @return The HTTTP status to answer with.
 */
static unsigned int	route(t_control_connection *conn, const t_htttp_message *msg, size_t *omitted)
{
	if (strncmp(msg->path, TETRISD_CONTROL_ROUTE_PLAYER,
			strlen(TETRISD_CONTROL_ROUTE_PLAYER)) == 0)
	{
		if (strcmp(msg->method, "KICK") == 0)
			return (kick_handler(conn, msg));
		return (404u);
	}
	if (strcmp(msg->path, TETRISD_CONTROL_ROUTE) != 0)
		return (404u);
	if (strcmp(msg->method, "STATUS") == 0)
		return (status_handler(conn));
	if (strcmp(msg->method, "ROOMS") == 0)
		return (rooms_handler(conn, omitted));
	if (strcmp(msg->method, "PLAYERS") == 0)
		return (players_handler(conn, omitted));
	if (strcmp(msg->method, "DROPPED") == 0)
		return (dropped_handler(conn));
	return (404u);
}

/**
 * @brief STATUS /admin - the server's Health (UC-22).
 *
 * @param conn Connection whose scratch body receives the encoding.
 * @return 200, or 500 when the report cannot be encoded.
 */
static unsigned int	status_handler(t_control_connection *conn)
{
	t_body_health	health;
	int				len;

	server_health_assemble(conn->srv, &health);
	len = body_health_encode(&health, conn->srv->control.body,
			TETRISD_CONTROL_BODY_MAX);
	if (len < 0)
		return (500u);
	conn->srv->control.body_len = (size_t)len;
	return (200u);
}

/**
 * @brief ROOMS /admin - the live room directory (UC-25).
 *
 * No open rooms is an empty body and a 200: an empty list is a valid answer,
 * not an error.
 *
 * @param conn Connection whose scratch body receives the encoding.
 * @param omitted Receives rows the cap left out.
 * @return 200, or 500 when the directory cannot be encoded.
 */
static unsigned int	rooms_handler(t_control_connection *conn, size_t *omitted)
{
	t_body_room_row	rows[LOBBY_MAX_ROOMS];
	size_t			count;
	int				len;

	count = server_rooms_list(conn->srv, rows, LOBBY_MAX_ROOMS);
	len = body_rooms_encode(rows, count, conn->srv->control.body,
			TETRISD_CONTROL_BODY_MAX);
	if (len < 0)
		return (500u);
	conn->srv->control.body_len = (size_t)len;
	*omitted = 0;
	return (200u);
}

/**
 * @brief PLAYERS /admin - every connection, logged in or not (UC-26).
 *
 * @param conn Connection whose scratch body receives the encoding.
 * @param omitted Receives connections the cap left out.
 * @return 200, or 500 when the listing cannot be encoded.
 */
static unsigned int	players_handler(t_control_connection *conn, size_t *omitted)
{
	static t_body_player_row	rows[BODY_PLAYERS_MAX];
	size_t						count;
	int							len;

	count = collect_players(conn->srv, rows, BODY_PLAYERS_MAX, omitted);
	len = body_players_encode(rows, count, conn->srv->control.body,
			TETRISD_CONTROL_BODY_MAX);
	if (len < 0)
		return (500u);
	conn->srv->control.body_len = (size_t)len;
	return (200u);
}

/**
 * @brief DROPPED /admin - the producer-side Dropped counter (UC-27).
 *
 * The logger's own state does not affect this answer. tetrisd falls back to
 * stderr when tetrislogd is absent, so there is no "logger unreachable"
 * failure to report here - and Rejected and Degraded are the logger's
 * counters, not this one, and are never added to it.
 *
 * @param conn Connection whose scratch body receives the encoding.
 * @return 200, or 500 when the counter cannot be encoded.
 */
static unsigned int	dropped_handler(t_control_connection *conn)
{
	t_body_dropped	dropped;
	int				len;

	dropped.dropped = logger_dropped_count(&conn->srv->log);
	len = body_dropped_encode(&dropped, conn->srv->control.body,
			TETRISD_CONTROL_BODY_MAX);
	if (len < 0)
		return (500u);
	conn->srv->control.body_len = (size_t)len;
	return (200u);
}

static unsigned int	kick_handler(t_control_connection *conn, const t_htttp_message *msg)
{
	t_player_id	pid;
	t_client	*target;

	pid = request_player_id(msg->path
			+ strlen(TETRISD_CONTROL_ROUTE_PLAYER), NULL);
	if (pid == 0)
		return (400u);
	target = registry_find_other(&conn->srv->reg, pid, NULL);
	if (target == NULL)
		return (404u);
	logger_emit(&conn->srv->log, COREIPC_LOG_WARNING,
		"admin kicked conn %u player %llu (%s)", target->conn_id,
		(unsigned long long)pid, target->username);
	client_kill(target);
	conn->srv->control.body_len = 0;
	return (200u);
}

/**
 * @brief Walks the connection registry into player rows.
 *
 * A connection still being handshaken is skipped: it holds no conn_id an
 * operator could act on, and the pool rather than the reactor owns it until it
 * is adopted.
 *
 * @param srv Server whose registry is walked.
 * @param rows Caller array receiving the rows.
 * @param cap Capacity of rows.
 * @param omitted Receives how many connections did not fit.
 * @return How many rows were written.
 */
static size_t	collect_players(t_server *srv, t_body_player_row *rows, size_t cap, size_t *omitted)
{
	t_client	*cli;
	size_t		count;
	size_t		i;

	count = 0;
	*omitted = 0;
	i = 0;
	while (i < srv->reg.cap)
	{
		cli = srv->reg.slots[i++];
		if (cli == NULL || cli->dead || cli->state == CLI_HANDSHAKE)
			continue ;
		if (count == cap)
		{
			(*omitted)++;
			continue ;
		}
		memset(&rows[count], 0, sizeof(rows[count]));
		rows[count].connection = cli->conn_id;
		rows[count].authenticated = cli->username[0] != '\0';
		if (rows[count].authenticated)
			snprintf(rows[count].username, BODY_USER_MAX, "%s", cli->username);
		snprintf(rows[count].room, BODY_NAME_MAX, "%s", cli->binding.room_name);
		count++;
	}
	return (count);
}

/**
 * @brief Writes the one Access line every admin exchange produces.
 *
 * The sender is rendered `(admin)`. Nothing arriving on this channel names a
 * Player, so there is no name to log and the word says which channel it came
 * in on rather than pretending to identify anybody.
 *
 * @param conn Connection the exchange happened on.
 * @param msg The parsed request.
 * @param status The status answered with.
 */
static void	access_line(t_control_connection *conn, const t_htttp_message *msg, unsigned int status)
{
	logger_emit(&conn->srv->log, COREIPC_LOG_INFO, "admin (admin) %s %s -> %u",
		msg->method, msg->path, status);
}

