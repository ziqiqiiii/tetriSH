#include "libft.h"

/**
 * @brief Copies up to dstsize - 1 characters from the string 'src' to 'dst', 
 *        null-terminating the result.
 *
 * If 'dstsize' is 0, it does not write anything to 'dst' and simply returns 
 * the length of 'src'. 
 *
 * @param dst The destination string.
 * @param src The source string.
 * @param dstsize Size of the destination string buffer.
 * @return The length of 'src'.
 */
size_t	ft_strlcpy(char *dst, const char *src, size_t dstsize)
{
	size_t	i;

	i = 0;
	if (!dstsize)
		return (ft_strlen(src));
	while (i < (dstsize - 1) && src[i])
	{
		dst[i] = src[i];
		++i;
	}
	dst[i] = '\0';
	while (src[i] != '\0')
		i++;
	return (i);
}
