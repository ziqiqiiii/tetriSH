#include "minishell.h"

/**
 * @brief Creates a new tree node.
 *
 * This function allocates memory for a new binary tree node and initializes it
 * with the given token type, value, left child, and right child.
 *
 * @param type Token type.
 * @param value Value of the token.
 * @param left Left child of the tree node.
 * @param right Right child of the tree node.
 * @return A pointer to the new tree node, or NULL if memory allocation fails.
 */
t_tree	*tree_node_new(t_token type, char *value, t_tree *left, t_tree *right)
{
	t_tree	*tree;

	tree = ft_calloc(1, sizeof(t_tree));
	if (!tree)
		return (NULL);
	tree->token = type;
	tree->value = value;
	tree->left = left;
	tree->right = right;
	return (tree);
}

/**
 * @brief Prints the structure of a binary tree.
 *
 * This function recursively prints the token type, value, left child,
 * and right child of each node in the binary tree, indicating the
 * current level of the tree for clarity.
 *
 * @param root Root of the binary tree.
 * @param b A control variable used internally for level tracking.
 */
void	print_tree(t_tree *root, int b)
{
	static int	level;

	if (b == 0)
		++level;
	if (root == NULL)
	{
		printf("NULL\n");
		if (b == 0)
			--level;
		return ;
	}
	printf("token (%u): (%s)\n", root->token, root->value);
	printf("left %i  ", level);
	print_tree(root->left, 0);
	printf("right %i  ", level);
	print_tree(root->right, 0);
	--level;
}
