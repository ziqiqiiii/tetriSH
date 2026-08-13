#include "libft.h"

/**
 * ft_strjoin - Concatenates two strings 's1' and 's2' into a newly 
 *              allocated string.
 * @param s1: The first string.
 * @param s2: The second string.
 *
 * @returns 
 * A pointer to the newly allocated string that contains the contents 
 * of 's1' followed by the contents of 's2', and null-terminated.
 * Or NULL if the allocation fails or if either 's1' or 's2' is NULL.
 */
char	*ft_strjoin(char const *s1, char const *s2)
{
	char			*array;
	size_t			len;

	if (s1 == NULL || s2 == NULL)
		return (NULL);
	len = ft_strlen(s1) + ft_strlen(s2) + 1;
	array = ft_calloc(len, sizeof(char));
	if (!array)
		return (NULL);
	ft_strlcpy(array, s1, len);
	ft_strlcat(array, s2, len);
	return (array);
}
