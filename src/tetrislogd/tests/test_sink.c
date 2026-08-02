/* ************************************************************************** */
/*                                                                            */
/*   test_sink.c - the log file and the lock that guards it                   */
/*                                                                            */
/*   The sink is the one resource that must have exactly one owner. These     */
/*   cases pin the exclusive lock, the verbatim append, the rotation reopen,   */
/*   and the rule that an unavailable sink is a working state rather than a    */
/*   fatal one.                                                               */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_open_creates_file_and_parents(void);
static void	test_open_takes_an_exclusive_lock(void);
static void	test_open_fails_when_the_parent_is_a_file(void);
static void	test_write_appends_verbatim(void);
static void	test_write_fails_when_the_sink_is_closed(void);
static void	test_reopen_starts_a_fresh_file(void);
static void	test_reopen_failure_leaves_the_sink_closed(void);
static void	test_teardown_is_safe_on_a_blank_sink(void);

static void	touch(const char *path);
static void	put(t_sink *sk, const char *line);

int	main(void)
{
	test_open_creates_file_and_parents();
	test_open_takes_an_exclusive_lock();
	test_open_fails_when_the_parent_is_a_file();
	test_write_appends_verbatim();
	test_write_fails_when_the_sink_is_closed();
	test_reopen_starts_a_fresh_file();
	test_reopen_failure_leaves_the_sink_closed();
	test_teardown_is_safe_on_a_blank_sink();
	return (0);
}

static void	test_open_creates_file_and_parents(void)
{
	t_fixture	fx;
	struct stat	st;
	t_sink		sk;
	char		path[256];

	assert(fx_make(&fx) == 0);
	snprintf(path, sizeof(path), "%s/deep/nested/logd.log", fx.dir);
	sink_blank(&sk);
	assert(sink_open(&sk, path) == 0);
	assert(sink_is_open(&sk) == true);
	assert(stat(path, &st) == 0 && S_ISREG(st.st_mode));
	sink_close(&sk);
	assert(sink_is_open(&sk) == false);
	fx_destroy(&fx);
	printf("PASS test_open_creates_file_and_parents\n");
}

/*
** dspawn does not stop a double launch - it registers the second process as
** tetrislogd.1 and runs it. Without this lock that second process would
** unlink the live socket (us_dgram_bind unlinks unconditionally) and leave
** the first logger holding a descriptor nobody sends to. The lock has to be
** distinguishable from an ordinary open failure, because "already running"
** and "cannot write here" call for different messages.
*/
static void	test_open_takes_an_exclusive_lock(void)
{
	t_fixture	fx;
	t_sink		first;
	t_sink		second;

	assert(fx_make(&fx) == 0);
	sink_blank(&first);
	sink_blank(&second);
	assert(sink_open(&first, fx.file_path) == 0);
	errno = 0;
	assert(sink_open(&second, fx.file_path) == -1);
	assert(errno == EWOULDBLOCK || errno == EAGAIN);
	assert(sink_is_open(&second) == false);
	sink_close(&first);
	assert(sink_open(&second, fx.file_path) == 0);
	sink_close(&second);
	fx_destroy(&fx);
	printf("PASS test_open_takes_an_exclusive_lock\n");
}

static void	test_open_fails_when_the_parent_is_a_file(void)
{
	t_fixture	fx;
	t_sink		sk;
	char		blocked[256];
	char		path[256];

	assert(fx_make(&fx) == 0);
	snprintf(blocked, sizeof(blocked), "%s/blocked", fx.dir);
	snprintf(path, sizeof(path), "%s/blocked/logd.log", fx.dir);
	touch(blocked);
	sink_blank(&sk);
	assert(sink_open(&sk, path) == -1);
	assert(sink_is_open(&sk) == false);
	fx_destroy(&fx);
	printf("PASS test_open_fails_when_the_parent_is_a_file\n");
}

/*
** lr_format_line already ends the line with a newline. Adding a second one is
** what makes tetrisd's stderr fallback print a blank line after every record,
** and a file full of blank lines is a file nobody greps twice.
*/
static void	test_write_appends_verbatim(void)
{
	t_fixture	fx;
	char		buf[512];
	t_sink		sk;

	assert(fx_make(&fx) == 0);
	sink_blank(&sk);
	assert(sink_open(&sk, fx.file_path) == 0);
	put(&sk, "first line\n");
	put(&sk, "second line\n");
	assert(sink_sync(&sk) == 0);
	assert(fx_slurp(fx.file_path, buf, sizeof(buf)) == 23);
	assert(strcmp(buf, "first line\nsecond line\n") == 0);
	assert(fx_line_count(fx.file_path) == 2);
	sink_close(&sk);
	fx_destroy(&fx);
	printf("PASS test_write_appends_verbatim\n");
}

static void	test_write_fails_when_the_sink_is_closed(void)
{
	t_sink	sk;

	sink_blank(&sk);
	assert(sink_write(&sk, "x\n", 2) == -1);
	assert(sink_sync(&sk) == 0);
	printf("PASS test_write_fails_when_the_sink_is_closed\n");
}

/*
** Rotation is done to the daemon, not by it: an external mover renames the
** file and sends SIGHUP. Reopening the unchanged path is what puts the next
** record in a fresh file while the renamed one keeps everything written
** before the signal.
*/
static void	test_reopen_starts_a_fresh_file(void)
{
	t_fixture	fx;
	t_sink		sk;
	char		moved[256];

	assert(fx_make(&fx) == 0);
	snprintf(moved, sizeof(moved), "%s.1", fx.file_path);
	sink_blank(&sk);
	assert(sink_open(&sk, fx.file_path) == 0);
	put(&sk, "before rotation\n");
	assert(sink_sync(&sk) == 0);
	assert(rename(fx.file_path, moved) == 0);
	assert(sink_reopen(&sk) == 0);
	assert(sink_is_open(&sk) == true);
	put(&sk, "after rotation\n");
	assert(sink_sync(&sk) == 0);
	assert(fx_contains(moved, "before rotation") == 1);
	assert(fx_contains(fx.file_path, "after rotation") == 1);
	assert(fx_contains(fx.file_path, "before rotation") == 0);
	sink_close(&sk);
	fx_destroy(&fx);
	printf("PASS test_reopen_starts_a_fresh_file\n");
}

/*
** Closing to rotate releases the flock, so the reopen can genuinely fail. The
** sink must then report itself closed rather than pretend - the caller reads
** that as "degrade to stderr and retry", which is the whole reason a failed
** reopen is not fatal.
*/
static void	test_reopen_failure_leaves_the_sink_closed(void)
{
	t_fixture	fx;
	t_sink		sk;
	char		sub[256];
	char		path[256];
	char		cmd[640];

	assert(fx_make(&fx) == 0);
	snprintf(sub, sizeof(sub), "%s/sub", fx.dir);
	snprintf(path, sizeof(path), "%s/sub/logd.log", fx.dir);
	sink_blank(&sk);
	assert(sink_open(&sk, path) == 0);
	snprintf(cmd, sizeof(cmd), "rm -rf %s && touch %s", sub, sub);
	assert(system(cmd) == 0);
	assert(sink_reopen(&sk) == -1);
	assert(sink_is_open(&sk) == false);
	sink_close(&sk);
	fx_destroy(&fx);
	printf("PASS test_reopen_failure_leaves_the_sink_closed\n");
}

/*
** Teardown is shared between a clean stop and a boot that failed before the
** sink was ever opened, and a zeroed fd is stdout - so a blank sink must not
** close the caller's standard output.
*/
static void	test_teardown_is_safe_on_a_blank_sink(void)
{
	t_sink	sk;

	sink_blank(&sk);
	assert(sk.fd == -1);
	assert(sink_is_open(&sk) == false);
	sink_close(&sk);
	sink_close(&sk);
	assert(write(STDOUT_FILENO, "", 0) == 0);
	printf("PASS test_teardown_is_safe_on_a_blank_sink\n");
}

static void	touch(const char *path)
{
	int	fd;

	fd = open(path, O_WRONLY | O_CREAT, TL_FILE_MODE);
	assert(fd >= 0);
	close(fd);
}

static void	put(t_sink *sk, const char *line)
{
	assert(sink_write(sk, line, strlen(line)) == 0);
}
