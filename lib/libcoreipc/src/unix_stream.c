#include "coreipc.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __linux__
# define STREAM_SEND_FLAGS MSG_NOSIGNAL
#else
# define STREAM_SEND_FLAGS 0
#endif

static int	stream_socket(void)
{
	int	fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
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

static int	stream_fill(const char *path, struct sockaddr_un *addr)
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

int	us_stream_listen(const char *path, int backlog, mode_t mode)
{
	struct sockaddr_un	addr;
	int					fd;

	fd = stream_socket();
	if (fd == -1)
		return (-1);
	if (unlink(path) == -1 && errno != ENOENT)
	{
		close(fd);
		return (-1);
	}
	if (stream_fill(path, &addr) == -1)
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
	if (listen(fd, backlog) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

int	us_stream_accept(int listen_fd)
{
	int	fd;

	do {
		fd = accept(listen_fd, NULL, NULL);
#ifndef __linux__
		if (fd != -1)
		{
			int	opt = 1;

			setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
		}
#endif
	} while (fd == -1 && errno == EINTR);
	return (fd);
}

int	us_stream_connect(const char *path)
{
	struct sockaddr_un	addr;
	int					fd;

	fd = stream_socket();
	if (fd == -1)
		return (-1);
	if (stream_fill(path, &addr) == -1)
	{
		close(fd);
		return (-1);
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

int	us_send_all(int fd, const void *buf, size_t len)
{
	const char	*p;
	size_t		remaining;
	ssize_t		n;

	p = buf;
	remaining = len;
	while (remaining > 0)
	{
		n = send(fd, p, remaining, STREAM_SEND_FLAGS);
		if (n == -1)
		{
			if (errno == EINTR)
				continue;
			return (-1);
		}
		p += n;
		remaining -= (size_t)n;
	}
	return (0);
}

int	us_recv_all(int fd, void *buf, size_t len)
{
	char	*p;
	size_t	remaining;
	ssize_t	n;

	p = buf;
	remaining = len;
	while (remaining > 0)
	{
		n = recv(fd, p, remaining, 0);
		if (n == -1)
		{
			if (errno == EINTR)
				continue;
			return (-1);
		}
		if (n == 0)
		{
			errno = EPIPE;
			return (-1);
		}
		p += n;
		remaining -= (size_t)n;
	}
	return (0);
}
