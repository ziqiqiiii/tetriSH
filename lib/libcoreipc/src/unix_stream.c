#include "coreipc.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Static Functions
static int	fill_addr(struct sockaddr_un *addr, const char *path);
static int	fail(int fd, int err);

/**
 * @brief Create, bind, chmod, and listen on a stream socket at a path.
 *
 * `mode` is the control plane's entire authorisation model - 0600 on
 * ctl_socket is what restricts tetrisctl to the operator, since that channel
 * carries no Player-Id and no session auth.
 *
 * @param path Filesystem path to bind (from .tetrishrc, never defaulted).
 * @param backlog Depth of the pending-connection queue.
 * @param mode Permission bits applied to the bound socket file.
 * @return The listening fd on success, -1 with errno set on failure.
 */
int	us_stream_listen(const char *path, int backlog, mode_t mode)
{
	struct sockaddr_un	addr;
	int					fd;

	if (fill_addr(&addr, path) == -1)
		return (-1);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return (-1);
	if (unlink(path) == -1 && errno != ENOENT)
		return (fail(fd, errno));
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		return (fail(fd, errno));
	if (chmod(path, mode) == -1)
		return (fail(fd, errno));
	if (listen(fd, backlog) == -1)
		return (fail(fd, errno));
	return (fd);
}

/**
 * @brief Accept one connection from a listening socket.
 *
 * @param listen_fd A listening fd from us_stream_listen.
 * @return The accepted connection fd, or -1 with errno set on failure.
 */
int	us_stream_accept(int listen_fd)
{
	int	fd;

	fd = accept(listen_fd, NULL, NULL);
	while (fd == -1 && errno == EINTR)
		fd = accept(listen_fd, NULL, NULL);
	return (fd);
}

/**
 * @brief Connect to a stream socket at a filesystem path.
 *
 * @param path Filesystem path of the listening socket.
 * @return The connected fd on success, -1 with errno set on failure
 *         (ENOENT no socket file, ECONNREFUSED nothing listening).
 */
int	us_stream_connect(const char *path)
{
	struct sockaddr_un	addr;
	int					fd;

	if (fill_addr(&addr, path) == -1)
		return (-1);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return (-1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		return (fail(fd, errno));
	return (fd);
}

/**
 * @brief Send a whole buffer, looping over short writes.
 *
 * MSG_NOSIGNAL keeps a peer that hung up from killing the process; it
 * surfaces as EPIPE instead.
 *
 * @param fd A connected stream fd.
 * @param buf The bytes to send.
 * @param len Number of bytes to send.
 * @return 0 when all len bytes were sent, -1 with errno set otherwise.
 */
int	us_send_all(int fd, const void *buf, size_t len)
{
	const unsigned char	*p;
	size_t				sent;
	ssize_t				n;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	p = (const unsigned char *)buf;
	sent = 0;
	while (sent < len)
	{
		n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);
		if (n == -1 && errno == EINTR)
			continue ;
		if (n == -1)
			return (-1);
		sent += (size_t)n;
	}
	return (0);
}

/**
 * @brief Receive exactly len bytes, looping over short reads.
 *
 * A peer that closes early is a truncated message, not a short read.
 *
 * @param fd A connected stream fd.
 * @param buf Destination for exactly len bytes.
 * @param len Number of bytes required.
 * @return 0 when all len bytes were read, -1 with errno set otherwise.
 */
int	us_recv_all(int fd, void *buf, size_t len)
{
	unsigned char	*p;
	size_t			got;
	ssize_t			n;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	p = (unsigned char *)buf;
	got = 0;
	while (got < len)
	{
		n = recv(fd, p + got, len - got, 0);
		if (n == -1 && errno == EINTR)
			continue ;
		if (n == -1)
			return (-1);
		if (n == 0)
		{
			errno = EPIPE;
			return (-1);
		}
		got += (size_t)n;
	}
	return (0);
}

/**
 * @brief Fill a sockaddr_un for an AF_UNIX path, rejecting over-long paths.
 *
 * @param addr The address to fill.
 * @param path The filesystem path to copy in.
 * @return 0 on success, -1 with errno set (EINVAL, ENAMETOOLONG) on failure.
 */
static int	fill_addr(struct sockaddr_un *addr, const char *path)
{
	if (!path || *path == '\0')
	{
		errno = EINVAL;
		return (-1);
	}
	if (strlen(path) >= sizeof(addr->sun_path))
	{
		errno = ENAMETOOLONG;
		return (-1);
	}
	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
	return (0);
}

/**
 * @brief Close a half-built socket and report the failure that caused it.
 *
 * @param fd The descriptor to close.
 * @param err The errno value to surface to the caller.
 * @return Always -1, with errno set to err.
 */
static int	fail(int fd, int err)
{
	close(fd);
	errno = err;
	return (-1);
}
