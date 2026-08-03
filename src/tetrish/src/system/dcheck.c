#include "system_program.h"

static void	dcheck(const char *project_root);
static void	dcheck_graveyard(const char *project_root);
static int	load_daemons(const char *reg_path, DaemonInfo *daemons);

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
 * @brief Read every well-formed entry from a daemon registry file.
 *
 * Malformed lines are skipped silently so they cannot disturb the table
 * layout. Liveness is not consulted here; callers decide what to do with
 * each entry.
 *
 * @param reg_path Path to the registry file to read.
 * @param daemons Out-array filled with the entries found (up to MAX_DAEMONS).
 * @return Number of entries loaded.
 */
static int load_daemons(const char *reg_path, DaemonInfo *daemons)
{
	FILE	*fd;
	char	line[1024];
	int		count;

	count = 0;
	fd = ft_fopen(reg_path, "r");
	if (!fd)
		return (0);

	while (fgets(line, sizeof(line), fd) && count < MAX_DAEMONS)
	{
		char	name[64];
		char	timestamp[128];
		int		pid;

		if (sscanf(line, "%63s %d %127[^\n]", name, &pid, timestamp) != 3)
			continue;

		strncpy(daemons[count].name, name, sizeof(daemons[count].name) - 1);
		daemons[count].name[sizeof(daemons[count].name) - 1] = '\0';
		daemons[count].pid = pid;
		strncpy(daemons[count].timestamp, timestamp,
			sizeof(daemons[count].timestamp) - 1);
		daemons[count].timestamp[sizeof(daemons[count].timestamp) - 1] = '\0';
		++count;
	}
	fclose(fd);

	return (count);
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
	char		reg_path[PATH_MAX];
	char		proc_path[128];
	DaemonInfo	daemons[MAX_DAEMONS];
	int			count;
	int			active;
	int			width;

	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/daemons.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	count = load_daemons(reg_path, daemons);
	/* Names are measured before anything prints so the pid column starts at
	 * the same offset on every row, however long the longest name is. */
	width = daemon_name_width(daemons[0].name, count, sizeof(DaemonInfo));

	daemon_table_header("", width);
	printf("\n");

	active = 0;
	for (int i = 0; i < count; ++i)
	{
		int	alive;

		snprintf(proc_path, sizeof(proc_path), "/proc/%d", daemons[i].pid);
		alive = (access(proc_path, F_OK) == 0);
		if (alive)
			++active;

		daemon_table_row("", width, daemons[i].name, daemons[i].pid, alive, daemons[i].timestamp);
	}
	printf("\n  %sactive: %d%s\n", CL_DIM, active, CL_RESET);
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
	char		reg_path[PATH_MAX];
	DaemonInfo	daemons[MAX_DAEMONS];
	int			count;
	int			width;

	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/cematary.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	count = load_daemons(reg_path, daemons);
	width = daemon_name_width(daemons[0].name, count, sizeof(DaemonInfo));

	printf("\n  %s%sgraveyard%s\n\n", CL_BOLD, CL_DIM, CL_RESET);
	daemon_table_header("", width);
	printf("\n");

	for (int i = 0; i < count; ++i)
		daemon_table_row("", width, daemons[i].name, daemons[i].pid, 0, daemons[i].timestamp);

	printf("\n  %sburied: %d%s\n\n", CL_DIM, count, CL_RESET);
}
