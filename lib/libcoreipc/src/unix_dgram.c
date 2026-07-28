#include "coreipc.h"

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
int	us_dgram_bind(const char *path, mode_t mode)
{
	/* TODO: socket(AF_UNIX, SOCK_DGRAM, 0); unlink ignoring ENOENT; fill
	   sockaddr_un (length -> ENAMETOOLONG); bind; chmod; us_set_nonblock. */
	(void)path;
	(void)mode;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Open a connected datagram sender aimed at a bound path.
 *
 * Neither blocks nor requires the receiver to exist yet — a missing receiver
 * surfaces later as ECONNREFUSED from us_dgram_send_nb.
 *
 * @param path Filesystem path of the peer's bound socket.
 * @return The connected fd on success, -1 with errno set on failure.
 */
int	us_dgram_open(const char *path)
{
	/* TODO: socket(AF_UNIX, SOCK_DGRAM, 0); fill sockaddr_un; connect;
	   us_set_nonblock. */
	(void)path;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Send one datagram without ever blocking.
 *
 * EAGAIN (receiver's queue full) and ECONNREFUSED (no receiver bound) both
 * mean dropped; the caller counts it.
 *
 * @param fd A sender fd from us_dgram_open.
 * @param buf The record to send.
 * @param len Length of buf in bytes; must fit one datagram.
 * @return 0 when the datagram was queued, -1 with errno set otherwise.
 */
int	us_dgram_send_nb(int fd, const void *buf, size_t len)
{
	/* TODO: send(fd, buf, len, MSG_DONTWAIT | MSG_NOSIGNAL); short sends are
	   impossible on SOCK_DGRAM, so treat != len as -1/EMSGSIZE. */
	(void)fd;
	(void)buf;
	(void)len;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Receive one datagram from a bound socket.
 *
 * A datagram longer than buflen is truncated, so size buf to the largest
 * record published.
 *
 * @param fd A bound fd from us_dgram_bind.
 * @param buf Destination for the datagram.
 * @param buflen Capacity of buf in bytes.
 * @return Bytes received, or -1 with errno set (EAGAIN when nothing queued).
 */
ssize_t	us_dgram_recv(int fd, void *buf, size_t buflen)
{
	/* TODO: recv(fd, buf, buflen, 0); retry on EINTR; let EAGAIN through. */
	(void)fd;
	(void)buf;
	(void)buflen;
	errno = ENOSYS;
	return (-1);
}
