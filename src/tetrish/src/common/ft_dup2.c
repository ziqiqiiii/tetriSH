#include "common.h"

/**
 * @brief Duplicates a file descriptor, exiting on failure.
 *
 * Calls dup2(2) to make old_fd refer to the same file as new_fd.
 * Terminates the process with an error message if the call fails.
 *
 * @param new_fd The source file descriptor to duplicate.
 * @param old_fd The target file descriptor to replace.
 * @return The value of old_fd on success, or exits on failure.
 */
int	ft_dup2(int new_fd, int old_fd)
{
	int	i;

	i = dup2(new_fd, old_fd);
	if (i < 0)
	{
		printf("Error: Failed to dup2\n");
		exit(EXIT_FAILURE);
	}
	return (i);
}
