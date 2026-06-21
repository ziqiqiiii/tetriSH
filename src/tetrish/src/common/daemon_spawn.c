#include "common.h"

/**
 * @brief Daemonizes the current process using a double-fork.
 *
 * Performs the standard Unix daemonization sequence: first fork exits the
 * parent, setsid() creates a new session, a second fork prevents the daemon
 * from reacquiring a terminal. Closes all open file descriptors and redirects
 * stdin/stdout/stderr to /dev/null.
 */
void	daemon_spawn(void)
{
	pid_t	pid;
	int		fd;
	int		null_fd;

	printf("some kind of daemon spawning program\n");
	printf("spawning a daemon, remember to run ./daemons_killer to kill them\n");
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);
	if (pid > 0)
		exit(EXIT_SUCCESS);
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
      close(fd);
    }
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
