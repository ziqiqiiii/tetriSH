/*
 * tests/unit/test_09b_cd.c
 *
 * Unit tests for cd() in src/09b_cd.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* --- helpers --- */

static char		g_orig_dir[PATH_MAX];
static t_list	*g_env;

static void	build_env(t_list **env, const char *home)
{
	char	*envp[3];
	char	home_pair[PATH_MAX];

	*env = NULL;
	if (home)
	{
		snprintf(home_pair, sizeof(home_pair), "HOME=%s", home);
		envp[0] = home_pair;
		envp[1] = NULL;
		env_link_list(envp, env);
	}
}

void	setUp(void)
{
	getcwd(g_orig_dir, sizeof(g_orig_dir));
	g_env = NULL;
	build_env(&g_env, getenv("HOME"));
}

void	tearDown(void)
{
	chdir(g_orig_dir);
	ft_lstclear(&g_env, del_data);
}

static int	capture_stderr_cd(char **cmd, char *buf, size_t size)
{
	int		pipefd[2];
	int		saved;
	ssize_t	n;

	if (pipe(pipefd) != 0)
		return (-1);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	cd(cmd, &g_env);
	dup2(saved, STDERR_FILENO);
	close(saved);
	n = read(pipefd[0], buf, size - 1);
	close(pipefd[0]);
	if (n < 0)
		return (-1);
	buf[n] = '\0';
	return ((int)n);
}

/* --- tests --- */

static void	test_cd_no_args_goes_home(void)
{
	char	*cmd[] = {"cd", NULL};
	char	*home;
	char	cwd[PATH_MAX];

	home = getenv("HOME");
	if (!home)
		TEST_IGNORE_MESSAGE("HOME not set");
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, cd(cmd, &g_env));
	getcwd(cwd, sizeof(cwd));
	TEST_ASSERT_EQUAL_STRING(home, cwd);
}

static void	test_cd_no_args_home_unset_fails(void)
{
	char	*cmd[] = {"cd", NULL};
	t_list	*empty_env;
	char	buf[256];
	int		pipefd[2];
	int		saved;
	int		ret;

	empty_env = NULL;
	pipe(pipefd);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	ret = cd(cmd, &empty_env);
	dup2(saved, STDERR_FILENO);
	close(saved);
	read(pipefd[0], buf, sizeof(buf));
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, ret);
}

static void	test_cd_valid_path(void)
{
	char	*cmd[] = {"cd", "/tmp", NULL};
	char	cwd[PATH_MAX];

	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, cd(cmd, &g_env));
	getcwd(cwd, sizeof(cwd));
	TEST_ASSERT_EQUAL_STRING("/tmp", cwd);
}

static void	test_cd_invalid_path_fails(void)
{
	char	*cmd[] = {"cd", "/nonexistent_dir_12345", NULL};
	char	buf[256];

	capture_stderr_cd(cmd, buf, sizeof(buf));
}

static void	test_cd_invalid_path_returns_failure(void)
{
	char	*cmd[] = {"cd", "/nonexistent_dir_12345", NULL};
	int		pipefd[2];
	int		saved;
	int		ret;

	pipe(pipefd);
	saved = dup(STDERR_FILENO);
	dup2(pipefd[1], STDERR_FILENO);
	close(pipefd[1]);
	ret = cd(cmd, &g_env);
	dup2(saved, STDERR_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_FAILURE, ret);
}

static void	test_cd_tilde_expands_to_home(void)
{
	char	*tilde;
	char	*cmd[3];
	char	*home;
	char	cwd[PATH_MAX];

	home = getenv("HOME");
	if (!home)
		TEST_IGNORE_MESSAGE("HOME not set");
	tilde = ft_strdup("~");
	cmd[0] = "cd";
	cmd[1] = tilde;
	cmd[2] = NULL;
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, cd(cmd, &g_env));
	getcwd(cwd, sizeof(cwd));
	TEST_ASSERT_EQUAL_STRING(home, cwd);
	free(cmd[1]);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_cd_no_args_goes_home);
	RUN_TEST(test_cd_no_args_home_unset_fails);
	RUN_TEST(test_cd_valid_path);
	RUN_TEST(test_cd_invalid_path_fails);
	RUN_TEST(test_cd_invalid_path_returns_failure);
	RUN_TEST(test_cd_tilde_expands_to_home);
	return UNITY_END();
}
