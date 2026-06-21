#include "minishell.h"

/**
 * @brief Frees the memory associated with a t_env node.
 *
 * Casts the generic content pointer to t_env, then frees the key,
 * value, and the struct itself.
 *
 * @param content Pointer to the t_env struct to be freed.
 */
void	del_data(void	*content)
{
	t_env	*data;

	data = (t_env *)content;
	free(data->key);
	free(data->value);
	free(data);
}

/**
 * @brief Resets the shell data, closes temporary file descriptors, 
 *        restores terminal attributes, and frees memory.
 *
 * This function is used to reset the state of the shell after execution 
 * of a command. It closes temporary file descriptors, restores terminal 
 * attributes, removes temporary files, and frees allocated memory.
 *
 * @param sh Pointer to the root shell structure.
 * @param cmd_lexer Pointer to the command lexer list.
 * @param head Pointer to the root of the syntax tree.
 */
void	reset_data(t_root *sh, t_list **cmd_lexer, t_tree **head)
{
	t_list	*current;
	t_list	*next;

	ft_dup2(sh->stdin_tmp, STDIN_FILENO);
	ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
	ft_tcsetattr(STDIN_FILENO, TCSANOW, &sh->current);
	if (access(".here_doc_tmp", F_OK & X_OK) == 0)
		unlink(".here_doc_tmp");
	free_tree(*head);
	sh->tree_arg_value = NULL;
	current = *cmd_lexer;
	while (current)
	{
		next = current->next;
		free(current);
		current = next;
	}
}

/**
 * @brief Frees a 2D array of strings.
 *
 * This function takes a pointer to an array of strings and frees
 * each string, followed by the array itself.
 *
 * @param str Pointer to the array of strings.
 */
void	free_2d(char **str)
{
	int	i;

	i = -1;
	while (str[++i])
		free(str[i]);
	free(str);
}

/**
 * @brief Frees a binary tree structure.
 *
 * This function takes a pointer to the root of a binary tree and
 * recursively frees each node, starting from the leaves and working
 * its way to the root.
 *
 * @param node Pointer to the root of the tree.
 */
void	free_tree(t_tree *node)
{
	if (node == NULL)
		return ;
	free_tree(node->left);
	free_tree(node->right);
	free(node->value);
	free(node);
}
