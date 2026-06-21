#include "minishell.h"

/**
 * @brief Checks if a character is a quote.
 *
 * @param c The character to check.
 * @return 1 if it's a single quote, 2 if it's a double quote,
 * or 0 otherwise.
 */
int	is_quote(char c)
{
	if (c == SINGLE_QUOTE)
		return (1);
	else if (c == DOUBLE_QUOTE)
		return (2);
	return (0);
}

/**
 * @brief Counts the length of the quoted string in a command.
 *
 * Checks the type of quote at the start and counts characters
 * until the closing quote. If a null character is encountered
 * before finding a closing quote, an error message is output.
 *
 * @param cmd The command string.
 * @return The length of the quoted string, including quotes,
 * or 0 if no quote at the start. -1 if an unclosed quote is found.
 */
int	quote_count(char *cmd)
{
	int	count;
	int	quote_type;

	quote_type = is_quote(*cmd++);
	if (!cmd || quote_type == 0)
		return (0);
	count = 1;
	while (is_quote(*cmd) != quote_type)
	{
		if (*cmd == '\0')
			return (-1);
		++count;
		++cmd;
	}
	++count;
	return (count);
}

/**
 * @brief Removes quotes from a string.
 *
 * This function takes a string that may contain quoted parts and removes
 * the quotes, copying the unquoted content into a new string.
 *
 * @param str The input string possibly containing quotes.
 * @return The newly allocated string without quotes.
 */
char	*remove_quote(char *str)
{
	int	i;
	int	j;
	int	quote_type;

	i = 0;
	j = 0;
	while (str[i] != '\0')
	{
		quote_type = is_quote(str[i]);
		if (quote_type != 0)
		{
			++i;
			while (is_quote(str[i]) != quote_type)
				str[j++] = str[i++];
			++i;
		}
		else
			str[j++] = str[i++];
	}
	str[j] = '\0';
	return (str);
}
