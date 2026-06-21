#include "common.h"

/**
 * @brief Creates a directory, reporting failure.
 *
 * Wraps mkdir(2): on failure it prints a "mkdir" error via perror.
 *
 * @param path Directory path to create.
 * @param mode Permission bits for the new directory (e.g. 0755).
 * @return 0 on success, -1 on failure.
 */
int	ft_mkdir(const char *path, mode_t mode)
{
	if (mkdir(path, mode) == -1)
	{
		perror("mkdir");
		return (-1);
	}
	return (0);
}
