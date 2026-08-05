/* ************************************************************************** */
/*                                                                            */
/*   test_lifecycle.c - starting, seeing, and stopping real processes         */
/*                                                                            */
/*   Every case here forks and execs an actual self-detaching daemon          */
/*   (tests/fakedaemon.c) rather than a stub, because what is being tested    */
/*   is the handover between two processes: does start learn that the boot    */
/*   failed, does status read the lock rather than the file, does stop block  */
/*   until teardown has finished, and does it happen in the reverse of        */
/*   launch order (docs/adr/0007)?                                            */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisctl.h"
#include <assert.h>
#include <sys/stat.h>
#include <time.h>

/*
** Static Variables
**
** PATH as this suite inherited it. Every case re-points PATH at tests/bin,
** and prepending to whatever PATH currently holds would grow it by one
** directory per case until it no longer fitted - which is a truncated PATH
** with the system's own bin directories cut off the end.
*/
static char	g_path[2048];

// Static Functions
static void	test_start_reports_a_daemon_that_came_up(void);
static void	test_start_reports_a_daemon_that_died_booting(void);
static void	test_a_second_start_is_a_no_op(void);
static void	test_stop_blocks_until_the_daemon_has_gone(void);
static void	test_stopping_what_is_not_running_is_success(void);
static void	test_teardown_reverses_launch_order(void);
static void	test_a_crash_left_pidfile_reads_as_stopped(void);

static void	arrange(t_ctl *ctl, char *dir, size_t cap);
static int	tmpdir(char *out, size_t cap);
static void	rmtree(const char *dir);
static void	slurp(const char *path, char *out, size_t cap);

int	main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	test_start_reports_a_daemon_that_came_up();
	test_start_reports_a_daemon_that_died_booting();
	test_a_second_start_is_a_no_op();
	test_stop_blocks_until_the_daemon_has_gone();
	test_stopping_what_is_not_running_is_success();
	test_teardown_reverses_launch_order();
	test_a_crash_left_pidfile_reads_as_stopped();
	return (0);
}

static void	test_start_reports_a_daemon_that_came_up(void)
{
	t_ctl	ctl;
	char	dir[96];
	pid_t	pid;

	arrange(&ctl, dir, sizeof(dir));
	assert(cmd_start(&ctl, "tetrislogd") == 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &pid) == TC_RUNNING);
	assert(pid > 0);
	assert(cmd_stop(&ctl, "tetrislogd") == 0);
	rmtree(dir);
	printf("PASS test_start_reports_a_daemon_that_came_up\n");
}

/*
** The blind spot this whole design exists to close. dspawn wrote its registry
** entry and then exec'd, so it never learned whether the program it launched
** started; the daemon holds a readiness pipe instead, and start's exit code
** is what that pipe said.
*/
static void	test_start_reports_a_daemon_that_died_booting(void)
{
	t_ctl	ctl;
	char	dir[96];
	pid_t	pid;

	arrange(&ctl, dir, sizeof(dir));
	setenv("FAKE_FAIL", "1", 1);
	assert(cmd_start(&ctl, "tetrislogd") == -1);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &pid) == TC_STOPPED);
	unsetenv("FAKE_FAIL");
	rmtree(dir);
	printf("PASS test_start_reports_a_daemon_that_died_booting\n");
}

/*
** Starting a stack that is already up is something an operator does by
** accident and a script does by design, so it is success with a report - not
** a second instance, and not an error.
*/
static void	test_a_second_start_is_a_no_op(void)
{
	t_ctl	ctl;
	char	dir[96];
	pid_t	first;
	pid_t	second;

	arrange(&ctl, dir, sizeof(dir));
	assert(cmd_start(&ctl, "tetrislogd") == 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &first) == TC_RUNNING);
	assert(cmd_start(&ctl, "tetrislogd") == 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &second) == TC_RUNNING);
	assert(first == second);
	assert(cmd_stop(&ctl, "tetrislogd") == 0);
	rmtree(dir);
	printf("PASS test_a_second_start_is_a_no_op\n");
}

/*
** The guarantee use_cases.md asks for, bought without a protocol: d_stop
** returns only once the pidfile lock is free, and the lock comes free as the
** daemon's last act. So the moment stop returns, the daemon is gone - not
** signalled, gone.
*/
static void	test_stop_blocks_until_the_daemon_has_gone(void)
{
	t_ctl	ctl;
	char	dir[96];
	pid_t	pid;

	arrange(&ctl, dir, sizeof(dir));
	assert(cmd_start(&ctl, "tetrislogd") == 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &pid) == TC_RUNNING);
	assert(d_stop(ctl_find(&ctl, "tetrislogd"), 5000) == 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &pid) == TC_STOPPED);
	assert(pid == 0);
	rmtree(dir);
	printf("PASS test_stop_blocks_until_the_daemon_has_gone\n");
}

static void	test_stopping_what_is_not_running_is_success(void)
{
	t_ctl	ctl;
	char	dir[96];

	arrange(&ctl, dir, sizeof(dir));
	assert(cmd_stop(&ctl, "tetrislogd") == 0);
	assert(cmd_status(&ctl, "tetrislogd") == 0);
	assert(cmd_start(&ctl, "nosuchd") == -1);
	assert(cmd_stop(&ctl, "nosuchd") == -1);
	rmtree(dir);
	printf("PASS test_stopping_what_is_not_running_is_success\n");
}

/*
** Stopping the logger first would push tetrisd's entire shutdown into its
** error file instead of the log. Each fake appends its name as it goes down,
** and d_stop blocks until each is fully gone, so the trace is the order.
*/
static void	test_teardown_reverses_launch_order(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	trace[192];
	char	seen[128];

	arrange(&ctl, dir, sizeof(dir));
	snprintf(trace, sizeof(trace), "%s/trace", dir);
	setenv("FAKE_TRACE", trace, 1);
	assert(cmd_start(&ctl, NULL) == 0);
	assert(cmd_stop(&ctl, NULL) == 0);
	slurp(trace, seen, sizeof(seen));
	assert(strcmp(seen, "tetrisd\ntetrislogd\n") == 0);
	unsetenv("FAKE_TRACE");
	rmtree(dir);
	printf("PASS test_teardown_reverses_launch_order\n");
}

/*
** A pidfile survives the process that wrote it, and its contents stay
** perfectly readable. Asking the lock rather than the file is what stops
** tetrisctl signalling a pid that some unrelated process now owns.
*/
static void	test_a_crash_left_pidfile_reads_as_stopped(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	text[64];
	pid_t	pid;

	arrange(&ctl, dir, sizeof(dir));
	assert(cmd_start(&ctl, "tetrislogd") == 0);
	assert(cmd_stop(&ctl, "tetrislogd") == 0);
	slurp(ctl_find(&ctl, "tetrislogd")->pid_path, text, sizeof(text));
	assert(atoi(text) > 0);
	assert(d_state(ctl_find(&ctl, "tetrislogd"), &pid) == TC_STOPPED);
	assert(pid == 0);
	rmtree(dir);
	printf("PASS test_a_crash_left_pidfile_reads_as_stopped\n");
}

/**
 * @brief Points a fresh roster at throwaway pidfiles and the fake daemons.
 *
 * PATH gains tests/bin because that is how tetrisctl finds what to run -
 * execvp, exactly as dspawn resolved its own target. The pidfile keys go into
 * the environment rather than a fixture rc file so that both ends read them
 * the same way the real deployment does.
 *
 * @param ctl Roster to load.
 * @param dir Buffer receiving the temporary directory to clean up.
 * @param cap Size of dir.
 */
static void	arrange(t_ctl *ctl, char *dir, size_t cap)
{
	char	path[3072];
	char	cwd[256];

	assert(tmpdir(dir, cap) == 0);
	assert(getcwd(cwd, sizeof(cwd)) != NULL);
	if (g_path[0] == '\0')
		snprintf(g_path, sizeof(g_path), "%s", getenv("PATH"));
	snprintf(path, sizeof(path), "%s/tests/bin:%s", cwd, g_path);
	setenv("PATH", path, 1);
	snprintf(path, sizeof(path), "%s/logd.pid", dir);
	setenv("TETRISLOGD_PID", path, 1);
	snprintf(path, sizeof(path), "%s/tetrisd.pid", dir);
	setenv("TETRISD_PID_PATH", path, 1);
	setenv("TETRISCTL_DAEMONS", "tetrislogd tetrisd", 1);
	assert(cfg_load(ctl, "/dev/null") == 0);
	ctl->stop_ms = 5000;
	ctl->rc_path[0] = '\0';
	assert(ctl->count == 2);
}

static int	tmpdir(char *out, size_t cap)
{
	snprintf(out, cap, "/tmp/tctlXXXXXX");
	if (mkdtemp(out) == NULL)
		return (-1);
	return (0);
}

static void	rmtree(const char *dir)
{
	char	cmd[192];

	snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
	if (system(cmd) != 0)
		fprintf(stderr, "test_lifecycle: could not remove %s\n", dir);
}

static void	slurp(const char *path, char *out, size_t cap)
{
	ssize_t	n;
	int		fd;

	out[0] = '\0';
	fd = open(path, O_RDONLY);
	assert(fd >= 0);
	n = read(fd, out, cap - 1);
	close(fd);
	assert(n >= 0);
	out[n] = '\0';
}
