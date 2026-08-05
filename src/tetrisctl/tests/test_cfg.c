/* ************************************************************************** */
/*                                                                            */
/*   test_cfg.c - the roster, and where it is allowed to come from            */
/*                                                                            */
/*   The one thing ADR-0007 insists must not be compiled into this program    */
/*   is which daemons run and in what order. These cases pin that it comes    */
/*   from .tetrishrc, that teardown is that order reversed, and that the      */
/*   pidfile each name resolves to is the one the daemon itself publishes.    */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisctl.h"
#include <assert.h>

// Static Functions
static void	test_defaults_name_both_daemons_in_launch_order(void);
static void	test_the_roster_comes_from_the_rc_file(void);
static void	test_pidfile_keys_are_read_from_the_daemons_prefixes(void);
static void	test_key_order_in_the_file_does_not_matter(void);
static void	test_an_unmanaged_name_fails_the_roster(void);
static void	test_env_overrides_file(void);
static void	test_the_shipped_rc_file_resolves(void);

static void	write_file(const char *path, const char *text);
static int	tmpdir(char *out, size_t cap);
static void	rmtree(const char *dir);

int	main(void)
{
	test_defaults_name_both_daemons_in_launch_order();
	test_the_roster_comes_from_the_rc_file();
	test_pidfile_keys_are_read_from_the_daemons_prefixes();
	test_key_order_in_the_file_does_not_matter();
	test_an_unmanaged_name_fails_the_roster();
	test_env_overrides_file();
	test_the_shipped_rc_file_resolves();
	return (0);
}

static void	test_defaults_name_both_daemons_in_launch_order(void)
{
	t_ctl	ctl;

	cfg_defaults(&ctl);
	assert(cfg_resolve(&ctl) == 0);
	assert(ctl.count == 2);
	assert(strcmp(ctl.daemons[0].name, "tetrislogd") == 0);
	assert(strcmp(ctl.daemons[1].name, "tetrisd") == 0);
	assert(ctl.daemons[0].pid_path[0] != '\0');
	assert(ctl.daemons[1].pid_path[0] != '\0');
	printf("PASS test_defaults_name_both_daemons_in_launch_order\n");
}

/*
** The order in the file is launch order and nothing else reads it, so a
** deployment that wants the game server up without a logger says so here and
** tetrisctl obeys - including stopping in the reverse, which is the property
** that keeps tetrisd's shutdown out of its error file.
*/
static void	test_the_roster_comes_from_the_rc_file(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	rc[192];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "export TETRISCTL_DAEMONS=\"tetrisd\"\n");
	assert(cfg_load(&ctl, rc) == 0);
	assert(ctl.count == 1);
	assert(strcmp(ctl.daemons[0].name, "tetrisd") == 0);
	assert(ctl_find(&ctl, "tetrisd") != NULL);
	assert(ctl_find(&ctl, "tetrislogd") == NULL);
	rmtree(dir);
	printf("PASS test_the_roster_comes_from_the_rc_file\n");
}

/*
** tetrisctl reads keys it does not own, and that is the point: the pidfile is
** named once, by the daemon that holds it, and tetrisctl signals whatever
** that name resolves to. Naming it twice is exactly how the two ends drift.
*/
static void	test_pidfile_keys_are_read_from_the_daemons_prefixes(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	rc[192];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc,
		"export TETRISLOGD_PID=/run/logd.pid\n"
		"export TETRISD_PID_PATH=/run/tetrisd.pid\n");
	assert(cfg_load(&ctl, rc) == 0);
	assert(ctl.count == 2);
	assert(strcmp(ctl.daemons[0].pid_path, "/run/logd.pid") == 0);
	assert(strcmp(ctl.daemons[1].pid_path, "/run/tetrisd.pid") == 0);
	rmtree(dir);
	printf("PASS test_pidfile_keys_are_read_from_the_daemons_prefixes\n");
}

/*
** A start-up file is written for people, so the roster may well be declared
** in a "daemons" section far below the settings it refers to. Resolution is
** deferred to the end of the load for exactly this reason.
*/
static void	test_key_order_in_the_file_does_not_matter(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	rc[192];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc,
		"export TETRISCTL_DAEMONS=\"tetrisd tetrislogd\"\n"
		"export TETRISLOGD_PID=/run/logd.pid\n");
	assert(cfg_load(&ctl, rc) == 0);
	assert(strcmp(ctl.daemons[0].name, "tetrisd") == 0);
	assert(strcmp(ctl.daemons[1].pid_path, "/run/logd.pid") == 0);
	rmtree(dir);
	printf("PASS test_key_order_in_the_file_does_not_matter\n");
}

/*
** Unknown *keys* are skipped, because the file is a shell script full of
** lines that are none of tetrisctl's business. An unknown *daemon* is not:
** it names something to be started and stopped, and quietly leaving it out
** would produce a stack that is half up with a zero exit code.
*/
static void	test_an_unmanaged_name_fails_the_roster(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	rc[192];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "export TETRISCTL_DAEMONS=\"tetrislogd tetrisu\"\n");
	assert(cfg_load(&ctl, rc) == -1);
	write_file(rc, "PATH=/usr/bin\nalias ll='ls -l'\n# a comment\n");
	assert(cfg_load(&ctl, rc) == 0);
	assert(ctl.count == 2);
	rmtree(dir);
	printf("PASS test_an_unmanaged_name_fails_the_roster\n");
}

static void	test_env_overrides_file(void)
{
	t_ctl	ctl;
	char	dir[96];
	char	rc[192];

	assert(tmpdir(dir, sizeof(dir)) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "export TETRISLOGD_PID=/run/from_file.pid\n");
	setenv("TETRISLOGD_PID", "/run/from_env.pid", 1);
	assert(cfg_load(&ctl, rc) == 0);
	assert(strcmp(ctl.daemons[0].pid_path, "/run/from_env.pid") == 0);
	unsetenv("TETRISLOGD_PID");
	rmtree(dir);
	printf("PASS test_env_overrides_file\n");
}

/*
** The .tetrishrc this repository ships is the file tetrisctl actually reads,
** and it is the only thing that pins the compiled pidfile fallbacks against
** the paths the daemons really use. A hand-written fixture cannot catch that
** drift, because it would be written from the same assumption.
*/
static void	test_the_shipped_rc_file_resolves(void)
{
	t_ctl	shipped;
	t_ctl	bare;
	int		i;

	assert(cfg_load(&shipped, "../../.tetrishrc") == 0);
	assert(shipped.count == 2);
	assert(strcmp(shipped.daemons[0].name, "tetrislogd") == 0);
	assert(strcmp(shipped.daemons[1].name, "tetrisd") == 0);
	cfg_defaults(&bare);
	assert(cfg_resolve(&bare) == 0);
	i = 0;
	while (i < shipped.count)
	{
		assert(strcmp(shipped.daemons[i].pid_path,
				bare.daemons[i].pid_path) == 0);
		i++;
	}
	printf("PASS test_the_shipped_rc_file_resolves\n");
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
		fprintf(stderr, "test_cfg: could not remove %s\n", dir);
}
