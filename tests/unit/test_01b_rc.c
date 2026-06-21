#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* get_next_line stub overrides the libft.a version to control input */

static int	g_gnl_call_count;
static char	**g_gnl_lines;
static int	g_gnl_total;

char	*get_next_line(int fd)
{
	(void)fd;
	if (g_gnl_call_count >= g_gnl_total)
		return (NULL);
	return (ft_strdup(g_gnl_lines[g_gnl_call_count++]));
}

/* ---- helpers ---- */

static char	g_saved_path[PATH_MAX * 2];

void	setUp(void)
{
	char	*p;

	g_gnl_call_count = 0;
	g_gnl_lines = NULL;
	g_gnl_total = 0;
	p = getenv("PATH");
	if (p)
		snprintf(g_saved_path, sizeof(g_saved_path), "%s", p);
	else
		g_saved_path[0] = '\0';
}

void	tearDown(void)
{
	if (g_saved_path[0])
		setenv("PATH", g_saved_path, 1);
}

/* ======== get_rc_paths tests ======== */

static void	test_get_rc_paths_sets_exe_dir_path(void)
{
	t_root	sh;
	char	rc_path[PATH_MAX];
	char	home_path[PATH_MAX];
	char	expected[PATH_MAX];
	char	*exe_dir;

	memset(&sh, 0, sizeof(sh));
	get_rc_paths(&sh, rc_path, home_path);
	exe_dir = resolve_project_root();
	snprintf(expected, sizeof(expected), "%s/.macminishellrc", exe_dir);
	TEST_ASSERT_EQUAL_STRING(expected, rc_path);
	free(exe_dir);
	free(sh.current_dir);
}

static void	test_get_rc_paths_sets_home_path(void)
{
	t_root		sh;
	char		rc_path[PATH_MAX];
	char		home_path[PATH_MAX];
	const char	*home;
	char		expected[PATH_MAX];

	memset(&sh, 0, sizeof(sh));
	home = getenv("HOME");
	get_rc_paths(&sh, rc_path, home_path);
	if (home)
	{
		snprintf(expected, sizeof(expected), "%s/.macminishellrc", home);
		TEST_ASSERT_EQUAL_STRING(expected, home_path);
	}
	else
		TEST_ASSERT_EQUAL_STRING("", home_path);
	free(sh.current_dir);
}

static void	test_get_rc_paths_stores_current_dir(void)
{
	t_root	sh;
	char	rc_path[PATH_MAX];
	char	home_path[PATH_MAX];

	memset(&sh, 0, sizeof(sh));
	get_rc_paths(&sh, rc_path, home_path);
	TEST_ASSERT_NOT_NULL(sh.current_dir);
	free(sh.current_dir);
}

/* ======== run_rc tests ======== */

static void	test_run_rc_empty_file(void)
{
	t_root	sh;
	int		pipefd[2];

	memset(&sh, 0, sizeof(sh));
	g_gnl_total = 0;
	g_gnl_lines = NULL;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(0, g_gnl_call_count);
}

static void	test_run_rc_skips_comments(void)
{
	t_root	sh;
	char	*lines[] = {"# comment\n", "  \n"};
	int		pipefd[2];
	char	*path_after;

	memset(&sh, 0, sizeof(sh));
	g_gnl_lines = lines;
	g_gnl_total = 2;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	path_after = getenv("PATH");
	TEST_ASSERT_EQUAL_STRING(g_saved_path, path_after);
}

static void	test_run_rc_handles_path_line(void)
{
	t_root	sh;
	char	*lines[] = {"PATH=/custom/bin\n"};
	int		pipefd[2];
	char	*path_after;

	memset(&sh, 0, sizeof(sh));
	g_gnl_lines = lines;
	g_gnl_total = 1;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	path_after = getenv("PATH");
	TEST_ASSERT_NOT_NULL(path_after);
	TEST_ASSERT_EQUAL_INT(0, strncmp(path_after, "/custom/bin", 11));
	ft_lstclear(&sh.env_list, del_data);
}

static void	test_run_rc_strips_trailing_newline(void)
{
	t_root	sh;
	char	*lines[] = {"PATH=/test\n"};
	int		pipefd[2];
	char	*path_after;

	memset(&sh, 0, sizeof(sh));
	g_gnl_lines = lines;
	g_gnl_total = 1;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	path_after = getenv("PATH");
	TEST_ASSERT_NOT_NULL(path_after);
	TEST_ASSERT_EQUAL_INT(0, strncmp(path_after, "/test:", 6));
	ft_lstclear(&sh.env_list, del_data);
}

static void	test_run_rc_strips_crlf(void)
{
	t_root	sh;
	char	*lines[] = {"PATH=/win\r\n"};
	int		pipefd[2];
	char	*path_after;

	memset(&sh, 0, sizeof(sh));
	g_gnl_lines = lines;
	g_gnl_total = 1;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	path_after = getenv("PATH");
	TEST_ASSERT_NOT_NULL(path_after);
	TEST_ASSERT_EQUAL_INT(0, strncmp(path_after, "/win:", 5));
	ft_lstclear(&sh.env_list, del_data);
}

static void	test_run_rc_multiple_lines(void)
{
	t_root	sh;
	char	*lines[] = {"# comment\n", "PATH=/a\n"};
	int		pipefd[2];
	char	*path_after;

	memset(&sh, 0, sizeof(sh));
	g_gnl_lines = lines;
	g_gnl_total = 2;
	pipe(pipefd);
	close(pipefd[1]);
	run_rc(pipefd[0], &sh, NULL);
	close(pipefd[0]);
	path_after = getenv("PATH");
	TEST_ASSERT_NOT_NULL(path_after);
	TEST_ASSERT_EQUAL_INT(0, strncmp(path_after, "/a:", 3));
	ft_lstclear(&sh.env_list, del_data);
}

/* ======== create_empty_rc tests ======== */

static void	test_create_empty_rc_creates_file(void)
{
	char	path[PATH_MAX];
	int		fd;
	char	buf[512];
	ssize_t	n;

	snprintf(path, sizeof(path), "/tmp/test_macmini_rc_%d", getpid());
	unlink(path);
	create_empty_rc(path);
	fd = open(path, O_RDONLY);
	TEST_ASSERT_TRUE(fd >= 0);
	n = read(fd, buf, sizeof(buf) - 1);
	TEST_ASSERT_TRUE(n > 0);
	buf[n] = '\0';
	TEST_ASSERT_NOT_NULL(strstr(buf, "MacMini Shell"));
	close(fd);
	unlink(path);
}

static void	test_create_empty_rc_header_has_comment(void)
{
	char	path[PATH_MAX];
	int		fd;
	char	buf[512];
	ssize_t	n;

	snprintf(path, sizeof(path), "/tmp/test_macmini_rc2_%d", getpid());
	unlink(path);
	create_empty_rc(path);
	fd = open(path, O_RDONLY);
	TEST_ASSERT_TRUE(fd >= 0);
	n = read(fd, buf, sizeof(buf) - 1);
	buf[n] = '\0';
	TEST_ASSERT_EQUAL_CHAR('#', buf[0]);
	close(fd);
	unlink(path);
}

/* ======== source_rc tests ======== */

static void	test_source_rc_creates_rc_when_missing(void)
{
	t_root	sh;
	char	*exe_dir;
	char	rc_check[PATH_MAX];

	exe_dir = resolve_project_root();
	if (!exe_dir)
		TEST_IGNORE_MESSAGE("resolve_project_root returned NULL");
	snprintf(rc_check, sizeof(rc_check), "%s/.macminishellrc", exe_dir);
	unlink(rc_check);
	memset(&sh, 0, sizeof(sh));
	g_gnl_total = 0;
	g_gnl_lines = NULL;
	unsetenv("HOME");
	source_rc(&sh, NULL);
	TEST_ASSERT_EQUAL_INT(0, access(rc_check, F_OK));
	unlink(rc_check);
	free(exe_dir);
	free(sh.current_dir);
	setenv("HOME", getenv("HOME") ? getenv("HOME") : "/tmp", 1);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_get_rc_paths_sets_exe_dir_path);
	RUN_TEST(test_get_rc_paths_sets_home_path);
	RUN_TEST(test_get_rc_paths_stores_current_dir);
	RUN_TEST(test_run_rc_empty_file);
	RUN_TEST(test_run_rc_skips_comments);
	RUN_TEST(test_run_rc_handles_path_line);
	RUN_TEST(test_run_rc_strips_trailing_newline);
	RUN_TEST(test_run_rc_strips_crlf);
	RUN_TEST(test_run_rc_multiple_lines);
	RUN_TEST(test_create_empty_rc_creates_file);
	RUN_TEST(test_create_empty_rc_header_has_comment);
	RUN_TEST(test_source_rc_creates_rc_when_missing);
	return (UNITY_END());
}
