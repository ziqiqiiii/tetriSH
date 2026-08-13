#include "tetrisd.h"

// Static Functions
static void	watch_writable(t_control_connection *conn, bool wanted);

/**
 * @brief Writes as much of this administrator's queued reply as the socket
 *        takes.
 *
 * The reply is already whole in the send buffer before this runs, so a short
 * write only advances a cursor - there is no reply that is half computed. That
 * is what lets one wedged Administrator cost nothing but its own slot: the
 * reactor never waits on it, and the other three connections are unaffected.
 *
 * @param conn Connection to write to.
 */
void	control_flush(t_control_connection *conn)
{
	ssize_t	n;

	if (conn == NULL || !conn->open)
		return ;
	while (conn->send.used < conn->send.len)
	{
		n = send(conn->fd, conn->send.data + conn->send.used,
				conn->send.len - conn->send.used, MSG_NOSIGNAL);
		if (n > 0)
			conn->send.used += (size_t)n;
		else if (n < 0 && errno == EINTR)
			continue ;
		else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return (watch_writable(conn, true));
		else
			return (control_hangup(conn));
	}
	watch_writable(conn, false);
}

/**
 * @brief Ends one admin connection and frees its slot.
 *
 * Safe to call twice, because both the read path and the write path can
 * discover the same closed peer within one batch.
 *
 * @param conn Connection to end.
 */
void	control_hangup(t_control_connection *conn)
{
	if (conn == NULL || !conn->open)
		return ;
	if (conn->fd >= 0)
	{
		epoll_ctl(conn->srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
		close(conn->fd);
	}
	conn->fd = -1;
	conn->open = false;
	conn->writable_armed = false;
	conn->recv.len = 0;
	conn->recv.used = 0;
	conn->send.len = 0;
	conn->send.used = 0;
}

/**
 * @brief Serialises one response and queues it behind the length prefix.
 *
 * `omitted` is how a capped listing reports what it left out. It rides as a
 * header because the listing bodies are a bare sequence of rows with nowhere
 * to put it, and the status stays 200 either way - a capped listing is a
 * complete answer to what was asked, not a partial failure.
 *
 * @param conn Connection to answer.
 * @param status Status code to send.
 * @param body Body text, or NULL for an empty response.
 * @param body_len Length of body.
 * @param omitted Rows the cap left out; no header is sent when it is 0.
 */
void	control_reply(t_control_connection *conn, unsigned int status, const char *body, size_t body_len, size_t omitted)
{
	t_htttp_message	resp;
	unsigned char	*bytes;
	char			date[HTTTP_DATE_BUFSIZE];
	char			count[32];
	size_t			len;

	if (conn == NULL || !conn->open)
		return ;
	htttp_message_init(&resp);
	if (htttp_message_make_response(&resp, status,
			htttp_reason_phrase(status)) != HTTTP_OK)
		return (htttp_message_free(&resp));
	if (htttp_format_date(time(NULL), date) == HTTTP_OK)
		htttp_message_set_header(&resp, "Date", date);
	if (omitted > 0)
	{
		snprintf(count, sizeof(count), "%zu", omitted);
		htttp_message_set_header(&resp, "Omitted", count);
	}
	if (body != NULL && body_len > 0)
	{
		htttp_message_set_header(&resp, "Content-Type",
			HTTTP_CONTENT_TYPE_STATUS);
		htttp_message_set_body(&resp, body, body_len);
	}
	if (htttp_serialize(&resp, &bytes, &len) == HTTTP_OK)
		control_queue(conn, bytes, len);
	htttp_message_free(&resp);
}

/**
 * @brief Puts serialised response bytes into the send buffer, length-prefixed.
 *
 * Takes ownership of `bytes` and frees them, since every caller has just
 * produced them and has nothing further to do with them.
 *
 * @param conn Connection to queue for; ended when the reply cannot be held.
 * @param bytes Serialised response, freed here.
 * @param len Length of bytes.
 */
void	control_queue(t_control_connection *conn, unsigned char *bytes, size_t len)
{
	if (conn == NULL || bytes == NULL)
		return (free(bytes));
	if (!conn->open || len > TETRISD_CONTROL_FRAME_MAX
		|| buffer_reserve(&conn->send,
			TETRISD_LENGTH_PREFIX_BYTES + len) != 0)
	{
		free(bytes);
		return (control_hangup(conn));
	}
	buffer_put_u32(conn->send.data, (uint32_t)len);
	memcpy(conn->send.data + TETRISD_LENGTH_PREFIX_BYTES, bytes, len);
	conn->send.len = TETRISD_LENGTH_PREFIX_BYTES + len;
	conn->send.used = 0;
	free(bytes);
}

/**
 * @brief Adds or drops EPOLLOUT for a connection, only when it changes.
 *
 * @param conn Connection whose interest set is adjusted.
 * @param wanted true to be told when the socket is writable again.
 */
static void	watch_writable(t_control_connection *conn, bool wanted)
{
	struct epoll_event	ev;

	if (!conn->open || conn->writable_armed == wanted)
		return ;
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	if (wanted)
		ev.events |= EPOLLOUT;
	ev.data.ptr = conn;
	if (epoll_ctl(conn->srv->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) != 0)
		return (control_hangup(conn));
	conn->writable_armed = wanted;
}
