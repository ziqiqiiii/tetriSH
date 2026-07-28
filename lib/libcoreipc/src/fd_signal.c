#include "coreipc.h"

/**
 * @brief Add O_NONBLOCK to an existing descriptor.
 *
 * @param fd The descriptor to modify.
 * @return 0 on success, -1 with errno set on failure.
 */
int	us_set_nonblock(int fd)
{
	/* TODO: fcntl(fd, F_GETFL) then F_SETFL with O_NONBLOCK added, so flags
	   the caller already set are preserved. */
	(void)fd;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Close a bound socket and remove its filesystem entry.
 *
 * Binding leaves a socket file that outlives the process; shutdown paths use
 * this rather than relying on the unlink-stale fallback in us_dgram_bind.
 *
 * @param fd The bound or listening fd to close.
 * @param path The path the fd was bound to.
 * @return 0 when both close and unlink succeeded, -1 with errno set otherwise.
 */
int	us_close_unlink(int fd, const char *path)
{
	/* TODO: close(fd); unlink(path) treating ENOENT as success; report the
	   first failure with its errno intact. */
	(void)fd;
	(void)path;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Create a non-blocking self-pipe for signal-to-event-loop wakeups.
 *
 * The loop polls fds[SP_READ] alongside its sockets, so SIGTERM and SIGHUP
 * are handled in the loop's context rather than in handler context.
 *
 * @param fds Filled with the read end at SP_READ and the write end at SP_WRITE.
 * @return 0 on success, -1 with errno set on failure.
 */
int	sp_pipe(int fds[2])
{
	/* TODO: pipe2(fds, O_NONBLOCK | O_CLOEXEC); without pipe2, fall back to
	   pipe() + us_set_nonblock + FD_CLOEXEC on both ends. */
	(void)fds;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Wake the event loop from a signal handler.
 *
 * The only async-signal-safe function in the library.
 *
 * @param write_fd The write end from sp_pipe (fds[SP_WRITE]).
 */
void	sp_notify(int write_fd)
{
	/* TODO: saved = errno; write(write_fd, "\1", 1); errno = saved. EAGAIN
	   means a wakeup is already pending. No error reporting is safe here. */
	(void)write_fd;
}

/**
 * @brief Drain every pending wakeup byte from the self-pipe.
 *
 * Coalescing is intended: N signals between two iterations are one wakeup.
 *
 * @param read_fd The read end from sp_pipe (fds[SP_READ]).
 * @return 0 once the pipe is empty, -1 with errno set on a real read error.
 */
int	sp_drain(int read_fd)
{
	/* TODO: loop read() into a scratch buffer until -1/EAGAIN (success) or a
	   different errno (failure); retry on EINTR. */
	(void)read_fd;
	errno = ENOSYS;
	return (-1);
}
