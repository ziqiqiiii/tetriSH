/* ************************************************************************** */
/*                                                                            */
/*   test_report.c - the table tetrisctl prints                               */
/*                                                                            */
/*   What is worth asserting about output is not which escape came out but    */
/*   that the columns line up and that the plain form is the coloured form    */
/*   with the escapes removed. So most cases here capture stdout, strip the   */
/*   escapes, and compare the result against the same call made with colour   */
/*   already off - which catches a %-*s that lost its width in one form and   */
/*   not the other, the way a golden string never would.                      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

/*
** The one row every alignment case renders, so that the coloured and plain
** renderings under test are the same call with only the switch changed.
*/
// Static Variables
static const char	*g_row_name = "tetrislogd";
static const int	g_row_width = 14;

// Static Functions
static void	test_width_floors_at_the_minimum(void);
static void	test_width_grows_to_the_longest_name(void);
static void	test_width_strides_over_a_struct_array(void);
static void	test_colour_off_is_the_coloured_form_stripped(void);
static void	test_header_and_row_align_on_the_same_columns(void);
static void	test_running_shows_its_pid_and_stopped_shows_none(void);
static void	test_unknown_is_neither_up_nor_down(void);
static void	test_notice_omits_a_pid_it_was_not_given(void);
static void	test_null_text_does_not_crash_a_row(void);
static void	test_uptime_renders_hours_below_a_day(void);
static void	test_uptime_splits_days_out_above_one(void);
static void	test_only_a_running_daemon_shows_an_uptime(void);
static void	test_uptime_of_this_process_is_recent(void);
static void	test_teardown_and_startup_notices_differ_in_colour(void);
static void	render_notice_killed(void);

static void	capture(void (*render)(void), char *out, size_t cap);
static void	strip_escapes(const char *in, char *out, size_t cap);
static void	render_row_coloured(void);
static void	render_row_plain(void);
static void	render_header(void);
static void	render_running(void);
static void	render_stopped(void);
static void	render_unknown(void);
static void	render_notice_with_pid(void);
static void	render_notice_without_pid(void);
static void	render_null_row(void);

int	main(void)
{
	test_width_floors_at_the_minimum();
	test_width_grows_to_the_longest_name();
	test_width_strides_over_a_struct_array();
	test_colour_off_is_the_coloured_form_stripped();
	test_header_and_row_align_on_the_same_columns();
	test_running_shows_its_pid_and_stopped_shows_none();
	test_unknown_is_neither_up_nor_down();
	test_notice_omits_a_pid_it_was_not_given();
	test_null_text_does_not_crash_a_row();
	test_uptime_renders_hours_below_a_day();
	test_uptime_splits_days_out_above_one();
	test_only_a_running_daemon_shows_an_uptime();
	test_uptime_of_this_process_is_recent();
	test_teardown_and_startup_notices_differ_in_colour();
	return (0);
}

/*
** The contrast is the feature: a restart prints both halves in one run, and
** an operator should tell them apart without reading the verbs. Asserting on
** the escapes is right here precisely because the colour is the behaviour,
** not decoration over it.
*/
static void	test_teardown_and_startup_notices_differ_in_colour(void)
{
	char	up[512];
	char	down[512];

	capture(render_notice_with_pid, up, sizeof(up));
	capture(render_notice_killed, down, sizeof(down));
	assert(strstr(up, DAEMON_CL_GREEN) != NULL);
	assert(strstr(up, DAEMON_CL_RED) == NULL);
	assert(strstr(down, DAEMON_CL_RED) != NULL);
	assert(strstr(down, DAEMON_CL_GREEN) == NULL);
	printf("PASS test_teardown_and_startup_notices_differ_in_colour\n");
}

static void	render_notice_killed(void)
{
	daemon_report_notice(DAEMON_NOTICE_DOWN, "killed", g_row_name, 4242);
}

static void	test_uptime_renders_hours_below_a_day(void)
{
	char	out[32];

	daemon_report_uptime(0, out, sizeof(out));
	assert(strcmp(out, "0:00:00") == 0);
	daemon_report_uptime(3661, out, sizeof(out));
	assert(strcmp(out, "1:01:01") == 0);
	daemon_report_uptime(86399, out, sizeof(out));
	assert(strcmp(out, "23:59:59") == 0);
	printf("PASS test_uptime_renders_hours_below_a_day\n");
}

/*
** Past a day the hours field alone stops answering "did this restart
** recently" - 150:00:00 takes arithmetic to read, 6d 6h does not.
*/
static void	test_uptime_splits_days_out_above_one(void)
{
	char	out[32];

	daemon_report_uptime(86400, out, sizeof(out));
	assert(strcmp(out, "1d 0h") == 0);
	daemon_report_uptime(540000, out, sizeof(out));
	assert(strcmp(out, "6d 6h") == 0);
	printf("PASS test_uptime_splits_days_out_above_one\n");
}

/*
** An age is a fact about a running process, so the two states that have no
** running process must not print one - not even when handed a plausible
** number, which is exactly what a caller reusing a stale variable would do.
*/
static void	test_only_a_running_daemon_shows_an_uptime(void)
{
	char	up[512];
	char	down[512];
	char	unknown[512];

	daemon_report_colour(false);
	capture(render_running, up, sizeof(up));
	capture(render_stopped, down, sizeof(down));
	capture(render_unknown, unknown, sizeof(unknown));
	assert(strstr(up, "1:01:01") != NULL);
	assert(strstr(down, "1:01:01") == NULL);
	assert(strstr(unknown, "1:01:01") == NULL);
	daemon_report_colour(true);
	printf("PASS test_only_a_running_daemon_shows_an_uptime\n");
}

/*
** The library reads /proc for this, so the one process whose age is known
** independently is this one: it was started moments ago by the runner.
*/
static void	test_uptime_of_this_process_is_recent(void)
{
	long	seconds;

	assert(daemon_pid_uptime(getpid(), &seconds) == 0);
	assert(seconds >= 0 && seconds < 300);
	assert(daemon_pid_uptime(0, &seconds) == -1 && errno == EINVAL);
	assert(daemon_pid_uptime(getpid(), NULL) == -1 && errno == EINVAL);
	printf("PASS test_uptime_of_this_process_is_recent\n");
}

static void	test_width_floors_at_the_minimum(void)
{
	const char	*names = "td";

	assert(daemon_report_width(names, 1, 3) == DAEMON_NAME_COL_MIN);
	assert(daemon_report_width(names, 0, 3) == DAEMON_NAME_COL_MIN);
	assert(daemon_report_width(NULL, 4, 3) == DAEMON_NAME_COL_MIN);
	printf("PASS test_width_floors_at_the_minimum\n");
}

static void	test_width_grows_to_the_longest_name(void)
{
	char	names[2][32];

	snprintf(names[0], sizeof(names[0]), "tetrisd");
	snprintf(names[1], sizeof(names[1]), "a_very_long_daemon_name_indeed");
	assert(daemon_report_width(names[0], 2, sizeof(names[0]))
		== (int)strlen(names[1]));
	printf("PASS test_width_grows_to_the_longest_name\n");
}

/*
** The stride form is the reason this function takes a base pointer at all:
** tetrisctl hands it a t_managed array and never copies the names out.
*/
static void	test_width_strides_over_a_struct_array(void)
{
	struct s_row
	{
		char	name[24];
		int		pid;
	}	rows[2];

	snprintf(rows[0].name, sizeof(rows[0].name), "tetrisd");
	snprintf(rows[1].name, sizeof(rows[1].name), "tetrislogd_extra");
	rows[0].pid = 1;
	rows[1].pid = 2;
	assert(daemon_report_width(rows[0].name, 2, sizeof(rows[0]))
		== (int)strlen(rows[1].name));
	printf("PASS test_width_strides_over_a_struct_array\n");
}

static void	test_colour_off_is_the_coloured_form_stripped(void)
{
	char	coloured[512];
	char	plain[512];
	char	stripped[512];

	capture(render_row_coloured, coloured, sizeof(coloured));
	capture(render_row_plain, plain, sizeof(plain));
	strip_escapes(coloured, stripped, sizeof(stripped));
	assert(strcmp(coloured, plain) != 0);
	assert(strcmp(stripped, plain) == 0);
	printf("PASS test_colour_off_is_the_coloured_form_stripped\n");
}

/*
** The header's labels and the row's values have to start at the same offsets,
** or the table is only accidentally a table. Comparing the column at which
** "pid" starts against the column its value starts at is the cheapest
** statement of that which still fails when a width specifier is dropped.
*/
static void	test_header_and_row_align_on_the_same_columns(void)
{
	char	head[512];
	char	row[512];

	daemon_report_colour(false);
	capture(render_header, head, sizeof(head));
	capture(render_running, row, sizeof(row));
	assert(strstr(head, "name") - head == strstr(row, g_row_name) - row);
	assert(strstr(head, "pid") - head == strstr(row, "4242") - row);
	assert(strstr(head, "state") - head == strstr(row, "up") - row);
	assert(strstr(head, "uptime") - head == strstr(row, "1:01:01") - row);
	daemon_report_colour(true);
	printf("PASS test_header_and_row_align_on_the_same_columns\n");
}

/*
** A stopped daemon's pidfile may still hold the pid it died as, so printing
** that number would offer the operator something to signal that is not there.
*/
static void	test_running_shows_its_pid_and_stopped_shows_none(void)
{
	char	up[512];
	char	down[512];

	daemon_report_colour(false);
	capture(render_running, up, sizeof(up));
	capture(render_stopped, down, sizeof(down));
	assert(strstr(up, "4242") != NULL);
	assert(strstr(up, "up") != NULL);
	assert(strstr(down, "4242") == NULL);
	assert(strstr(down, "down") != NULL);
	daemon_report_colour(true);
	printf("PASS test_running_shows_its_pid_and_stopped_shows_none\n");
}

static void	test_unknown_is_neither_up_nor_down(void)
{
	char	plain[512];
	char	coloured[512];

	daemon_report_colour(false);
	capture(render_unknown, plain, sizeof(plain));
	daemon_report_colour(true);
	capture(render_unknown, coloured, sizeof(coloured));
	assert(strstr(plain, "unknown") != NULL);
	assert(strstr(plain, "4242") == NULL);
	/* Yellow, not the red a genuinely stopped daemon gets. */
	assert(strstr(coloured, DAEMON_CL_YELLOW) != NULL);
	assert(strstr(coloured, DAEMON_CL_RED) == NULL);
	printf("PASS test_unknown_is_neither_up_nor_down\n");
}

static void	test_notice_omits_a_pid_it_was_not_given(void)
{
	char	with[512];
	char	without[512];

	daemon_report_colour(false);
	capture(render_notice_with_pid, with, sizeof(with));
	capture(render_notice_without_pid, without, sizeof(without));
	assert(strstr(with, "started") != NULL && strstr(with, "4242") != NULL);
	assert(strstr(without, "not running") != NULL);
	assert(strstr(without, "0") == NULL);
	daemon_report_colour(true);
	printf("PASS test_notice_omits_a_pid_it_was_not_given\n");
}

/*
** tetrisctl reports on a daemon whose detail is a path it may not have
** resolved, so a NULL there has to render as an empty column rather than
** taking the table down.
*/
static void	test_null_text_does_not_crash_a_row(void)
{
	char	out[512];

	daemon_report_colour(false);
	capture(render_null_row, out, sizeof(out));
	assert(strstr(out, g_row_name) != NULL);
	daemon_report_colour(true);
	printf("PASS test_null_text_does_not_crash_a_row\n");
}

/**
 * @brief Runs a render function with stdout redirected into a buffer.
 *
 * @param render The rendering call to capture.
 * @param out Receives the captured text, NUL-terminated.
 * @param cap Size of out.
 */
static void	capture(void (*render)(void), char *out, size_t cap)
{
	int		pipe_fds[2];
	int		saved;
	ssize_t	n;

	assert(pipe(pipe_fds) == 0);
	fflush(stdout);
	saved = dup(STDOUT_FILENO);
	assert(saved >= 0);
	assert(dup2(pipe_fds[1], STDOUT_FILENO) >= 0);
	close(pipe_fds[1]);
	render();
	fflush(stdout);
	assert(dup2(saved, STDOUT_FILENO) >= 0);
	close(saved);
	n = read(pipe_fds[0], out, cap - 1);
	close(pipe_fds[0]);
	if (n < 0)
		n = 0;
	out[n] = '\0';
}

/**
 * @brief Copies text with every ANSI escape sequence removed.
 *
 * @param in Source text.
 * @param out Destination buffer.
 * @param cap Size of out.
 */
static void	strip_escapes(const char *in, char *out, size_t cap)
{
	size_t	w;

	w = 0;
	while (*in != '\0' && w + 1 < cap)
	{
		if (*in == '\x1b')
		{
			while (*in != '\0' && *in != 'm')
				in++;
			if (*in == 'm')
				in++;
			continue ;
		}
		out[w++] = *in++;
	}
	out[w] = '\0';
}

static void	render_row_coloured(void)
{
	daemon_report_colour(true);
	daemon_report_row("", g_row_width, g_row_name, DAEMON_REPORT_RUNNING, 4242, 3661,
		"tmp/tetrislogd/tetrislogd.pid");
}

static void	render_row_plain(void)
{
	daemon_report_colour(false);
	daemon_report_row("", g_row_width, g_row_name, DAEMON_REPORT_RUNNING, 4242, 3661,
		"tmp/tetrislogd/tetrislogd.pid");
	daemon_report_colour(true);
}

static void	render_header(void)
{
	daemon_report_header("", g_row_width, 30);
}

static void	render_running(void)
{
	daemon_report_row("", g_row_width, g_row_name, DAEMON_REPORT_RUNNING, 4242, 3661,
		"tmp/tetrislogd/tetrislogd.pid");
}

static void	render_stopped(void)
{
	daemon_report_row("", g_row_width, g_row_name, DAEMON_REPORT_STOPPED, 4242, 3661,
		"tmp/tetrislogd/tetrislogd.pid");
}

static void	render_unknown(void)
{
	daemon_report_row("", g_row_width, g_row_name, DAEMON_REPORT_UNKNOWN, 4242, 3661,
		"tmp/tetrislogd/tetrislogd.pid");
}

static void	render_notice_with_pid(void)
{
	daemon_report_notice(DAEMON_NOTICE_UP, "started", g_row_name, 4242);
}

static void	render_notice_without_pid(void)
{
	daemon_report_notice(DAEMON_NOTICE_IDLE, "not running", g_row_name, 0);
}

static void	render_null_row(void)
{
	daemon_report_row(NULL, g_row_width, g_row_name, DAEMON_REPORT_STOPPED, 0, -1,
		NULL);
}
