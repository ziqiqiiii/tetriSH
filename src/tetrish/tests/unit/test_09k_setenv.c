/*
 * tests/unit/test_09k_setenv.c
 *
 * Unit tests for set_env() in src/09k_setenv.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

static t_list	*g_env;

void	setUp(void)
{
	g_env = NULL;
}

void	tearDown(void)
{
	ft_lstclear(&g_env, del_data);
}

/* --- tests --- */

static void	test_set_env_adds_variable(void)
{
	char	*cmd[] = {"setenv", "FOO=bar", NULL};

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, set_env(cmd, &g_env));
	TEST_ASSERT_EQUAL_STRING("bar", existed_env("FOO", &g_env));
}

static void	test_set_env_returns_success(void)
{
	char	*cmd[] = {"setenv", "A=1", NULL};

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, set_env(cmd, &g_env));
}

static void	test_set_env_returns_failure_for_invalid(void)
{
	char	*cmd[] = {"setenv", "1BAD=x", NULL};
	int		pipefd[2];
	int		saved;
	int		ret;

	fflush(stdout);
	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = set_env(cmd, &g_env);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, ret);
}

static void	test_set_env_updates_existing(void)
{
	char	*envp[] = {"KEY=old", NULL};
	char	*cmd[] = {"setenv", "KEY=new", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, set_env(cmd, &g_env));
	TEST_ASSERT_EQUAL_STRING("new", existed_env("KEY", &g_env));
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_set_env_adds_variable);
	RUN_TEST(test_set_env_returns_success);
	RUN_TEST(test_set_env_returns_failure_for_invalid);
	RUN_TEST(test_set_env_updates_existing);
	return UNITY_END();
}
