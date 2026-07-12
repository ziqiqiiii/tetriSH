#include "internal.h"
#include <errno.h>
#include <sys/socket.h>

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

int	tsh_write_exact(int fd, const void *buf, size_t len)
{
	const unsigned char	*in;
	size_t				done;
	ssize_t				n;

	in = buf;
	done = 0;
	while (done < len)
	{
		/* AI-assisted: convert closed-peer SIGPIPE into EPIPE so one broken
		 * connection cannot terminate its daemon process. */
		n = send(fd, in + done, len - done, MSG_NOSIGNAL);
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

int	tsh_write_u32(int fd, uint32_t value)
{
	unsigned char	buf[4];

	buf[0] = (unsigned char)((value >> 24) & 0xffu);
	buf[1] = (unsigned char)((value >> 16) & 0xffu);
	buf[2] = (unsigned char)((value >> 8) & 0xffu);
	buf[3] = (unsigned char)(value & 0xffu);
	return (tsh_write_exact(fd, buf, sizeof(buf)));
}

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

unsigned char	tsh_send_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_CLIENT_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_SERVER_MARKER);
	return (0u);
}

unsigned char	tsh_recv_marker(t_tetrissh_role role)
{
	if (role == TETRISSH_ROLE_CLIENT)
		return (TSH_SERVER_MARKER);
	if (role == TETRISSH_ROLE_SERVER)
		return (TSH_CLIENT_MARKER);
	return (0u);
}
