#include "minishell.h"

static char	*nextstr(char const **s, char c);
static int	countstr(char const *s, char c);

/**
 * @brief Splits a string by a delimiter, taking quotes into account.
 *
 * This function splits the input string by a specified character delimiter,
 * considering quotes. A quoted segment is treated as a single unit,
 * preserving the content between quotes.
 *
 * @param s Input string.
 * @param c Delimiter character.
 * @return An array of strings or NULL if allocation fails.
 */
char	**cmd_quote_handler(char const *s, char c)
{
	char	**res;
	int		i;
	int		n;

	if (s == NULL)
		return (NULL);
	n = countstr(s, c);
	res = ft_calloc(n + 1, sizeof(char *));
	i = 0;
	while (i < n)
	{
		res[i] = nextstr(&s, c);
		if (!res[i])
		{
			free_2d(res);
			return (NULL);
		}
		++i;
	}
	return (res);
}

/**
 * @brief Retrieves the next string segment in the split process.
 *
 * This function calculates the length of the next segment, taking quotes
 * into account, then allocates and copies the content into a new string.
 *
 * @param s Pointer to the current position in the input string.
 * @param c Delimiter character.
 * @return A newly allocated string containing the next segment.
 */
static char	*nextstr(char const **s, char c)
{
	char	*start;
	char	*str;
	int		quote_len;
	int		str_len;

	str_len = 0;
	while (**s == c)
		++(*s);
	start = (char *)*s;
	quote_len = quote_count((char *)*s);
	while ((quote_len > 0 || **s != c) && **s != '\0')
	{
		*s += quote_len;
		str_len += quote_len;
		quote_len = 0;
		if (is_quote(**s) != 0)
			quote_len = quote_count((char *)*s);
		else if (**s != c && **s != '\0')
		{
			++(*s);
			++str_len;
		}
	}
	str = ft_substr(start, 0, str_len);
	return (remove_quote(str));
}

/**
 * @brief Counts the number of segments in the split process.
 *
 * This function calculates the number of segments that will be produced
 * by splitting the input string by the given delimiter, considering quotes.
 *
 * @param s Input string.
 * @param c Delimiter character.
 * @return The number of segments in the input string.
 */
static int	countstr(char const *s, char c)
{
	int	count;
	int	quote_len;

	count = 0;
	while (*s != '\0')
	{
		if (*s != c)
		{
			++count;
			quote_len = quote_count((char *)s);
			while ((quote_len > 0 || *s != c) && *s != '\0')
			{
				s += quote_len;
				quote_len = 0;
				if (is_quote(*s) != 0)
					quote_len = quote_count((char *)s);
				else if (*s != SPACE && *s != '\0')
					++s;
			}
		}
		else
			++s;
	}
	return (count);
}
