#include "coreipc.h"

/**
 * @brief Create, bind, chmod, and listen on a stream socket at a path.
 *
 * `mode` is the control plane's entire authorisation model — 0600 on
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
	/* TODO: socket(AF_UNIX, SOCK_STREAM, 0); unlink stale; bind; chmod;
	   listen(backlog). */
	(void)path;
	(void)backlog;
	(void)mode;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Accept one connection from a listening socket.
 *
 * @param listen_fd A listening fd from us_stream_listen.
 * @return The accepted connection fd, or -1 with errno set on failure.
 */
int	us_stream_accept(int listen_fd)
{
	/* TODO: loop accept(listen_fd, NULL, NULL) while errno == EINTR; the
	   peer address is unnamed, so it is discarded. */
	(void)listen_fd;
	errno = ENOSYS;
	return (-1);
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
	/* TODO: socket(AF_UNIX, SOCK_STREAM, 0); fill sockaddr_un; connect. */
	(void)path;
	errno = ENOSYS;
	return (-1);
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
	/* TODO: loop send(fd, p, remaining, MSG_NOSIGNAL); retry on EINTR;
	   advance p by the returned count. */
	(void)fd;
	(void)buf;
	(void)len;
	errno = ENOSYS;
	return (-1);
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
	/* TODO: loop recv(fd, p, remaining, 0); retry on EINTR; a 0 return means
	   the peer closed early -> errno = EPIPE, return (-1). */
	(void)fd;
	(void)buf;
	(void)len;
	errno = ENOSYS;
	return (-1);
}
