#include "minishell.h"

static void	quote_cmd_mod(char **cmd, char **tokens, int *i, int *j);
static void	pipe_tokenize(char **cmd, char **tokens, int *i, int *j);
static void	redir_tokenize(char **cmd, char **tokens, int *i, int *j);

/**
 * @brief Modifies the command to handle special tokens.
 *
 * This function processes the command string and handles special characters
 * such as quotes, pipes, and redirections, tokenizing them into an array.
 *
 * @param cmd Input command string.
 * @param tokens Array to store the tokenized command components.
 */
void	cmd_modifier(char *cmd, char **tokens)
{
	int	i;
	int	j;

	i = 0;
	while (*cmd != '\0')
	{
		j = 0;
		while (*cmd == SPACE)
			++cmd;
		if (*cmd == '|')
			pipe_tokenize(&cmd, tokens, &i, &j);
		else if (ft_strchr("<>", *cmd) != NULL && *cmd != '\0')
			redir_tokenize(&cmd, tokens, &i, &j);
		else if (*cmd != '\0')
		{
			tokens[i] = ft_calloc(count_char(cmd) + 1, sizeof(char));
			while (ft_strchr("|<>", *cmd) == NULL && *cmd != '\0')
				quote_cmd_mod(&cmd, tokens, &i, &j);
			tokens[i++][j] = '\0';
		}
	}
}

/**
 * @brief Handles quote modifications within the command.
 *
 * This function processes quotes within the command and modifies the tokens
 * array to include the quoted content as required.
 *
 * @param cmd Pointer to the current position in the command string.
 * @param tokens Array to store the tokenized command components.
 * @param i Pointer to the current token index.
 * @param j Pointer to the current character index within the token.
 */
static void	quote_cmd_mod(char **cmd, char **tokens, int *i, int *j)
{
	int	quote_len;

	quote_len = quote_count(*cmd);
	if (quote_len == 0)
	{
		tokens[*i][(*j)++] = **cmd;
		++(*cmd);
	}
	else
	{
		while (quote_len-- > 0)
		{
			tokens[*i][(*j)++] = **cmd;
			++(*cmd);
		}
	}
}

/**
 * @brief Tokenizes the pipe characters within the command.
 *
 * This function handles pipe characters '|' in the command string and
 * tokenizes them into the tokens array.
 *
 * @param cmd Pointer to the current position in the command string.
 * @param tokens Array to store the tokenized command components.
 * @param i Pointer to the current token index.
 * @param j Pointer to the current character index within the token.
 */
static void	pipe_tokenize(char **cmd, char **tokens, int *i, int *j)
{
	tokens[*i] = ft_calloc(count_sp_char(*cmd) + 1, sizeof(char));
	tokens[*i][(*j)++] = **cmd;
	++(*cmd);
	while (**cmd == *(*cmd - 1))
	{
		tokens[*i][(*j)++] = **cmd;
		++(*cmd);
	}
	tokens[(*i)++][*j] = '\0';
}

/**
 * @brief Tokenizes the redirection characters within the command.
 *
 * This function handles redirection characters '<' and '>' in the command
 * string and tokenizes them, along with related spaces, into the tokens array.
 *
 * @param cmd Pointer to the current position in the command string.
 * @param tokens Array to store the tokenized command components.
 * @param i Pointer to the current token index.
 * @param j Pointer to the current character index within the token.
 */
static void	redir_tokenize(char **cmd, char **tokens, int *i, int *j)
{
	tokens[*i] = ft_calloc(count_sp_char(*cmd) + 1, sizeof(char));
	tokens[*i][(*j)++] = **cmd;
	++(*cmd);
	while (**cmd == *(*cmd - 1) || **cmd == SPACE)
	{
		tokens[*i][(*j)++] = **cmd;
		++(*cmd);
	}
	if (ft_strchr("<>", *(*cmd - 1)) != NULL && **cmd != ' ' && **cmd != '\0')
		tokens[*i][(*j)++] = SPACE;
	while (ft_strchr("|<> ", **cmd) == NULL && **cmd != '\0')
		quote_cmd_mod(cmd, tokens, i, j);
	tokens[*i][*j] = '\0';
	++(*i);
}
