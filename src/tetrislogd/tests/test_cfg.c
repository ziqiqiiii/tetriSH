/* ************************************************************************** */
/*                                                                            */
/*   test_cfg.c - tetrislogd configuration                                    */
/*                                                                            */
/*   Both paths come from .tetrishrc; nothing is hard-coded. These cases pin  */
/*   the TETRISLOGD_ namespace, the resolution order (argv -> $TETRISHRC ->   */
/*   ./.tetrishrc), the `export KEY=VALUE` line format, and the invariant     */
/*   that keeps the logger's socket key equal to the one tetrisd dials.       */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_defaults_are_complete(void);
static void	test_set_known_keys(void);
static void	test_set_rejects_bad_values(void);
static void	test_parse_line_forms(void);
static void	test_load_reads_rc_file(void);
static void	test_env_overrides_file(void);
static void	test_resolve_rc_order(void);
static void	test_mkdir_p_creates_parents(void);
static void	test_the_shipped_rc_file_loads(void);
static void	test_shipped_rc_socket_keys_agree(void);

static void	write_file(const char *path, const char *text);
static int	rc_value(const char *path, const char *key, char *out, size_t cap);

int	main(void)
{
	test_defaults_are_complete();
	test_set_known_keys();
	test_set_rejects_bad_values();
	test_parse_line_forms();
	test_load_reads_rc_file();
	test_env_overrides_file();
	test_resolve_rc_order();
	test_mkdir_p_creates_parents();
	test_the_shipped_rc_file_loads();
	test_shipped_rc_socket_keys_agree();
	return (0);
}

static void	test_defaults_are_complete(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(strcmp(cfg.sock_path, TL_DEF_SOCK) == 0);
	assert(strcmp(cfg.file_path, TL_DEF_FILE) == 0);
	printf("PASS test_defaults_are_complete\n");
}

/*
** The prefix filter is what keeps two daemons in one start-up file from
** reading each other's settings: tetrisd refuses anything that is not
** TETRISD_, and the logger refuses anything that is not TETRISLOGD_.
*/
static void	test_set_known_keys(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg_set(&cfg, "TETRISLOGD_SOCK", "/run/logd.sock") == 0);
	assert(strcmp(cfg.sock_path, "/run/logd.sock") == 0);
	assert(cfg_set(&cfg, "TETRISLOGD_FILE", "/var/log/logd.log") == 0);
	assert(strcmp(cfg.file_path, "/var/log/logd.log") == 0);
	assert(cfg_set(&cfg, "TETRISD_LOG_IPC", "/run/other.sock") == -1);
	assert(cfg_set(&cfg, "PATH", "/usr/bin") == -1);
	assert(cfg_set(&cfg, "TETRISLOGD_NONSENSE", "x") == -1);
	printf("PASS test_set_known_keys\n");
}

static void	test_set_rejects_bad_values(void)
{
	char	toolong[TL_PATH_MAX + 32];
	t_cfg	cfg;

	cfg_defaults(&cfg);
	memset(toolong, 'a', sizeof(toolong) - 1);
	toolong[sizeof(toolong) - 1] = '\0';
	assert(cfg_set(&cfg, "TETRISLOGD_SOCK", "") == -1);
	assert(cfg_set(&cfg, "TETRISLOGD_FILE", "") == -1);
	assert(cfg_set(&cfg, "TETRISLOGD_FILE", toolong) == -1);
	assert(strcmp(cfg.sock_path, TL_DEF_SOCK) == 0);
	assert(strcmp(cfg.file_path, TL_DEF_FILE) == 0);
	printf("PASS test_set_rejects_bad_values\n");
}

/*
** .tetrishrc is a shell script first and a config file second, so a line the
** logger does not own is skipped, not refused - otherwise every dspawn call
** and every TETRISD_ setting would fail the load.
*/
static void	test_parse_line_forms(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg_parse_line(&cfg, "export TETRISLOGD_SOCK=/a/b.sock") == 0);
	assert(strcmp(cfg.sock_path, "/a/b.sock") == 0);
	assert(cfg_parse_line(&cfg, "TETRISLOGD_FILE=\"/c/d.log\"") == 0);
	assert(strcmp(cfg.file_path, "/c/d.log") == 0);
	assert(cfg_parse_line(&cfg, "# export TETRISLOGD_SOCK=/no.sock") == 0);
	assert(cfg_parse_line(&cfg, "dspawn tetrisd -- tetrisd") == 0);
	assert(cfg_parse_line(&cfg, "export TETRISD_PORT=4242") == 0);
	assert(cfg_parse_line(&cfg, "") == 0);
	assert(strcmp(cfg.sock_path, "/a/b.sock") == 0);
	assert(cfg_parse_line(&cfg, "export TETRISLOGD_FILE=") == -1);
	printf("PASS test_parse_line_forms\n");
}

static void	test_load_reads_rc_file(void)
{
	t_fixture	fx;
	t_cfg		cfg;
	char		rc[256];

	assert(fx_make(&fx) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", fx.dir);
	write_file(rc,
		"# a comment\n"
		"export TETRISLOGD_SOCK=/tmp/x.sock\n"
		"export TETRISLOGD_FILE=/tmp/x.log\n"
		"export TETRISD_PORT=4242\n");
	assert(cfg_load(&cfg, rc) == 0);
	assert(strcmp(cfg.sock_path, "/tmp/x.sock") == 0);
	assert(strcmp(cfg.file_path, "/tmp/x.log") == 0);
	assert(strcmp(cfg.rc_path, rc) == 0);
	fx_destroy(&fx);
	printf("PASS test_load_reads_rc_file\n");
}

/*
** dspawn exports these before exec'ing the daemon, so a value inherited from
** the environment is the more specific one and wins over the file.
*/
static void	test_env_overrides_file(void)
{
	t_fixture	fx;
	t_cfg		cfg;
	char		rc[256];

	assert(fx_make(&fx) == 0);
	snprintf(rc, sizeof(rc), "%s/rc", fx.dir);
	write_file(rc, "export TETRISLOGD_FILE=/tmp/from_file.log\n");
	setenv("TETRISLOGD_FILE", "/tmp/from_env.log", 1);
	assert(cfg_load(&cfg, rc) == 0);
	assert(strcmp(cfg.file_path, "/tmp/from_env.log") == 0);
	unsetenv("TETRISLOGD_FILE");
	fx_destroy(&fx);
	printf("PASS test_env_overrides_file\n");
}

static void	test_resolve_rc_order(void)
{
	char	out[TL_PATH_MAX];

	unsetenv("TETRISHRC");
	assert(cfg_resolve_rc(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "./" TL_RC_NAME) == 0);
	setenv("TETRISHRC", "/tmp/env.rc", 1);
	assert(cfg_resolve_rc(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/env.rc") == 0);
	assert(cfg_resolve_rc("/tmp/argv.rc", out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/argv.rc") == 0);
	unsetenv("TETRISHRC");
	printf("PASS test_resolve_rc_order\n");
}

static void	test_mkdir_p_creates_parents(void)
{
	t_fixture	fx;
	struct stat	st;
	char		deep[256];

	assert(fx_make(&fx) == 0);
	snprintf(deep, sizeof(deep), "%s/a/b/c", fx.dir);
	assert(cfg_mkdir_p(deep) == 0);
	assert(stat(deep, &st) == 0 && S_ISDIR(st.st_mode));
	assert(cfg_mkdir_p(deep) == 0);
	fx_destroy(&fx);
	printf("PASS test_mkdir_p_creates_parents\n");
}

/*
** The .tetrishrc this repository ships is the file the daemon boots from, so
** a TETRISLOGD_ key documented there but never wired into cfg_set would stop
** tetrislogd starting at all. Only parsing the real file catches that.
*/
static void	test_the_shipped_rc_file_loads(void)
{
	t_cfg	cfg;

	assert(cfg_load(&cfg, "../../.tetrishrc") == 0);
	assert(cfg.sock_path[0] != '\0');
	assert(cfg.file_path[0] != '\0');
	printf("PASS test_the_shipped_rc_file_loads\n");
}

/*
** Two keys name one socket: tetrisd dials TETRISD_LOG_IPC, tetrislogd binds
** TETRISLOGD_SOCK. Nothing at runtime can notice them drifting apart - the
** logger simply sits on a socket nobody sends to, and every record ends up
** in tetrisd's stderr fallback. This assertion is the only thing keeping the
** two lines equal, so it fails the build rather than the deployment.
*/
static void	test_shipped_rc_socket_keys_agree(void)
{
	char	dialled[TL_PATH_MAX];
	char	bound[TL_PATH_MAX];

	assert(rc_value("../../.tetrishrc", "TETRISD_LOG_IPC",
			dialled, sizeof(dialled)) == 0);
	assert(rc_value("../../.tetrishrc", "TETRISLOGD_SOCK",
			bound, sizeof(bound)) == 0);
	assert(strcmp(dialled, bound) == 0);
	printf("PASS test_shipped_rc_socket_keys_agree\n");
}

static void	write_file(const char *path, const char *text)
{
	FILE	*f;

	f = fopen(path, "w");
	assert(f != NULL);
	fputs(text, f);
	fclose(f);
}

/*
** Reads one assignment straight out of an rc file, without going through
** cfg_set - the drift check has to see both keys, and cfg_set deliberately
** refuses the one belonging to the other daemon.
*/
static int	rc_value(const char *path, const char *key, char *out, size_t cap)
{
	char	line[TL_LINE_MAX];
	char	*at;
	FILE	*f;

	f = fopen(path, "r");
	if (f == NULL)
		return (-1);
	while (fgets(line, sizeof(line), f) != NULL)
	{
		at = strstr(line, key);
		if (at == NULL || line[0] == '#' || at[strlen(key)] != '=')
			continue ;
		at += strlen(key) + 1;
		at[strcspn(at, " \t#\n")] = '\0';
		snprintf(out, cap, "%s", at);
		fclose(f);
		return (0);
	}
	fclose(f);
	return (-1);
}
