#include "coredaemon.h"

// Static Functions
static int	publish(int fd, pid_t pid);

/**
 * @brief Puts a zeroed pidfile into the "nothing is held" state.
 *
 * A zeroed fd is stdin, so a pidfile that was never claimed would have
 * daemon_pid_release close the daemon's standard input. Boot can fail before the
 * claim, and teardown is shared between that path and an ordinary stop, so
 * the safe state has to exist before anything can fail.
 *
 * @param pf Pidfile to blank.
 */
void	daemon_pid_blank(t_pidfile *pf)
{
	if (pf == NULL)
		return ;
	memset(pf, 0, sizeof(*pf));
	pf->fd = -1;
}

/**
 * @brief Claims the pidfile, which is what makes this the only instance.
 *
 * Call it after daemon_detach and never before: the pid written has to be the
 * detached process's, and the lock has to be held by the process that will
 * still be here to hold it.
 *
 * The lock is taken before the pid is written, so two daemons racing cannot
 * both believe they won. A loser leaves errno at EWOULDBLOCK, which is how
 * the caller tells "another instance is running" - the one message worth
 * printing - from "cannot open the file at all".
 *
 * @param pf Pidfile to fill.
 * @param path Path to claim.
 * @return 0 on success, -1 with errno set on failure.
 */
int	daemon_pid_claim(t_pidfile *pf, const char *path)
{
	int	fd;
	int	saved;

	if (pf == NULL || path == NULL || path[0] == '\0'
		|| strlen(path) >= DAEMON_PATH_MAX)
	{
		errno = EINVAL;
		return (-1);
	}
	if (daemon_mkdir_parent(path) != 0)
		return (-1);
	fd = open(path, O_RDWR | O_CREAT, DAEMON_FILE_MODE);
	if (fd < 0)
		return (-1);
	if (flock(fd, LOCK_EX | LOCK_NB) != 0 || publish(fd, getpid()) != 0)
	{
		saved = errno;
		close(fd);
		errno = saved;
		return (-1);
	}
	pf->fd = fd;
	pf->pid = getpid();
	snprintf(pf->path, DAEMON_PATH_MAX, "%s", path);
	return (0);
}

/**
 * @brief Releases the lock and lets go of the pidfile.
 *
 * The file is left on disk rather than unlinked. A pidfile whose lock is free
 * already reads as stale, so removing it buys nothing and costs the operator
 * the last pid of a daemon that has gone - and an unlink races a second
 * instance that has just claimed the same path.
 *
 * @param pf Pidfile to release; safe on one that was never claimed.
 */
void	daemon_pid_release(t_pidfile *pf)
{
	if (pf == NULL || pf->fd < 0)
		return ;
	flock(pf->fd, LOCK_UN);
	close(pf->fd);
	pf->fd = -1;
	pf->pid = 0;
}

/**
 * @brief Writes one decimal pid and a newline over whatever was there.
 *
 * @param fd Locked descriptor, opened for writing.
 * @param pid Pid to record.
 * @return 0 on success, -1 with errno set on failure.
 */
static int	publish(int fd, pid_t pid)
{
	char	text[32];
	ssize_t	n;
	size_t	len;
	size_t	done;

	len = (size_t)snprintf(text, sizeof(text), "%d\n", (int)pid);
	if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) != 0)
		return (-1);
	done = 0;
	while (done < len)
	{
		n = write(fd, text + done, len - done);
		if (n < 0 && errno == EINTR)
			continue ;
		if (n <= 0)
			return (-1);
		done += (size_t)n;
	}
	return (0);
}
