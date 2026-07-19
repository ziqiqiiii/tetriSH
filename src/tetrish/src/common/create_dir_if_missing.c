#include "common.h"

/**
 * @brief Creates a directory if it does not already exist.
 *
 * If the path already exists and is a directory, this is a no-op. If it exists
 * but is not a directory, the call fails.
 *
 * @param path Directory path to create.
 * @param mode Permission bits for the new directory (e.g. 0755).
 * @return 0 if the directory exists or was created, -1 on error.
 */
int	create_dir_if_missing(const char *path, mode_t mode)
{
	int	status;

	status = ft_stat(path);
	if (status != 1)
		return (status);
	return (ft_mkdir(path, mode));
}
