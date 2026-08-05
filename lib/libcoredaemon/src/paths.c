#include "coredaemon.h"

// Static Functions
static int	make_one(const char *path);

/**
 * @brief Creates a directory and every missing directory above it.
 *
 * Both daemons keep their pidfile and error file under tmp/, which `make
 * reset` deletes wholesale, so requiring the tree to exist would make a boot
 * after a reset fail for a reason that has nothing to do with the daemon.
 *
 * @param path Directory to create.
 * @return 0 when the directory exists afterwards, -1 with errno set otherwise.
 */
int	cd_mkdir_p(const char *path)
{
	char	work[CD_PATH_MAX];
	size_t	i;

	if (path == NULL || path[0] == '\0' || strlen(path) >= CD_PATH_MAX)
	{
		errno = EINVAL;
		return (-1);
	}
	snprintf(work, CD_PATH_MAX, "%s", path);
	i = 1;
	while (work[i] != '\0')
	{
		if (work[i] == '/')
		{
			work[i] = '\0';
			if (make_one(work) != 0)
				return (-1);
			work[i] = '/';
		}
		i++;
	}
	return (make_one(work));
}

/**
 * @brief Creates the directories a file path needs, ignoring the file itself.
 *
 * A path with no slash in it names a file in the working directory, which
 * needs nothing created - that is a success, not a missing parent.
 *
 * @param path File path whose parents are wanted.
 * @return 0 on success, -1 with errno set on failure.
 */
int	cd_mkdir_parent(const char *path)
{
	char	work[CD_PATH_MAX];
	char	*slash;

	if (path == NULL || path[0] == '\0' || strlen(path) >= CD_PATH_MAX)
	{
		errno = EINVAL;
		return (-1);
	}
	snprintf(work, CD_PATH_MAX, "%s", path);
	slash = strrchr(work, '/');
	if (slash == NULL || slash == work)
		return (0);
	*slash = '\0';
	return (cd_mkdir_p(work));
}

/**
 * @brief Creates one directory, accepting only a directory as already there.
 *
 * mkdir reports EEXIST for a regular file in the way as readily as for the
 * directory that was wanted, so the two are told apart here. Treating a file
 * as success would push the failure down to whichever open() came next and
 * report it against the wrong path.
 *
 * @param path Single directory to create.
 * @return 0 when path is a directory afterwards, -1 with errno set otherwise.
 */
static int	make_one(const char *path)
{
	struct stat	st;

	if (mkdir(path, CD_DIR_MODE) == 0)
		return (0);
	if (errno != EEXIST)
		return (-1);
	if (stat(path, &st) != 0)
		return (-1);
	if (!S_ISDIR(st.st_mode))
	{
		errno = ENOTDIR;
		return (-1);
	}
	return (0);
}
