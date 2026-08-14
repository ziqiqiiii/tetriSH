#include "tetrisctl.h"

// Static Functions
static int	ctl_connect(const t_managed *d);
static int	read_response(int fd, char *body, size_t cap, size_t *body_len);
static int	take_status(t_htttp_message *resp, char *body, size_t cap, size_t *body_len);
static int	deadline_socket(int fd);
static int	send_request(int fd, const char *method, const char *path);

/*
** The asking end of the Control channel.
**
** One connection, one request, one reply, then close - which is why tetrisd
** can cap the channel at four connections and call that a ceiling rather than
** a budget. Nothing here is retried: a local socket either answers or the
** daemon is not there, and a retry would only turn a clear answer into a
** delayed one.
**
** No session, no handshake, no OpenSSL. Authorisation is the operator's
** ability to open a 0600 socket, so there is nothing to present and nothing to
** negotiate - the frame is plaintext HTTTP behind a 4-byte length prefix.
*/

/**
 * @brief Asks one daemon one admin question and returns its body.
 *
 * @param d The daemon to ask; its control_path must be set.
 * @param method The HTTTP method - STATUS, ROOMS, PLAYERS or DROPPED.
 * @param body Buffer receiving the response body.
 * @param cap Size of body.
 * @param body_len Receives the body length.
 * @return The HTTTP status code on an answered request, or -1 when the
 *         channel could not be reached or the reply made no sense.
 */
int	control_ask(const t_managed *d, const char *method, const char *path, char *body, size_t cap, size_t *body_len)
{
	int	status;
	int	fd;

	if (d == NULL || method == NULL || path == NULL || body == NULL
		|| body_len == NULL)
		return (-1);
	*body_len = 0;
	if (d->control_path[0] == '\0')
		return (-1);
	fd = ctl_connect(d);
	if (fd < 0)
		return (-1);
	status = -1;
	if (send_request(fd, method, path) == 0)
		status = read_response(fd, body, cap, body_len);
	close(fd);
	return (status);
}

/**
 * @brief Opens the daemon's control socket under a bounded deadline.
 *
 * @param d The daemon whose channel is dialled.
 * @return A connected descriptor, or -1.
 */
static int	ctl_connect(const t_managed *d)
{
	int	fd;

	fd = unixsock_stream_connect(d->control_path);
	if (fd < 0)
		return (-1);
	if (deadline_socket(fd) != 0)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

/**
 * @brief Serialises one request and writes it behind its length prefix.
 *
 * The request carries no body and names no Player. There is nothing to
 * authenticate with on this channel, so a Player-Id header would be a field
 * the server is required to ignore.
 *
 * @param fd Connected control descriptor.
 * @param method Method to send.
 * @return 0 on success, -1 on failure.
 */
static int	send_request(int fd, const char *method, const char *path)
{
	t_htttp_message	req;
	unsigned char	prefix[TETRISCTL_LENGTH_PREFIX_BYTES];
	unsigned char	*bytes;
	size_t			len;
	int				rc;

	htttp_message_init(&req);
	if (htttp_message_make_request(&req, method, path) != HTTTP_OK
		|| htttp_message_set_header(&req, "Host", "tetrish.local") != HTTTP_OK
		|| htttp_message_set_header(&req, "Client",
			TETRISCTL_COMPONENT_NAME) != HTTTP_OK
		|| htttp_serialize(&req, &bytes, &len) != HTTTP_OK)
	{
		htttp_message_free(&req);
		return (-1);
	}
	htttp_message_free(&req);
	prefix[0] = (unsigned char)(len >> 24);
	prefix[1] = (unsigned char)(len >> 16);
	prefix[2] = (unsigned char)(len >> 8);
	prefix[3] = (unsigned char)len;
	rc = 0;
	if (unixsock_send_all(fd, prefix, sizeof(prefix)) != 0
		|| unixsock_send_all(fd, bytes, len) != 0)
		rc = -1;
	free(bytes);
	return (rc);
}

/**
 * @brief Reads one length-prefixed response and hands back its body.
 *
 * @param fd Connected control descriptor.
 * @param body Buffer receiving the response body.
 * @param cap Size of body.
 * @param body_len Receives the body length.
 * @return The HTTTP status code, or -1 on a malformed or oversized reply.
 */
static int	read_response(int fd, char *body, size_t cap, size_t *body_len)
{
	t_htttp_message	resp;
	unsigned char	prefix[TETRISCTL_LENGTH_PREFIX_BYTES];
	unsigned char	*bytes;
	uint32_t		len;
	int				status;

	if (unixsock_recv_all(fd, prefix, sizeof(prefix)) != 0)
		return (-1);
	len = ((uint32_t)prefix[0] << 24) | ((uint32_t)prefix[1] << 16)
		| ((uint32_t)prefix[2] << 8) | (uint32_t)prefix[3];
	if (len == 0 || len > HTTTP_MAX_MESSAGE_SIZE)
		return (-1);
	bytes = malloc(len);
	if (bytes == NULL)
		return (-1);
	htttp_message_init(&resp);
	status = -1;
	if (unixsock_recv_all(fd, bytes, len) == 0
		&& htttp_parse(bytes, len, &resp) == HTTTP_OK)
		status = take_status(&resp, body, cap, body_len);
	htttp_message_free(&resp);
	free(bytes);
	return (status);
}

/**
 * @brief Copies a parsed response's body out and reports its status.
 *
 * A body that does not fit is a failure rather than a truncation: every caller
 * hands the bytes straight to a libstatusbody decoder, and half a body decodes
 * as malformed rather than as less.
 *
 * @param resp The parsed response.
 * @param body Buffer receiving the body.
 * @param cap Size of body.
 * @param body_len Receives the body length.
 * @return The status code, or -1 when the message is not a response or the
 *         body does not fit.
 */
static int	take_status(t_htttp_message *resp, char *body, size_t cap, size_t *body_len)
{
	if (resp->type != HTTTP_MESSAGE_RESPONSE)
		return (-1);
	if (resp->body_len >= cap)
		return (-1);
	if (resp->body != NULL && resp->body_len > 0)
		memcpy(body, resp->body, resp->body_len);
	body[resp->body_len] = '\0';
	*body_len = resp->body_len;
	return ((int)resp->status_code);
}

/**
 * @brief Bounds every read and write on the channel by one deadline.
 *
 * A wedged daemon is the case this exists for. Without it an administrator
 * asking a question would wait for as long as the server felt like taking,
 * which is the one outcome a status command must not have.
 *
 * @param fd Descriptor to bound.
 * @return 0 on success, -1 when the timeouts could not be set.
 */
static int	deadline_socket(int fd)
{
	struct timeval	tv;

	tv.tv_sec = TETRISCTL_CONTROL_MS / 1000;
	tv.tv_usec = (TETRISCTL_CONTROL_MS % 1000) * 1000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0
		|| setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
		return (-1);
	return (0);
}