/*
 * tests/unit/test_05_parser.c
 *
 * Unit tests for the parser: src/05_parser.c and src/05a_parser_utils.c.
 *
 * Run with: make unit
 */
#include "unity.h"
#include "minishell.h"

int	g_exit_status = 0;

static t_root	g_sh;

static void init_tkchk(void)
{
	g_sh.tkchk[0].token = RDAPP;
	g_sh.tkchk[0].op = RDAPP_OP;
	g_sh.tkchk[1].token = HEREDOC;
	g_sh.tkchk[1].op = HEREDOC_OP;
	g_sh.tkchk[2].token = RDIN;
	g_sh.tkchk[2].op = RDIN_OP;
	g_sh.tkchk[3].token = RDOUT;
	g_sh.tkchk[3].op = RDOUT_OP;
	g_sh.tkchk[4].token = PIPE;
	g_sh.tkchk[4].op = PIPE_OP;
	g_sh.tkchk[5].token = COMMAND;
	g_sh.tkchk[5].op = NULL;
	g_sh.tkchk[6].token = END;
	g_sh.tkchk[6].op = NULL;
}

void setUp(void)    { init_tkchk(); }
void tearDown(void) {}

/* free_tree() calls free(node->value), which crashes on string literals.
   This helper only frees the t_tree nodes themselves. */
static void free_tree_nodes(t_tree *node)
{
	if (!node)
		return ;
	free_tree_nodes(node->left);
	free_tree_nodes(node->right);
	free(node);
}

/* ---------- tree_node_new ---------- */

static void test_tree_node_new_basic(void)
{
	t_tree	*node;

	node = tree_node_new(COMMAND, "ls", NULL, NULL);
	TEST_ASSERT_NOT_NULL(node);
	TEST_ASSERT_EQUAL_INT(COMMAND, node->token);
	TEST_ASSERT_EQUAL_STRING("ls", node->value);
	TEST_ASSERT_NULL(node->left);
	TEST_ASSERT_NULL(node->right);
	free(node);
}

static void test_tree_node_new_with_children(void)
{
	t_tree	*left;
	t_tree	*right;
	t_tree	*root;

	left = tree_node_new(COMMAND, "ls", NULL, NULL);
	right = tree_node_new(COMMAND, "grep", NULL, NULL);
	root = tree_node_new(PIPE, "|", left, right);
	TEST_ASSERT_NOT_NULL(root);
	TEST_ASSERT_EQUAL_INT(PIPE, root->token);
	TEST_ASSERT_EQUAL_PTR(left, root->left);
	TEST_ASSERT_EQUAL_PTR(right, root->right);
	free(root);
	free(left);
	free(right);
}

/* ---------- parser ---------- */

static void test_parser_null_returns_null(void)
{
	TEST_ASSERT_NULL(parser(NULL, 0, &g_sh));
}

static void test_parser_zero_tokens_returns_null(void)
{
	t_list	node;

	node.content = "ls";
	node.next = NULL;
	TEST_ASSERT_NULL(parser(&node, 0, &g_sh));
}

static void test_parser_single_command(void)
{
	t_list	node;
	t_tree	*tree;

	node.content = "ls -la";
	node.next = NULL;
	tree = parser(&node, 1, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(COMMAND, tree->token);
	TEST_ASSERT_EQUAL_STRING("ls -la", tree->value);
	TEST_ASSERT_NULL(tree->left);
	TEST_ASSERT_NULL(tree->right);
	free_tree_nodes(tree);
}

static void test_parser_pipe(void)
{
	t_list	n1, n2, n3;
	t_tree	*tree;

	n1.content = "ls";
	n1.next = &n2;
	n2.content = "|";
	n2.next = &n3;
	n3.content = "grep foo";
	n3.next = NULL;
	tree = parser(&n1, 3, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(PIPE, tree->token);
	TEST_ASSERT_NOT_NULL(tree->left);
	TEST_ASSERT_EQUAL_INT(COMMAND, tree->left->token);
	TEST_ASSERT_EQUAL_STRING("ls", tree->left->value);
	TEST_ASSERT_NOT_NULL(tree->right);
	TEST_ASSERT_EQUAL_INT(COMMAND, tree->right->token);
	TEST_ASSERT_EQUAL_STRING("grep foo", tree->right->value);
	free_tree_nodes(tree);
}

static void test_parser_redirect_out(void)
{
	t_list	n1, n2;
	t_tree	*tree;

	n1.content = "echo hi";
	n1.next = &n2;
	n2.content = "> out.txt";
	n2.next = NULL;
	tree = parser(&n1, 2, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(RDOUT, tree->token);
	TEST_ASSERT_NOT_NULL(tree->left);
	TEST_ASSERT_EQUAL_INT(COMMAND, tree->left->token);
	free_tree_nodes(tree);
}

static void test_parser_redirect_in(void)
{
	t_list	n1, n2;
	t_tree	*tree;

	n1.content = "< input.txt";
	n1.next = &n2;
	n2.content = "cat";
	n2.next = NULL;
	tree = parser(&n1, 2, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(RDIN, tree->token);
	free_tree_nodes(tree);
}

static void test_parser_heredoc(void)
{
	t_list	n1, n2;
	t_tree	*tree;

	n1.content = "<< EOF";
	n1.next = &n2;
	n2.content = "cat";
	n2.next = NULL;
	tree = parser(&n1, 2, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(HEREDOC, tree->token);
	free_tree_nodes(tree);
}

static void test_parser_append(void)
{
	t_list	n1, n2;
	t_tree	*tree;

	n1.content = "echo hi";
	n1.next = &n2;
	n2.content = ">> out.txt";
	n2.next = NULL;
	tree = parser(&n1, 2, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(RDAPP, tree->token);
	free_tree_nodes(tree);
}

static void test_parser_pipe_has_priority_over_redirect(void)
{
	t_list	n1, n2, n3, n4;
	t_tree	*tree;

	n1.content = "echo hi";
	n1.next = &n2;
	n2.content = "|";
	n2.next = &n3;
	n3.content = "> out.txt";
	n3.next = &n4;
	n4.content = "cat";
	n4.next = NULL;
	tree = parser(&n1, 4, &g_sh);
	TEST_ASSERT_NOT_NULL(tree);
	TEST_ASSERT_EQUAL_INT(PIPE, tree->token);
	free_tree_nodes(tree);
}

int main(void)
{
	UNITY_BEGIN();
	/* tree_node_new */
	RUN_TEST(test_tree_node_new_basic);
	RUN_TEST(test_tree_node_new_with_children);
	/* parser */
	RUN_TEST(test_parser_null_returns_null);
	RUN_TEST(test_parser_zero_tokens_returns_null);
	RUN_TEST(test_parser_single_command);
	RUN_TEST(test_parser_pipe);
	RUN_TEST(test_parser_redirect_out);
	RUN_TEST(test_parser_redirect_in);
	RUN_TEST(test_parser_heredoc);
	RUN_TEST(test_parser_append);
	RUN_TEST(test_parser_pipe_has_priority_over_redirect);
	return UNITY_END();
}
