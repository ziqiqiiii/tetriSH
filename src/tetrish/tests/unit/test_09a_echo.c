/*
 * tests/unit/test_09a_echo.c
 *
 * Unit tests for echo_command() in src/09a_echo.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* Redirect fd 1 into a pipe, call echo_command(cmd), restore fd 1.
   Fills buf with the captured bytes and null-terminates it.
   Returns the number of bytes written, or -1 on pipe error. */
static int capture_echo(char **cmd, char *buf, size_t size)
{
	int		pipefd[2];
	int		saved;
	ssize_t	n;

	if (pipe(pipefd) != 0)
		return (-1);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	echo_command(cmd);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	n = read(pipefd[0], buf, size - 1);
	close(pipefd[0]);
	if (n < 0)
		return (-1);
	buf[n] = '\0';
	return (int)n;
}

void setUp(void)    {}
void tearDown(void) {}

/* --- tests --- */

static void test_no_args_prints_newline(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("\n", buf);
}

static void test_single_word_with_newline(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", "hello", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hello\n", buf);
}

static void test_multiple_words_space_separated(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", "hello", "world", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hello world\n", buf);
}

static void test_n_flag_suppresses_newline(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", "-n", "hello", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hello", buf);
}

static void test_repeated_n_flags_suppresses_newline(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", "-n", "-n", "hi", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hi", buf);
}

static void test_n_only_produces_no_output(void)
{
	char	buf[64];
	char	*cmd[] = {"echo", "-n", NULL};
	int		n;

	n = capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT(0, n);
}

static void test_n_flag_after_word_is_literal(void)
{
	/* Once a non-flag word has been printed, flag2 is set and
	   further -n tokens are treated as regular words. */
	char	buf[64];
	char	*cmd[] = {"echo", "hello", "-n", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hello -n\n", buf);
}

static void test_nnn_flag_suppresses_newline(void)
{
	/* -nnn: strncmp("-nnn", "-n", 2)==0 and loop_n("nn")==1, so it
	   counts as the -n flag. */
	char	buf[64];
	char	*cmd[] = {"echo", "-nnn", "hi", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("hi", buf);
}

static void test_invalid_n_flag_treated_as_word(void)
{
	/* -nx: loop_n("x")==0, so -nx is printed as a regular word. */
	char	buf[64];
	char	*cmd[] = {"echo", "-nx", "hi", NULL};

	capture_echo(cmd, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_STRING("-nx hi\n", buf);
}

static void test_returns_exit_success(void)
{
	char	*cmd[] = {"echo", "test", NULL};

	/* discard output; only check the return value */
	int pipefd[2];
	int saved;
	int ret;

	pipe(pipefd);
	saved = dup(STDOUT_FILENO);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);
	ret = echo_command(cmd);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(pipefd[0]);
	TEST_ASSERT_EQUAL_INT(EXIT_SUCCESS, ret);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_no_args_prints_newline);
	RUN_TEST(test_single_word_with_newline);
	RUN_TEST(test_multiple_words_space_separated);
	RUN_TEST(test_n_flag_suppresses_newline);
	RUN_TEST(test_repeated_n_flags_suppresses_newline);
	RUN_TEST(test_n_only_produces_no_output);
	RUN_TEST(test_n_flag_after_word_is_literal);
	RUN_TEST(test_nnn_flag_suppresses_newline);
	RUN_TEST(test_invalid_n_flag_treated_as_word);
	RUN_TEST(test_returns_exit_success);
	return UNITY_END();
}
