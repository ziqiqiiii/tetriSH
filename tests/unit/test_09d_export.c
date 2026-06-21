/*
 * tests/unit/test_09d_export.c
 *
 * Unit tests for export() and invalid_identifier() in src/09d_export.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* --- helpers --- */

static t_list	*g_env;

static void	make_env(char **envp)
{
	env_link_list(envp, &g_env);
}

void	setUp(void)
{
	g_env = NULL;
	g_exit_status = 0;
}

void	tearDown(void)
{
	ft_lstclear(&g_env, del_data);
}

static int	capture_stdout_export(char **cmd, char *buf, size_t size)
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
	export(cmd, &g_env);
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

/* --- invalid_identifier tests --- */

static void	test_invalid_identifier_starts_with_digit(void)
{
	int		pipefd[2];
	int		saved;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, invalid_identifier("1BAD=val"));
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
}

static void	test_invalid_identifier_equals_only(void)
{
	int		pipefd[2];
	int		saved;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, invalid_identifier("=val"));
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
}

static void	test_invalid_identifier_special_char(void)
{
	int		pipefd[2];
	int		saved;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, invalid_identifier("A-B=val"));
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
}

static void	test_valid_identifier_simple(void)
{
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, invalid_identifier("FOO=bar"));
}

static void	test_valid_identifier_underscore(void)
{
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, invalid_identifier("_VAR=1"));
}

static void	test_valid_identifier_no_equals(void)
{
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, invalid_identifier("FOO"));
}

/* --- export tests --- */

static void	test_export_no_args_prints_declare(void)
{
	char	*envp[] = {"FOO=bar", NULL};
	char	*cmd[] = {"export", NULL};
	char	buf[512];

	make_env(envp);
	capture_stdout_export(cmd, buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "declare -x FOO=\"bar\""));
}

static void	test_export_no_args_null_value_no_quotes(void)
{
	char	*cmd[] = {"export", NULL};
	char	buf[512];

	creat_new_env_node("BARE", "BARE", &g_env);
	capture_stdout_export(cmd, buf, sizeof(buf));
	TEST_ASSERT_NOT_NULL(strstr(buf, "declare -x BARE\n"));
	TEST_ASSERT_NULL(strstr(buf, "declare -x BARE=\""));
}

static void	test_export_adds_new_variable(void)
{
	char	*cmd[] = {"export", "NEW=val", NULL};
	char	*val;

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, export(cmd, &g_env));
	val = existed_env("NEW", &g_env);
	TEST_ASSERT_EQUAL_STRING("val", val);
}

static void	test_export_modifies_existing_variable(void)
{
	char	*envp[] = {"KEY=old", NULL};
	char	*cmd[] = {"export", "KEY=new", NULL};
	char	*val;

	make_env(envp);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, export(cmd, &g_env));
	val = existed_env("KEY", &g_env);
	TEST_ASSERT_EQUAL_STRING("new", val);
}

static void	test_export_invalid_returns_failure(void)
{
	char	*cmd[] = {"export", "1BAD=val", NULL};
	int		pipefd[2];
	int		saved;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, export(cmd, &g_env));
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_invalid_identifier_starts_with_digit);
	RUN_TEST(test_invalid_identifier_equals_only);
	RUN_TEST(test_invalid_identifier_special_char);
	RUN_TEST(test_valid_identifier_simple);
	RUN_TEST(test_valid_identifier_underscore);
	RUN_TEST(test_valid_identifier_no_equals);
	RUN_TEST(test_export_no_args_prints_declare);
	RUN_TEST(test_export_no_args_null_value_no_quotes);
	RUN_TEST(test_export_adds_new_variable);
	RUN_TEST(test_export_modifies_existing_variable);
	RUN_TEST(test_export_invalid_returns_failure);
	return UNITY_END();
}
