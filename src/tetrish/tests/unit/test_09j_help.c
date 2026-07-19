/*
 * tests/unit/test_09j_help.c
 *
 * Unit tests for help() in src/09j_help.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

void	setUp(void) {}
void	tearDown(void) {}

static int	capture_help(char *buf, size_t size)
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
	help();
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

static void	test_help_returns_success(void)
{
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = help();
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
}

static void	test_help_contains_header(void)
{
	char	buf[2048];

	capture_help(buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "Built-in commands:"));
}

static void	test_help_lists_echo(void)
{
	char	buf[2048];

	capture_help(buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "echo"));
}

static void	test_help_lists_cd(void)
{
	char	buf[2048];

	capture_help(buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "cd"));
}

static void	test_help_lists_exit(void)
{
	char	buf[2048];

	capture_help(buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "exit"));
}

static void	test_help_shows_usage_hint(void)
{
	char	buf[2048];

	capture_help(buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "usage"));
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_help_returns_success);
	RUN_TEST(test_help_contains_header);
	RUN_TEST(test_help_lists_echo);
	RUN_TEST(test_help_lists_cd);
	RUN_TEST(test_help_lists_exit);
	RUN_TEST(test_help_shows_usage_hint);
	return UNITY_END();
}
