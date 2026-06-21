#include "libft.h"

/**
 * ft_strtrim - Trims characters from the start and end of a string.
 * @param s1: The string to be trimmed.
 * @param set: The set of characters to trim from s1.
 *
 * @returns A new string with characters in 'set' removed
 * 			from the start and end of 's1',
 * 			or NULL if memory allocation fails.
 */
char	*ft_strtrim(char const *s1, char const *set)
{
	char			*array;
	size_t			slen;

	if (s1 == NULL || set == NULL)
		return (NULL);
	slen = ft_strlen(s1);
	while (*s1 != '\0' && ft_strchr(set, *s1) != NULL)
	{
		++s1;
		--slen;
	}
	while (slen > 0 && ft_strrchr(set, s1[slen - 1]) != NULL)
		--slen;
	array = ft_calloc((slen + 1), sizeof(char));
	if (!array)
		return (NULL);
	ft_strlcpy(array, s1, slen + 1);
	return (array);
}
