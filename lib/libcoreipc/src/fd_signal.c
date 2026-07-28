#include "coreipc.h"

#include <fcntl.h>
#include <unistd.h>

/**
 * @brief Add O_NONBLOCK to an existing descriptor.
 *
 * Retrieves the current file-status flags via fcntl(F_GETFL), ORs in
 * O_NONBLOCK, and writes them back with fcntl(F_SETFL).  fcntl is used
 * instead of ioctl so the flag change is atomic and the descriptor type
 * is irrelevant.
 *
 * @param fd Open file descriptor.
 * @return 0 on success, -1 on error (errno is set by fcntl).
 */
int	us_set_nonblock(int fd)
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
 * Calls close(2) on the descriptor, then unlink(2) on the path.
 * If the socket has already been unlinked (ENOENT) the error is
 * silently ignored so that double-cleanup is safe.
 *
 * @param fd   Open file descriptor (socket or FIFO).
 * @param path Filesystem path to remove.
 * @return 0 on success, -1 on error (errno is set by the failing call).
 */
int	us_close_unlink(int fd, const char *path)
{
	if (close(fd) == -1)
		return (-1);
	if (unlink(path) == -1 && errno != ENOENT)
		return (-1);
	return (0);
}

/**
 * @brief Create a non-blocking self-pipe for signal-to-event-loop wakeups.
 *
 * Wraps pipe(2) with O_NONBLOCK and FD_CLOEXEC on both ends so that
 * the pipe is safe to use inside an event loop (reads never block, fds
 * are not leaked into child processes).  On any failure after the pipe
 * is created the two fds are closed and errno is preserved from the
 * failing call.
 *
 * @param fds Array of two ints; on success fds[0] is the read end and
 *            fds[1] is the write end.
 * @return 0 on success, -1 on error (errno is set by pipe, fcntl, or
 *         us_set_nonblock).
 */
int	sp_pipe(int fds[2])
{
	int	saved;

	if (pipe(fds) == -1)
		return (-1);
	if (us_set_nonblock(fds[0]) == -1 || us_set_nonblock(fds[1]) == -1)
	{
		saved = errno;
		close(fds[0]);
		close(fds[1]);
		errno = saved;
		return (-1);
	}
	if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1
		|| fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1)
	{
		saved = errno;
		close(fds[0]);
		close(fds[1]);
		errno = saved;
		return (-1);
	}
	return (0);
}

/**
 * @brief Wake the event loop from a signal handler.
 *
 * Writes a single byte into the self-pipe; the event loop picks it up
 * in sp_drain.  This is the only async-signal-safe function in the
 * library — it calls only write(2) and preserves errno — and uses a
 * portable pipe()-based implementation with no platform ifdefs.
 *
 * If the kernel buffer is full (EAGAIN) the write is silently dropped;
 * the event loop will still be woken by any byte already in the pipe.
 *
 * @param write_fd The write end of the self-pipe.
 */
void	sp_notify(int write_fd)
{
	int	saved;

	saved = errno;
	if (write(write_fd, "\1", 1) == -1 && errno == EAGAIN)
		(void)0;
	errno = saved;
}

/**
 * @brief Drain every pending wakeup byte from the self-pipe.
 *
 * Reads in a loop until the pipe is empty (EAGAIN).  N signals that
 * arrive between two event-loop iterations coalesce to a single wakeup
 * because sp_notify and sp_drain are decoupled — one successful read
 * drains all accumulated bytes.  If the read end has seen EOF (n == 0),
 * the function returns 0 without error so that a half-closed pipe does
 * not wedge the loop.
 *
 * @param read_fd The read end of the self-pipe.
 * @return 0 on success or EOF, -1 on error (errno is set by read).
 */
int	sp_drain(int read_fd)
{
	char	buf[64];
	ssize_t	n;

	for (;;)
	{
		n = read(read_fd, buf, sizeof(buf));
		if (n == -1)
		{
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				return (0);
			return (-1);
		}
		if (n == 0)
			return (0);
	}
}
