#include "tetrisctl.h"

// Static Functions
static int	check_selection(const t_ctl *ctl, const char *only);
static int	start_run(const t_ctl *ctl, const char *only);
static int	stop_run(const t_ctl *ctl, const char *only);
static int	skip(const t_managed *d, const char *only);
static int	start_one(const t_ctl *ctl, const t_managed *d);
static int	stop_one(const t_ctl *ctl, const t_managed *d);
static void	show(const t_managed *d, t_managed_state state, pid_t pid, int name_width);
static t_report_state	report_state(t_managed_state state);
static int	detail_width(const t_ctl *ctl, const char *only);

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

	if (check_selection(ctl, only) != 0)
		return (-1);
	daemon_report_break();
	rc = start_run(ctl, only);
	daemon_report_break();
	return (rc);
}

/**
 * @brief Starts the selection without bracketing the run in blank lines.
 *
 * Split out so that restart_command can run a stop and a start back to back
 * inside one pair of breaks. Bracketing each half instead would put two blank
 * lines through the middle of a single restart.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon is up, -1 otherwise.
 */
static int	start_run(const t_ctl *ctl, const char *only)
{
	int	rc;
	int	i;

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
	int		width;
	int		running;
	int		rc;
	int		i;

	if (check_selection(ctl, only) != 0)
		return (-1);
	/* Sized before anything prints, so every row starts at the same offset
	 * however long the longest daemon name and pidfile path turn out to be. */
	width = daemon_report_width(ctl->daemons[0].name, ctl->count, sizeof(ctl->daemons[0]));
	daemon_report_header("", width, detail_width(ctl, only));
	rc = 0;
	running = 0;
	i = 0;
	while (i < ctl->count)
	{
		if (!skip(&ctl->daemons[i], only))
		{
			state = managed_state(&ctl->daemons[i], &pid);
			show(&ctl->daemons[i], state, pid, width);
			if (state == MANAGED_RUNNING)
				running++;
			if (state == MANAGED_UNKNOWN)
				rc = -1;
		}
		i++;
	}
	daemon_report_footer("running", running);
	/*
	** Health comes after the pidfile lines, never instead of them. The lock is
	** the question that needs no running server, so it is asked first and its
	** answer stands however the second question goes - a stopped daemon is a
	** successful report, and an unreachable channel does not retract it.
	**
	** It is asked only when something is actually up, since "not running" has
	** already been said and saying it twice in different words is worse than
	** saying it once.
	*/
	/*
	** Flushed first because the rows go to stdout and any failure below goes
	** to stderr. Piped, stdout is block-buffered and stderr is not, so without
	** this an unreachable-channel message lands above the table it is meant to
	** follow.
	*/
	fflush(stdout);
	if (running > 0 && health_report(ctl, only) != 0)
		rc = -1;
	return (rc);
}

/**
 * @brief Stops the roster in the reverse of launch order, or one daemon.
 *
 * The reversal is the point. Stopping the logger first would push tetrisd's
 * entire shutdown - every disconnect, every room torn down - into its error
 * file instead of the log, which is exactly the record an operator reaches
 * for after a shutdown goes wrong.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon is down, -1 otherwise.
 */
int	stop_command(const t_ctl *ctl, const char *only)
{
	int	rc;

	if (check_selection(ctl, only) != 0)
		return (-1);
	daemon_report_break();
	rc = stop_run(ctl, only);
	daemon_report_break();
	return (rc);
}

/**
 * @brief Stops the selection without bracketing the run in blank lines.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 when every selected daemon is down, -1 otherwise.
 */
static int	stop_run(const t_ctl *ctl, const char *only)
{
	int	rc;
	int	i;

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
	int	rc;

	if (check_selection(ctl, only) != 0)
		return (-1);
	/* One pair of breaks around both halves, so a restart reads as the single
	 * operation it is rather than as a stop and a start that happened to run
	 * together. */
	daemon_report_break();
	rc = stop_run(ctl, only);
	if (rc == 0)
		rc = start_run(ctl, only);
	daemon_report_break();
	return (rc);
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
		daemon_report_error(TETRISCTL_COMPONENT_NAME,
			TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS", "no daemons configured");
		return (-1);
	}
	if (only != NULL && ctl_find_daemon(ctl, only) == NULL)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, only,
			"is not in " TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS");
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
		daemon_report_notice(DAEMON_NOTICE_IDLE, "running", d->name, pid);
		return (0);
	}
	if (state == MANAGED_UNKNOWN)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, d->pid_path,
			strerror(errno));
		return (-1);
	}
	if (managed_start(d, ctl->rc_path) != 0)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, d->name,
			"did not start");
		return (-1);
	}
	managed_state(d, &pid);
	daemon_report_notice(DAEMON_NOTICE_UP, "started", d->name, pid);
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
		daemon_report_notice(DAEMON_NOTICE_IDLE, "not running", d->name, 0);
		return (0);
	}
	if (managed_stop(d, ctl->stop_ms) != 0)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, d->name,
			strerror(errno));
		return (-1);
	}
	daemon_report_notice(DAEMON_NOTICE_DOWN, "killed", d->name, pid);
	return (0);
}

/**
 * @brief Prints one status row.
 *
 * @param d Daemon being reported.
 * @param state What managed_state said.
 * @param pid The running pid, meaningful only when state is MANAGED_RUNNING.
 * @param name_width Width of the name column, shared by every row.
 */
static void	show(const t_managed *d, t_managed_state state, pid_t pid,
		int name_width)
{
	long	uptime;

	/* Asked only of a daemon that is up, and a failure to answer is not one
	 * worth reporting: the row still has everything else to say, and the
	 * column falls back to a dash. */
	if (state != MANAGED_RUNNING || daemon_pid_uptime(pid, &uptime) != 0)
		uptime = -1;
	daemon_report_row("", name_width, d->name, report_state(state), pid,
		uptime, d->pid_path);
}

/**
 * @brief Translates a lifecycle state into the one the table renders.
 *
 * The two enums are kept apart on purpose. This one is about what tetrisctl
 * may do next - start it, signal it, give up - and the other is about what a
 * row says; letting the renderer's vocabulary into managed_state would make
 * the lifecycle answer to the table rather than the other way round.
 *
 * @param state What managed_state said.
 * @return The matching report state.
 */
static t_report_state	report_state(t_managed_state state)
{
	if (state == MANAGED_RUNNING)
		return (DAEMON_REPORT_RUNNING);
	if (state == MANAGED_UNKNOWN)
		return (DAEMON_REPORT_UNKNOWN);
	return (DAEMON_REPORT_STOPPED);
}

/**
 * @brief Widest pidfile path in the selection, for sizing the header rule.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for all of them.
 * @return Width in characters, 0 when nothing is selected.
 */
static int	detail_width(const t_ctl *ctl, const char *only)
{
	int	width;
	int	len;
	int	i;

	width = 0;
	i = 0;
	while (i < ctl->count)
	{
		if (!skip(&ctl->daemons[i], only))
		{
			len = (int)strlen(ctl->daemons[i].pid_path);
			if (len > width)
				width = len;
		}
		i++;
	}
	return (width);
}
