#include "tetrisd.h"

#include <assert.h>

// Static Functions
static void	test_records_reach_the_logger(void);
static void	test_level_filter_skips_quieter_records(void);
static void	test_absent_logger_falls_back_to_stderr(void);

static void	make_tmp_dir(char *out, size_t cap);
static void	logger_cfg(t_cfg *cfg, const char *sock_path, int level);
static int	wait_record(int fd, t_log_record *out, int timeout_ms);

int	main(void)
{
	test_records_reach_the_logger();
	test_level_filter_skips_quieter_records();
	test_absent_logger_falls_back_to_stderr();
	return (0);
}

static void	test_records_reach_the_logger(void)
{
	t_log_record	rec;
	t_logger		lg;
	t_cfg			cfg;
	char			dir[128];
	char			sock[192];
	int				rx;

	make_tmp_dir(dir, sizeof(dir));
	snprintf(sock, sizeof(sock), "%s/log.sock", dir);
	rx = us_dgram_bind(sock, 0600);
	assert(rx >= 0);
	logger_cfg(&cfg, sock, CIPC_LOG_DEBUG);
	assert(log_init(&lg, &cfg) == 0);
	log_emit(&lg, CIPC_LOG_INFO, "player %llu joined %s",
		(unsigned long long)7, "S-01");
	assert(wait_record(rx, &rec, 2000) == 0);
	assert(lr_validate(&rec, sizeof(rec)) == 0);
	assert(rec.level == CIPC_LOG_INFO);
	assert(strcmp(rec.component, TD_COMPONENT) == 0);
	assert(strcmp(rec.msg, "player 7 joined S-01") == 0);
	assert(log_dropped(&lg) == 0);
	log_shutdown(&lg);
	us_close_unlink(rx, sock);
	rmdir(dir);
	printf("PASS test_records_reach_the_logger\n");
}

static void	test_level_filter_skips_quieter_records(void)
{
	t_log_record	rec;
	t_logger		lg;
	t_cfg			cfg;
	char			dir[128];
	char			sock[192];
	int				rx;

	make_tmp_dir(dir, sizeof(dir));
	snprintf(sock, sizeof(sock), "%s/log.sock", dir);
	rx = us_dgram_bind(sock, 0600);
	assert(rx >= 0);
	logger_cfg(&cfg, sock, CIPC_LOG_WARNING);
	assert(log_init(&lg, &cfg) == 0);
	log_emit(&lg, CIPC_LOG_DEBUG, "chatter");
	log_emit(&lg, CIPC_LOG_INFO, "chatter");
	log_emit(&lg, CIPC_LOG_ERROR, "the roof is on fire");
	assert(wait_record(rx, &rec, 2000) == 0);
	assert(rec.level == CIPC_LOG_ERROR);
	assert(strcmp(rec.msg, "the roof is on fire") == 0);
	assert(wait_record(rx, &rec, 100) == -1);
	log_shutdown(&lg);
	us_close_unlink(rx, sock);
	rmdir(dir);
	printf("PASS test_level_filter_skips_quieter_records\n");
}

static void	test_absent_logger_falls_back_to_stderr(void)
{
	t_logger	lg;
	t_cfg		cfg;
	char		dir[128];
	char		sock[192];
	char		out[192];
	char		buf[512];
	FILE		*f;
	int		saved;
	int		fd;

	make_tmp_dir(dir, sizeof(dir));
	snprintf(sock, sizeof(sock), "%s/nobody.sock", dir);
	snprintf(out, sizeof(out), "%s/stderr.txt", dir);
	logger_cfg(&cfg, sock, CIPC_LOG_DEBUG);
	fd = open(out, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	assert(fd >= 0);
	saved = dup(STDERR_FILENO);
	assert(saved >= 0);
	assert(dup2(fd, STDERR_FILENO) >= 0);
	assert(log_init(&lg, &cfg) == 0);
	log_emit(&lg, CIPC_LOG_ERROR, "logger is missing");
	log_shutdown(&lg);
	fflush(stderr);
	assert(dup2(saved, STDERR_FILENO) >= 0);
	close(saved);
	close(fd);
	f = fopen(out, "r");
	assert(f != NULL);
	buf[0] = '\0';
	assert(fgets(buf, sizeof(buf), f) != NULL);
	fclose(f);
	assert(strstr(buf, "logger is missing") != NULL);
	unlink(out);
	rmdir(dir);
	printf("PASS test_absent_logger_falls_back_to_stderr\n");
}

static void	make_tmp_dir(char *out, size_t cap)
{
	snprintf(out, cap, "tests/tmp/logXXXXXX");
	assert(net_mkdir_p("tests/tmp") == 0);
	assert(mkdtemp(out) != NULL);
}

static void	logger_cfg(t_cfg *cfg, const char *sock_path, int level)
{
	cfg_defaults(cfg);
	snprintf(cfg->log_ipc, TD_PATH_MAX, "%s", sock_path);
	cfg->log_level = level;
}

static int	wait_record(int fd, t_log_record *out, int timeout_ms)
{
	struct pollfd	pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeout_ms) <= 0)
		return (-1);
	if (us_dgram_recv(fd, out, sizeof(*out)) != (ssize_t)sizeof(*out))
		return (-1);
	return (0);
}
