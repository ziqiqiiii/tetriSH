#include "libft.h"

/*
**	The ft_strlen() function computes the length of the string s.
**
**	The ft_strlen() function returns the number of characters that precede the 
**	terminating NUL character.
*/
int	ft_strlen(const char *s)
{
	int	i;

	i = 0;
	while (s[i] != '\0')
		++i;
	return (i);
}
