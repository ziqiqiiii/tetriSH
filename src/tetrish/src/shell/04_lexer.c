/******************************************************************************/

#include "minishell.h"

static char	**tokenizer(char *cmd);

/**
 * @brief Converts a command into a linked list of tokens.
 *
 * This function tokenizes the command using the tokenizer function and then
 * constructs a linked list from the tokens, trimming any extraneous whitespace.
 *
 * @param cmd The command string to be tokenized.
 * @return A linked list of tokens or NULL if an error occurs.
 */
t_list	*lexer(char *cmd)
{
	char	**tokens;
	char	**tokens_head;
	t_list	*head;
	t_list	*node;

	tokens = tokenizer(cmd);
	if (!tokens)
		return (NULL);
	tokens_head = tokens;
	head = ft_lstnew(ft_strtrim(*tokens++, " "));
	if (!head)
		return (NULL);
	node = head;
	while (*tokens)
	{
		node->next = ft_lstnew(ft_strtrim(*tokens++, " "));
		if (!(node->next))
		{
			ft_lstclear(&head, free);
			return (NULL);
		}
		node = node->next;
	}
	free_2d(tokens_head);
	return (head);
}

/**
 * @brief Tokenizes the given command.
 *
 * This function uses the cmd_modifier to split the command string into
 * individual tokens and returns them in a dynamically allocated array.
 *
 * @param cmd The command string to tokenize.
 * @return An array of tokens or NULL if an error occurs.
 */
static char	**tokenizer(char *cmd)
{
	int		token_num;
	char	**tokens;

	if (!cmd)
		return (NULL);
	if (!(*cmd))
	{
		free(cmd);
		return (NULL);
	}
	token_num = count_token(cmd);
	if (token_num == -1)
	{
		free(cmd);
		return (NULL);
	}
	tokens = ft_calloc(token_num + 1, sizeof(char *));
	if (!tokens)
	{
		free(cmd);
		return (NULL);
	}
	cmd_modifier(cmd, tokens);
	if (!*tokens)
	{
		free(tokens);
		free(cmd);
		return (NULL);
	}
	return (tokens);
}
