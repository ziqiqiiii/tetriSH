#include "libft.h"

/**
 * @brief Counts the number of substrings separated by a character in a string.
 *
 * Substrings are sequences of characters not including the separator character.
 *
 * @param s The string to count substrings in.
 * @param c The separator character.
 * @return The number of substrings separated by 'c' in 's'.
 */
static int	countstr(char const *s, char c)
{
	int		count;

	count = 0;
	while (*s != '\0')
	{
		if (*s != c)
		{
			++count;
			s = ft_strchr(s, c);
			if (!s)
				break ;
		}
		else
			++s;
	}
	return (count);
}

/**
 * @brief Finds the next substring in a string separated by a character.
 *
 * The substring is the sequence of characters from the current position to 
 * the next occurrence of the separator character.
 * The function allocates memory for the substring and returns it.
 * The source string pointer is updated to point to the end of the found 
 * substring.
 *
 * @param s Pointer to the string pointer.
 * @param c The separator character.
 * @return Pointer to the found substring, or NULL if memory allocation fails.
 */
static char	*nextstr(char const **s, char c)
{
	char	*start;
	char	*end;
	char	*str;

	while (**s == c)
		++(*s);
	start = (char *)*s;
	end = ft_strchr(start, c);
	if (!end)
		end = ft_strchr(start, '\0');
	*s = end;
	str = ft_substr(start, 0, end - start);
	return (str);
}

/**
 * @brief Splits a string into an array of substrings separated by a character.
 *
 * The function allocates memory for the array and the substrings.
 * If a memory allocation fails, all previously allocated memory is freed.
 *
 * @param s The source string.
 * @param c The separator character.
 * @return Pointer to the array of substrings, 
 *         or NULL if the source string is NULL or memory allocation fails.
 */
char	**ft_split(char const *s, char c)
{
	char	**res;
	int		i;
	int		n;

	if (s == NULL)
		return (NULL);
	n = countstr(s, c);
	res = ft_calloc(n + 1, sizeof(char *));
	if (!res)
		return (NULL);
	i = 0;
	while (i < n)
	{
		res[i] = nextstr(&s, c);
		if (!res[i])
		{
			while (i > 0)
				free(res[--i]);
			free(res);
			return (NULL);
		}
		++i;
	}
	return (res);
}
