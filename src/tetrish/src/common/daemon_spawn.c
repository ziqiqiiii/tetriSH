#include "common.h"

/**
 * @brief Daemonizes the current process using a double-fork.
 *
 * Performs the standard Unix daemonization sequence: first fork exits the
 * parent, setsid() creates a new session, a second fork prevents the daemon
 * from reacquiring a terminal. Closes all open file descriptors except
 * keep_fd, and redirects stdin/stdout/stderr to /dev/null.
 *
 * /dev/null is the floor, not the policy: a caller that wants to keep what the
 * daemon says about itself reopens stderr afterwards (dspawn does, onto a file
 * named for the registry entry).
 *
 * Emits no output of its own. Anything printed here would run before the
 * first fork() and be flushed once per process when stdout is not a terminal,
 * which is why the old startup banner appeared twice under a pipe. A caller
 * that wants to announce the spawn should open the terminal beforehand and
 * pass the descriptor as keep_fd: after setsid() the daemon has no
 * controlling terminal and can no longer open /dev/tty itself.
 *
 * @param keep_fd Descriptor to leave open across daemonisation, or -1.
 * @param ready_fd Out-parameter receiving the write end of a readiness pipe,
 *                 or NULL. The originating process blocks until the daemon
 *                 closes it (see daemon_ready), so that anything the daemon
 *                 prints lands before the caller's shell draws its next
 *                 prompt. Set to -1 when no pipe could be created.
 */
void	daemon_spawn(int keep_fd, int *ready_fd)
{
	pid_t	pid;
	int		fd;
	int		null_fd;
	int		ready[2];
	char	done;

	if (!ready_fd || pipe(ready) == -1)
	{
		ready[0] = -1;
		ready[1] = -1;
	}
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);
	if (pid > 0)
	{
		/* Hold the terminal until the daemon reports itself ready. read()
		 * returns 0 when the last writer closes, which also covers the
		 * daemon dying before it got that far. */
		if (ready[0] != -1)
		{
			close(ready[1]);
			while (read(ready[0], &done, 1) == -1 && errno == EINTR)
				;
			close(ready[0]);
		}
		exit(EXIT_SUCCESS);
	}
	if (ready[0] != -1)
		close(ready[0]);
	if (setsid() < 0)
		exit(EXIT_FAILURE);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);
	if (pid > 0)
		exit(EXIT_SUCCESS);
	umask(0);
    for (fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
      if (fd != keep_fd && fd != ready[1])
        close(fd);
    }
	if (ready_fd)
		*ready_fd = ready[1];
	null_fd = open("/dev/null", O_RDWR);
	if (null_fd != -1)
	{
		dup2(null_fd, STDIN_FILENO);
		dup2(null_fd, STDOUT_FILENO);
		dup2(null_fd, STDERR_FILENO);
		if (null_fd > 2)
			close(null_fd);
	}
}

/**
 * @brief Release the process waiting on daemon_spawn()'s readiness pipe.
 *
 * Closing the write end makes the originating process's read() return 0, so
 * it stops holding the terminal and exits. Call this once the daemon has
 * finished anything that should appear before the caller's next shell prompt.
 *
 * @param ready_fd Descriptor from daemon_spawn(), or -1 to do nothing.
 */
void	daemon_ready(int ready_fd)
{
	if (ready_fd != -1)
		close(ready_fd);
}
