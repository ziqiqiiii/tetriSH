#include "internal.h"

t_tsh_io_result	tsh_read_exact(int fd, void *buf, size_t len)
{
	(void)fd;
	(void)buf;
	(void)len;
	return (TSH_IO_ERR);
}

int	tsh_write_exact(int fd, const void *buf, size_t len)
{
	(void)fd;
	(void)buf;
	(void)len;
	return (-1);
}

t_tsh_io_result	tsh_read_u32(int fd, uint32_t *value)
{
	(void)fd;
	(void)value;
	return (TSH_IO_ERR);
}

int	tsh_write_u32(int fd, uint32_t value)
{
	(void)fd;
	(void)value;
	return (-1);
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
