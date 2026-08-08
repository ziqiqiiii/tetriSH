#include "internal.h"

// Static Functions
static int	valid_session(const t_session *sess);

/**
 * @brief Encrypts one plaintext message and writes it as a single frame.
 *
 * A blocking wrapper over session_frame_seal: it owns the buffer and the two
 * socket writes, and the codec owns the crypto. Both writes are all-or-nothing
 * for the caller, since a short write on a stream socket leaves the peer
 * holding half a frame and the connection is finished either way.
 *
 * @param sess The established session; must be valid and established.
 * @param buf Plaintext to send; may be null only when len is 0.
 * @param len Plaintext length; must not exceed TETRISSH_MAX_PLAINTEXT.
 * @return The plaintext byte count on success, -1 on invalid state, oversized
 *         input, allocation, crypto failure, or socket failure.
 */
ssize_t	session_send(t_session *sess, const void *buf, size_t len)
{
	unsigned char	*frame;
	ssize_t			frame_len;
	int				write_status;

	if (!valid_session(sess) || (buf == NULL && len != 0)
		|| len > TETRISSH_MAX_PLAINTEXT)
		return (-1);
	frame = malloc(TETRISSH_FRAME_OVERHEAD + len);
	if (frame == NULL)
		return (-1);
	frame_len = session_frame_seal(sess, buf, len, frame, TETRISSH_FRAME_OVERHEAD + len);
	if (frame_len < 0)
		return (free(frame), -1);
	write_status = sessionio_write_u32(sess->fd, (uint32_t)frame_len);
	if (write_status == 0)
		write_status = sessionio_write_exact(sess->fd, frame,
				(size_t)frame_len);
	OPENSSL_cleanse(frame, (size_t)frame_len);
	free(frame);
	if (write_status != 0)
		return (-1);
	return ((ssize_t)len);
}

/**
 * @brief Reads and authenticates one frame, returning its plaintext.
 *
 * A blocking wrapper over session_frame_open: it reads the length prefix,
 * bounds it, reads that many bytes, and hands the frame to the codec. The
 * receive sequence number is incremented only on a fully accepted frame.
 *
 * @param sess The established session; must be valid and established.
 * @param buf Destination for the decrypted plaintext.
 * @param max_len Capacity of buf in bytes.
 * @return The plaintext byte count on success, 0 on clean EOF before the next
 *         frame prefix, -1 on invalid state, malformed frame, authentication
 *         failure, insufficient capacity, or socket failure.
 */
ssize_t	session_recv(t_session *sess, void *buf, size_t max_len)
{
	uint32_t			frame_len;
	unsigned char		*frame;
	ssize_t				plain_len;
	t_sessionio_result	res;

	if (!valid_session(sess) || buf == NULL)
		return (-1);
	res = sessionio_read_u32(sess->fd, &frame_len);
	if (res == SESSIONIO_EOF)
		return (0);
	if (res != SESSIONIO_OK || frame_len < TETRISSH_FRAME_OVERHEAD || frame_len > TETRISSH_MAX_FRAME)
		return (-1);
	frame = malloc(frame_len);
	if (frame == NULL)
		return (-1);
	if (sessionio_read_exact(sess->fd, frame, frame_len) != SESSIONIO_OK)
	{
		OPENSSL_cleanse(frame, frame_len);
		free(frame);
		return (-1);
	}
	plain_len = session_frame_open(sess, frame, frame_len, buf, max_len);
	OPENSSL_cleanse(frame, frame_len);
	free(frame);
	return (plain_len);
}

/**
 * @brief Wipes cryptographic state and marks the session unusable.
 *
 * Cleanses the AES key, clears the sequence counters and establishment flag,
 * and sets fd to -1. It does not call close(2); the caller owns the descriptor.
 *
 * @param sess The session to reset; a null pointer is ignored.
 */
void	session_close(t_session *sess)
{
	if (sess == NULL)
		return ;
	OPENSSL_cleanse(sess->aes_key, sizeof(sess->aes_key));
	sess->fd = -1;
	sess->role = TETRISSH_ROLE_NONE;
	sess->send_seq = 0;
	sess->recv_seq = 0;
	sess->established = 0;
}

/**
 * @brief Reports whether a session is ready for frame I/O.
 *
 * @param sess The session to check; may be null.
 * @return Non-zero if sess can seal and open frames and also holds a valid
 *         descriptor to move them over; 0 otherwise.
 */
static int	valid_session(const t_session *sess)
{
	return (session_frame_ready(sess) && sess->fd >= 0);
}
