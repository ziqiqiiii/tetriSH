#include "common.h"

/**
 * @brief Ensures the archive directory exists under <project_root>/archive.
 *
 * Creates the archive/ directory if missing so backup can drop its tarballs
 * there on a fresh checkout.
 *
 * @param project_root Absolute path of the project root.
 * @return 0 if the directory exists or was created, -1 on error.
 */
int	ensure_archive_dir(const char *project_root)
{
	char	path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/archive", project_root);
	return (create_dir_if_missing(path, 0755));
}
