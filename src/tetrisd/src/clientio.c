#include "tetrisd.h"

// Static Functions
static bool	read_chunk(t_client *cli);
static bool	drain_frames(t_client *cli);
static bool	load_frame(t_client *cli);
static void	watch_writable(t_client *cli, bool wanted);

/**
 * @brief Reads whatever the peer has sent and answers every complete frame.
 *
 * The reactor owns the socket and the buffer; libtetrissh only opens frames
 * out of it. Reading continues until the kernel has nothing left, then the
 * replies the requests produced are written in the same pass, so an ordinary
 * request-response round trip costs one wake-up rather than two.
 *
 * @param cli Client whose descriptor is readable.
 */
void	client_readable(t_client *cli)
{
	if (cli == NULL || cli->dead || !cli->watched)
		return ;
	while (read_chunk(cli))
	{
		if (!drain_frames(cli))
			return ;
	}
	client_flush(cli);
}

/**
 * @brief Writes as much of this client's queued output as the socket takes.
 *
 * A non-blocking socket may take a frame in pieces, and the retry loop that
 * used to hide that is not available to a thread that also drives every other
 * connection - so the send buffer keeps a cursor, and only a short write or
 * EAGAIN asks for EPOLLOUT. Sending is the reactor's alone: two threads
 * sealing into one session would interleave its sequence numbers.
 *
 * @param cli Client to write to.
 */
void	client_flush(t_client *cli)
{
	ssize_t	n;

	if (cli == NULL || cli->dead || !cli->watched)
		return ;
	while (true)
	{
		if (cli->send.used == cli->send.len && !load_frame(cli))
			break ;
		n = send(cli->fd, cli->send.data + cli->send.used,
				cli->send.len - cli->send.used, MSG_NOSIGNAL);
		if (n > 0)
			cli->send.used += (size_t)n;
		else if (n < 0 && errno == EINTR)
			continue ;
		else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return (watch_writable(cli, true));
		else
			return (client_kill(cli));
	}
	if (!cli->dead)
		watch_writable(cli, false);
}

/**
 * @brief Serialises a message and queues it for this client.
 *
 * Handlers and the tick never touch the socket: they hand bytes to the outbox
 * and move on, which is what keeps one slow peer from holding up a room. The
 * reactor seals and writes them when it can.
 *
 * @param cli Client to send to.
 * @param msg Message to serialise; the caller still owns and frees it.
 * @param is_state true for a STATE push (latest-wins mailbox).
 */
void	client_send(t_client *cli, t_htttp_message *msg, bool is_state)
{
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	if (cli == NULL || msg == NULL)
		return ;
	if (htttp_serialize(msg, &bytes, &len) != HTTTP_OK)
		return ;
	if (is_state)
		rc = outbox_push_state(&cli->outbox, bytes, len);
	else
		rc = outbox_push(&cli->outbox, bytes, len);
	if (rc != 0)
		free(bytes);
}

/**
 * @brief Reads one chunk into the receive buffer.
 *
 * @param cli Client to read from; ended here on error or a closed peer.
 * @return true when bytes arrived and reading may continue, false when the
 *         socket is drained, the peer is gone, or the client was ended.
 */
static bool	read_chunk(t_client *cli)
{
	size_t	want;
	ssize_t	n;

	buffer_compact(&cli->recv);
	want = cli->recv.len + TETRISD_READ_CHUNK_BYTES;
	if (want > TETRISD_RECV_BUFFER_MAX)
		want = TETRISD_RECV_BUFFER_MAX;
	if (want <= cli->recv.len || buffer_reserve(&cli->recv, want) != 0)
		return (client_kill(cli), false);
	n = recv(cli->fd, cli->recv.data + cli->recv.len,
			cli->recv.cap - cli->recv.len, 0);
	if (n > 0)
	{
		cli->recv.len += (size_t)n;
		return (true);
	}
	if (n < 0 && errno == EINTR)
		return (true);
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return (false);
	return (client_kill(cli), false);
}

/**
 * @brief Opens and dispatches every complete frame sitting in the buffer.
 *
 * A length that no frame could carry ends the connection instead of being
 * skipped: the stream is encrypted, so a prefix that does not make sense means
 * the peer is not speaking the protocol and nothing later will resynchronise.
 *
 * @param cli Client whose buffer is examined.
 * @return true when the client is still alive, false when it was ended.
 */
static bool	drain_frames(t_client *cli)
{
	uint32_t	frame_len;
	ssize_t		plain_len;

	while (cli->recv.len - cli->recv.used >= TETRISD_LENGTH_PREFIX_BYTES)
	{
		frame_len = buffer_get_u32(cli->recv.data + cli->recv.used);
		if (frame_len < TETRISSH_FRAME_OVERHEAD
			|| frame_len > TETRISSH_MAX_FRAME)
			return (client_kill(cli), false);
		if (cli->recv.len - cli->recv.used
			< TETRISD_LENGTH_PREFIX_BYTES + (size_t)frame_len)
			return (true);
		plain_len = session_frame_open(&cli->sess, cli->recv.data
				+ cli->recv.used + TETRISD_LENGTH_PREFIX_BYTES, frame_len,
				cli->srv->scratch, TETRISSH_MAX_PLAINTEXT);
		cli->recv.used += TETRISD_LENGTH_PREFIX_BYTES + (size_t)frame_len;
		if (plain_len < 0)
			return (client_kill(cli), false);
		client_handle_frame(cli, cli->srv->scratch, (size_t)plain_len);
		if (cli->dead || cli->outbox.overflowed)
			return (client_kill(cli), false);
	}
	return (true);
}

/**
 * @brief Seals the next queued message into the send buffer.
 *
 * @param cli Client whose outbox is drawn from; ended on a crypto failure.
 * @return true when a frame is ready to write, false when nothing was queued
 *         or the client was ended.
 */
static bool	load_frame(t_client *cli)
{
	t_outbound_message	msg;
	ssize_t				frame_len;

	if (outbox_pop(&cli->outbox, &msg) != 0)
		return (false);
	if (buffer_reserve(&cli->send, TETRISD_LENGTH_PREFIX_BYTES
			+ TETRISSH_FRAME_OVERHEAD + msg.len) != 0)
		return (free(msg.bytes), client_kill(cli), false);
	frame_len = session_frame_seal(&cli->sess, msg.bytes, msg.len,
			cli->send.data + TETRISD_LENGTH_PREFIX_BYTES,
			cli->send.cap - TETRISD_LENGTH_PREFIX_BYTES);
	free(msg.bytes);
	if (frame_len < 0)
		return (client_kill(cli), false);
	buffer_put_u32(cli->send.data, (uint32_t)frame_len);
	cli->send.len = TETRISD_LENGTH_PREFIX_BYTES + (size_t)frame_len;
	cli->send.used = 0;
	return (true);
}

/**
 * @brief Adds or drops EPOLLOUT for a client, only when it actually changes.
 *
 * @param cli Client whose interest set is adjusted.
 * @param wanted true to be told when the socket is writable again.
 */
static void	watch_writable(t_client *cli, bool wanted)
{
	struct epoll_event	ev;

	if (!cli->watched || cli->writable_armed == wanted)
		return ;
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	if (wanted)
		ev.events |= EPOLLOUT;
	ev.data.ptr = cli;
	if (epoll_ctl(cli->srv->epoll_fd, EPOLL_CTL_MOD, cli->fd, &ev) != 0)
	{
		client_kill(cli);
		return ;
	}
	cli->writable_armed = wanted;
}
