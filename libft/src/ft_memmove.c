#include "libft.h"

/**
 * @brief Copies 'len' bytes from string 'src' to string 'dst'.
 *
 * The two strings may overlap. Copy is always done in a non-destructive 
 * manner. Returns the original value of 'dst'.
 *
 * @param dst The destination string.
 * @param src The source string.
 * @param len Number of bytes to copy.
 * @return Original 'dst' pointer, or NULL if both 'dst' and 'src' are NULL.
 */
void	*ft_memmove(void *dst, const void *src, size_t len)
{
	unsigned char	*d;

	if (!dst && !src)
		return (NULL);
	d = dst;
	if (d > (unsigned char *)src)
	{
		d += len - 1;
		src += len - 1;
		while (len-- != 0)
			*(unsigned char *)d-- = *(unsigned char *)src--;
	}
	else
	{
		while (len-- != 0)
			*(unsigned char *)d++ = *(unsigned char *)src++;
	}
	return (dst);
}
