/* ************************************************************************** */
/*                                                                            */
/*   test_cfg.c - tetrisd configuration                                       */
/*                                                                            */
/*   Every path and setting comes from .tetrishrc; nothing is hard-coded.     */
/*   These cases pin the resolution order ($TETRISHRC -> ./.tetrishrc ->      */
/*   argv override), the `export KEY=VALUE` line format, value validation,    */
/*   and the boot-time certificate check.                                     */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"
#include <assert.h>

// Static Functions
static void	test_defaults_are_complete(void);
static void	test_set_known_keys(void);
static void	test_set_rejects_bad_values(void);
static void	test_parse_line_forms(void);
static void	test_load_reads_rc_file(void);
static void	test_env_overrides_file(void);
static void	test_resolve_rc_order(void);
static void	test_validate_requires_certificates(void);
static void	test_the_shipped_rc_file_loads(void);

static void	write_file(const char *path, const char *text);
static void	make_tmp_dir(char *out, size_t cap);

int	main(void)
{
	test_defaults_are_complete();
	test_set_known_keys();
	test_set_rejects_bad_values();
	test_parse_line_forms();
	test_load_reads_rc_file();
	test_env_overrides_file();
	test_resolve_rc_order();
	test_validate_requires_certificates();
	test_the_shipped_rc_file_loads();
	return (0);
}

static void	test_defaults_are_complete(void)
{
	t_config	cfg;

	config_defaults(&cfg);
	assert(cfg.port == TETRISD_DEFAULT_PORT);
	assert(strcmp(cfg.data_dir, TETRISD_DEFAULT_DATA_DIR) == 0);
	assert(strcmp(cfg.config_dir, TETRISD_DEFAULT_CONFIG_DIR) == 0);
	assert(strcmp(cfg.cert_path, TETRISD_DEFAULT_CERT_PATH) == 0);
	assert(strcmp(cfg.key_path, TETRISD_DEFAULT_KEY_PATH) == 0);
	assert(strcmp(cfg.ca_path, TETRISD_DEFAULT_CA_PATH) == 0);
	assert(strcmp(cfg.log_ipc, TETRISD_DEFAULT_LOG_IPC_PATH) == 0);
	assert(strcmp(cfg.pid_path, TETRISD_DEFAULT_PID_PATH) == 0);
	assert(strcmp(cfg.err_path, TETRISD_DEFAULT_ERR_PATH) == 0);
	assert(cfg.max_clients == TETRISD_DEFAULT_MAX_CLIENTS);
	assert(cfg.tick_ms == TETRISD_DEFAULT_TICK_MS);
	assert(cfg.br_slots == TETRISD_DEFAULT_BATTLE_ROYALE_SLOTS);
	assert(cfg.log_level == COREIPC_LOG_INFO);
	printf("PASS test_defaults_are_complete\n");
}

/*
** The .tetrishrc this repository ships is the one the daemon boots from, and
** an unknown TETRISD_ key fails the whole load - so a setting documented in
** the file but never wired into config_set stops tetrisd starting at all. Only
** parsing the real file catches that; a hand-written fixture cannot.
*/
static void	test_the_shipped_rc_file_loads(void)
{
	t_config	cfg;

	assert(config_load(&cfg, "../../.tetrishrc") == 0);
	assert(cfg.input_burst == TETRISD_DEFAULT_INPUT_BURST);
	assert(cfg.input_rate == TETRISD_DEFAULT_INPUT_RATE);
	printf("PASS test_the_shipped_rc_file_loads\n");
}

static void	test_set_known_keys(void)
{
	t_config	cfg;

	config_defaults(&cfg);
	assert(config_set(&cfg, "TETRISD_PORT", "5555") == 0);
	assert(cfg.port == 5555);
	assert(config_set(&cfg, "TETRISD_DATA_DIR", "/var/lib/tetrisd") == 0);
	assert(strcmp(cfg.data_dir, "/var/lib/tetrisd") == 0);
	assert(config_set(&cfg, "TETRISD_LOG_LEVEL", "debug") == 0);
	assert(cfg.log_level == COREIPC_LOG_DEBUG);
	assert(config_set(&cfg, "TETRISD_TICK_MS", "16") == 0);
	assert(cfg.tick_ms == 16);
	assert(config_set(&cfg, "TETRISD_BR_SLOTS", "8") == 0);
	assert(cfg.br_slots == 8);
	assert(config_set(&cfg, "TETRISD_INPUT_BURST", "40") == 0);
	assert(cfg.input_burst == 40);
	assert(config_set(&cfg, "TETRISD_INPUT_RATE", "20") == 0);
	assert(cfg.input_rate == 20);
	assert(config_set(&cfg, "TETRISD_PID_PATH", "/run/tetrisd.pid") == 0);
	assert(strcmp(cfg.pid_path, "/run/tetrisd.pid") == 0);
	assert(config_set(&cfg, "TETRISD_ERR_PATH", "/var/log/tetrisd.err") == 0);
	assert(strcmp(cfg.err_path, "/var/log/tetrisd.err") == 0);
	assert(config_set(&cfg, "PATH", "/usr/bin") == -1);
	assert(config_set(&cfg, "TETRISD_NONSENSE", "x") == -1);
	printf("PASS test_set_known_keys\n");
}

static void	test_set_rejects_bad_values(void)
{
	t_config	cfg;

	config_defaults(&cfg);
	assert(config_set(&cfg, "TETRISD_PORT", "99999") == -1);
	assert(config_set(&cfg, "TETRISD_PORT", "-1") == -1);
	assert(config_set(&cfg, "TETRISD_PORT", "abc") == -1);
	assert(config_set(&cfg, "TETRISD_TICK_MS", "0") == -1);
	assert(config_set(&cfg, "TETRISD_MAX_CLIENTS", "0") == -1);
	assert(config_set(&cfg, "TETRISD_BR_SLOTS", "1") == -1);
	assert(config_set(&cfg, "TETRISD_LOG_LEVEL", "chatty") == -1);
	assert(config_set(&cfg, "TETRISD_DATA_DIR", "") == -1);
	assert(cfg.port == TETRISD_DEFAULT_PORT);
	assert(cfg.tick_ms == TETRISD_DEFAULT_TICK_MS);
	printf("PASS test_set_rejects_bad_values\n");
}

static void	test_parse_line_forms(void)
{
	t_config	cfg;

	config_defaults(&cfg);
	assert(config_parse_line(&cfg, "export TETRISD_PORT=7000") == 0);
	assert(cfg.port == 7000);
	assert(config_parse_line(&cfg, "  TETRISD_PORT=7001  ") == 0);
	assert(cfg.port == 7001);
	assert(config_parse_line(&cfg, "export TETRISD_CERT_PATH=\"/tmp/a.crt\"") == 0);
	assert(strcmp(cfg.cert_path, "/tmp/a.crt") == 0);
	assert(config_parse_line(&cfg, "# export TETRISD_PORT=1") == 0);
	assert(config_parse_line(&cfg, "") == 0);
	assert(config_parse_line(&cfg, "dspawn tetrislogd") == 0);
	assert(config_parse_line(&cfg, "PATH=/usr/bin") == 0);
	assert(cfg.port == 7001);
	assert(config_parse_line(&cfg, "export TETRISD_PORT=notanumber") == -1);
	printf("PASS test_parse_line_forms\n");
}

static void	test_load_reads_rc_file(void)
{
	t_config	cfg;
	char	dir[64];
	char	rc[128];

	make_tmp_dir(dir, sizeof(dir));
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "# tetrisd\nexport TETRISD_PORT=6001\n"
		"export TETRISD_DATA_DIR=/tmp/dd\ndspawn tetrisd\n");
	unsetenv("TETRISD_PORT");
	assert(config_load(&cfg, rc) == 0);
	assert(cfg.port == 6001);
	assert(strcmp(cfg.data_dir, "/tmp/dd") == 0);
	assert(strcmp(cfg.rc_path, rc) == 0);
	assert(strcmp(cfg.cert_path, TETRISD_DEFAULT_CERT_PATH) == 0);
	unlink(rc);
	rmdir(dir);
	printf("PASS test_load_reads_rc_file\n");
}

static void	test_env_overrides_file(void)
{
	t_config	cfg;
	char	dir[64];
	char	rc[128];

	make_tmp_dir(dir, sizeof(dir));
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "export TETRISD_PORT=6001\n");
	setenv("TETRISD_PORT", "6002", 1);
	assert(config_load(&cfg, rc) == 0);
	assert(cfg.port == 6002);
	unsetenv("TETRISD_PORT");
	unlink(rc);
	rmdir(dir);
	printf("PASS test_env_overrides_file\n");
}

static void	test_resolve_rc_order(void)
{
	char	out[TETRISD_FS_PATH_MAX];

	setenv("TETRISHRC", "/tmp/from-env", 1);
	assert(config_resolve_rc_path(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/from-env") == 0);
	assert(config_resolve_rc_path("/tmp/from-argv", out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/from-argv") == 0);
	unsetenv("TETRISHRC");
	assert(config_resolve_rc_path(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "./" TETRISD_RC_FILENAME) == 0);
	printf("PASS test_resolve_rc_order\n");
}

static void	test_validate_requires_certificates(void)
{
	t_config	cfg;
	char	dir[64];
	char	path[128];

	make_tmp_dir(dir, sizeof(dir));
	config_defaults(&cfg);
	snprintf(path, sizeof(path), "%s/missing.crt", dir);
	assert(config_set(&cfg, "TETRISD_CERT_PATH", path) == 0);
	assert(config_validate(&cfg) == -1);
	write_file(path, "cert\n");
	snprintf(path, sizeof(path), "%s/server.key", dir);
	assert(config_set(&cfg, "TETRISD_KEY_PATH", path) == 0);
	assert(config_validate(&cfg) == -1);
	write_file(path, "key\n");
	assert(config_validate(&cfg) == 0);
	snprintf(path, sizeof(path), "%s/missing.crt", dir);
	unlink(path);
	snprintf(path, sizeof(path), "%s/server.key", dir);
	unlink(path);
	rmdir(dir);
	printf("PASS test_validate_requires_certificates\n");
}

static void	write_file(const char *path, const char *text)
{
	FILE	*f;

	f = fopen(path, "w");
	assert(f != NULL);
	fputs(text, f);
	fclose(f);
}

static void	make_tmp_dir(char *out, size_t cap)
{
	snprintf(out, cap, "tests/tmp/cfgXXXXXX");
	assert(cd_mkdir_p("tests/tmp") == 0);
	assert(mkdtemp(out) != NULL);
}
