/* ************************************************************************** */
/*                                                                            */
/*   test_pidfile.c - the lock that is the single-instance guard              */
/*                                                                            */
/*   One mechanism has to answer three questions: which pid to signal,        */
/*   whether a second instance may start, and whether a pidfile left behind    */
/*   is stale. These cases pin all three, and in particular that a file with   */
/*   a plausible pid in it counts as nothing once its writer is gone.         */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_claim_writes_the_pid_and_makes_parents(void);
static void	test_a_second_claim_loses_the_lock(void);
static void	test_release_hands_the_lock_on(void);
static void	test_probe_reports_nothing_for_a_missing_pidfile(void);
static void	test_probe_reports_nothing_for_a_stale_pidfile(void);
static void	test_probe_reports_the_pid_of_a_live_holder(void);
static void	test_read_parses_an_unheld_pidfile(void);
static void	test_release_is_safe_on_a_blank_pidfile(void);

static void	pid_path(char *out, size_t cap, const char *dir, const char *leaf);

int	main(void)
{
	test_claim_writes_the_pid_and_makes_parents();
	test_a_second_claim_loses_the_lock();
	test_release_hands_the_lock_on();
	test_probe_reports_nothing_for_a_missing_pidfile();
	test_probe_reports_nothing_for_a_stale_pidfile();
	test_probe_reports_the_pid_of_a_live_holder();
	test_read_parses_an_unheld_pidfile();
	test_release_is_safe_on_a_blank_pidfile();
	return (0);
}

static void	test_claim_writes_the_pid_and_makes_parents(void)
{
	t_pidfile	pf;
	char		dir[FX_DIR_MAX];
	char		path[FX_PATH_MAX];
	char		text[64];
	char		want[64];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "deep/nested/x.pid");
	daemon_pid_blank(&pf);
	assert(daemon_pid_claim(&pf, path) == 0);
	assert(pf.pid == getpid());
	assert(fx_slurp(path, text, sizeof(text)) > 0);
	snprintf(want, sizeof(want), "%d\n", (int)getpid());
	assert(strcmp(text, want) == 0);
	daemon_pid_release(&pf);
	fx_rmtree(dir);
	printf("PASS test_claim_writes_the_pid_and_makes_parents\n");
}

static void	test_a_second_claim_loses_the_lock(void)
{
	t_holder	h;
	t_pidfile	pf;
	char		dir[FX_DIR_MAX];
	char		path[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "x.pid");
	assert(fx_hold(&h, path) == 0);
	daemon_pid_blank(&pf);
	errno = 0;
	assert(daemon_pid_claim(&pf, path) == -1);
	assert(errno == EWOULDBLOCK || errno == EAGAIN);
	assert(pf.fd == -1);
	fx_stop(&h);
	fx_rmtree(dir);
	printf("PASS test_a_second_claim_loses_the_lock\n");
}

static void	test_release_hands_the_lock_on(void)
{
	t_holder	h;
	t_pidfile	pf;
	char		dir[FX_DIR_MAX];
	char		path[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "x.pid");
	assert(fx_hold(&h, path) == 0);
	fx_stop(&h);
	daemon_pid_blank(&pf);
	assert(daemon_pid_claim(&pf, path) == 0);
	assert(pf.pid == getpid());
	daemon_pid_release(&pf);
	assert(pf.fd == -1);
	fx_rmtree(dir);
	printf("PASS test_release_hands_the_lock_on\n");
}

static void	test_probe_reports_nothing_for_a_missing_pidfile(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	pid_t	pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "absent.pid");
	pid = 4242;
	assert(daemon_pid_probe(path, &pid) == 0);
	assert(pid == 0);
	fx_rmtree(dir);
	printf("PASS test_probe_reports_nothing_for_a_missing_pidfile\n");
}

static void	test_probe_reports_nothing_for_a_stale_pidfile(void)
{
	t_holder	h;
	char		dir[FX_DIR_MAX];
	char		path[FX_PATH_MAX];
	char		text[64];
	pid_t		pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "x.pid");
	assert(fx_hold(&h, path) == 0);
	fx_stop(&h);
	assert(fx_slurp(path, text, sizeof(text)) > 0);
	pid = 4242;
	assert(daemon_pid_probe(path, &pid) == 0);
	assert(pid == 0);
	fx_rmtree(dir);
	printf("PASS test_probe_reports_nothing_for_a_stale_pidfile\n");
}

static void	test_probe_reports_the_pid_of_a_live_holder(void)
{
	t_holder	h;
	char		dir[FX_DIR_MAX];
	char		path[FX_PATH_MAX];
	pid_t		pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "x.pid");
	assert(fx_hold(&h, path) == 0);
	pid = 0;
	assert(daemon_pid_probe(path, &pid) == 1);
	assert(pid == h.pid);
	assert(daemon_pid_wait(path, 40) == -1 && errno == ETIMEDOUT);
	fx_stop(&h);
	assert(daemon_pid_wait(path, 1000) == 0);
	fx_rmtree(dir);
	printf("PASS test_probe_reports_the_pid_of_a_live_holder\n");
}

static void	test_read_parses_an_unheld_pidfile(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	pid_t	pid;
	int		fd;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	pid_path(path, sizeof(path), dir, "x.pid");
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, DAEMON_FILE_MODE);
	assert(fd >= 0 && write(fd, "1234\n", 5) == 5);
	close(fd);
	pid = 0;
	assert(daemon_pid_read(path, &pid) == 0);
	assert(pid == 1234);
	fd = open(path, O_WRONLY | O_TRUNC);
	assert(fd >= 0 && write(fd, "not-a-pid\n", 10) == 10);
	close(fd);
	assert(daemon_pid_read(path, &pid) == -1);
	fx_rmtree(dir);
	printf("PASS test_read_parses_an_unheld_pidfile\n");
}

static void	test_release_is_safe_on_a_blank_pidfile(void)
{
	t_pidfile	pf;

	daemon_pid_blank(&pf);
	assert(pf.fd == -1);
	daemon_pid_release(&pf);
	daemon_pid_release(&pf);
	daemon_pid_release(NULL);
	assert(daemon_pid_claim(NULL, "x") == -1);
	assert(daemon_pid_claim(&pf, NULL) == -1);
	assert(daemon_pid_read(NULL, NULL) == -1);
	assert(daemon_pid_probe(NULL, NULL) == -1);
	printf("PASS test_release_is_safe_on_a_blank_pidfile\n");
}

static void	pid_path(char *out, size_t cap, const char *dir, const char *leaf)
{
	snprintf(out, cap, "%s/%s", dir, leaf);
}
