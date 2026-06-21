#include "libft.h"

/*
**	The ft_isalnum() function tests for any character for which ft_isalpha or
**	ft_isdigit is true.
**
**	The ft_isalnum() function returns zero if the character tests false and
**	returns non-zero if the character tests true.
*/
int	ft_isalnum(int c)
{
	if (ft_isalpha(c) || ft_isdigit(c))
		return (1);
	return (0);
}
