#include "common.h"

/**
 * @brief Forks a child process, exiting on failure.
 *
 * Calls fork(2) and terminates the process with an error message if
 * the system call fails.
 *
 * @return The PID of the child process in the parent (0 in the child),
 *         or exits on failure.
 */
int	ft_fork(void)
{
	pid_t	child;

	child = fork();
	if (child < 0)
	{
		printf("fork: %d\n", getpid());
		ft_putstr_fd("Error: Failed to create child process\n", 2);
		exit(EXIT_FAILURE);
	}
	return (child);
}
