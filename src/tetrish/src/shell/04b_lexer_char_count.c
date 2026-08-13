#include "minishell.h"

static void	count_quote_char(char **cmd, int *count);

/**
 * @brief Counts the special characters in a given command string.
 *
 * Special characters are '|', '<', '>', and any character in quotes.
 * If '<' or '>' is followed by a non-space, non-null character, that character
 * is also counted as a special character.
 *
 * @param cmd The command string.
 * @return The number of special characters in the string.
 */
int	count_sp_char(char *cmd)
{
	int	count;

	count = 0;
	if (*cmd == '|')
	{
		while (*cmd == '|')
		{
			++count;
			++cmd;
		}
	}
	else if (*cmd != '\0' && ft_strchr("<>", *cmd++) != NULL)
	{
		++count;
		while (*cmd == *(cmd - 1) || *cmd == SPACE)
		{
			++count;
			++cmd;
		}
		if (ft_strchr("<>", *(cmd - 1)) != NULL && *cmd != ' ' && *cmd != '\0')
			++count;
		while (*cmd != '\0' && ft_strchr("|<> ", *cmd) == NULL)
			count_quote_char(&cmd, &count);
	}
	return (count);
}

/**
 * @brief Counts the characters in a given command string excluding special
 * characters '|', '<', and '>'.
 *
 * Special characters within quotes are counted. Uses `count_quote_char`
 * function to handle quoted characters.
 *
 * @param cmd The command string.
 * @return The number of characters excluding '|', '<', and '>'.
 */
int	count_char(char *cmd)
{
	int	count;

	count = 0;
	while (ft_strchr("|<>", *cmd) == NULL && *cmd != '\0')
		count_quote_char(&cmd, &count);
	return (count);
}

/**
 * @brief Counts the characters in a given command string, taking into
 * account quoted sequences.
 *
 * Uses the `quote_count` function to find the length of the quoted string.
 * If `quote_count` returns 1, increments count and cmd.
 * Otherwise, increments count and cmd for each character in the quoted string.
 *
 * @param cmd Pointer to the command string pointer.
 * @param count Pointer to the count of characters.
 */
static void	count_quote_char(char **cmd, int *count)
{
	int	quote_len;

	quote_len = quote_count(*cmd);
	if (quote_len == 0)
	{
		++(*count);
		++(*cmd);
	}
	else
	{
		while (quote_len-- > 0)
		{
			++(*count);
			++(*cmd);
		}
	}
}
