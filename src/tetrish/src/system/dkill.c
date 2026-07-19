#include "system_program.h"

static void     dkill(const char *project_root);
static void     add_to_graveyard(const char *project_root, DaemonInfo rip);
static int      load_active_daemons(const char *reg_path, DaemonInfo *daemons);
static void     print_active_daemons(DaemonInfo *daemons, int count);
static void     kill_one(const char *project_root, DaemonInfo daemon);
static void     kill_all(const char *project_root, const char *reg_path, DaemonInfo *daemons, int count);

/**
 * @brief Entry point for the dkill daemon termination utility.
 *
 * Resolves the project root, ensures the daemon bookkeeping files exist,
 * then runs the interactive kill prompt.
 *
 * @param argc Number of command-line arguments (unused).
 * @param argv Array of command-line arguments (unused).
 * @return 0 on success.
 */
int main(int argc, char **argv)
{
	(void)  argc;
	(void)  argv;

	char *project_root = resolve_project_root();
	ensure_daemon_files(project_root);
	dkill(project_root);
	free(project_root);

	return 0;
}

/**
 * @brief Record a killed daemon in the graveyard registry.
 *
 * Appends an entry to "<project_root>/tmp/cematary.reg", suffixing the name
 * with ".<count>" if other entries share the same base name so each record
 * stays unique.
 *
 * @param project_root Resolved project root containing the tmp directory.
 * @param rip Info for the daemon being buried.
 */
static void add_to_graveyard(const char *project_root, DaemonInfo rip)
{
	char	reg_path[PATH_MAX];
	char	modified_name[128];
	char 	*ts;
	FILE	*fp;
	time_t	now;
	int		count = 0;
	int		fd;

	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	// just like in pet cematary, try not to respawn it lol
	strncat(reg_path, "/tmp/cematary.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	fp = ft_fopen(reg_path, "r");

	if (fp) {
		char line[1024];
		while (fgets(line, sizeof(line), fp)) {
				if (strncmp(line, rip.name, strlen(rip.name)) == 0 &&
					(line[strlen(rip.name)] == ' ' ||
						line[strlen(rip.name)] == '.'))
						count++;
		}
		fclose(fp);
	}

	printf("adding to graveyard\n");

	if (count == 0)
			snprintf(modified_name, sizeof(modified_name), "%s", rip.name);
	else
			snprintf(modified_name, sizeof(modified_name), "%s.%d",
						rip.name, count);
	// add entry
	fd = ft_open(reg_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	now = time(NULL);
	ts = ctime(&now);
	if (ts)
		ts[strcspn(ts, "\n")] = 0;
	dprintf(fd, "%s %d %s\n", modified_name, getpid(), ts);
	close(fd);
}

/**
 * @brief Load daemons that are still running from the registry.
 *
 * Parses each entry in reg_path and, for entries whose /proc/<pid> still
 * exists, stores their name, pid and timestamp into the daemons array (up
 * to MAX_DAEMONS).
 *
 * @param reg_path Path to the daemons registry file.
 * @param daemons Out-array filled with the active daemons found.
 * @return Number of active daemons loaded.
 */
static int load_active_daemons(const char *reg_path, DaemonInfo *daemons)
{
	FILE	*fd;
	char	line[1024];
	int		count = 0;

	// check for daemons created with the same name and add a number to
	// differentiate it
	fd = ft_fopen(reg_path, "r");

	while (fgets(line, sizeof(line), fd) && count < MAX_DAEMONS) {
			char	name[64];
			char	timestamp[128];
			int		pid;
			char	proc_path[128];

			if (sscanf(line, "%63s %d %127[^\n]", name, &pid, timestamp) != 3) {
				ft_putstr_fd(line, 1);
				continue;
			}

			snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
			// check all the pids just in case
			if (access(proc_path, F_OK) == 0) {
					strncpy(daemons[count].name, name, sizeof(daemons[count].name));
					daemons[count].pid = pid;
					strncpy(daemons[count].timestamp, timestamp, sizeof(daemons[count].timestamp));
					++count;
			}
	}
	fclose(fd);

        return count;
}

/**
 * @brief Print a numbered list of the active daemons.
 *
 * @param daemons Array of active daemons.
 * @param count Number of entries in the daemons array.
 */
static void print_active_daemons(DaemonInfo *daemons, int count)
{
	printf("Active daemons: \n");

	for (int i = 0; i < count; ++i)
	{
		printf("    %2d) %-12s PID: %-6d Started %s\n", i + 1,
				daemons[i].name, daemons[i].pid, daemons[i].timestamp);
	}

}

/**
 * @brief Send SIGTERM to a single daemon and bury it on success.
 *
 * On a successful kill the daemon is added to the graveyard registry;
 * otherwise the kill error is reported via perror.
 *
 * @param project_root Resolved project root (for the graveyard registry).
 * @param daemon Info for the daemon to terminate.
 */
static void kill_one(const char *project_root, DaemonInfo daemon)
{
	if (kill(daemon.pid, SIGTERM) == 0)
	{
		printf("Killed %-12s (PID %d)\n", daemon.name, daemon.pid);
		add_to_graveyard(project_root, daemon);
	}
	else
	{
		perror("kill");
	}
}

/**
 * @brief Kill every active daemon and clear the registry.
 *
 * Terminates each daemon in turn via kill_one(), then truncates the
 * daemons registry file.
 *
 * @param project_root Resolved project root (for the graveyard registry).
 * @param reg_path Path to the daemons registry to empty afterwards.
 * @param daemons Array of active daemons to kill.
 * @param count Number of entries in the daemons array.
 */
static void kill_all(const char *project_root, const char *reg_path, DaemonInfo *daemons, int count)
{
	for (int i = 0; i < count; ++i)
		kill_one(project_root, daemons[i]);

	// empty the register after killing all of them
	fclose(ft_fopen(reg_path, "w"));
}

/**
 * @brief Interactive prompt to kill one or all active daemons.
 *
 * Loads the active daemons, lists them, and prompts the user to choose one
 * by number, '0' to cancel, or 'a' to kill them all.
 *
 * @param project_root Resolved project root containing the tmp directory.
 */
static void dkill(const char *project_root)
{
	char		reg_path[PATH_MAX];
	char		input[8];
	DaemonInfo	daemons[MAX_DAEMONS];
	int			count;
	int			idx;

	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/daemons.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	count = load_active_daemons(reg_path, daemons);
	if (count == 0) {
			printf("No active daemons found\n");
			return;
	}

	print_active_daemons(daemons, count);
	printf("Enter number to kill [1-%d], 0 to cancel, or 'a' to kill all daemons: ", count);

	if (!fgets(input, sizeof(input), stdin))
			input[0] = '0';
	if (tolower(input[0]) == 'a') {
			kill_all(project_root, reg_path, daemons, count);
	} else {
			idx = atoi(input) - 1;
			if (idx >= 0 && idx < count)
					kill_one(project_root, daemons[idx]);
			else
					printf("Invalid input \n");
	}
}
