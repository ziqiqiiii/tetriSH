#include "common.h"

/**
 * @brief Opens a file, exiting on failure.
 *
 * Calls open(2) with the given flags and permission bits. Terminates the
 * process with an error message if the file cannot be opened.
 *
 * @param file Path to the file to open.
 * @param flags Open flags (e.g. O_RDONLY, O_CREAT).
 * @param permission Permission bits applied when creating a new file.
 * @return The file descriptor, or exits on failure.
 */
int	ft_open(const char *file, int flags, int permission)
{
	int	fd;

	fd = open(file, flags, permission);
	if (fd < 0)
	{
		printf("open: %d\n", getpid());
		ft_putstr_fd("Error: Failed to open file\n", 2);
		exit(EXIT_FAILURE);
	}
	return (fd);
}
