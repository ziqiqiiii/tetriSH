/* ************************************************************************** */
/*                                                                            */
/*   test_detach.c - the one byte that separates "started" from "died"        */
/*                                                                            */
/*   The whole reason this library exists rather than reusing the shell's     */
/*   daemon_spawn is that the shell's pipe closes on success and on death     */
/*   alike, so its parent always exits EXIT_SUCCESS. These cases pin the      */
/*   difference: a daemon that reports ready exits the parent 0, and one that  */
/*   dies during boot exits it non-zero, which is what makes a failed launch   */
/*   testable in a script at all.                                             */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"
#include <assert.h>

// Static Functions
static void	test_a_ready_daemon_exits_the_parent_zero(void);
static void	test_a_dying_daemon_exits_the_parent_nonzero(void);
static void	test_the_daemon_is_not_a_session_leader(void);
static void	test_stderr_redirect_moves_the_stream_to_a_file(void);
static void	test_stderr_redirect_refuses_a_bad_path(void);

static void	ready_daemon(const char *marker);
static void	dying_daemon(void);
static void	touch(const char *path);

int	main(void)
{
	/* This suite forks, and cd_detach flushes stdio before it does - so a
	 * PASS line still sitting in a block-buffered pipe would be written once
	 * by this process and again by every child. Line buffering retires each
	 * line before the next fork can inherit it. */
	setvbuf(stdout, NULL, _IOLBF, 0);
	test_a_ready_daemon_exits_the_parent_zero();
	test_a_dying_daemon_exits_the_parent_nonzero();
	test_the_daemon_is_not_a_session_leader();
	test_stderr_redirect_moves_the_stream_to_a_file();
	test_stderr_redirect_refuses_a_bad_path();
	return (0);
}

static void	test_a_ready_daemon_exits_the_parent_zero(void)
{
	char	dir[FX_DIR_MAX];
	char	marker[FX_PATH_MAX];
	pid_t	pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(marker, sizeof(marker), "%s/up", dir);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
		ready_daemon(marker);
	assert(fx_reap(pid) == EXIT_SUCCESS);
	assert(fx_wait_file(marker, 2000) == 0);
	fx_rmtree(dir);
	printf("PASS test_a_ready_daemon_exits_the_parent_zero\n");
}

static void	test_a_dying_daemon_exits_the_parent_nonzero(void)
{
	pid_t	pid;

	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
		dying_daemon();
	assert(fx_reap(pid) == EXIT_FAILURE);
	printf("PASS test_a_dying_daemon_exits_the_parent_nonzero\n");
}

static void	test_the_daemon_is_not_a_session_leader(void)
{
	char	dir[FX_DIR_MAX];
	char	marker[FX_PATH_MAX];
	char	text[64];
	pid_t	pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(marker, sizeof(marker), "%s/sid", dir);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
		ready_daemon(marker);
	assert(fx_reap(pid) == EXIT_SUCCESS);
	assert(fx_wait_file(marker, 2000) == 0);
	assert(fx_slurp(marker, text, sizeof(text)) > 0);
	assert(strcmp(text, "detached\n") == 0);
	fx_rmtree(dir);
	printf("PASS test_the_daemon_is_not_a_session_leader\n");
}

static void	test_stderr_redirect_moves_the_stream_to_a_file(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	char	text[128];
	pid_t	pid;

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(path, sizeof(path), "%s/deep/err.log", dir);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
	{
		if (cd_stderr_redirect(path) != 0)
			_exit(1);
		fprintf(stderr, "moved\n");
		fflush(stderr);
		_exit(0);
	}
	assert(fx_reap(pid) == 0);
	assert(fx_slurp(path, text, sizeof(text)) > 0);
	assert(strcmp(text, "moved\n") == 0);
	fx_rmtree(dir);
	printf("PASS test_stderr_redirect_moves_the_stream_to_a_file\n");
}

static void	test_stderr_redirect_refuses_a_bad_path(void)
{
	char	dir[FX_DIR_MAX];
	char	path[FX_PATH_MAX];
	char	blocker[FX_PATH_MAX];

	assert(fx_tmpdir(dir, sizeof(dir)) == 0);
	snprintf(blocker, sizeof(blocker), "%s/wall", dir);
	touch(blocker);
	snprintf(path, sizeof(path), "%s/wall/err.log", dir);
	assert(cd_stderr_redirect(path) == -1);
	assert(cd_stderr_redirect(NULL) == -1);
	assert(cd_stderr_redirect("") == -1);
	fx_rmtree(dir);
	printf("PASS test_stderr_redirect_refuses_a_bad_path\n");
}

/**
 * @brief Body of a daemon that boots cleanly: detach, prove it, report ready.
 *
 * The marker is written before cd_ready and the session check with it, so the
 * assertions in the parent cannot pass on a process that merely forked.
 *
 * @param marker Path this daemon creates once it is detached.
 */
static void	ready_daemon(const char *marker)
{
	int		ready;
	int		fd;

	if (cd_detach(&ready) != 0)
		_exit(1);
	if (getpid() == getsid(0))
		_exit(1);
	fd = open(marker, O_WRONLY | O_CREAT | O_TRUNC, CD_FILE_MODE);
	if (fd < 0)
		_exit(1);
	if (write(fd, "detached\n", 9) != 9)
		_exit(1);
	close(fd);
	cd_ready(ready);
	_exit(0);
}

/**
 * @brief Body of a daemon whose boot fails before it ever reports ready.
 */
static void	dying_daemon(void)
{
	int	ready;

	if (cd_detach(&ready) != 0)
		_exit(1);
	_exit(3);
}

/**
 * @brief Creates an empty regular file.
 *
 * @param path File to create.
 */
static void	touch(const char *path)
{
	int	fd;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, CD_FILE_MODE);
	assert(fd >= 0);
	close(fd);
}
