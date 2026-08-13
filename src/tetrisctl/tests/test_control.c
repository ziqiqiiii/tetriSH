/* ************************************************************************** */
/*                                                                            */
/*   test_control.c - the Control channel, from the asking end                */
/*                                                                            */
/*   Which daemon has a channel is compiled in beside the pidfile key, and    */
/*   the path it uses is not. These cases pin both halves of that, and the    */
/*   two failures an operator will actually meet: a daemon that is down, and  */
/*   one that accepts the connection and then says nothing.                   */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisctl.h"
#include <assert.h>
#include <sys/socket.h>

#define FIXTURE_DIR_MAX		96
#define FIXTURE_PATH_MAX	256

// Static Functions
static void	test_only_tetrisd_has_a_channel(void);
static void	test_a_missing_control_key_is_not_fatal(void);
static void	test_env_overrides_the_control_path(void);
static void	test_an_absent_daemon_is_refused_not_awaited(void);
static void	test_a_silent_daemon_does_not_hang_the_operator(void);
static void	test_status_of_a_channelless_daemon_is_a_report(void);

static void	write_rc(const char *path, const char *dir, const char *control);
static void	write_file(const char *path, const char *text);
static int	tmpdir(char *out, size_t cap);
static void	rmtree(const char *dir);

int	main(void)
{
	test_only_tetrisd_has_a_channel();
	test_a_missing_control_key_is_not_fatal();
	test_env_overrides_the_control_path();
	test_an_absent_daemon_is_refused_not_awaited();
	test_a_silent_daemon_does_not_hang_the_operator();
	test_status_of_a_channelless_daemon_is_a_report();
	return (0);
}

/*
** tetrisd serves a Control channel and tetrislogd does not, and that is
** knowledge about the programs rather than about a deployment of them - so
** setting the key resolves onto exactly one of the two, and no amount of
** configuration gives the logger a channel it does not implement.
*/
static void	test_only_tetrisd_has_a_channel(void)
{
	t_ctl	ctl;
	char	dir[FIXTURE_DIR_MAX];
	char	rc[FIXTURE_PATH_MAX];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_rc(rc, dir, "tmp/tetrisd/tetrisd.ctl");
	assert(config_load(&ctl, rc) == 0);
	assert(ctl.count == 2);
	assert(strcmp(ctl.daemons[0].name, "tetrislogd") == 0);
	assert(ctl.daemons[0].control_path[0] == '\0');
	assert(strcmp(ctl.daemons[1].name, "tetrisd") == 0);
	assert(strcmp(ctl.daemons[1].control_path, "tmp/tetrisd/tetrisd.ctl") == 0);
	rmtree(dir);
	printf("PASS test_only_tetrisd_has_a_channel\n");
}

/*
** A pidfile is how every verb works and a channel is how four of them work, so
** a roster missing the control key still loads and still starts and stops a
** stack. Refusing it would make a signal-managed deployment unloadable over a
** feature it never asked for.
*/
static void	test_a_missing_control_key_is_not_fatal(void)
{
	t_ctl	ctl;
	char	dir[FIXTURE_DIR_MAX];
	char	rc[FIXTURE_PATH_MAX];
	char	body[64];
	size_t	len;

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_rc(rc, dir, NULL);
	assert(config_load(&ctl, rc) == 0);
	assert(ctl.count == 2);
	assert(ctl.daemons[1].control_path[0] == '\0');
	/* and asking is then refused rather than dialled at some guessed path */
	assert(control_ask(&ctl.daemons[1], "STATUS", body, sizeof(body),
			&len) == -1);
	rmtree(dir);
	printf("PASS test_a_missing_control_key_is_not_fatal\n");
}

static void	test_env_overrides_the_control_path(void)
{
	t_ctl	ctl;
	char	dir[FIXTURE_DIR_MAX];
	char	rc[FIXTURE_PATH_MAX];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_rc(rc, dir, "from/the/file.ctl");
	assert(setenv("TETRISD_CONTROL_PATH", "from/the/env.ctl", 1) == 0);
	assert(config_load(&ctl, rc) == 0);
	assert(strcmp(ctl.daemons[1].control_path, "from/the/env.ctl") == 0);
	assert(unsetenv("TETRISD_CONTROL_PATH") == 0);
	rmtree(dir);
	printf("PASS test_env_overrides_the_control_path\n");
}

/*
** The ordinary case an operator meets: the server is not running. It has to
** come back as a refusal rather than a wait, because a status command that
** blocks on a stopped daemon is worse than one that says it is stopped.
*/
static void	test_an_absent_daemon_is_refused_not_awaited(void)
{
	t_managed	d;
	char		dir[FIXTURE_DIR_MAX];
	char		body[64];
	size_t		len;

	assert(tmpdir(dir, sizeof(dir)) == 0);
	memset(&d, 0, sizeof(d));
	snprintf(d.name, sizeof(d.name), "tetrisd");
	snprintf(d.control_path, sizeof(d.control_path), "%s/absent.ctl", dir);
	assert(control_ask(&d, "STATUS", body, sizeof(body), &len) == -1);
	assert(len == 0);
	rmtree(dir);
	printf("PASS test_an_absent_daemon_is_refused_not_awaited\n");
}

/*
** A socket that accepts and then never answers is the case the deadline
** exists for. Without it this call would wait as long as the far side felt
** like taking, which is the one outcome an admin CLI must not have.
*/
static void	test_a_silent_daemon_does_not_hang_the_operator(void)
{
	t_managed	d;
	char		dir[FIXTURE_DIR_MAX];
	char		body[64];
	size_t		len;
	time_t		started;
	int			listen_fd;

	assert(tmpdir(dir, sizeof(dir)) == 0);
	memset(&d, 0, sizeof(d));
	snprintf(d.name, sizeof(d.name), "tetrisd");
	snprintf(d.control_path, sizeof(d.control_path), "%s/silent.ctl", dir);
	listen_fd = unixsock_stream_listen(d.control_path, 4, 0600);
	assert(listen_fd >= 0);
	started = time(NULL);
	assert(control_ask(&d, "STATUS", body, sizeof(body), &len) == -1);
	/* it gave up on its own deadline, not on the peer's goodwill */
	assert(time(NULL) - started <= (TETRISCTL_CONTROL_MS / 1000) + 2);
	close(listen_fd);
	rmtree(dir);
	printf("PASS test_a_silent_daemon_does_not_hang_the_operator\n");
}

/*
** `status tetrislogd` asks about a daemon that implements no Control channel.
** That has to stay a successful report: the pidfile rows are the whole of what
** can be said about it, and there is no Health missing - there is none to
** fetch. Turning "this daemon has no channel" into an exit code would make
** status unusable on half the roster.
*/
static void	test_status_of_a_channelless_daemon_is_a_report(void)
{
	t_ctl	ctl;
	char	dir[FIXTURE_DIR_MAX];
	char	rc[FIXTURE_PATH_MAX];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_rc(rc, dir, "tmp/tetrisd/tetrisd.ctl");
	assert(config_load(&ctl, rc) == 0);
	assert(health_report(&ctl, "tetrislogd") == 0);
	/* and a roster where nothing has a channel is a report too */
	write_rc(rc, dir, NULL);
	assert(config_load(&ctl, rc) == 0);
	assert(health_report(&ctl, NULL) == 0);
	rmtree(dir);
	printf("PASS test_status_of_a_channelless_daemon_is_a_report\n");
}

/**
 * @brief Writes a start-up file naming both daemons and their pidfiles.
 *
 * @param path Where to write it.
 * @param dir Directory the pidfiles sit in.
 * @param control Control path to declare, or NULL to leave the key out.
 */
static void	write_rc(const char *path, const char *dir, const char *control)
{
	char	text[FIXTURE_PATH_MAX * 4];
	char	line[FIXTURE_PATH_MAX];

	line[0] = '\0';
	if (control != NULL)
		snprintf(line, sizeof(line), "export TETRISD_CONTROL_PATH=%s\n",
			control);
	snprintf(text, sizeof(text),
		"export TETRISCTL_DAEMONS=\"tetrislogd tetrisd\"\n"
		"export TETRISLOGD_PID_PATH=%s/logd.pid\n"
		"export TETRISD_PID_PATH=%s/d.pid\n"
		"%s", dir, dir, line);
	write_file(path, text);
}

static void	write_file(const char *path, const char *text)
{
	FILE	*f;

	f = fopen(path, "w");
	assert(f != NULL);
	assert(fputs(text, f) >= 0);
	fclose(f);
}

static int	tmpdir(char *out, size_t cap)
{
	snprintf(out, cap, "/tmp/tctlctlXXXXXX");
	if (mkdtemp(out) == NULL)
		return (-1);
	return (0);
}

static void	rmtree(const char *dir)
{
	char	cmd[FIXTURE_PATH_MAX];

	snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
	if (system(cmd) != 0)
		return ;
}
