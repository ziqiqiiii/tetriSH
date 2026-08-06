#include "coreipc.h"

#include <fcntl.h>
#include <unistd.h>

/**
 * @brief Add O_NONBLOCK to an existing descriptor.
 *
 * @param fd The descriptor to modify.
 * @return 0 on success, -1 with errno set on failure.
 */
int	unixsock_set_nonblock(int fd)
{
	int	flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return (-1);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return (-1);
	return (0);
}

/**
 * @brief Close a bound socket and remove its filesystem entry.
 *
 * Binding leaves a socket file that outlives the process; shutdown paths use
 * this rather than relying on the unlink-stale fallback in unixsock_dgram_bind.
 *
 * @param fd The bound or listening fd to close.
 * @param path The path the fd was bound to.
 * @return 0 when both close and unlink succeeded, -1 with errno set otherwise.
 */
int	unixsock_close_unlink(int fd, const char *path)
{
	int	saved;

	saved = 0;
	if (close(fd) == -1)
		saved = errno;
	if (path && unlink(path) == -1 && errno != ENOENT && saved == 0)
		saved = errno;
	if (saved != 0)
	{
		errno = saved;
		return (-1);
	}
	return (0);
}

/**
 * @brief Create a non-blocking self-pipe for signal-to-event-loop wakeups.
 *
 * The loop polls fds[SELFPIPE_READ] alongside its sockets, so SIGTERM and SIGHUP
 * are handled in the loop's context rather than in handler context.
 *
 * @param fds Filled with the read end at SELFPIPE_READ and the write end at SELFPIPE_WRITE.
 * @return 0 on success, -1 with errno set on failure.
 */
int	selfpipe_open(int fds[2])
{
	if (!fds)
	{
		errno = EINVAL;
		return (-1);
	}
	if (pipe2(fds, O_NONBLOCK | O_CLOEXEC) == -1)
		return (-1);
	return (0);
}

/**
 * @brief Wake the event loop from a signal handler.
 *
 * The only async-signal-safe function in the library. A failed write means a
 * wakeup is already pending, which is exactly the outcome the caller wanted.
 *
 * @param write_fd The write end from selfpipe_open (fds[SELFPIPE_WRITE]).
 */
void	selfpipe_notify(int write_fd)
{
	int		saved;
	ssize_t	n;

	saved = errno;
	n = write(write_fd, "\1", 1);
	(void)n;
	errno = saved;
}

/**
 * @brief Drain every pending wakeup byte from the self-pipe.
 *
 * Coalescing is intended: N signals between two iterations are one wakeup.
 *
 * @param read_fd The read end from selfpipe_open (fds[SELFPIPE_READ]).
 * @return 0 once the pipe is empty, -1 with errno set on a real read error.
 */
int	selfpipe_drain(int read_fd)
{
	char	buf[64];
	ssize_t	n;

	while (1)
	{
		n = read(read_fd, buf, sizeof(buf));
		if (n > 0)
			continue ;
		if (n == 0)
			return (0);
		if (errno == EINTR)
			continue ;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return (0);
		return (-1);
	}
}
