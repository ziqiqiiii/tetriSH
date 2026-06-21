/*
 * tests/unit/test_09l_unsetenv.c
 *
 * Unit tests for unset_env() in src/09l_unsetenv.c.
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

static void	test_unset_env_removes_variable(void)
{
	char	*envp[] = {"FOO=bar", "BAZ=qux", NULL};
	char	*cmd[] = {"unsetenv", "FOO", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, unset_env(cmd, &g_env));
	TEST_ASSERT_NULL(existed_env("FOO", &g_env));
	TEST_ASSERT_NOT_NULL(existed_env("BAZ", &g_env));
}

static void	test_unset_env_returns_success(void)
{
	char	*envp[] = {"BAR=1", NULL};
	char	*cmd[] = {"unsetenv", "BAR", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, unset_env(cmd, &g_env));
}

static void	test_unset_env_nonexistent_is_noop(void)
{
	char	*envp[] = {"A=1", NULL};
	char	*cmd[] = {"unsetenv", "NOPE", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, unset_env(cmd, &g_env));
	TEST_ASSERT_NOT_NULL(existed_env("A", &g_env));
}

static void	test_unset_env_multiple_args(void)
{
	char	*envp[] = {"A=1", "B=2", "C=3", NULL};
	char	*cmd[] = {"unsetenv", "A", "C", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, unset_env(cmd, &g_env));
	TEST_ASSERT_NULL(existed_env("A", &g_env));
	TEST_ASSERT_NOT_NULL(existed_env("B", &g_env));
	TEST_ASSERT_NULL(existed_env("C", &g_env));
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_unset_env_removes_variable);
	RUN_TEST(test_unset_env_returns_success);
	RUN_TEST(test_unset_env_nonexistent_is_noop);
	RUN_TEST(test_unset_env_multiple_args);
	return UNITY_END();
}
