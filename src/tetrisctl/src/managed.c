#include "tetrisctl.h"

// Static Functions
static void	spawn(const t_managed *d, const char *rc_path);

/**
 * @brief Reports whether a daemon is running, and which pid it is.
 *
 * The question is asked of the pidfile lock, never of /proc and never of the
 * file's contents alone: a lock that can be taken means its writer is gone,
 * so a pidfile left behind by a crash reads as stopped rather than as a pid
 * worth signalling.
 *
 * @param d Daemon to inspect.
 * @param pid Receives the running pid, or 0 when nothing is running.
 * @return MANAGED_RUNNING, MANAGED_STOPPED, or MANAGED_UNKNOWN when the pidfile is there but
 * cannot be inspected or parsed.
 */
t_managed_state	managed_state(const t_managed *d, pid_t *pid)
{
	int	rc;

	if (d == NULL || pid == NULL)
		return (MANAGED_UNKNOWN);
	rc = daemon_pid_probe(d->pid_path, pid);
	if (rc < 0)
		return (MANAGED_UNKNOWN);
	if (rc == 1)
		return (MANAGED_RUNNING);
	return (MANAGED_STOPPED);
}

/**
 * @brief Launches a daemon and waits to hear whether it actually started.
 *
 * The binary daemonises itself, so the child forked here is the daemon's own
 * launching process - it survives exactly as long as the boot does and exits
 * with the verdict. Waiting for it is therefore not a delay, it is the answer
 * to the question dspawn could never answer: was the program started, or did
 * it die with a reason nobody recorded (docs/adr/0007)?
 *
 * The rc path is handed on as argv[1] so the daemon reads its settings out of
 * the same start-up file this roster came from, rather than re-resolving one
 * and possibly finding a different answer.
 *
 * @param d Daemon to launch; resolved through PATH, as dspawn resolved its.
 * @param rc_path Start-up file to pass on, or NULL to let the daemon resolve.
 * @return 0 when the daemon reported itself up, -1 otherwise.
 */
int	managed_start(const t_managed *d, const char *rc_path)
{
	pid_t	pid;
	int		status;

	if (d == NULL)
		return (-1);
	fflush(NULL);
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid == 0)
		spawn(d, rc_path);
	while (waitpid(pid, &status, 0) < 0)
	{
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
		return (-1);
	return (0);
}

/**
 * @brief Stops a daemon and blocks until its teardown has finished.
 *
 * SIGTERM then the pidfile lock. The lock comes free as the daemon's last
 * act, after it has drained what it had, reclaimed its sink and written its
 * exit line - so waiting on it gives the "blocks until teardown completes"
 * guarantee without either side needing a protocol to say so.
 *
 * A daemon that is already stopped is success, not an error: `stop` is a
 * statement about the end state, and an operator who runs it twice has not
 * done anything wrong.
 *
 * @param d Daemon to stop.
 * @param timeout_ms How long to wait for teardown before giving up.
 * @return 0 when the daemon is down, -1 with errno set otherwise.
 */
int	managed_stop(const t_managed *d, int timeout_ms)
{
	t_managed_state	state;
	pid_t	pid;

	if (d == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	state = managed_state(d, &pid);
	if (state == MANAGED_STOPPED)
		return (0);
	if (state == MANAGED_UNKNOWN)
		return (-1);
	if (kill(pid, SIGTERM) != 0 && errno != ESRCH)
		return (-1);
	return (daemon_pid_wait(d->pid_path, timeout_ms));
}

/**
 * @brief Child half of managed_start: become the daemon binary; never returns.
 *
 * @param d Daemon to exec.
 * @param rc_path Start-up file to pass as argv[1], or NULL.
 */
static void	spawn(const t_managed *d, const char *rc_path)
{
	char	*argv[3];
	char	name[TETRISCTL_NAME_MAX];
	char	rc[TETRISCTL_FS_PATH_MAX];

	snprintf(name, sizeof(name), "%s", d->name);
	argv[0] = name;
	argv[1] = NULL;
	argv[2] = NULL;
	if (rc_path != NULL && rc_path[0] != '\0')
	{
		snprintf(rc, sizeof(rc), "%s", rc_path);
		argv[1] = rc;
	}
	execvp(name, argv);
	fprintf(stderr, "%s: cannot run %s: %s\n",
		TETRISCTL_COMPONENT_NAME, name, strerror(errno));
	_exit(EXIT_FAILURE);
}
