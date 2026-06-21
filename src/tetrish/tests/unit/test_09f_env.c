/*
 * tests/unit/test_09f_env.c
 *
 * Unit tests for get_env(), env_link_list(), existed_env(),
 * creat_new_env_node() in src/09f_env.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

static t_list	*g_env;

static void	free_env_node(void *content)
{
	t_env	*data;

	data = (t_env *)content;
	free(data->key);
	free(data->value);
	free(data);
}

void	setUp(void)
{
	g_env = NULL;
}

void	tearDown(void)
{
	ft_lstclear(&g_env, free_env_node);
}

/* --- capture helpers --- */

static int	capture_stdout(char *buf, size_t size, void (*fn)(void))
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
	fn();
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

/* --- env_link_list tests --- */

static void	test_env_link_list_single_entry(void)
{
	char	*envp[] = {"FOO=bar", NULL};
	t_env	*data;

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, env_link_list(envp, &g_env));
	TEST_ASSERT_NOT_NULL(g_env);
	data = (t_env *)g_env->content;
	TEST_ASSERT_EQUAL_STRING("FOO", data->key);
	TEST_ASSERT_EQUAL_STRING("bar", data->value);
	TEST_ASSERT_NULL(g_env->next);
}

static void	test_env_link_list_multiple_entries(void)
{
	char	*envp[] = {"A=1", "B=2", "C=3", NULL};
	int		count;
	t_list	*tmp;

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, env_link_list(envp, &g_env));
	count = 0;
	tmp = g_env;
	while (tmp)
	{
		count++;
		tmp = tmp->next;
	}
	TEST_ASSERT_EQUAL_INT(3, count);
}

static void	test_env_link_list_value_with_equals(void)
{
	char	*envp[] = {"KEY=val=ue", NULL};
	t_env	*data;

	env_link_list(envp, &g_env);
	data = (t_env *)g_env->content;
	TEST_ASSERT_EQUAL_STRING("KEY", data->key);
	TEST_ASSERT_EQUAL_STRING("val=ue", data->value);
}

static void	test_env_link_list_empty_value(void)
{
	char	*envp[] = {"EMPTY=", NULL};
	t_env	*data;

	env_link_list(envp, &g_env);
	data = (t_env *)g_env->content;
	TEST_ASSERT_EQUAL_STRING("EMPTY", data->key);
	TEST_ASSERT_EQUAL_STRING("", data->value);
}

/* --- existed_env tests --- */

static void	test_existed_env_found(void)
{
	char	*envp[] = {"HOME=/Users/test", NULL};
	char	*val;

	env_link_list(envp, &g_env);
	val = existed_env("HOME", &g_env);
	TEST_ASSERT_EQUAL_STRING("/Users/test", val);
}

static void	test_existed_env_not_found(void)
{
	char	*envp[] = {"HOME=/Users/test", NULL};
	char	*val;

	env_link_list(envp, &g_env);
	val = existed_env("NOEXIST", &g_env);
	TEST_ASSERT_NULL(val);
}

static void	test_existed_env_null_key(void)
{
	char	*envp[] = {"HOME=/Users/test", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_NULL(existed_env(NULL, &g_env));
}

static void	test_existed_env_partial_match_no_hit(void)
{
	char	*envp[] = {"HOME=/Users/test", NULL};

	env_link_list(envp, &g_env);
	TEST_ASSERT_NULL(existed_env("HOM", &g_env));
}

/* --- creat_new_env_node tests --- */

static void	test_creat_new_env_node_with_value(void)
{
	t_env	*data;

	creat_new_env_node("NEW", "NEW=hello", &g_env);
	TEST_ASSERT_NOT_NULL(g_env);
	data = (t_env *)g_env->content;
	TEST_ASSERT_EQUAL_STRING("NEW", data->key);
	TEST_ASSERT_EQUAL_STRING("hello", data->value);
}

static void	test_creat_new_env_node_no_value(void)
{
	t_env	*data;

	creat_new_env_node("BARE", "BARE", &g_env);
	TEST_ASSERT_NOT_NULL(g_env);
	data = (t_env *)g_env->content;
	TEST_ASSERT_EQUAL_STRING("BARE", data->key);
	TEST_ASSERT_NULL(data->value);
}

/* --- get_env tests --- */

static t_list	**g_env_ptr_for_capture;

static void	call_get_env(void)
{
	get_env(g_env_ptr_for_capture);
}

static void	test_get_env_prints_key_value(void)
{
	char	*envp[] = {"FOO=bar", NULL};
	char	buf[256];

	env_link_list(envp, &g_env);
	g_env_ptr_for_capture = &g_env;
	capture_stdout(buf, sizeof(buf), call_get_env);
	TEST_ASSERT_EQUAL_STRING("FOO=bar\n", buf);
}

static void	test_get_env_skips_null_value(void)
{
	char	buf[256];

	creat_new_env_node("BARE", "BARE", &g_env);
	g_env_ptr_for_capture = &g_env;
	capture_stdout(buf, sizeof(buf), call_get_env);
	TEST_ASSERT_EQUAL_STRING("", buf);
}

static void	test_get_env_returns_success(void)
{
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, get_env(&g_env));
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_env_link_list_single_entry);
	RUN_TEST(test_env_link_list_multiple_entries);
	RUN_TEST(test_env_link_list_value_with_equals);
	RUN_TEST(test_env_link_list_empty_value);
	RUN_TEST(test_existed_env_found);
	RUN_TEST(test_existed_env_not_found);
	RUN_TEST(test_existed_env_null_key);
	RUN_TEST(test_existed_env_partial_match_no_hit);
	RUN_TEST(test_creat_new_env_node_with_value);
	RUN_TEST(test_creat_new_env_node_no_value);
	RUN_TEST(test_get_env_prints_key_value);
	RUN_TEST(test_get_env_skips_null_value);
	RUN_TEST(test_get_env_returns_success);
	return UNITY_END();
}
