#include "tetrislogd.h"

// Static Functions
static int	open_sink(const char *path);
static void	adopt(t_sink *sk, int fd);

/**
 * @brief Puts a zeroed sink into the "nothing is open yet" state.
 *
 * A zeroed fd is stdout, so a sink that was never opened would have
 * sink_close close the caller's standard output. Boot can fail before the
 * sink is opened, and teardown is shared between that path and an ordinary
 * stop, so the safe state has to exist before anything can fail.
 *
 * @param sk Sink to blank.
 */
void	sink_blank(t_sink *sk)
{
	if (sk == NULL)
		return ;
	memset(sk, 0, sizeof(*sk));
	sk->fd = -1;
}

/**
 * @brief Opens the log file for appending.
 *
 * No lock is taken. This used to hold the exclusive flock that stopped a
 * second tetrislogd interleaving into the same file, which made the sink the
 * single-instance guard as well as the sink - so deleting tmp/ removed both
 * at once and the reclaim path had to restore both. The pidfile is the guard
 * now (docs/adr/0007), claimed in main.c before the daemon opens anything.
 *
 * @param sk Sink to open.
 * @param path Log file path.
 * @return 0 on success, -1 with errno set on failure.
 */
int	sink_open(t_sink *sk, const char *path)
{
	int	fd;

	if (sk == NULL || path == NULL || path[0] == '\0' || strlen(path) >= TETRISLOGD_FS_PATH_MAX)
	{
		errno = EINVAL;
		return (-1);
	}
	fd = open_sink(path);
	if (fd < 0)
		return (-1);
	snprintf(sk->path, TETRISLOGD_FS_PATH_MAX, "%s", path);
	adopt(sk, fd);
	return (0);
}

/**
 * @brief Appends one already-formatted line to the log file.
 *
 * Nothing is added to the text: logrecord_format_line already terminates the line
 * with a newline, and appending a second one is what puts a blank line after
 * every record in tetrisd's stderr fallback.
 *
 * @param sk Sink to write to.
 * @param line Formatted line, newline included.
 * @param len Length of line in bytes.
 * @return 0 on success, -1 with errno set when the sink is closed or the
 * write fails.
 */
int	sink_write(t_sink *sk, const char *line, size_t len)
{
	ssize_t	n;
	size_t	done;

	if (sk == NULL || sk->fd < 0 || line == NULL)
	{
		errno = EBADF;
		return (-1);
	}
	done = 0;
	while (done < len)
	{
		n = write(sk->fd, line + done, len - done);
		if (n < 0 && errno == EINTR)
			continue ;
		if (n <= 0)
			return (-1);
		done += (size_t)n;
	}
	sk->dirty = true;
	return (0);
}

/**
 * @brief Flushes the log file to disk if anything is unsynced.
 *
 * Called on the idle tick, on rotation, and at shutdown - never per record,
 * which would make disk latency the ceiling on log throughput and push
 * tetrisd into its stderr fallback under ordinary load.
 *
 * @param sk Sink to sync.
 * @return 0 on success or when there is nothing to do, -1 on failure.
 */
int	sink_sync(t_sink *sk)
{
	if (sk == NULL || sk->fd < 0 || sk->dirty == false)
		return (0);
	if (fdatasync(sk->fd) != 0)
		return (-1);
	sk->dirty = false;
	return (0);
}

/**
 * @brief Reopens the log file at its configured path, for rotation.
 *
 * The reopen can genuinely fail - the directory may be gone - and when it
 * does the sink stays closed rather than the process exiting. The caller
 * reads that as "degrade to stderr and retry", because a logger that quits
 * over a disk hiccup takes the whole log path with it.
 *
 * @param sk Sink to reopen.
 * @return 0 on success, -1 with errno set on failure.
 */
int	sink_reopen(t_sink *sk)
{
	int	fd;

	if (sk == NULL || sk->path[0] == '\0')
	{
		errno = EINVAL;
		return (-1);
	}
	sink_sync(sk);
	if (sk->fd >= 0)
		close(sk->fd);
	sk->fd = -1;
	fd = open_sink(sk->path);
	if (fd < 0)
		return (-1);
	adopt(sk, fd);
	return (0);
}

/**
 * @brief Reopens a closed sink, if it has somewhere to reopen to.
 *
 * This is the whole of the recovery policy, kept behind the sink rather than
 * spelled out by its caller: the loop should be able to ask "did the file come
 * back?" without knowing that the answer depends on a stored path.
 *
 * @param sk Sink to retry.
 * @return 0 when the sink was closed and is now open, -1 otherwise.
 */
int	sink_retry(t_sink *sk)
{
	if (sk == NULL || sink_is_open(sk) || sk->path[0] == '\0')
		return (-1);
	return (sink_reopen(sk));
}

/**
 * @brief Syncs and closes the log file.
 *
 * Safe on a blanked or already-closed sink, because teardown is shared
 * between a clean stop and a boot that failed before the sink was opened.
 *
 * @param sk Sink to close.
 */
void	sink_close(t_sink *sk)
{
	if (sk == NULL || sk->fd < 0)
		return ;
	sink_sync(sk);
	close(sk->fd);
	sk->fd = -1;
	sk->dirty = false;
}

/**
 * @brief Reports whether the sink currently has the file open.
 *
 * @param sk Sink to query, or NULL.
 * @return true when a record would reach the file, false when it would go to
 * stderr instead.
 */
bool	sink_is_open(const t_sink *sk)
{
	return (sk != NULL && sk->fd >= 0);
}

/**
 * @brief Reports whether the open descriptor still holds the configured path.
 *
 * A write to an unlinked file succeeds, so nothing on the writing side ever
 * fails to say the log has gone: `make reset` deletes tmp/ under a running
 * logger, and every record after that lands in an inode no one can open. The
 * daemon has to ask the question itself, and this is the question.
 *
 * A path that cannot be stat'ed counts as stale, which covers the deletion of
 * the file and of the directory above it alike. Answering false for a closed
 * sink keeps this distinct from sink_retry's case: nothing to reclaim yet.
 *
 * @param sk Sink to test, or NULL.
 * @return true when the sink is open on a file the path no longer names.
 */
bool	sink_is_stale(const t_sink *sk)
{
	struct stat	st;

	if (sk == NULL || sk->fd < 0 || sk->ino == 0)
		return (false);
	if (stat(sk->path, &st) != 0)
		return (true);
	return (st.st_dev != sk->dev || st.st_ino != sk->ino);
}

/**
 * @brief Opens one log file for appending, creating the directory above it.
 *
 * O_APPEND rather than a seek, so a log file rotated out from under the
 * daemon and a fresh one behave identically: every write lands at the end of
 * whatever the descriptor currently names.
 *
 * @param path Log file path.
 * @return The open fd on success, -1 with errno set on failure.
 */
static int	open_sink(const char *path)
{
	if (daemon_mkdir_parent(path) != 0)
		return (-1);
	return (open(path, O_WRONLY | O_APPEND | O_CREAT, TETRISLOGD_FILE_MODE));
}

/**
 * @brief Takes ownership of a freshly opened descriptor.
 *
 * Recording which file it is happens here rather than at either call site
 * because the two paths into a new descriptor - the first open and every
 * reopen - must record it identically. A sink that skipped it would look
 * permanently fresh and never notice its file was replaced.
 *
 * An fstat that fails leaves both fields 0, which sink_is_stale reads as "do
 * not know, do not claim staleness": worse than detecting a lost log, but
 * better than reopening a good one in a loop.
 *
 * @param sk Sink to update; sk->path must already be set.
 * @param fd Freshly opened descriptor from open_sink.
 */
static void	adopt(t_sink *sk, int fd)
{
	struct stat	st;

	sk->fd = fd;
	sk->dirty = false;
	sk->dev = 0;
	sk->ino = 0;
	if (fstat(fd, &st) == 0)
	{
		sk->dev = st.st_dev;
		sk->ino = st.st_ino;
	}
}
