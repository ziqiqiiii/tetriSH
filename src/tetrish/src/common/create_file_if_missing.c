#include "common.h"

/**
 * @brief Creates an empty file if it does not already exist.
 *
 * If the file already exists its contents are left untouched (no truncation).
 *
 * @param path File path to create.
 * @param mode Permission bits for a newly created file (e.g. 0644).
 * @return 0 if the file exists or was created, -1 on error.
 */
int	create_file_if_missing(const char *path, mode_t mode)
{
	int	fd;

	fd = ft_open(path, O_WRONLY | O_CREAT, mode);
	ft_close(fd);
	return (0);
}
