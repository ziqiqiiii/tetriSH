/*
 * tests/unit/test_09i_usage.c
 *
 * Unit tests for usage() in src/09i_usage.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

void	setUp(void) {}
void	tearDown(void) {}

static int	capture_stdout(char **cmd, char *buf, size_t size)
{
	int		pipefd[2];
	int		saved;
	ssize_t	n;

	fflush(stdout);
	if (pipe(pipefd) != 0)
		return (-1);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	usage(cmd);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	n = read(pipefd[0], buf, size - 1);
	close(pipefd[0]);
	if (n < 0)
		return (-1);
	buf[n] = '\0';
	return ((int)n);
}

static void	test_usage_no_args_returns_success(void)
{
	char	*cmd[] = {"usage", NULL};
	char	buf[1024];
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = usage(cmd);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
}

static void	test_usage_no_args_prints_general(void)
{
	char	*cmd[] = {"usage", NULL};
	char	buf[1024];

	capture_stdout(cmd, buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "minishell usage:"));
}

static void	test_usage_echo_returns_success(void)
{
	char	*cmd[] = {"usage", "echo", NULL};
	char	buf[1024];
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = usage(cmd);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
}

static void	test_usage_echo_prints_detail(void)
{
	char	*cmd[] = {"usage", "echo", NULL};
	char	buf[1024];

	capture_stdout(cmd, buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "echo"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "-n"));
}

static void	test_usage_cd_prints_detail(void)
{
	char	*cmd[] = {"usage", "cd", NULL};
	char	buf[1024];

	capture_stdout(cmd, buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "cd"));
}

static void	test_usage_unknown_returns_failure(void)
{
	char	*cmd[] = {"usage", "nonexistent", NULL};
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	ret = usage(cmd);
	dup2(saved, STDERR_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, ret);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_usage_no_args_returns_success);
	RUN_TEST(test_usage_no_args_prints_general);
	RUN_TEST(test_usage_echo_returns_success);
	RUN_TEST(test_usage_echo_prints_detail);
	RUN_TEST(test_usage_cd_prints_detail);
	RUN_TEST(test_usage_unknown_returns_failure);
	return UNITY_END();
}
