#include "coreipc.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __linux__
# define DG_SEND_FLAGS (MSG_DONTWAIT | MSG_NOSIGNAL)
#else
# define DG_SEND_FLAGS MSG_DONTWAIT
#endif

static int	dgram_socket(void)
{
	int	fd;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return (-1);
#ifndef __linux__
	{
		int	opt = 1;

		if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) == -1)
		{
			close(fd);
			return (-1);
		}
	}
#endif
	return (fd);
}

static int	dgram_fill(const char *path, struct sockaddr_un *addr)
{
	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(addr->sun_path))
	{
		errno = ENAMETOOLONG;
		return (-1);
	}
	strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
	addr->sun_path[sizeof(addr->sun_path) - 1] = '\0';
	return (0);
}

int	us_dgram_bind(const char *path, mode_t mode)
{
	struct sockaddr_un	addr;
	int					fd;

	fd = dgram_socket();
	if (fd == -1)
		return (-1);
	if (unlink(path) == -1 && errno != ENOENT)
	{
		close(fd);
		return (-1);
	}
	if (dgram_fill(path, &addr) == -1)
	{
		close(fd);
		return (-1);
	}
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return (-1);
	}
	if (chmod(path, mode) == -1)
	{
		close(fd);
		return (-1);
	}
	if (us_set_nonblock(fd) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

int	us_dgram_open(const char *path)
{
	struct sockaddr_un	addr;
	int					fd;

	fd = dgram_socket();
	if (fd == -1)
		return (-1);
	if (dgram_fill(path, &addr) == -1)
	{
		close(fd);
		return (-1);
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return (-1);
	}
	if (us_set_nonblock(fd) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

int	us_dgram_send_nb(int fd, const void *buf, size_t len)
{
	ssize_t	n;

	n = send(fd, buf, len, DG_SEND_FLAGS);
	if (n == -1)
	{
#ifndef __linux__
		/* macOS reports an unbound peer as ENOENT rather than ECONNREFUSED. */
		if (errno == ENOENT)
			errno = ECONNREFUSED;
#endif
		return (-1);
	}
	if ((size_t)n != len)
	{
		errno = EMSGSIZE;
		return (-1);
	}
	return (0);
}

ssize_t	us_dgram_recv(int fd, void *buf, size_t buflen)
{
	ssize_t	n;

	do {
		n = recv(fd, buf, buflen, 0);
	} while (n == -1 && errno == EINTR);
	return (n);
}
