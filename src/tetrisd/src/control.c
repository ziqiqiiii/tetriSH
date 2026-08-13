#include "tetrisd.h"

// Static Functions
static void	blank_connection(t_control_connection *conn, t_server *srv);
static void	reset_connection(t_control_connection *conn, int fd);
static int	watch_control(t_server *srv, int fd, void *ptr, bool writable);
static void	seat_connection(t_server *srv, int fd);
static bool	read_chunk(t_control_connection *conn);
static bool	drain_frames(t_control_connection *conn);

/*
** The Control channel: the local, Administrator-only way in, separate from the
** port players connect to.
**
** It lives in the reactor's own epoll set rather than on a thread of its own.
** The reason is the invariant, not the mechanism: every answer an admin route
** gives is read straight out of the lobby, the registry and the logger, and
** the reactor is the single owner of all three. A listener thread would have
** had to hand each request over the wake fd and wait for it, which is a
** round trip and a rendezvous bought to reach state the reactor was already
** sitting on. Here the request is answered on the thread that owns the answer,
** so serving ROOMS, PLAYERS and DROPPED needs no lock and no handoff.
**
** Nothing arriving here names a Player. Authorisation is the 0600 mode on the
** socket - the reachability is the credential - so there is no session, no
** handshake and no encryption on this path, and the frame is plaintext HTTTP
** behind the same 4-byte length prefix the game path uses.
*/

/**
 * @brief Opens the Control channel and puts its listener in the epoll set.
 *
 * @param srv Server being brought up; its epoll set must already be open.
 * @return 0 on success, -1 when the socket could not be created or watched.
 */
int	control_open(t_server *srv)
{
	int	i;

	srv->control.tag.source = EVENT_CONTROL_LISTENER;
	srv->control.listen_fd = -1;
	srv->control.stop_requested = false;
	i = 0;
	while (i < TETRISD_CONTROL_MAX_CONNECTIONS)
		blank_connection(&srv->control.slots[i++], srv);
	snprintf(srv->control.path, TETRISD_FILESYSTEM_PATH_MAX, "%s",
		srv->cfg.control_path);
	if (daemon_mkdir_parent(srv->control.path) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR,
			"cannot create the directory for %s: %s", srv->control.path,
			strerror(errno));
		return (-1);
	}
	srv->control.listen_fd = unixsock_stream_listen(srv->control.path,
			TETRISD_CONTROL_BACKLOG, TETRISD_CONTROL_SOCKET_MODE);
	if (srv->control.listen_fd < 0)
		return (logger_emit(&srv->log, COREIPC_LOG_ERROR,
				"cannot listen on the control channel %s: %s",
				srv->control.path, strerror(errno)), -1);
	if (unixsock_set_nonblock(srv->control.listen_fd) != 0
		|| watch_control(srv, srv->control.listen_fd, &srv->control.tag,
			false) != 0)
		return (-1);
	logger_emit(&srv->log, COREIPC_LOG_INFO, "control channel on %s",
		srv->control.path);
	return (0);
}

/**
 * @brief Closes every admin connection and takes the socket off the
 *        filesystem.
 *
 * Unlinking is part of closing rather than left to the next boot: the path is
 * how an Administrator finds the channel, and one left behind after exit is a
 * socket that accepts nothing.
 *
 * @param srv Server whose control channel is torn down.
 */
void	control_close(t_server *srv)
{
	int	i;

	i = 0;
	while (i < TETRISD_CONTROL_MAX_CONNECTIONS)
	{
		if (srv->control.slots[i].open)
			control_hangup(&srv->control.slots[i]);
		buffer_free(&srv->control.slots[i].recv);
		buffer_free(&srv->control.slots[i].send);
		i++;
	}
	if (srv->control.listen_fd >= 0)
		unixsock_close_unlink(srv->control.listen_fd, srv->control.path);
	srv->control.listen_fd = -1;
}

/**
 * @brief Accepts every administrator currently pending on the channel.
 *
 * The connection ceiling is enforced by hanging up immediately rather than by
 * leaving the connection in the backlog: an Administrator told nothing would
 * wait for a reply that no longer has a slot to be computed in.
 *
 * @param srv Server whose control listener is ready.
 */
void	control_accept_ready(t_server *srv)
{
	int	fd;

	fd = unixsock_stream_accept(srv->control.listen_fd);
	while (fd >= 0)
	{
		seat_connection(srv, fd);
		fd = unixsock_stream_accept(srv->control.listen_fd);
	}
}

/**
 * @brief Handles one epoll event on an established admin connection.
 *
 * @param conn Connection the event names.
 * @param events The epoll event mask reported for it.
 */
void	control_connection_ready(t_control_connection *conn, uint32_t events)
{
	if (conn == NULL || !conn->open)
		return ;
	if (events & EPOLLOUT)
		control_flush(conn);
	if (conn->open && (events & (EPOLLIN | EPOLLHUP | EPOLLERR)))
	{
		while (read_chunk(conn))
		{
			if (!drain_frames(conn))
				return ;
		}
		control_flush(conn);
	}
}

/**
 * @brief Puts one connection slot into the "nothing open here" state.
 *
 * @param conn Slot to blank.
 * @param srv Server the slot belongs to.
 */
static void	blank_connection(t_control_connection *conn, t_server *srv)
{
	memset(conn, 0, sizeof(*conn));
	conn->tag.source = EVENT_CONTROL;
	conn->srv = srv;
	conn->fd = -1;
	conn->open = false;
}

/**
 * @brief Readies a slot for a new administrator, keeping its buffers.
 *
 * Deliberately not blank_connection. That memsets the slot, which would zero
 * the receive and send buffer pointers rather than free them - so every
 * administrator after the first would leak the buffers the last one grew. The
 * allocations are a per-slot pool instead: reused across connections, and
 * released once by control_close.
 *
 * @param conn Slot to ready; its srv and buffers are left as they are.
 * @param fd The accepted descriptor.
 */
static void	reset_connection(t_control_connection *conn, int fd)
{
	conn->tag.source = EVENT_CONTROL;
	conn->fd = fd;
	conn->open = true;
	conn->writable_armed = false;
	conn->recv.len = 0;
	conn->recv.used = 0;
	conn->send.len = 0;
	conn->send.used = 0;
}

/**
 * @brief Adds or modifies one control descriptor in the epoll set.
 *
 * @param srv Server holding the epoll descriptor.
 * @param fd Descriptor to watch.
 * @param ptr What the reactor reads the event back as; its tag comes first.
 * @param writable true to also be told when the socket can take more.
 * @return 0 on success, -1 on failure.
 */
static int	watch_control(t_server *srv, int fd, void *ptr, bool writable)
{
	struct epoll_event	ev;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	if (writable)
		ev.events |= EPOLLOUT;
	ev.data.ptr = ptr;
	if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, fd, &ev) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR,
			"cannot watch control fd %d: %s", fd, strerror(errno));
		return (-1);
	}
	return (0);
}

/**
 * @brief Puts one accepted descriptor into a free slot, or refuses it.
 *
 * @param srv Server whose slots are searched.
 * @param fd The accepted descriptor; closed here when nothing is free.
 */
static void	seat_connection(t_server *srv, int fd)
{
	t_control_connection	*conn;
	int						i;

	i = 0;
	while (i < TETRISD_CONTROL_MAX_CONNECTIONS && srv->control.slots[i].open)
		i++;
	if (i == TETRISD_CONTROL_MAX_CONNECTIONS)
	{
		logger_emit(&srv->log, COREIPC_LOG_WARNING,
			"refused an admin connection: all %d control slots are in use",
			TETRISD_CONTROL_MAX_CONNECTIONS);
		close(fd);
		return ;
	}
	conn = &srv->control.slots[i];
	reset_connection(conn, fd);
	if (unixsock_set_nonblock(fd) != 0
		|| watch_control(srv, fd, conn, false) != 0)
		control_hangup(conn);
}

/**
 * @brief Reads one chunk from an administrator into its receive buffer.
 *
 * @param conn Connection to read from; hung up here on error or a closed peer.
 * @return true when bytes arrived and reading may continue, false when the
 *         socket is drained, the peer is gone, or the connection was ended.
 */
static bool	read_chunk(t_control_connection *conn)
{
	size_t	want;
	ssize_t	n;

	buffer_compact(&conn->recv);
	want = conn->recv.len + TETRISD_READ_CHUNK_BYTES;
	if (want > TETRISD_LENGTH_PREFIX_BYTES + TETRISD_CONTROL_FRAME_MAX)
		want = TETRISD_LENGTH_PREFIX_BYTES + TETRISD_CONTROL_FRAME_MAX;
	if (want <= conn->recv.len || buffer_reserve(&conn->recv, want) != 0)
		return (control_hangup(conn), false);
	n = recv(conn->fd, conn->recv.data + conn->recv.len,
			want - conn->recv.len, 0);
	if (n > 0)
	{
		conn->recv.len += (size_t)n;
		return (true);
	}
	if (n < 0 && errno == EINTR)
		return (true);
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return (false);
	return (control_hangup(conn), false);
}

/**
 * @brief Answers every complete request sitting in the buffer.
 *
 * A length no frame could carry ends the connection rather than being skipped.
 * Nothing here resynchronises: an Administrator that is not speaking the
 * protocol is a bug or a probe, and neither improves by being read further.
 *
 * A request arriving while a reply is still queued is left in the buffer.
 * Admin requests serialise, and tetrisctl sends one and waits, so this only
 * bounds a client that does not.
 *
 * @param conn Connection whose buffer is examined.
 * @return true when the connection is still open, false when it was ended.
 */
static bool	drain_frames(t_control_connection *conn)
{
	uint32_t	frame_len;

	while (conn->recv.len - conn->recv.used >= TETRISD_LENGTH_PREFIX_BYTES)
	{
		if (conn->send.used < conn->send.len)
			return (true);
		frame_len = buffer_get_u32(conn->recv.data + conn->recv.used);
		if (frame_len == 0 || frame_len > TETRISD_CONTROL_FRAME_MAX)
			return (control_hangup(conn), false);
		if (conn->recv.len - conn->recv.used
			< TETRISD_LENGTH_PREFIX_BYTES + (size_t)frame_len)
			return (true);
		control_handle_frame(conn, conn->recv.data + conn->recv.used
			+ TETRISD_LENGTH_PREFIX_BYTES, (size_t)frame_len);
		conn->recv.used += TETRISD_LENGTH_PREFIX_BYTES + (size_t)frame_len;
		if (!conn->open)
			return (false);
	}
	return (true);
}
