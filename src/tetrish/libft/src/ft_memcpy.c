#include "libft.h"

/*
**	The ft_memcpy() function copies len bytes from memory area src to memory
**	area dst. If dst and src overlap, behavior is undefined.
**
**	The ft_memcpy() function returns the original value of dst.
*/
void	*ft_memcpy(void *dst, const void *src, size_t len)
{
	unsigned char	*d;

	if (!dst && !src)
		return (NULL);
	d = dst;
	while (len-- != 0)
		*d++ = *(unsigned char *)src++;
	return (dst);
}
