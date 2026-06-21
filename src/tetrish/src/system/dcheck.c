#include "system_program.h"

static void	dcheck(const char *project_root);
static void	dcheck_graveyard(const char *project_root);

/**
 * @brief Entry point for the dcheck daemon status utility.
 *
 * Resolves the project root, ensures the daemon bookkeeping files exist,
 * then prints the registered daemons and the daemon "graveyard" listings.
 *
 * @param argc Number of command-line arguments (unused).
 * @param argv Array of command-line arguments (unused).
 * @return 0 on success.
 */
int main(int argc, char **argv)
{
	(void)	argc;
	(void)	argv;

	char	*project_root;

	project_root = resolve_project_root();
	ensure_daemon_files(project_root);

	dcheck(project_root);
	dcheck_graveyard(project_root);

	free(project_root);

	return 0;
}

/**
 * @brief Print the registered daemons and which are still alive.
 *
 * Reads "<project_root>/tmp/daemons.reg", and for each entry checks whether
 * /proc/<pid> exists to determine liveness. Prints a status line per daemon
 * and a total count of active ones.
 *
 * @param project_root Resolved project root containing the tmp directory.
 */
static void dcheck(const char *project_root)
{
	char	reg_path[PATH_MAX];
	char	line[1024];
	char	name[128];
	char	ts[128];
	char	proc_path[128];
	int		pid;
	int		check;
	int		count;
	FILE	*fd;

	count = 0;
	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/daemons.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	// check for daemons created with the same name and add a number to differentiate it
	fd = ft_fopen(reg_path, "r");

	printf("------------------------------------------\n");
	printf("Registered Daemons (%s)\n", "tmp/daemons.reg");
	printf("------------------------------------------\n");

	while (fgets(line, sizeof(line), fd))
	{
		if (sscanf(line, "%s %d %[^\n]", name, &pid, ts) != 3)
			continue;

		snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
		check = (access(proc_path, F_OK) == 0);

		if (check)
			++count;

		printf("[%c] %-12s \tPID: %-6d %s%s\n", check ? '1' : '-', name,
		       pid, check ? "Started: " : "(inactive)",
		       check ? ts : "");
	}

	printf("------------------------------------------\n");
	printf("Active daemons: %d\n", count);

	fclose(fd);
}

/**
 * @brief Print the daemon "graveyard" of previously killed daemons.
 *
 * Reads "<project_root>/tmp/cematary.reg" and prints each recorded daemon
 * as inactive, followed by a total count of buried daemons.
 *
 * @param project_root Resolved project root containing the tmp directory.
 */
static void dcheck_graveyard(const char *project_root)
{
	char	reg_path[PATH_MAX];
	char	line[1024];
	char	name[128];
	char	ts[128];
	int		pid;
	int		count;
	FILE	*fd;

	count = 0;
	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/cematary.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	// check for daemons created with the same name and add a number to differentiate it
	fd = ft_fopen(reg_path, "r");

	printf("\n------------------------------------------\n");
	printf("Daemon Cematary (%s)\n", "tmp/cematary.reg");
	printf("------------------------------------------\n");

	while (fgets(line, sizeof(line), fd))
	{
		if (sscanf(line, "%s %d %[^\n]", name, &pid, ts) != 3)
			continue;

		printf("[%c] %-12s \tPID: %-6d %s%s\n", '-', name, pid, "(inactive)", "");

		++count;
	}
	printf("------------------------------------------\n");
	printf("Daemons burried: %d\n\n", count);

	fclose(fd);
}
