#include "libft.h"

/**
 * ft_strdup - Duplicates a string by allocating enough memory, 
 *             copying the characters, and returning a pointer to the new string.
 *
 * @param src: The source string to be duplicated.
 *
 * @return 
 * A pointer to the newly allocated string, or NULL if memory allocation fails.
 * The caller is responsible for freeing the memory when it is no longer needed.
 */
char	*ft_strdup(const char *src)
{
	char	*dup;
	char	*d;

	dup = ft_calloc((ft_strlen(src) + 1), sizeof(char));
	if (!dup)
		return (NULL);
	d = dup;
	while (*src != '\0')
		*d++ = *src++;
	*d = '\0';
	return (dup);
}
