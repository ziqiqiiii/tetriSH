#include "system_program.h"

static int	get_project_root(char **project_root);
static int	get_backup_path(char **backup_path, char *project_root);
static int	create_backup(const char *project_root, char *backup_path);
static char	*build_archive_path(const char *project_root, const char *base);
static int	run_backup_tar(char *backup_path, const char *base, const char *archive_path);

/**
 * @brief Entry point for the backup utility.
 *
 * Resolves the project root, reads the BACKUP_DIR target, and creates a
 * timestamped tar.gz archive of it under the project's archive directory.
 *
 * @param argc Number of command-line arguments (unused).
 * @param argv Array of command-line arguments (unused).
 * @return EXIT_SUCCESS on a successful backup, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv) {
	(void)		argc;
	(void)		argv;

	char	*project_root;
	char	*backup_path;
	int		status;

	if (get_project_root(&project_root) == EXIT_FAILURE)
		return (EXIT_FAILURE);
	if (get_backup_path(&backup_path, project_root) == EXIT_FAILURE)
		return (EXIT_FAILURE);

	status = create_backup(project_root, backup_path);

	free(project_root);

	return (status);
}

/**
 * @brief Resolve the project root and ensure its archive directory exists.
 *
 * Stores the resolved root in *project_root and creates the archive
 * directory under it if missing. On failure the allocated root is freed.
 *
 * @param project_root Out-param receiving the allocated project root path.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE if the archive dir cannot
 *         be created.
 */
static int	get_project_root(char **project_root)
{
	*project_root = resolve_project_root();

	if (ensure_archive_dir(*project_root) == -1) {
		ft_putstr_fd("Error: Failed to create archive directory\n", 2);
		free(*project_root);
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Read the backup target path from the BACKUP_DIR environment variable.
 *
 * Stores the value of BACKUP_DIR in *backup_path. If the variable is unset
 * the previously allocated project_root is freed before returning failure.
 *
 * @param backup_path Out-param receiving the BACKUP_DIR value (not allocated).
 * @param project_root Project root to free on failure.
 * @return EXIT_SUCCESS if BACKUP_DIR is set, EXIT_FAILURE otherwise.
 */
static int	get_backup_path(char **backup_path, char *project_root)
{
	*backup_path = getenv("BACKUP_DIR");
	if (!*backup_path) {
		fprintf(stderr, "BACKUP_DIR not set\n");
		free(project_root);
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Build the archive path and tar the backup target into it.
 *
 * Derives the archive basename from backup_path, builds a timestamped
 * archive path under the project root, and runs tar to create it.
 *
 * @param project_root Resolved project root containing the archive dir.
 * @param backup_path Path to the directory being backed up.
 * @return EXIT_SUCCESS on a successful archive, EXIT_FAILURE otherwise.
 */
static int	create_backup(const char *project_root, char *backup_path)
{
	char	*archive_path;
	char	*base;

	base = basename(backup_path);
	archive_path = build_archive_path(project_root, base);
	if (!archive_path)
		return (EXIT_FAILURE);

	if (run_backup_tar(backup_path, base, archive_path) == EXIT_FAILURE) {
		fprintf(stderr, "Failed to archive\n");
		free(archive_path);
		return (EXIT_FAILURE);
	}

	printf("Archived %s into %s\n", backup_path, archive_path);
	free(archive_path);
	return (EXIT_SUCCESS);
}

/**
 * @brief Build a timestamped tar.gz archive path under the project root.
 *
 * Produces "<project_root>/archive/<base>_<YYYYmmdd_HHMMSS>.tar.gz" using
 * the current local time.
 *
 * @param project_root Resolved project root containing the archive dir.
 * @param base Basename of the backup target, used in the archive filename.
 * @return Newly allocated archive path (caller frees), or NULL on malloc
 *         failure.
 */
static char	*build_archive_path(const char *project_root, const char *base)
{
	time_t		now;
	struct tm	*tm_info;
	char		timebuff[64];
	size_t		len;
	char		*archive_path;

	now = time(NULL);
	tm_info = localtime(&now);
	strftime(timebuff, sizeof(timebuff), "%Y%m%d_%H%M%S", tm_info);

	len = snprintf(NULL, 0, "%s/archive/%s_%s.tar.gz",
			project_root, base, timebuff) + 1;
	archive_path = malloc(len);
	if (!archive_path) {
		perror("malloc");
		return (NULL);
	}
	snprintf(archive_path, len, "%s/archive/%s_%s.tar.gz",
		project_root, base, timebuff);
	return (archive_path);
}

/**
 * @brief Fork and exec tar to create the gzip archive.
 *
 * Runs "tar -czf <archive_path> -C <dirname(backup_path)> <base>" in a child
 * process and waits for it to complete.
 *
 * @param backup_path Path to the backup target; dirname() is used as tar's
 *        working directory (note: this call mutates backup_path).
 * @param base Basename of the backup target passed to tar.
 * @param archive_path Destination path for the created archive.
 * @return EXIT_SUCCESS if tar exits with status 0, EXIT_FAILURE otherwise.
 */
static int	run_backup_tar(char *backup_path, const char *base,
				const char *archive_path)
{
	pid_t	pid;
	int		status;

	pid = ft_fork();
	if (pid == 0) {
		execlp("tar", "tar", "-czf", archive_path, "-C",
			dirname(backup_path), base, (char *)NULL);
		perror("execlp tar");
		exit(EXIT_FAILURE);
	}
	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return (EXIT_SUCCESS);
	return (EXIT_FAILURE);
}
