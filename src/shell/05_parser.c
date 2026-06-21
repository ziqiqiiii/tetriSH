#include "minishell.h"

static t_tree	*token_check(t_list *lexer, char *op, int n_token, t_root *sh);
static t_token	type_assign(char *value, t_root *sh);

/**
 * @brief Parses tokens from the lexer and constructs a syntax tree.
 *
 * This function takes a list of tokens from the lexer and parses them into
 * a syntax tree, considering different types of operations, including pipes
 * and redirection. 
 *
 * @param lexer List of tokens.
 * @param n_token Number of tokens in the list.
 * @param sh Shell root structure.
 * @return Root of the constructed syntax tree, or NULL if parsing fails.
 */
t_tree	*parser(t_list *lexer, int n_token, t_root *sh)
{
	t_tree	*root;

	root = NULL;
	if (!lexer || n_token <= 0)
		return (NULL);
	root = token_check(lexer, PIPE_OP, n_token, sh);
	if (!root)
		root = token_check(lexer, RDIN_OP, n_token, sh);
	if (!root)
		root = token_check(lexer, RDOUT_OP, n_token, sh);
	if (!root)
		root = token_check(lexer, NULL, n_token, sh);
	return (root);
}

/**
 * @brief Checks tokens for specific operators and constructs a subtree.
 *
 * This recursive function inspects tokens for specific operators (e.g., pipes,
 * redirection) and constructs a subtree based on the type and occurrence of
 * the operator.
 *
 * @param lexer List of tokens.
 * @param op Operator to look for.
 * @param n_token Number of tokens in the list.
 * @param sh Shell root structure.
 * @return A subtree constructed based on the operator, or NULL if not found.
 */
static t_tree	*token_check(t_list *lexer, char *op, int n_token, t_root *sh)
{
	t_tree	*left;
	t_tree	*right;
	t_list	*head;
	t_token	type;
	int		i;

	i = 0;
	head = lexer;
	if (op == NULL)
		return (tree_node_new(COMMAND, lexer->content, NULL, NULL));
	while (lexer != NULL && i < n_token)
	{
		if (ft_strncmp(lexer->content, op, ft_strlen(op)) == 0)
		{
			left = parser(head, i, sh);
			right = parser(lexer->next, n_token - i - 1, sh);
			type = type_assign(lexer->content, sh);
			return (tree_node_new(type, lexer->content, left, right));
		}
		lexer = lexer->next;
		++i;
	}
	return (NULL);
}

/**
 * @brief Assigns a token type based on a given value.
 *
 * This function maps a given value to a corresponding token type by
 * looking up the value in the shell's token check structure.
 *
 * @param value Value to look for.
 * @param sh Shell root structure.
 * @return The corresponding token type, or END if the value is not found.
 */
static t_token	type_assign(char *value, t_root *sh)
{
	int				j;

	j = 0;
	while (sh->tkchk[j].op != NULL)
	{
		if (!ft_strncmp(value, sh->tkchk[j].op, ft_strlen(sh->tkchk[j].op)))
			return (sh->tkchk[j].token);
		++j;
	}
	return (END);
}
