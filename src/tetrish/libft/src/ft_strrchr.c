#include "libft.h"

/**
 * ft_strrchr - Locates the occurrence of 'c' in string 's' in reverse order.
 * @param s: The string to be scanned.
 * @param c: The character to be located.
 * Returns: A pointer to the last occurrence of 'c' in 's', 
 * 			or NULL if 'c' is not found.
 */
char	*ft_strrchr(const char *s, int c)
{
	int	len;

	len = ft_strlen(s);
	s += len;
	if (c >= 256)
		c -= 256;
	while (len-- >= 0)
	{
		if (*s == c)
			return ((char *)s);
		--s;
	}
	return (NULL);
}
