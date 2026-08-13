/*
 * tests/unit/test_11_signal.c
 *
 * Unit tests for signal handling in src/11_signal.c and src/11a_sigwait.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

void setUp(void)    {}
void tearDown(void) {}

/* Helper: query the current handler for a signal. */
static void	get_handler(int sig, struct sigaction *out)
{
	sigaction(sig, NULL, out);
}

/* ---------- shell_ignore_signals ---------- */

static void test_shell_ignore_signals_sigint_is_custom(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	get_handler(SIGINT, &sa);
	TEST_ASSERT_NOT_EQUAL(SIG_DFL, sa.sa_handler);
	TEST_ASSERT_NOT_EQUAL(SIG_IGN, sa.sa_handler);
}

static void test_shell_ignore_signals_sigquit_ignored(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	get_handler(SIGQUIT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_IGN, sa.sa_handler);
}

static void test_shell_ignore_signals_sigtstp_ignored(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	get_handler(SIGTSTP, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_IGN, sa.sa_handler);
}

/* ---------- child_restore_signals ---------- */

static void test_child_restore_sigint_default(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	child_restore_signals();
	get_handler(SIGINT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_DFL, sa.sa_handler);
}

static void test_child_restore_sigquit_default(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	child_restore_signals();
	get_handler(SIGQUIT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_DFL, sa.sa_handler);
}

static void test_child_restore_sigtstp_default(void)
{
	struct sigaction sa;

	shell_ignore_signals();
	child_restore_signals();
	get_handler(SIGTSTP, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_DFL, sa.sa_handler);
}

/* ---------- heredoc_restore_signals ---------- */

static void test_heredoc_restore_sigint_is_custom(void)
{
	struct sigaction sa;

	heredoc_restore_signals();
	get_handler(SIGINT, &sa);
	TEST_ASSERT_NOT_EQUAL(SIG_DFL, sa.sa_handler);
	TEST_ASSERT_NOT_EQUAL(SIG_IGN, sa.sa_handler);
}

static void test_heredoc_restore_sigquit_ignored(void)
{
	struct sigaction sa;

	heredoc_restore_signals();
	get_handler(SIGQUIT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_IGN, sa.sa_handler);
}

static void test_heredoc_restore_sigtstp_ignored(void)
{
	struct sigaction sa;

	heredoc_restore_signals();
	get_handler(SIGTSTP, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_IGN, sa.sa_handler);
}

/* ---------- sigint_ignore / sigint_restore ---------- */

static void test_sigint_ignore_sets_ign(void)
{
	struct sigaction old;
	struct sigaction sa;

	child_restore_signals();
	sigint_ignore(&old);
	get_handler(SIGINT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_IGN, sa.sa_handler);
	sigint_restore(&old);
}

static void test_sigint_ignore_saves_previous(void)
{
	struct sigaction old;

	child_restore_signals();
	sigint_ignore(&old);
	TEST_ASSERT_EQUAL_PTR(SIG_DFL, old.sa_handler);
	sigint_restore(&old);
}

static void test_sigint_restore_reverts_handler(void)
{
	struct sigaction old;
	struct sigaction sa;

	child_restore_signals();
	sigint_ignore(&old);
	sigint_restore(&old);
	get_handler(SIGINT, &sa);
	TEST_ASSERT_EQUAL_PTR(SIG_DFL, sa.sa_handler);
}

static void test_sigint_ignore_restore_roundtrip_with_custom(void)
{
	struct sigaction old;
	struct sigaction sa;

	shell_ignore_signals();
	get_handler(SIGINT, &sa);
	sigint_ignore(&old);
	TEST_ASSERT_NOT_EQUAL(SIG_DFL, old.sa_handler);
	TEST_ASSERT_NOT_EQUAL(SIG_IGN, old.sa_handler);
	sigint_restore(&old);
	get_handler(SIGINT, &sa);
	TEST_ASSERT_NOT_EQUAL(SIG_DFL, sa.sa_handler);
	TEST_ASSERT_NOT_EQUAL(SIG_IGN, sa.sa_handler);
}

int main(void)
{
	UNITY_BEGIN();
	/* shell_ignore_signals */
	RUN_TEST(test_shell_ignore_signals_sigint_is_custom);
	RUN_TEST(test_shell_ignore_signals_sigquit_ignored);
	RUN_TEST(test_shell_ignore_signals_sigtstp_ignored);
	/* child_restore_signals */
	RUN_TEST(test_child_restore_sigint_default);
	RUN_TEST(test_child_restore_sigquit_default);
	RUN_TEST(test_child_restore_sigtstp_default);
	/* heredoc_restore_signals */
	RUN_TEST(test_heredoc_restore_sigint_is_custom);
	RUN_TEST(test_heredoc_restore_sigquit_ignored);
	RUN_TEST(test_heredoc_restore_sigtstp_ignored);
	/* sigint_ignore / sigint_restore */
	RUN_TEST(test_sigint_ignore_sets_ign);
	RUN_TEST(test_sigint_ignore_saves_previous);
	RUN_TEST(test_sigint_restore_reverts_handler);
	RUN_TEST(test_sigint_ignore_restore_roundtrip_with_custom);
	return UNITY_END();
}
