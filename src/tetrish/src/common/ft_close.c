#include "common.h"

/**
 * @brief Closes a file descriptor, exiting on failure.
 *
 * Calls close(2) and terminates the process with an error message if
 * the system call fails.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, or exits on failure.
 */
int	ft_close(int fd)
{
	if (close(fd) < 0)
	{
		printf("close: %d\n", getpid());
		ft_putstr_fd("Error: Failed to close the fd\n", 2);
		exit(EXIT_FAILURE);
	}
	return (0);
}
