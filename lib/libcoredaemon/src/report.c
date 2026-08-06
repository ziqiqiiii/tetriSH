#include "coredaemon.h"

// Static Variables
static bool	g_colour = true;

// Static Functions
static const char	*paint(const char *escape);
static const char	*state_label(t_report_state state);
static const char	*state_colour(t_report_state state);
static const char	*notice_colour(t_notice_kind kind);

/**
 * @brief Turns the ANSI escapes in every daemon_report_* function on or off.
 *
 * On by default, for the operator at a terminal this was written for; off for
 * a redirect or a test, where an escape is noise in the middle of a name. A
 * switch rather than an isatty() check, so a caller that has already decided
 * is not overruled.
 *
 * @param on true to emit escapes, false to print plain text.
 */
void	daemon_report_colour(bool on)
{
	g_colour = on;
}

/**
 * @brief Width of the name column for a set of daemons.
 *
 * Sized to the longest name present so that one long entry cannot push the
 * pid column out of alignment, with a floor of DAEMON_NAME_COL_MIN.
 *
 * @param names First name to measure. Base pointer plus stride so a caller can
 *              measure a struct array in place - tetrisctl passes t_managed.
 * @param count Number of entries to measure; 0 gives the floor.
 * @param stride Byte distance between consecutive names.
 * @return Column width in characters.
 */
int	daemon_report_width(const char *names, int count, size_t stride)
{
	int		width;
	size_t	len;
	int		i;

	width = DAEMON_NAME_COL_MIN;
	if (names == NULL || stride == 0)
		return (width);
	i = 0;
	while (i < count)
	{
		len = strlen(names + (size_t)i * stride);
		if ((int)len > width)
			width = (int)len;
		i++;
	}
	return (width);
}

/**
 * @brief Prints the dimmed column labels and the rule underneath them.
 *
 * The rule is sized from detail_width because the last column holds a pidfile
 * path, longer than any fixed guess. The labels leave their final column
 * unpadded: trailing spaces are invisible on screen but real in a redirect.
 *
 * @param indent Leading text occupying the index column, "" for none.
 * @param name_width Width of the name column, from daemon_report_width().
 * @param detail_width Width of the trailing detail column, 0 for none.
 */
void	daemon_report_header(const char *indent, int name_width,
		int detail_width)
{
	int	rule;
	int	i;

	if (indent == NULL)
		indent = "";
	printf("  %s%s%-*s %-7s %-7s %-9s%s%s\n", paint(DAEMON_CL_DIM), indent,
		name_width, "name", "pid", "state", "uptime",
		detail_width > 0 ? " detail" : "", paint(DAEMON_CL_RESET));
	rule = (int)strlen(indent) + name_width + 1 + 7 + 1 + 7 + 1 + 9;
	if (detail_width > 0)
		rule += 1 + detail_width;
	printf("  %s", paint(DAEMON_CL_DIM));
	i = 0;
	while (i < rule)
	{
		printf("-");
		i++;
	}
	printf("%s\n", paint(DAEMON_CL_RESET));
}

/**
 * @brief Prints one daemon row: name, pid, state, uptime, detail.
 *
 * A stopped daemon has no pid worth printing - the pidfile may still hold the
 * one it died as - so its pid column is a dash rather than a number a reader
 * could mistake for something to signal. Its uptime is a dash for the same
 * reason: an age is a fact about a running process.
 *
 * @param indent Leading text occupying the index column, "" for none.
 * @param name_width Width of the name column, from daemon_report_width().
 * @param name Daemon name as configured.
 * @param state What the pidfile lock said.
 * @param pid The running pid; ignored unless state is DAEMON_REPORT_RUNNING.
 * @param uptime Seconds the daemon has been up, or negative when unknown.
 * @param detail Trailing free text such as the pidfile path, or NULL.
 */
void	daemon_report_row(const char *indent, int name_width, const char *name,
		t_report_state state, pid_t pid, long uptime, const char *detail)
{
	char	pid_text[32];
	char	up_text[32];

	if (indent == NULL)
		indent = "";
	if (state == DAEMON_REPORT_RUNNING)
		snprintf(pid_text, sizeof(pid_text), "%d", (int)pid);
	else
		snprintf(pid_text, sizeof(pid_text), "-");
	if (state == DAEMON_REPORT_RUNNING && uptime >= 0)
		daemon_report_uptime(uptime, up_text, sizeof(up_text));
	else
		snprintf(up_text, sizeof(up_text), "-");
	printf("  %s%s%s%-*s %s%-7s%s %s%-7s%s %s%-9s%s",
		paint(DAEMON_CL_DIM), indent, paint(DAEMON_CL_RESET),
		name_width, name,
		paint(DAEMON_CL_BLUE), pid_text, paint(DAEMON_CL_RESET),
		paint(state_colour(state)), state_label(state),
		paint(DAEMON_CL_RESET),
		paint(DAEMON_CL_DIM), up_text, paint(DAEMON_CL_RESET));
	if (detail != NULL && detail[0] != '\0')
		printf(" %s%s%s", paint(DAEMON_CL_DIM), detail,
			paint(DAEMON_CL_RESET));
	printf("\n");
}

/**
 * @brief Renders elapsed seconds as an age a person can read at a glance.
 *
 * H:MM:SS below a day, as dcheck prints it. Past that, days are split out:
 * the usual question is "did this restart recently", which "6d 6h" answers
 * and "150:00:00" does not.
 *
 * @param seconds Elapsed seconds; negative renders as "-".
 * @param out Destination buffer.
 * @param cap Size of out.
 */
void	daemon_report_uptime(long seconds, char *out, size_t cap)
{
	long	days;

	if (out == NULL || cap == 0)
		return ;
	if (seconds < 0)
	{
		snprintf(out, cap, "-");
		return ;
	}
	days = seconds / 86400;
	if (days > 0)
		snprintf(out, cap, "%ldd %ldh", days, (seconds % 86400) / 3600);
	else
		snprintf(out, cap, "%ld:%02ld:%02ld", seconds / 3600,
			(seconds % 3600) / 60, seconds % 60);
}

/**
 * @brief Prints the blank line that sets a run of notices apart.
 *
 * A whole run is bracketed rather than each notice, which is where this
 * departs from dspawn: dspawn only ever prints one line, so `tetrisctl start`
 * would otherwise gap between every daemon it launched.
 */
void	daemon_report_break(void)
{
	printf("\n");
}

/**
 * @brief Announces a single lifecycle event, in dspawn's one-line shape.
 *
 * The shell's "spawned <name> <pid>" notice, generalised over the verb and
 * over the direction, so teardown carries dkill's red. A pid of 0 prints no
 * pid, which is what a stop that found nothing running has to say.
 *
 * @param kind Which way the event went, which picks the colour.
 * @param verb What happened, for example "started" or "killed".
 * @param name Daemon the event concerns.
 * @param pid Pid to show, or 0 for none.
 */
void	daemon_report_notice(t_notice_kind kind, const char *verb,
		const char *name, pid_t pid)
{
	printf("  %s%-*s%s ", paint(notice_colour(kind)), DAEMON_VERB_COL_MIN,
		verb, paint(DAEMON_CL_RESET));
	if (pid > 0)
		printf("%-*s %s%d%s", DAEMON_NAME_COL_MIN, name,
			paint(DAEMON_CL_BLUE), (int)pid, paint(DAEMON_CL_RESET));
	else
		printf("%s", name);
	printf("\n");
}

/**
 * @brief Prints the dimmed tally that closes a table, as dcheck's does.
 *
 * @param label What is being counted, for example "running".
 * @param count How many.
 */
void	daemon_report_footer(const char *label, int count)
{
	printf("\n  %s%s: %d%s\n", paint(DAEMON_CL_DIM), label, count,
		paint(DAEMON_CL_RESET));
}

/**
 * @brief Reports a failure on stderr, in the same colours as the table.
 *
 * On stderr so that a status table being parsed keeps its failures out of the
 * stream being read. The component name is passed in because every daemon
 * links this library too, and the name a person needs is the one they typed.
 *
 * @param component Program reporting the failure, for example "tetrisctl".
 * @param subject What it concerns - a daemon name or a path.
 * @param reason Why it failed, typically strerror(errno).
 */
void	daemon_report_error(const char *component, const char *subject,
		const char *reason)
{
	fprintf(stderr, "%s%s%s: %s: %s\n", paint(DAEMON_CL_RED),
		component ? component : "", paint(DAEMON_CL_RESET),
		subject ? subject : "", reason ? reason : "");
}

/**
 * @brief Returns an escape when colour is on, and an empty string when not.
 *
 * Every escape goes through here, so the plain form is the coloured form with
 * the escapes emptied out - the two layouts stay identical by construction.
 *
 * @param escape The ANSI escape to emit.
 * @return The escape, or "".
 */
static const char	*paint(const char *escape)
{
	if (!g_colour)
		return ("");
	return (escape);
}

/**
 * @brief The word printed in a row's state column.
 *
 * @param state State being rendered.
 * @return A fixed string; never NULL.
 */
static const char	*state_label(t_report_state state)
{
	if (state == DAEMON_REPORT_RUNNING)
		return ("up");
	if (state == DAEMON_REPORT_UNKNOWN)
		return ("unknown");
	return ("down");
}

/**
 * @brief The colour a state is printed in.
 *
 * Unknown is yellow, not red: a pidfile that cannot be read is a question
 * this program failed to answer, not a daemon that is down.
 *
 * @param state State being rendered.
 * @return An ANSI escape; never NULL.
 */
static const char	*state_colour(t_report_state state)
{
	if (state == DAEMON_REPORT_RUNNING)
		return (DAEMON_CL_GREEN);
	if (state == DAEMON_REPORT_UNKNOWN)
		return (DAEMON_CL_YELLOW);
	return (DAEMON_CL_RED);
}

/**
 * @brief The colour a notice's verb is printed in.
 *
 * Red for teardown is dkill's choice, not an alarm - a daemon asked to stop
 * has not failed. It buys a restart whose two halves are told apart without
 * reading the verbs. Idle events, already in the asked-for state, are dim.
 *
 * @param kind Which way the event went.
 * @return An ANSI escape; never NULL.
 */
static const char	*notice_colour(t_notice_kind kind)
{
	if (kind == DAEMON_NOTICE_UP)
		return (DAEMON_CL_GREEN);
	if (kind == DAEMON_NOTICE_DOWN)
		return (DAEMON_CL_RED);
	return (DAEMON_CL_DIM);
}
