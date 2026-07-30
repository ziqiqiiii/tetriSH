#include "internal.h"

/**
 * @brief Reads exactly len bytes from a socket, retrying short reads.
 *
 * Loops over recv(2) until the full length is read, restarting on EINTR.
 * A clean peer close with no bytes yet read is reported as EOF; a close
 * mid-buffer is an error, since the caller expected a complete field.
 *
 * @param fd The connected socket descriptor.
 * @param buf Destination buffer of at least len bytes.
 * @param len Number of bytes to read.
 * @return TSH_IO_OK on success, TSH_IO_EOF on clean EOF before any byte,
 *         TSH_IO_ERR on error or partial EOF.
 */
t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len)
{
	unsigned char	*out;
	size_t			done;
	ssize_t			n;

	out = buf;
	done = 0;
	while (done < len)
	{
		n = recv(fd, out + done, len - done, 0);
		if (n == 0)
			return (done == 0 ? TSH_IO_EOF : TSH_IO_ERR);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return (TSH_IO_ERR);
		}
		done += (size_t)n;
	}
	return (TSH_IO_OK);
}

/**
 * @brief Writes exactly len bytes to a socket, retrying short writes.
 *
 * Loops over send(2) until the full length is written, restarting on EINTR.
 * Uses MSG_NOSIGNAL per send when available, or enables SO_NOSIGPIPE on the
 * socket when supported, so a closed peer cannot raise SIGPIPE.
 *
 * @param fd The connected socket descriptor.
 * @param buf Source buffer of at least len bytes.
 * @param len Number of bytes to write.
 * @return 0 on success, -1 on socket failure.
 */
int	tsh_write_exact(int fd, const void *buf, size_t len)
{
	const unsigned char	*in;
	size_t				done;
	ssize_t				n;
	int					send_flags;
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
	int					one;
#endif

	in = buf;
	done = 0;
	if (len == 0)
		return (0);
#ifdef MSG_NOSIGNAL
	send_flags = MSG_NOSIGNAL;
#else
	send_flags = 0;
# ifdef SO_NOSIGPIPE
	one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) < 0)
		return (-1);
# endif
#endif
	while (done < len)
	{
		n = send(fd, in + done, len - done, send_flags);
		if (n <= 0)
		{
			if (n < 0 && errno == EINTR)
				continue;
			return (-1);
		}
		done += (size_t)n;
	}
	return (0);
}

/**
 * @brief Reads a big-endian unsigned 32-bit integer from a socket.
 *
 * @param fd The connected socket descriptor.
 * @param value Destination for the decoded value.
 * @return TSH_IO_OK on success, TSH_IO_EOF on clean EOF, TSH_IO_ERR on
 *         error or a null value pointer.
 */
t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value)
{
	unsigned char	buf[4];
	t_tsh_io_result	res;

	if (value == NULL)
		return (TSH_IO_ERR);
	res = tsh_read_exact(fd, buf, sizeof(buf));
	if (res != TSH_IO_OK)
		return (res);
	*value = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
		| ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	return (TSH_IO_OK);
}

/**
 * @brief Writes an unsigned 32-bit integer to a socket in big-endian order.
 *
 * @param fd The connected socket descriptor.
 * @param value The value to encode and send.
 * @return 0 on success, -1 on socket failure.
 */
int	tsh_write_u32(int fd, uint32_t value)
{
	unsigned char	buf[4];

	buf[0] = (unsigned char)((value >> 24) & 0xffu);
	buf[1] = (unsigned char)((value >> 16) & 0xffu);
	buf[2] = (unsigned char)((value >> 8) & 0xffu);
	buf[3] = (unsigned char)(value & 0xffu);
	return (tsh_write_exact(fd, buf, sizeof(buf)));
}

/**
 * @brief Encodes an unsigned 64-bit integer into 8 big-endian bytes.
 *
 * @param value The value to encode.
 * @param out Destination buffer of exactly 8 bytes.
 */
void	tsh_u64_be(uint64_t value, unsigned char out[8])
{
	int	i;

	i = 7;
	while (i >= 0)
	{
		out[i] = (unsigned char)(value & 0xffu);
		value >>= 8;
		i--;
	}
}

/**
 * @brief Returns the GCM direction marker this role stamps on frames it sends.
 *
 * @param role The local endpoint role.
 * @return The client or server marker byte, or 0 for an invalid role.
 */
unsigned char	tsh_send_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_CLIENT_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_SERVER_MARKER);
	return (0u);
}

/**
 * @brief Returns the GCM direction marker this role expects on frames it receives.
 *
 * @param role The local endpoint role.
 * @return The opposite endpoint's marker byte, or 0 for an invalid role.
 */
unsigned char	tsh_recv_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_SERVER_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_CLIENT_MARKER);
	return (0u);
}
