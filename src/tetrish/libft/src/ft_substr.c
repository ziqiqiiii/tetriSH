#include "libft.h"

/**
 * @brief Extracts a substring from a string, starting at 'start' for 'len'
 *        characters.
 *
 * If 'start' is beyond the string length, it returns an empty string.
 * If 's' is NULL, it also returns NULL.
 *
 * @param s The source string.
 * @param start Starting index from where substring begins.
 * @param len Length of the substring to be extracted.
 * @return Pointer to the new substring, NULL on error or if 's' is NULL.
 */
char	*ft_substr(char const *s, unsigned int start, size_t len)
{
	char			*sub;
	size_t			i;

	if (s == NULL)
		return (NULL);
	if (start > (unsigned int)ft_strlen(s))
		return (ft_strdup(""));
	s += start;
	i = ft_strlen(s);
	if (i < len)
		len = i;
	sub = ft_calloc((len + 1), sizeof(char));
	if (!sub)
		return (NULL);
	ft_strlcpy(sub, s, len + 1);
	return (sub);
}
