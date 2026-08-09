#include "coredaemon.h"

// Static Functions
static int	lock_state(const char *path);
static void	nap_ms(int ms);
static int	proc_starttime(pid_t pid, unsigned long long *ticks);
static int	boot_time(long *seconds);

/**
 * @brief Reads the pid out of a pidfile, whether or not anyone holds it.
 *
 * This answers only "what does the file say". Whether that pid is still alive
 * is daemon_pid_probe's question, and the two are kept apart because a pidfile
 * left behind by a crash reads perfectly well and means nothing.
 *
 * @param path Pidfile to read.
 * @param out Receives the pid on success.
 * @return 0 on success, -1 with errno set when the file is absent or its
 * contents are not a single positive pid.
 */
int	daemon_pid_read(const char *path, pid_t *out)
{
	char	text[32];
	char	*end;
	long	value;
	ssize_t	n;
	int		fd;

	if (path == NULL || out == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);
	n = read(fd, text, sizeof(text) - 1);
	close(fd);
	if (n <= 0)
	{
		errno = EINVAL;
		return (-1);
	}
	text[n] = '\0';
	errno = 0;
	value = strtol(text, &end, 10);
	if (end == text || value <= 0 || (*end != '\0' && *end != '\n'))
	{
		errno = EINVAL;
		return (-1);
	}
	*out = (pid_t)value;
	return (0);
}

/**
 * @brief Answers whether a daemon is running, and which pid it is.
 *
 * The lock is the question, not the file and not /proc. A lock that can be
 * taken means its writer is gone, so a pidfile left behind by a crash and a
 * pidfile that was never written are the same answer, with no probe of the
 * process table and no window between checking and signalling.
 *
 * @param path Pidfile to inspect.
 * @param out Receives the running pid, or 0 when nothing is running.
 * @return 1 when a daemon holds the pidfile, 0 when none does, -1 with errno
 * set when the file exists but cannot be inspected or parsed.
 */
int	daemon_pid_probe(const char *path, pid_t *out)
{
	int	held;

	if (path == NULL || out == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	*out = 0;
	held = lock_state(path);
	if (held <= 0)
		return (held);
	if (daemon_pid_read(path, out) != 0)
		return (-1);
	return (1);
}

/**
 * @brief Blocks until nothing holds the pidfile any more.
 *
 * This is how tetrisctl confirms a stop. The lock comes free as the daemon's
 * last act, after it has drained, reclaimed its sink and written its exit
 * line, so waiting on it gives "blocks until teardown completes" without the
 * daemon needing a protocol to say so.
 *
 * @param path Pidfile to wait on.
 * @param timeout_ms How long to wait; negative waits indefinitely.
 * @return 0 once the lock is free, -1 with errno set to ETIMEDOUT on timeout.
 */
int	daemon_pid_wait(const char *path, int timeout_ms)
{
	int	waited;
	int	held;

	if (path == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	waited = 0;
	while (true)
	{
		held = lock_state(path);
		if (held == 0)
			return (0);
		if (timeout_ms >= 0 && waited >= timeout_ms)
		{
			errno = ETIMEDOUT;
			return (-1);
		}
		nap_ms(DAEMON_WAIT_STEP_MS);
		waited += DAEMON_WAIT_STEP_MS;
	}
}

/**
 * @brief How long a running daemon has been up, in seconds.
 *
 * Asked of the kernel rather than of the pidfile. The obvious alternative is
 * the pidfile's mtime, and it is wrong for the case that matters: the file
 * outlives its writer, so a crash-left pidfile would report a confident
 * uptime for a daemon that is not running. /proc/<pid>/stat is a record of
 * the process, so it cannot answer at all once the process is gone.
 *
 * The caller is expected to have established that the pid is running - by
 * daemon_pid_probe - since a pid alone is racy: the number can be reused.
 *
 * @param pid Daemon to ask about.
 * @param seconds Receives the elapsed seconds since it started.
 * @return 0 on success, -1 with errno set when the process is gone or /proc
 * cannot be read (ENOSYS where there is no /proc at all).
 */
int	daemon_pid_uptime(pid_t pid, long *seconds)
{
	unsigned long long	ticks;
	long				booted;
	long				hz;
	long				started;

	if (pid <= 0 || seconds == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	if (proc_starttime(pid, &ticks) != 0 || boot_time(&booted) != 0)
		return (-1);
	hz = sysconf(_SC_CLK_TCK);
	if (hz <= 0)
		hz = 100;
	started = booted + (long)(ticks / (unsigned long long)hz);
	*seconds = (long)time(NULL) - started;
	/* A clock stepped backwards since boot is not worth a failure; report
	 * nothing rather than a negative age the caller would have to filter. */
	if (*seconds < 0)
		*seconds = 0;
	return (0);
}

/**
 * @brief Reads field 22 of /proc/<pid>/stat, the process start time.
 *
 * The field is counted from the closing parenthesis of the command name
 * rather than by splitting on spaces, because that name is arbitrary and may
 * contain both spaces and parentheses - "(my daemon) (1)" is a legal comm.
 *
 * @param pid Process to read.
 * @param ticks Receives the start time in clock ticks since boot.
 * @return 0 on success, -1 with errno set otherwise.
 */
static int	proc_starttime(pid_t pid, unsigned long long *ticks)
{
	char	path[64];
	char	buf[1024];
	char	*after;
	ssize_t	n;
	int		fd;

	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
	{
		errno = EINVAL;
		return (-1);
	}
	buf[n] = '\0';
	after = strrchr(buf, ')');
	if (after == NULL
		|| sscanf(after + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u"
			" %*u %*u %*d %*d %*d %*d %*d %*d %llu", ticks) != 1)
	{
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

/**
 * @brief Reads the "btime" line of /proc/stat, the epoch second of boot.
 *
 * @param seconds Receives the boot time.
 * @return 0 on success, -1 with errno set otherwise.
 */
static int	boot_time(long *seconds)
{
	FILE	*fp;
	char	line[256];
	int		found;

	fp = fopen("/proc/stat", "r");
	if (fp == NULL)
		return (-1);
	found = 0;
	while (found == 0 && fgets(line, sizeof(line), fp) != NULL)
	{
		if (sscanf(line, "btime %ld", seconds) == 1)
			found = 1;
	}
	fclose(fp);
	if (found == 0)
	{
		errno = ENOSYS;
		return (-1);
	}
	return (0);
}

/**
 * @brief Reports whether anything currently holds the pidfile's lock.
 *
 * A shared lock is enough to ask the question and is the gentler one to ask
 * with: two operators running `tetrisctl status` at once do not queue behind
 * each other, and neither holds anything exclusive over a daemon that happens
 * to be booting.
 *
 * @param path Pidfile to test.
 * @return 1 when held, 0 when free or absent, -1 with errno set on error.
 */
static int	lock_state(const char *path)
{
	int	fd;
	int	saved;

	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		if (errno == ENOENT)
			return (0);
		return (-1);
	}
	if (flock(fd, LOCK_SH | LOCK_NB) == 0)
	{
		flock(fd, LOCK_UN);
		close(fd);
		return (0);
	}
	saved = errno;
	close(fd);
	if (saved == EWOULDBLOCK || saved == EAGAIN)
		return (1);
	errno = saved;
	return (-1);
}

/**
 * @brief Sleeps for a few milliseconds between lock retries.
 *
 * @param ms Milliseconds to sleep.
 */
static void	nap_ms(int ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
