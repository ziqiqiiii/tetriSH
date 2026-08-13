#include "common.h"

/**
 * @brief Ensures the daemon working files exist under <project_root>/tmp.
 *
 * Creates the tmp/ directory if missing, then creates the active-daemon
 * registry, the graveyard registry, and the spawn log if they do not yet
 * exist. This lets the daemon utilities read them without failing on a fresh
 * checkout where tmp/ has not been populated.
 *
 * @param project_root Absolute path of the project root.
 */
void	ensure_daemon_files(const char *project_root)
{
	char	path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/tmp", project_root);
	create_dir_if_missing(path, 0755);
	snprintf(path, sizeof(path), "%s/tmp/daemons.reg", project_root);
	create_file_if_missing(path, 0644);
	snprintf(path, sizeof(path), "%s/tmp/cematary.reg", project_root);
	create_file_if_missing(path, 0644);
	snprintf(path, sizeof(path), "%s/tmp/dspawn.log", project_root);
	create_file_if_missing(path, 0644);
}
