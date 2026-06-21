#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

/* ---- helpers ---- */

static char	g_saved_path[PATH_MAX * 2];

void	setUp(void)
{
	char	*p;

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

/* ======== classify_rc_line tests ======== */

static void	test_classify_empty_string(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_EMPTY, type);
	TEST_ASSERT_NULL(val);
}

static void	test_classify_comment_line(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("# this is a comment", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_EMPTY, type);
	TEST_ASSERT_NULL(val);
}

static void	test_classify_whitespace_only(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("   \t  ", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_EMPTY, type);
	TEST_ASSERT_NULL(val);
}

static void	test_classify_whitespace_then_comment(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("  \t# indented comment", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_EMPTY, type);
	TEST_ASSERT_NULL(val);
}

static void	test_classify_path_line(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("PATH=/usr/local/bin", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_PATH, type);
	TEST_ASSERT_EQUAL_STRING("/usr/local/bin", val);
}

static void	test_classify_path_with_leading_spaces(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("  PATH=/opt/bin", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_PATH, type);
	TEST_ASSERT_EQUAL_STRING("/opt/bin", val);
}

static void	test_classify_path_empty_value(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("PATH=", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_PATH, type);
	TEST_ASSERT_EQUAL_STRING("", val);
}

static void	test_classify_command_line(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("echo hello", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_COMMAND, type);
	TEST_ASSERT_EQUAL_STRING("echo hello", val);
}

static void	test_classify_command_with_leading_whitespace(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("\t  ls -la", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_COMMAND, type);
	TEST_ASSERT_EQUAL_STRING("ls -la", val);
}

static void	test_classify_path_like_but_not_at_start(void)
{
	const char		*val;
	t_rc_line_type	type;

	type = classify_rc_line("export PATH=/usr/bin", &val);
	TEST_ASSERT_EQUAL_INT(RC_LINE_COMMAND, type);
	TEST_ASSERT_EQUAL_STRING("export PATH=/usr/bin", val);
}

/* ======== set_path tests ======== */

static void	test_set_path_prepends_to_existing(void)
{
	t_root	sh;
	char	*new_path;

	memset(&sh, 0, sizeof(sh));
	set_path(&sh, "/my/custom/bin");
	new_path = getenv("PATH");
	TEST_ASSERT_NOT_NULL(new_path);
	TEST_ASSERT_EQUAL_INT(0, strncmp(new_path, "/my/custom/bin:", 15));
	ft_lstclear(&sh.env_list, del_data);
}

static void	test_set_path_contains_old_path(void)
{
	t_root	sh;
	char	*new_path;

	memset(&sh, 0, sizeof(sh));
	if (g_saved_path[0] == '\0')
		TEST_IGNORE_MESSAGE("PATH not set");
	set_path(&sh, "/extra");
	new_path = getenv("PATH");
	TEST_ASSERT_NOT_NULL(strstr(new_path, g_saved_path));
	ft_lstclear(&sh.env_list, del_data);
}

static void	test_set_path_without_existing_path(void)
{
	t_root	sh;
	char	*result;

	memset(&sh, 0, sizeof(sh));
	unsetenv("PATH");
	set_path(&sh, "/only/this");
	result = getenv("PATH");
	TEST_ASSERT_NOT_NULL(result);
	TEST_ASSERT_EQUAL_STRING("/only/this", result);
	ft_lstclear(&sh.env_list, del_data);
}

int	main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_classify_empty_string);
	RUN_TEST(test_classify_comment_line);
	RUN_TEST(test_classify_whitespace_only);
	RUN_TEST(test_classify_whitespace_then_comment);
	RUN_TEST(test_classify_path_line);
	RUN_TEST(test_classify_path_with_leading_spaces);
	RUN_TEST(test_classify_path_empty_value);
	RUN_TEST(test_classify_command_line);
	RUN_TEST(test_classify_command_with_leading_whitespace);
	RUN_TEST(test_classify_path_like_but_not_at_start);
	RUN_TEST(test_set_path_prepends_to_existing);
	RUN_TEST(test_set_path_contains_old_path);
	RUN_TEST(test_set_path_without_existing_path);
	return (UNITY_END());
}
