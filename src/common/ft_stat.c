#include "common.h"

/**
 * @brief Check whether a path exists and is a directory.
 *
 * @param path Filesystem path to inspect.
 * @return 0 if the path exists and is a directory; -1 if it exists but is
 *         not a directory (an error is printed to stderr); 1 if it does
 *         not exist.
 */
int	ft_stat(const char *path)
{
	struct stat	st;

	if (stat(path, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
			return (0);
		ft_putstr_fd("Error: It's not dir\n", 2);
		return (-1);
	}
	return (1);
}
