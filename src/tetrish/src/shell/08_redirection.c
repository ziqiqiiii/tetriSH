#include "minishell.h"

static void	tree_transversal(t_tree *node, char **envp, t_root *sh);

/**
 * @brief Handles input redirection from a file.
 *
 * Reads from the file specified in the node's value and
 * redirects it to the standard input.
 *
 * @param node Pointer to the node in the binary syntax tree.
 * @param envp Environment variables.
 * @param sh Shell state.
 */
void	rdin_handler(t_tree *node, char **envp, t_root *sh)
{
	int	fd;

	fd = 0;
	fd = rdin_fd(node->value, sh);
	if (fd < 0)
		return ;
	ft_dup2(fd, STDIN_FILENO);
	ft_close(fd);
	tree_transversal(node, envp, sh);
}

/**
 * @brief Handles output redirection to a file.
 *
 * Redirects the standard output to the file specified
 * in the node's value.
 *
 * @param node Pointer to the node in the binary syntax tree.
 * @param envp Environment variables.
 * @param sh Shell state.
 */
void	rdout_handler(t_tree *node, char **envp, t_root *sh)
{
	int	fd;

	fd = 0;
	fd = rdout_fd(node->value, sh);
	if (fd < 0)
		return ;
	ft_dup2(fd, STDOUT_FILENO);
	ft_close(fd);
	tree_transversal(node, envp, sh);
}

/**
 * @brief Handles appending output to a file.
 *
 * Redirects the standard output to the file specified
 * in the node's value, appending to the file.
 *
 * @param node Pointer to the node in the binary syntax tree.
 * @param envp Environment variables.
 * @param sh Shell state.
 */
void	rdapp_handler(t_tree *node, char **envp, t_root *sh)
{
	int	fd;
	int	i;

	i = 0;
	while (node->value[i] == RDOUT_OP[0])
		++i;
	if (i == 3)
		ft_putstr_fd("minishell: syntax error near unexpected token `<'\n", 2);
	else if (i > 3)
		ft_putstr_fd("minishell: syntax error near unexpected token `<<'\n", 2);
	if (i > 2)
	{
		g_exit_status = 258;
		return ;
	}
	fd = 0;
	fd = rdapp_fd(node->value, sh);
	if (fd < 0)
		return ;
	ft_dup2(fd, STDOUT_FILENO);
	ft_close(fd);
	tree_transversal(node, envp, sh);
}

/**
 * @brief Handles here-document input redirection.
 *
 * Reads from a temporary file created from here-document
 * syntax and redirects it to standard input.
 *
 * @param node Pointer to the node in the binary syntax tree.
 * @param envp Environment variables.
 * @param sh Shell state.
 */
void	heredoc_handler(t_tree *node, char **envp, t_root *sh)
{
	int	fd;
	int	i;

	i = 0;
	while (node->value[i] == RDIN_OP[0])
		++i;
	if (i == 3)
		ft_putstr_fd("minishell: syntax error near unexpected token `<'\n", 2);
	else if (i > 3)
		ft_putstr_fd("minishell: syntax error near unexpected token `<<'\n", 2);
	if (i > 2)
	{
		g_exit_status = 258;
		return ;
	}
	fd = 0;
	ft_dup2(sh->stdin_tmp, STDIN_FILENO);
	fd = heredoc_fd(node->value, sh);
	if (fd < 0)
		return ;
	if (node->right == NULL || node->right->token != HEREDOC)
		ft_dup2(fd, STDIN_FILENO);
	ft_close(fd);
	tree_transversal(node, envp, sh);
}

/**
 * @brief Traverses tree for input/output redirection.
 *
 * Calls a recursive function for the right and left children
 * of a node for redirection tokens found during BST traversal.
 *
 * @param node Pointer to the current node in the BST.
 * @param envp Environment variables.
 * @param sh Shell state.
 */
static void	tree_transversal(t_tree *node, char **envp, t_root *sh)
{
	if (node->right != NULL)
	{
		if (node->left != NULL && node->left->token == COMMAND && \
			node->right->token == COMMAND)
			sh->tree_arg_value = node->right->value;
		else
			recurse_bst(node->right, envp, sh);
	}
	if (node->left != NULL)
		recurse_bst(node->left, envp, sh);
}
