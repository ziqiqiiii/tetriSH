#include "libft.h"

/*
**	The ft_bzero() function writes n zeroed bytes to the string s.  If n is zero, 
**	ft_bzero() does nothing.
*/

void	ft_bzero(void *s, size_t n)
{
	ft_memset(s, 0, n);
}
