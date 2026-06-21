#include "minishell.h"

static int	pipe_helper(char **cmd, int *token_count);
static int	redirection_helper(char **cmd, int *token_count);
static int	quotes_helper(char **cmd);

/**
 * @brief Counts tokens in a given command string.
 *
 * A token is a non-space sequence, or a '|', '<', or '>'. Uses helper
 * functions to handle pipes, redirections, and quotes. If a quote is
 * not properly closed, the function will return -1.
 *
 * @param cmd The command string.
 * @return Number of tokens or -1 if unclosed quote is encountered.
 */
int	count_token(char *cmd)
{
	int	token_count;

	token_count = 0;
	while (*cmd != '\0')
	{
		while (*cmd == SPACE)
				++cmd;
		if (*cmd == '|')
			pipe_helper(&cmd, &token_count);
		else if (*cmd != '\0' && ft_strchr("<>", *cmd) != NULL)
		{
			if (redirection_helper(&cmd, &token_count) == -1)
				return (-1);
		}
		else
		{
			while (*cmd != '\0' && ft_strchr("|<>", *cmd) == NULL)
				if (quotes_helper(&cmd) == -1)
					return (-1);
			++token_count;
		}
	}
	return (token_count);
}

/**
 * @brief Handles the pipe operator '|' in the command string.
 *
 * Increments the command pointer and the token count.
 *
 * @param cmd Pointer to the command string pointer.
 * @param token_count Pointer to the token count.
 * @return Always returns 1.
 */
static int	pipe_helper(char **cmd, int *token_count)
{
	while (**cmd == '|')
		++(*cmd);
	++(*token_count);
	return (1);
}

/**
 * @brief Handles redirection operators '<' and '>' in the command string.
 *
 * Moves the command string pointer past redirection operator and any
 * subsequent spaces or identical redirection operators. It handles
 * quotes and returns -1 if an unclosed quote is found.
 *
 * @param cmd Pointer to the command string pointer.
 * @param token_count Pointer to the token count.
 * @return 1 if successful or -1 if unclosed quote is encountered.
 */
static int	redirection_helper(char **cmd, int *token_count)
{
	++(*cmd);
	while (**cmd == *(*cmd - 1) || **cmd == SPACE)
		++(*cmd);
	while (**cmd != '\0' && ft_strchr("|<> ", **cmd) == NULL)
		if (quotes_helper(cmd) == -1)
			return (-1);
	++(*token_count);
	return (1);
}

/**
 * @brief Handles quotes in the command string.
 *
 * Uses the `quote_count` function to find length of quoted string and
 * moves the command string pointer past the quoted string.
 *
 * @param cmd Pointer to the command string pointer.
 * @return 1 if successful or -1 if an unclosed quote is found.
 */
static int	quotes_helper(char **cmd)
{
	int	quote_len;

	quote_len = quote_count((*cmd));
	if (quote_len == -1)
	{
		ft_putstr_fd("Error: unclosed quote.\n", STDERR_FILENO);
		return (-1);
	}
	else if (quote_len == 0)
		++(*cmd);
	else if (quote_len > 0)
		*cmd += quote_len;
	return (1);
}
