#include "system_program.h"

static void     daemon_register(const char *project_root, const char *name, char *out_name, size_t out_size);
static void     daemon_spawn_log(const char *project_root);
static void     daemon_work(const char *project_root);
static int      target_is_executable(const char *target);
static int      open_tty(void);
static void     report_spawned(int tty, const char *registered);

/**
 * @brief Entry point for the dspawn daemon launcher.
 *
 * Resolves the project root, ensures the daemon bookkeeping files exist,
 * daemonises the process, registers the new daemon, logs its startup, and
 * either execs a target program or enters the perpetual work loop.
 *
 * Usage:
 *   dspawn                    built-in work loop, registered as deamon_eskimo
 *   dspawn <name>             built-in work loop, registered as <name>
 *   dspawn <name> -- <cmd>…   exec <cmd>, registered as <name>
 *
 * A bare argument is always a name, never a program: names like "tetrisd"
 * have no binary yet, and treating them as one made dspawn's behaviour depend
 * on whether something of that name happened to exist on PATH. Running a real
 * program is opt-in via "--".
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return 0 on success, 127 when the target program cannot be found.
 */
int main(int argc, char **argv)
{
	char		*project_root;
	const char	*name;
	char		**target;
	char		registered[64];
	int			tty;
	int			ready;

	project_root = resolve_project_root();
	ensure_daemon_files(project_root);

	/* The first argument is the registry name. Everything after a "--"
	 * separator is a program to exec; without it the daemon runs the built-in
	 * work loop, so a name is never mistaken for a binary. */
	name = (argc > 1) ? argv[1] : "deamon_eskimo";
	target = NULL;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--") == 0)
		{
			/* A leading "--" leaves the default name in place. */
			if (i == 1)
				name = "deamon_eskimo";
			target = (i + 1 < argc) ? &argv[i + 1] : NULL;
			break ;
		}
	}

	/* Validate the target before daemonising or touching the registry: a
	 * failed execvp would otherwise leave a registry entry pointing at a
	 * process that has already exited, which dcheck reports as "down".
	 * Checking here also lets the error reach the user's terminal, since
	 * daemon_spawn() redirects stdout/stderr to /dev/null. */
	if (target && !target_is_executable(target[0]))
	{
		fprintf(stderr, "dspawn: %s: command not found\n", target[0]);
		free(project_root);
		return (127);
	}

	/* Opened before daemonisation and carried across it: setsid() drops the
	 * controlling terminal, so the daemon cannot reach /dev/tty on its own. */
	tty = open_tty();
	daemon_spawn(tty, &ready);
	daemon_register(project_root, name, registered, sizeof(registered));
	report_spawned(tty, registered);
	/* Notice is on the terminal; let the originating process exit so the
	 * shell prompt is drawn after it rather than racing against it. */
	daemon_ready(ready);
	daemon_spawn_log(project_root);
	daemon_log(project_root, "start of new deamon before deamon work");
	if (target)
	{
		execvp(target[0], target);
		/* Only reachable if the target vanished or became non-executable
		 * between the check above and here; the registry entry is left for
		 * dcheck to show as "down". */
		daemon_log(project_root, "execvp of target daemon failed");
		free(project_root);
		return (1);
	}
	daemon_work(project_root);
	free(project_root);
	return (0);
}

/**
 * @brief Report whether a target command can be executed.
 *
 * Mirrors how execvp() resolves its first argument: a name containing '/' is
 * tested directly, otherwise each PATH component is tried in turn. Uses
 * access(X_OK) so the check matches the caller's effective permissions.
 *
 * @param target Command name or path to test.
 * @return Non-zero when the target resolves to an executable file.
 */
static int target_is_executable(const char *target)
{
	char		candidate[PATH_MAX];
	const char	*path;
	const char	*start;
	const char	*colon;
	size_t		len;

	if (!target || !*target)
		return (0);
	if (strchr(target, '/'))
		return (access(target, X_OK) == 0);

	path = getenv("PATH");
	if (!path || !*path)
		path = "/usr/local/bin:/usr/bin:/bin";

	start = path;
	while (1)
	{
		colon = strchr(start, ':');
		len = colon ? (size_t)(colon - start) : strlen(start);
		/* An empty PATH component means the current directory. */
		if (len == 0)
			snprintf(candidate, sizeof(candidate), "./%s", target);
		else
			snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)len,
				start, target);
		if (access(candidate, X_OK) == 0)
			return (1);
		if (!colon)
			return (0);
		start = colon + 1;
	}
}

/**
 * @brief Append a startup record for this daemon to tmp/dspawn.log.
 *
 * Opens (creating if needed) "<project_root>/tmp/dspawn.log" and writes a
 * timestamped line containing the daemon's PID.
 *
 * @param project_root Resolved project root containing the tmp directory.
 */
static void daemon_spawn_log(const char *project_root)
{
	char	log_path[PATH_MAX];
	int 	fd;
	time_t	now;

	strncpy(log_path, project_root, sizeof(log_path) - 1);
	strncat(log_path, "/tmp/dspawn.log", sizeof(log_path) - strlen(log_path) - 1);

	fd = ft_open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	now = time(NULL);
	dprintf(fd, "%sStarted dspawn daemon [%d].\n", ctime(&now), getpid());
	
	close(fd);
}

/**
 * @brief Register the running daemon in tmp/daemons.reg.
 *
 * Scans the registry for existing entries sharing the same base name and,
 * if any exist, suffixes the name with ".<count>" to keep it unique. Appends
 * a line of "<name> <pid> <timestamp>" to the registry.
 *
 * @param project_root Resolved project root containing the tmp directory.
 * @param name Base name to register the daemon under.
 * @param out_name Buffer receiving the final (possibly suffixed) name.
 * @param out_size Size of the out_name buffer.
 */
static void daemon_register(const char *project_root, const char *name,
		char *out_name, size_t out_size)
{
	char	reg_path[PATH_MAX];
	FILE	*fp ;
	int		count = 0;
	char	modified_name[64];
	int		fd;
	time_t	now;
	char	*ts;

	strncpy(reg_path, project_root, sizeof(reg_path) - 1);
	strncat(reg_path, "/tmp/daemons.reg", sizeof(reg_path) - strlen(reg_path) - 1);

	// check for daemons created with the same name and add a number to
	// differentiate it
	fp = ft_fopen(reg_path, "r");

	if (fp)
	{
		char line[1024];
		while (fgets(line, sizeof(line), fp))
		{
			if (strncmp(line, name, strlen(name)) == 0 && (line[strlen(name)] == ' ' || line[strlen(name)] == '.'))
				count++;
		}
		fclose(fp);
	}

	if (count == 0)
		snprintf(modified_name, sizeof(modified_name), "%s", name);
	else
		snprintf(modified_name, sizeof(modified_name), "%s.%d", name, count);

	// add entry
	fd = ft_open(reg_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	now = time(NULL);
	ts = ctime(&now);
	if (ts)
		ts[strcspn(ts, "\n")] = 0;
	dprintf(fd, "%s %d %s\n", modified_name, getpid(), ts);
	close(fd);

	if (out_name && out_size)
		snprintf(out_name, out_size, "%s", modified_name);
}

/**
 * @brief Open the controlling terminal for the spawn notice.
 *
 * Must be called before daemon_spawn(), while the terminal is still attached.
 * The descriptor is marked close-on-exec so it is not inherited by a target
 * daemon started via execvp(). Returns -1 when there is no terminal, which
 * simply makes the notice silent.
 *
 * @return An open descriptor for /dev/tty, or -1.
 */
static int open_tty(void)
{
	int	fd;

	fd = open("/dev/tty", O_WRONLY);
	if (fd < 0)
		return (-1);
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return (fd);
}

/**
 * @brief Announce a successful spawn on the controlling terminal.
 *
 * Writes to the descriptor captured by open_tty() before daemonisation. The
 * terminal cannot be reopened at this point: daemon_spawn() calls setsid(),
 * which drops the controlling terminal, so a fresh open("/dev/tty") would
 * fail. Staying silent without a descriptor keeps dspawn usable from scripts
 * and cron.
 *
 * @param tty Descriptor from open_tty(), or -1 when there is no terminal.
 * @param registered Final registry name of the daemon just spawned.
 */
static void report_spawned(int tty, const char *registered)
{
	if (tty < 0)
		return ;
	dprintf(tty, "\n  %sspawned%s %-14s %s%d%s\n\n", CL_GREEN, CL_RESET,
		registered, CL_BLUE, getpid(), CL_RESET);
	close(tty);
}

/**
 * @brief Perpetual daemon work loop.
 *
 * Logs a heartbeat message once per cycle and sleeps 10 seconds between
 * cycles. Never returns; the daemon runs until it is killed.
 *
 * @param project_root Resolved project root used for logging.
 */
static void daemon_work(const char *project_root)
{
	while (1)
	{
		daemon_log(project_root, "deamon one work cycle");
		sleep(10);
	}
}
