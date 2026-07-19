#include "common.h"

/**
 * @brief Creates a pipe, exiting on failure.
 *
 * Calls pipe(2) and terminates the process with an error message if
 * the system call fails.
 *
 * @param p Two-element array to receive the read and write file descriptors.
 * @return The read end file descriptor, or exits on failure.
 */
int	ft_pipe(int p[2])
{
	if (pipe(p) == -1)
	{
		printf("pipe: %d\n", getpid());
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	return (*p);
}
