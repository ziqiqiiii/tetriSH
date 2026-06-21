/*
 * tests/unit/test_04_lexer.c
 *
 * Unit tests for the lexer pipeline: src/04_lexer.c, src/04a_lexer_token_count.c,
 * src/04b_lexer_char_count.c, src/04c_lexer_cmd_mod.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

void setUp(void)    {}
void tearDown(void) {}

/* Helper: collect linked-list contents into a stack array for easy assertion. */
static int collect(t_list *head, char **out, int max)
{
	int	n;

	n = 0;
	while (head && n < max)
	{
		out[n++] = head->content;
		head = head->next;
	}
	return (n);
}

/* ---------- count_token ---------- */

static void test_count_token_simple_command(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_token("ls"));
}

static void test_count_token_command_with_args(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_token("echo hello world"));
}

static void test_count_token_pipe(void)
{
	TEST_ASSERT_EQUAL_INT(3, count_token("ls | grep foo"));
}

static void test_count_token_redirect_out(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_token("echo hi > file"));
}

static void test_count_token_redirect_in(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_token("cat < file"));
}

static void test_count_token_append(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_token("echo hi >> file"));
}

static void test_count_token_heredoc(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_token("cat << EOF"));
}

static void test_count_token_unclosed_quote(void)
{
	TEST_ASSERT_EQUAL_INT(-1, count_token("echo 'hello"));
}

static void test_count_token_quoted_pipe(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_token("echo '|'"));
}

static void test_count_token_empty_string(void)
{
	TEST_ASSERT_EQUAL_INT(0, count_token(""));
}

static void test_count_token_only_spaces(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_token("   "));
}

/* ---------- count_sp_char ---------- */

static void test_count_sp_char_pipe(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_sp_char("|"));
}

static void test_count_sp_char_double_pipe(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_sp_char("||"));
}

static void test_count_sp_char_redirect_out(void)
{
	TEST_ASSERT_EQUAL_INT(1, count_sp_char(">"));
}

static void test_count_sp_char_append(void)
{
	TEST_ASSERT_EQUAL_INT(2, count_sp_char(">>"));
}

static void test_count_sp_char_redirect_with_file(void)
{
	int n = count_sp_char(">file");
	TEST_ASSERT_GREATER_OR_EQUAL_INT(1, n);
}

static void test_count_sp_char_no_special(void)
{
	TEST_ASSERT_EQUAL_INT(0, count_sp_char("hello"));
}

/* ---------- count_char ---------- */

static void test_count_char_simple(void)
{
	TEST_ASSERT_EQUAL_INT(5, count_char("hello"));
}

static void test_count_char_stops_at_pipe(void)
{
	TEST_ASSERT_EQUAL_INT(3, count_char("foo|bar"));
}

static void test_count_char_stops_at_redirect(void)
{
	TEST_ASSERT_EQUAL_INT(3, count_char("foo>bar"));
}

static void test_count_char_with_quotes(void)
{
	TEST_ASSERT_EQUAL_INT(7, count_char("'hello'"));
}

static void test_count_char_empty(void)
{
	TEST_ASSERT_EQUAL_INT(0, count_char(""));
}

/* ---------- lexer (integration through the full pipeline) ---------- */

static void test_lexer_simple_command(void)
{
	char	*tokens[8];
	t_list	*head;
	int		n;
	char	*input;

	input = ft_strdup("echo hello");
	head = lexer(input);
	TEST_ASSERT_NOT_NULL(head);
	n = collect(head, tokens, 8);
	TEST_ASSERT_EQUAL_INT(1, n);
	TEST_ASSERT_EQUAL_STRING("echo hello", tokens[0]);
	ft_lstclear(&head, free);
	free(input);
}

static void test_lexer_pipe(void)
{
	char	*tokens[8];
	t_list	*head;
	int		n;
	char	*input;

	input = ft_strdup("ls | grep foo");
	head = lexer(input);
	TEST_ASSERT_NOT_NULL(head);
	n = collect(head, tokens, 8);
	TEST_ASSERT_EQUAL_INT(3, n);
	TEST_ASSERT_EQUAL_STRING("ls", tokens[0]);
	TEST_ASSERT_EQUAL_STRING("|", tokens[1]);
	TEST_ASSERT_EQUAL_STRING("grep foo", tokens[2]);
	ft_lstclear(&head, free);
	free(input);
}

static void test_lexer_redirect_out(void)
{
	char	*tokens[8];
	t_list	*head;
	int		n;
	char	*input;

	input = ft_strdup("echo hi > out.txt");
	head = lexer(input);
	TEST_ASSERT_NOT_NULL(head);
	n = collect(head, tokens, 8);
	TEST_ASSERT_EQUAL_INT(2, n);
	TEST_ASSERT_EQUAL_STRING("echo hi", tokens[0]);
	ft_lstclear(&head, free);
	free(input);
}

static void test_lexer_null_returns_null(void)
{
	TEST_ASSERT_NULL(lexer(NULL));
}

static void test_lexer_empty_returns_null(void)
{
	TEST_ASSERT_NULL(lexer(ft_strdup("")));
}

static void test_lexer_heredoc(void)
{
	char	*tokens[8];
	t_list	*head;
	int		n;
	char	*input;

	input = ft_strdup("cat << EOF");
	head = lexer(input);
	TEST_ASSERT_NOT_NULL(head);
	n = collect(head, tokens, 8);
	TEST_ASSERT_EQUAL_INT(2, n);
	ft_lstclear(&head, free);
	free(input);
}

int main(void)
{
	UNITY_BEGIN();
	/* count_token */
	RUN_TEST(test_count_token_simple_command);
	RUN_TEST(test_count_token_command_with_args);
	RUN_TEST(test_count_token_pipe);
	RUN_TEST(test_count_token_redirect_out);
	RUN_TEST(test_count_token_redirect_in);
	RUN_TEST(test_count_token_append);
	RUN_TEST(test_count_token_heredoc);
	RUN_TEST(test_count_token_unclosed_quote);
	RUN_TEST(test_count_token_quoted_pipe);
	RUN_TEST(test_count_token_empty_string);
	RUN_TEST(test_count_token_only_spaces);
	/* count_sp_char */
	RUN_TEST(test_count_sp_char_pipe);
	RUN_TEST(test_count_sp_char_double_pipe);
	RUN_TEST(test_count_sp_char_redirect_out);
	RUN_TEST(test_count_sp_char_append);
	RUN_TEST(test_count_sp_char_redirect_with_file);
	RUN_TEST(test_count_sp_char_no_special);
	/* count_char */
	RUN_TEST(test_count_char_simple);
	RUN_TEST(test_count_char_stops_at_pipe);
	RUN_TEST(test_count_char_stops_at_redirect);
	RUN_TEST(test_count_char_with_quotes);
	RUN_TEST(test_count_char_empty);
	/* lexer */
	RUN_TEST(test_lexer_simple_command);
	RUN_TEST(test_lexer_pipe);
	RUN_TEST(test_lexer_redirect_out);
	RUN_TEST(test_lexer_null_returns_null);
	RUN_TEST(test_lexer_empty_returns_null);
	RUN_TEST(test_lexer_heredoc);
	return UNITY_END();
}
