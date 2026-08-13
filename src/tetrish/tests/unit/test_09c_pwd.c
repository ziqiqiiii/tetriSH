/*
 * tests/unit/test_09c_pwd.c
 *
 * Unit tests for pwd() in src/09c_pwd.c.
 * Uses extern declaration only; no project headers are included so the
 * test binary does not need libft.a, readline, or curses linked.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* Redirect fd 1 into a pipe, call pwd(), flush and restore fd 1.
   Fills buf with the captured bytes and null-terminates it.
   Returns the number of bytes captured, or -1 on pipe error. */
static int	capture_pwd(char *buf, size_t size)
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
	pwd();
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

void	setUp(void) {}
void	tearDown(void) {}

/* --- tests --- */

static void	test_pwd_returns_success(void)
{
	int	pipefd[2];
	int	saved;
	int	ret;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = pwd();
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
}

static void	test_pwd_output_ends_with_newline(void)
{
	char	buf[512];
	int		n;

	n = capture_pwd(buf, sizeof(buf));
	TEST_ASSERT_TRUE(n > 0);
	TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);
}

static void	test_pwd_output_matches_getcwd(void)
{
	char	buf[512];
	char	expected[256];
	int		n;

	TEST_ASSERT_NOT_NULL(getcwd(expected, sizeof(expected)));
	n = capture_pwd(buf, sizeof(buf));
	TEST_ASSERT_TRUE(n > 0);
	if (buf[n - 1] == '\n')
		buf[n - 1] = '\0';
	TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void	test_pwd_output_is_absolute_path(void)
{
	char	buf[512];
	int		n;

	n = capture_pwd(buf, sizeof(buf));
	TEST_ASSERT_TRUE(n > 0);
	TEST_ASSERT_EQUAL_CHAR('/', buf[0]);
}

static void	test_pwd_output_has_no_extra_newlines(void)
{
	char	buf[512];
	int		n;
	int		i;
	int		newline_count;

	n = capture_pwd(buf, sizeof(buf));
	TEST_ASSERT_TRUE(n > 0);
	newline_count = 0;
	i = 0;
	while (i < n)
	{
		if (buf[i] == '\n')
			newline_count++;
		i++;
	}
	TEST_ASSERT_EQUAL_INT(1, newline_count);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_pwd_returns_success);
	RUN_TEST(test_pwd_output_ends_with_newline);
	RUN_TEST(test_pwd_output_matches_getcwd);
	RUN_TEST(test_pwd_output_is_absolute_path);
	RUN_TEST(test_pwd_output_has_no_extra_newlines);
	return UNITY_END();
}
