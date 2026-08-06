#include "tetrisctl.h"

// Static Functions
static int	check_selection(const t_ctl *ctl, const char *only);
static int	skip(const t_managed *d, const char *only);
static int	start_one(const t_ctl *ctl, const t_managed *d);
static int	stop_one(const t_ctl *ctl, const t_managed *d);
static void	show(const t_managed *d, t_managed_state state, pid_t pid);

/**
 * @brief Starts the roster in launch order, or one named daemon.
 *
 * Launch order is the logger first, so that everything the game server says
 * about its own boot has somewhere to go. A daemon already running is
 * reported and stepped over rather than treated as a failure - `start` is a
 * statement about the end state.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon is up, -1 otherwise.
 */
int	start_command(const t_ctl *ctl, const char *only)
{
	int	rc;
	int	i;

	if (check_selection(ctl, only) != 0)
		return (-1);
	rc = 0;
	i = 0;
	while (i < ctl->count)
	{
		if (!skip(&ctl->daemons[i], only))
		{
			if (start_one(ctl, &ctl->daemons[i]) != 0)
				rc = -1;
		}
		i++;
	}
	return (rc);
}

/**
 * @brief Prints what each daemon is doing, one line each.
 *
 * Exits non-zero only when a question could not be answered - an unknown name
 * or a pidfile that cannot be inspected. A daemon that is simply stopped is a
 * successful report, not a failure, or `status` could never be used to ask
 * about a stack that is deliberately down.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon could be reported on, -1 otherwise.
 */
int	status_command(const t_ctl *ctl, const char *only)
{
	t_managed_state	state;
	pid_t	pid;
	int		rc;
	int		i;

	if (check_selection(ctl, only) != 0)
		return (-1);
	rc = 0;
	i = 0;
	while (i < ctl->count)
	{
		if (!skip(&ctl->daemons[i], only))
		{
			state = managed_state(&ctl->daemons[i], &pid);
			show(&ctl->daemons[i], state, pid);
			if (state == MANAGED_UNKNOWN)
				rc = -1;
		}
		i++;
	}
	return (rc);
}

/**
 * @brief Stops the roster in the reverse of launch order, or one daemon.
 *
 * The reversal is the point. Stopping the logger first would push tetrisd's
 * entire shutdown - every disconnect, every room torn down - into its error
 * file instead of the log, which is exactly the record an operator reaches
 * for after a shutdown goes wrong (docs/adr/0007).
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon is down, -1 otherwise.
 */
int	stop_command(const t_ctl *ctl, const char *only)
{
	int	rc;
	int	i;

	if (check_selection(ctl, only) != 0)
		return (-1);
	rc = 0;
	i = ctl->count - 1;
	while (i >= 0)
	{
		if (!skip(&ctl->daemons[i], only))
		{
			if (stop_one(ctl, &ctl->daemons[i]) != 0)
				rc = -1;
		}
		i--;
	}
	return (rc);
}

/**
 * @brief Stops everything selected, then starts it again.
 *
 * The stop has to complete first, and it does: managed_stop blocks on the pidfile
 * lock, so the restart cannot race the old instance for the socket or the
 * pidfile it is still holding.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when everything selected came back up, -1 otherwise.
 */
int	restart_command(const t_ctl *ctl, const char *only)
{
	if (stop_command(ctl, only) != 0)
		return (-1);
	return (start_command(ctl, only));
}

/**
 * @brief Checks the roster is usable and that a named daemon is in it.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when the command can proceed, -1 after reporting why.
 */
static int	check_selection(const t_ctl *ctl, const char *only)
{
	if (ctl == NULL || ctl->count == 0)
	{
		fprintf(stderr, "%s: no daemons configured (%s)\n",
			TETRISCTL_COMPONENT_NAME, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS");
		return (-1);
	}
	if (only != NULL && ctl_find_daemon(ctl, only) == NULL)
	{
		fprintf(stderr, "%s: %s is not in %s\n",
			TETRISCTL_COMPONENT_NAME, only, TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS");
		return (-1);
	}
	return (0);
}

/**
 * @brief Reports whether a daemon falls outside a single-daemon selection.
 *
 * @param d Daemon being considered.
 * @param only One daemon's name, or NULL for all of them.
 * @return 1 to skip this daemon, 0 to act on it.
 */
static int	skip(const t_managed *d, const char *only)
{
	return (only != NULL && strcmp(d->name, only) != 0);
}

/**
 * @brief Starts one daemon and says what happened.
 *
 * @param ctl Roster, for the rc path handed on to the daemon.
 * @param d Daemon to start.
 * @return 0 when it is up, -1 otherwise.
 */
static int	start_one(const t_ctl *ctl, const t_managed *d)
{
	pid_t	pid;
	t_managed_state	state;

	state = managed_state(d, &pid);
	if (state == MANAGED_RUNNING)
	{
		printf("%-12s already running   pid %d\n", d->name, (int)pid);
		return (0);
	}
	if (state == MANAGED_UNKNOWN)
	{
		fprintf(stderr, "%s: cannot read %s: %s\n",
			TETRISCTL_COMPONENT_NAME, d->pid_path, strerror(errno));
		return (-1);
	}
	if (managed_start(d, ctl->rc_path) != 0)
	{
		fprintf(stderr, "%s: %s did not start\n", TETRISCTL_COMPONENT_NAME, d->name);
		return (-1);
	}
	managed_state(d, &pid);
	printf("%-12s started           pid %d\n", d->name, (int)pid);
	return (0);
}

/**
 * @brief Stops one daemon and says what happened.
 *
 * @param ctl Roster, for how long to wait for teardown.
 * @param d Daemon to stop.
 * @return 0 when it is down, -1 otherwise.
 */
static int	stop_one(const t_ctl *ctl, const t_managed *d)
{
	pid_t	pid;

	if (managed_state(d, &pid) == MANAGED_STOPPED)
	{
		printf("%-12s not running\n", d->name);
		return (0);
	}
	if (managed_stop(d, ctl->stop_ms) != 0)
	{
		fprintf(stderr, "%s: %s did not stop: %s\n",
			TETRISCTL_COMPONENT_NAME, d->name, strerror(errno));
		return (-1);
	}
	printf("%-12s stopped           pid %d\n", d->name, (int)pid);
	return (0);
}

/**
 * @brief Prints one status line.
 *
 * @param d Daemon being reported.
 * @param state What managed_state said.
 * @param pid The running pid, meaningful only when state is MANAGED_RUNNING.
 */
static void	show(const t_managed *d, t_managed_state state, pid_t pid)
{
	const char	*label;
	char		detail[32];

	label = "stopped";
	snprintf(detail, sizeof(detail), "-");
	if (state == MANAGED_RUNNING)
	{
		label = "running";
		snprintf(detail, sizeof(detail), "pid %d", (int)pid);
	}
	else if (state == MANAGED_UNKNOWN)
		label = "unknown";
	printf("%-12s %-9s %-12s %s\n", d->name, label, detail, d->pid_path);
}
