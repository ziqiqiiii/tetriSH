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
	return (0);
}

static void	test_defaults_are_complete(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg.port == TD_DEF_PORT);
	assert(strcmp(cfg.data_dir, TD_DEF_DATA_DIR) == 0);
	assert(strcmp(cfg.config_dir, TD_DEF_CONFIG_DIR) == 0);
	assert(strcmp(cfg.cert_path, TD_DEF_CERT) == 0);
	assert(strcmp(cfg.key_path, TD_DEF_KEY) == 0);
	assert(strcmp(cfg.ca_path, TD_DEF_CA) == 0);
	assert(strcmp(cfg.log_ipc, TD_DEF_LOG_IPC) == 0);
	assert(cfg.max_clients == TD_DEF_MAX_CLIENTS);
	assert(cfg.tick_ms == TD_DEF_TICK_MS);
	assert(cfg.br_slots == TD_DEF_BR_SLOTS);
	assert(cfg.log_level == CIPC_LOG_INFO);
	printf("PASS test_defaults_are_complete\n");
}

static void	test_set_known_keys(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg_set(&cfg, "TETRISD_PORT", "5555") == 0);
	assert(cfg.port == 5555);
	assert(cfg_set(&cfg, "TETRISD_DATA_DIR", "/var/lib/tetrisd") == 0);
	assert(strcmp(cfg.data_dir, "/var/lib/tetrisd") == 0);
	assert(cfg_set(&cfg, "TETRISD_LOG_LEVEL", "debug") == 0);
	assert(cfg.log_level == CIPC_LOG_DEBUG);
	assert(cfg_set(&cfg, "TETRISD_TICK_MS", "16") == 0);
	assert(cfg.tick_ms == 16);
	assert(cfg_set(&cfg, "TETRISD_BR_SLOTS", "8") == 0);
	assert(cfg.br_slots == 8);
	assert(cfg_set(&cfg, "PATH", "/usr/bin") == -1);
	assert(cfg_set(&cfg, "TETRISD_NONSENSE", "x") == -1);
	printf("PASS test_set_known_keys\n");
}

static void	test_set_rejects_bad_values(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg_set(&cfg, "TETRISD_PORT", "99999") == -1);
	assert(cfg_set(&cfg, "TETRISD_PORT", "-1") == -1);
	assert(cfg_set(&cfg, "TETRISD_PORT", "abc") == -1);
	assert(cfg_set(&cfg, "TETRISD_TICK_MS", "0") == -1);
	assert(cfg_set(&cfg, "TETRISD_MAX_CLIENTS", "0") == -1);
	assert(cfg_set(&cfg, "TETRISD_BR_SLOTS", "1") == -1);
	assert(cfg_set(&cfg, "TETRISD_LOG_LEVEL", "chatty") == -1);
	assert(cfg_set(&cfg, "TETRISD_DATA_DIR", "") == -1);
	assert(cfg.port == TD_DEF_PORT);
	assert(cfg.tick_ms == TD_DEF_TICK_MS);
	printf("PASS test_set_rejects_bad_values\n");
}

static void	test_parse_line_forms(void)
{
	t_cfg	cfg;

	cfg_defaults(&cfg);
	assert(cfg_parse_line(&cfg, "export TETRISD_PORT=7000") == 0);
	assert(cfg.port == 7000);
	assert(cfg_parse_line(&cfg, "  TETRISD_PORT=7001  ") == 0);
	assert(cfg.port == 7001);
	assert(cfg_parse_line(&cfg, "export TETRISD_CERT_PATH=\"/tmp/a.crt\"") == 0);
	assert(strcmp(cfg.cert_path, "/tmp/a.crt") == 0);
	assert(cfg_parse_line(&cfg, "# export TETRISD_PORT=1") == 0);
	assert(cfg_parse_line(&cfg, "") == 0);
	assert(cfg_parse_line(&cfg, "dspawn tetrislogd") == 0);
	assert(cfg_parse_line(&cfg, "PATH=/usr/bin") == 0);
	assert(cfg.port == 7001);
	assert(cfg_parse_line(&cfg, "export TETRISD_PORT=notanumber") == -1);
	printf("PASS test_parse_line_forms\n");
}

static void	test_load_reads_rc_file(void)
{
	t_cfg	cfg;
	char	dir[64];
	char	rc[128];

	make_tmp_dir(dir, sizeof(dir));
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "# tetrisd\nexport TETRISD_PORT=6001\n"
		"export TETRISD_DATA_DIR=/tmp/dd\ndspawn tetrisd\n");
	unsetenv("TETRISD_PORT");
	assert(cfg_load(&cfg, rc) == 0);
	assert(cfg.port == 6001);
	assert(strcmp(cfg.data_dir, "/tmp/dd") == 0);
	assert(strcmp(cfg.rc_path, rc) == 0);
	assert(strcmp(cfg.cert_path, TD_DEF_CERT) == 0);
	unlink(rc);
	rmdir(dir);
	printf("PASS test_load_reads_rc_file\n");
}

static void	test_env_overrides_file(void)
{
	t_cfg	cfg;
	char	dir[64];
	char	rc[128];

	make_tmp_dir(dir, sizeof(dir));
	snprintf(rc, sizeof(rc), "%s/rc", dir);
	write_file(rc, "export TETRISD_PORT=6001\n");
	setenv("TETRISD_PORT", "6002", 1);
	assert(cfg_load(&cfg, rc) == 0);
	assert(cfg.port == 6002);
	unsetenv("TETRISD_PORT");
	unlink(rc);
	rmdir(dir);
	printf("PASS test_env_overrides_file\n");
}

static void	test_resolve_rc_order(void)
{
	char	out[TD_PATH_MAX];

	setenv("TETRISHRC", "/tmp/from-env", 1);
	assert(cfg_resolve_rc(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/from-env") == 0);
	assert(cfg_resolve_rc("/tmp/from-argv", out, sizeof(out)) == 0);
	assert(strcmp(out, "/tmp/from-argv") == 0);
	unsetenv("TETRISHRC");
	assert(cfg_resolve_rc(NULL, out, sizeof(out)) == 0);
	assert(strcmp(out, "./" TD_RC_NAME) == 0);
	printf("PASS test_resolve_rc_order\n");
}

static void	test_validate_requires_certificates(void)
{
	t_cfg	cfg;
	char	dir[64];
	char	path[128];

	make_tmp_dir(dir, sizeof(dir));
	cfg_defaults(&cfg);
	snprintf(path, sizeof(path), "%s/missing.crt", dir);
	assert(cfg_set(&cfg, "TETRISD_CERT_PATH", path) == 0);
	assert(cfg_validate(&cfg) == -1);
	write_file(path, "cert\n");
	snprintf(path, sizeof(path), "%s/server.key", dir);
	assert(cfg_set(&cfg, "TETRISD_KEY_PATH", path) == 0);
	assert(cfg_validate(&cfg) == -1);
	write_file(path, "key\n");
	assert(cfg_validate(&cfg) == 0);
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
	assert(net_mkdir_p("tests/tmp") == 0);
	assert(mkdtemp(out) != NULL);
}
