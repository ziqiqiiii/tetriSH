/*
 * tests/unit/test_10_quote.c
 *
 * Unit tests for quote handling in src/10_quote.c and src/10a_quote_utils.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

void setUp(void)    {}
void tearDown(void) {}

/* ---------- is_quote ---------- */

static void test_is_quote_single(void)
{
	TEST_ASSERT_EQUAL_INT(1, is_quote('\''));
}

static void test_is_quote_double(void)
{
	TEST_ASSERT_EQUAL_INT(2, is_quote('"'));
}

static void test_is_quote_regular_char(void)
{
	TEST_ASSERT_EQUAL_INT(0, is_quote('a'));
}

static void test_is_quote_space(void)
{
	TEST_ASSERT_EQUAL_INT(0, is_quote(' '));
}

static void test_is_quote_null(void)
{
	TEST_ASSERT_EQUAL_INT(0, is_quote('\0'));
}

/* ---------- quote_count ---------- */

static void test_quote_count_single_quoted(void)
{
	TEST_ASSERT_EQUAL_INT(5, quote_count("'abc'"));
}

static void test_quote_count_double_quoted(void)
{
	TEST_ASSERT_EQUAL_INT(5, quote_count("\"abc\""));
}

static void test_quote_count_empty_single_quotes(void)
{
	TEST_ASSERT_EQUAL_INT(2, quote_count("''"));
}

static void test_quote_count_empty_double_quotes(void)
{
	TEST_ASSERT_EQUAL_INT(2, quote_count("\"\""));
}

static void test_quote_count_no_quote(void)
{
	TEST_ASSERT_EQUAL_INT(0, quote_count("abc"));
}

static void test_quote_count_unclosed_single(void)
{
	TEST_ASSERT_EQUAL_INT(-1, quote_count("'abc"));
}

static void test_quote_count_unclosed_double(void)
{
	TEST_ASSERT_EQUAL_INT(-1, quote_count("\"abc"));
}

static void test_quote_count_single_inside_double(void)
{
	/* "it's" — the inner single quote is not a closer for double */
	TEST_ASSERT_EQUAL_INT(6, quote_count("\"it's\""));
}

static void test_quote_count_double_inside_single(void)
{
	/* '"hi"' — the inner double quotes are not closers for single */
	TEST_ASSERT_EQUAL_INT(6, quote_count("'\"hi\"'"));
}

/* ---------- remove_quote ---------- */

static void test_remove_quote_single(void)
{
	char buf[] = "'hello'";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("hello", res);
}

static void test_remove_quote_double(void)
{
	char buf[] = "\"hello\"";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("hello", res);
}

static void test_remove_quote_no_quotes(void)
{
	char buf[] = "hello";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("hello", res);
}

static void test_remove_quote_empty_quotes(void)
{
	char buf[] = "''";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("", res);
}

static void test_remove_quote_mixed(void)
{
	char buf[] = "he'llo'wo\"rld\"";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("helloworld", res);
}

static void test_remove_quote_preserves_inner_single_in_double(void)
{
	char buf[] = "\"it's\"";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("it's", res);
}

static void test_remove_quote_preserves_inner_double_in_single(void)
{
	char buf[] = "'say \"hi\"'";
	char *res = remove_quote(buf);
	TEST_ASSERT_EQUAL_STRING("say \"hi\"", res);
}

/* ---------- cmd_quote_handler ---------- */

static void helper_assert_split(char **result, const char **expected, int count)
{
	int i;

	for (i = 0; i < count; i++)
		TEST_ASSERT_EQUAL_STRING(expected[i], result[i]);
	TEST_ASSERT_NULL(result[count]);
	free_2d(result);
}

static void test_cmd_quote_handler_null_input(void)
{
	TEST_ASSERT_NULL(cmd_quote_handler(NULL, ' '));
}

static void test_cmd_quote_handler_simple_split(void)
{
	const char *expected[] = {"hello", "world"};
	char **res = cmd_quote_handler("hello world", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 2);
}

static void test_cmd_quote_handler_single_word(void)
{
	const char *expected[] = {"hello"};
	char **res = cmd_quote_handler("hello", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 1);
}

static void test_cmd_quote_handler_multiple_spaces(void)
{
	const char *expected[] = {"hello", "world"};
	char **res = cmd_quote_handler("hello   world", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 2);
}

static void test_cmd_quote_handler_leading_trailing_spaces(void)
{
	const char *expected[] = {"hello", "world"};
	char **res = cmd_quote_handler("  hello  world  ", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 2);
}

static void test_cmd_quote_handler_quoted_space(void)
{
	const char *expected[] = {"hello world"};
	char **res = cmd_quote_handler("'hello world'", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 1);
}

static void test_cmd_quote_handler_double_quoted_space(void)
{
	const char *expected[] = {"hello world"};
	char **res = cmd_quote_handler("\"hello world\"", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 1);
}

static void test_cmd_quote_handler_mixed_quoted_unquoted(void)
{
	const char *expected[] = {"hello world", "foo"};
	char **res = cmd_quote_handler("'hello world' foo", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 2);
}

static void test_cmd_quote_handler_empty_string(void)
{
	char **res = cmd_quote_handler("", ' ');

	TEST_ASSERT_NOT_NULL(res);
	TEST_ASSERT_NULL(res[0]);
	free_2d(res);
}

static void test_cmd_quote_handler_only_spaces(void)
{
	char **res = cmd_quote_handler("   ", ' ');

	TEST_ASSERT_NOT_NULL(res);
	TEST_ASSERT_NULL(res[0]);
	free_2d(res);
}

static void test_cmd_quote_handler_adjacent_quoted_segments(void)
{
	const char *expected[] = {"helloworld"};
	char **res = cmd_quote_handler("'hello'\"world\"", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 1);
}

static void test_cmd_quote_handler_three_words(void)
{
	const char *expected[] = {"echo", "hello", "world"};
	char **res = cmd_quote_handler("echo hello world", ' ');

	TEST_ASSERT_NOT_NULL(res);
	helper_assert_split(res, expected, 3);
}

int main(void)
{
	UNITY_BEGIN();
	/* is_quote */
	RUN_TEST(test_is_quote_single);
	RUN_TEST(test_is_quote_double);
	RUN_TEST(test_is_quote_regular_char);
	RUN_TEST(test_is_quote_space);
	RUN_TEST(test_is_quote_null);
	/* quote_count */
	RUN_TEST(test_quote_count_single_quoted);
	RUN_TEST(test_quote_count_double_quoted);
	RUN_TEST(test_quote_count_empty_single_quotes);
	RUN_TEST(test_quote_count_empty_double_quotes);
	RUN_TEST(test_quote_count_no_quote);
	RUN_TEST(test_quote_count_unclosed_single);
	RUN_TEST(test_quote_count_unclosed_double);
	RUN_TEST(test_quote_count_single_inside_double);
	RUN_TEST(test_quote_count_double_inside_single);
	/* remove_quote */
	RUN_TEST(test_remove_quote_single);
	RUN_TEST(test_remove_quote_double);
	RUN_TEST(test_remove_quote_no_quotes);
	RUN_TEST(test_remove_quote_empty_quotes);
	RUN_TEST(test_remove_quote_mixed);
	RUN_TEST(test_remove_quote_preserves_inner_single_in_double);
	RUN_TEST(test_remove_quote_preserves_inner_double_in_single);
	/* cmd_quote_handler */
	RUN_TEST(test_cmd_quote_handler_null_input);
	RUN_TEST(test_cmd_quote_handler_simple_split);
	RUN_TEST(test_cmd_quote_handler_single_word);
	RUN_TEST(test_cmd_quote_handler_multiple_spaces);
	RUN_TEST(test_cmd_quote_handler_leading_trailing_spaces);
	RUN_TEST(test_cmd_quote_handler_quoted_space);
	RUN_TEST(test_cmd_quote_handler_double_quoted_space);
	RUN_TEST(test_cmd_quote_handler_mixed_quoted_unquoted);
	RUN_TEST(test_cmd_quote_handler_empty_string);
	RUN_TEST(test_cmd_quote_handler_only_spaces);
	RUN_TEST(test_cmd_quote_handler_adjacent_quoted_segments);
	RUN_TEST(test_cmd_quote_handler_three_words);
	return UNITY_END();
}
