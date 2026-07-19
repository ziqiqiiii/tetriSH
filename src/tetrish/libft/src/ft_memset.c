#include "libft.h"

/*
**	The ft_memset() function writes len bytes of value c
**	(converted to an unsigned char) to the string b.
**
**	The ft_memset() function returns its first argument.
*/

void	*ft_memset(void *b, int c, size_t len)
{
	unsigned char	*ptr;

	ptr = (unsigned char *)b;
	while (len-- != 0)
		*ptr++ = (unsigned char) c;
	return (b);
}
