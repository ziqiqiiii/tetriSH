#include "common.h"

/**
 * @brief Resolves the absolute path of the project root directory.
 *
 * Reads the executable's path via /proc/self/exe. If the binary resides in a
 * "bin" subdirectory (the standalone system programs), navigates one level up
 * so the result is the project root rather than bin/. Falls back to $HOME or
 * "." if the symlink cannot be read.
 *
 * @return A freshly malloc'd absolute path string that the caller must free,
 *         or NULL on allocation failure.
 */
char	*resolve_project_root(void)
{
	char		path[PATH_MAX];
	const char	*home;
	char		*dir;
	ssize_t		n;

	n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (n == -1)
	{
		perror("readlink");
		home = getenv("HOME");
		if (home)
			return (ft_strdup(home));
		return (ft_strdup("."));
	}
	path[n] = '\0';
	dir = dirname(path);
	if (strcmp(basename(dir), "bin") == 0)
		dir = dirname(dir);
	return (ft_strdup(dir));
}
