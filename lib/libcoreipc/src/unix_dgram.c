#include "coreipc.h"

// Static Functions
static int	fill_addr(struct sockaddr_un *addr, const char *path);
static int	fail(int fd, int err);

/**
 * @brief Create, bind, and chmod a non-blocking datagram socket at a path.
 *
 * Unlinks any stale socket file first, so a crashed daemon does not block its
 * own restart.
 *
 * @param path Filesystem path to bind (from .tetrishrc, never defaulted).
 * @param mode Permission bits applied to the bound socket file.
 * @return The bound fd on success, -1 with errno set on failure.
 */
int	unixsock_dgram_bind(const char *path, mode_t mode)
{
	struct sockaddr_un	addr;
	int					fd;

	if (fill_addr(&addr, path) == -1)
		return (-1);
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return (-1);
	if (unlink(path) == -1 && errno != ENOENT)
		return (fail(fd, errno));
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		return (fail(fd, errno));
	if (chmod(path, mode) == -1)
		return (fail(fd, errno));
	if (unixsock_set_nonblock(fd) == -1)
		return (fail(fd, errno));
	return (fd);
}

/**
 * @brief Open a connected datagram sender aimed at a bound path.
 *
 * Neither blocks nor requires the receiver to exist yet - a missing receiver
 * surfaces later as ECONNREFUSED from unixsock_dgram_send_nonblock.
 *
 * @param path Filesystem path of the peer's bound socket.
 * @return The connected fd on success, -1 with errno set on failure.
 */
int	unixsock_dgram_open(const char *path)
{
	struct sockaddr_un	addr;
	int					fd;

	if (fill_addr(&addr, path) == -1)
		return (-1);
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return (-1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		return (fail(fd, errno));
	if (unixsock_set_nonblock(fd) == -1)
		return (fail(fd, errno));
	return (fd);
}

/**
 * @brief Send one datagram without ever blocking.
 *
 * EAGAIN (receiver's queue full) and ECONNREFUSED (no receiver bound) both
 * mean dropped; the caller counts it.
 *
 * @param fd A sender fd from unixsock_dgram_open.
 * @param buf The record to send.
 * @param len Length of buf in bytes; must fit one datagram.
 * @return 0 when the datagram was queued, -1 with errno set otherwise.
 */
int	unixsock_dgram_send_nonblock(int fd, const void *buf, size_t len)
{
	ssize_t	n;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	n = send(fd, buf, len, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (n == -1)
		return (-1);
	if ((size_t)n != len)
	{
		errno = EMSGSIZE;
		return (-1);
	}
	return (0);
}

/**
 * @brief Receive one datagram from a bound socket.
 *
 * A datagram longer than buflen is truncated, so size buf to the largest
 * record published.
 *
 * @param fd A bound fd from unixsock_dgram_bind.
 * @param buf Destination for the datagram.
 * @param buflen Capacity of buf in bytes.
 * @return Bytes received, or -1 with errno set (EAGAIN when nothing queued).
 */
ssize_t	unixsock_dgram_recv(int fd, void *buf, size_t buflen)
{
	ssize_t	n;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	n = recv(fd, buf, buflen, 0);
	while (n == -1 && errno == EINTR)
		n = recv(fd, buf, buflen, 0);
	return (n);
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
