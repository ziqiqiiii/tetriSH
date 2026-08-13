/*
 * tests/unit/test_09h_exit.c
 *
 * Unit tests for exit_status() and exit_command() in src/09h_exit.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

static t_root	g_sh;

void	setUp(void)
{
	g_exit_status = 0;
	memset(&g_sh, 0, sizeof(t_root));
}

void	tearDown(void) {}

/* --- exit_status tests --- */

static void	test_exit_status_normal_exit_zero(void)
{
	int	status;

	/* simulate WIFEXITED with code 0 */
	status = 0;
	TEST_ASSERT_EQUAL_INT(0, exit_status(status));
}

static void	test_exit_status_normal_exit_nonzero(void)
{
	/* WEXITSTATUS encodes exit code in bits 8..15 */
	int	status;

	status = 42 << 8;
	TEST_ASSERT_EQUAL_INT(42, exit_status(status));
}

/* --- exit_command tests --- */

static void	test_exit_command_no_args_returns_g_exit_status(void)
{
	char	*cmd[] = {"exit", NULL};
	int		ret;

	g_exit_status = 7;
	ret = exit_command(cmd, &g_sh);
	TEST_ASSERT_EQUAL_INT(7, ret);
	TEST_ASSERT_EQUAL_INT(1, g_sh.exit_cmd_flag);
}

static void	test_exit_command_numeric_arg(void)
{
	char	*cmd[] = {"exit", "42", NULL};
	int		ret;

	ret = exit_command(cmd, &g_sh);
	TEST_ASSERT_EQUAL_INT(42, ret);
	TEST_ASSERT_EQUAL_INT(1, g_sh.exit_cmd_flag);
}

static void	test_exit_command_negative_arg(void)
{
	char	*cmd[] = {"exit", "-1", NULL};
	int		ret;

	ret = exit_command(cmd, &g_sh);
	TEST_ASSERT_EQUAL_INT(-1, ret);
}

static void	test_exit_command_non_numeric_prints_error(void)
{
	char	*cmd[] = {"exit", "abc", NULL};
	char	buf[256];
	int		ret;

	/* capture stderr */
	int		pipefd[2];
	int		saved;

	pipe(pipefd);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	ret = exit_command(cmd, &g_sh);
	dup2(saved, STDERR_FILENO);
	close(saved);
	read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_RANGE, ret);
}

static void	test_exit_command_too_many_args(void)
{
	char	*cmd[] = {"exit", "1", "2", NULL};
	char	buf[256];
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	ret = exit_command(cmd, &g_sh);
	dup2(saved, STDERR_FILENO);
	close(saved);
	read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, ret);
	TEST_ASSERT_EQUAL_INT(0, g_sh.exit_cmd_flag);
}

static void	test_exit_command_zero(void)
{
	char	*cmd[] = {"exit", "0", NULL};
	int		ret;

	ret = exit_command(cmd, &g_sh);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_exit_status_normal_exit_zero);
	RUN_TEST(test_exit_status_normal_exit_nonzero);
	RUN_TEST(test_exit_command_no_args_returns_g_exit_status);
	RUN_TEST(test_exit_command_numeric_arg);
	RUN_TEST(test_exit_command_negative_arg);
	RUN_TEST(test_exit_command_non_numeric_prints_error);
	RUN_TEST(test_exit_command_too_many_args);
	RUN_TEST(test_exit_command_zero);
	return UNITY_END();
}
