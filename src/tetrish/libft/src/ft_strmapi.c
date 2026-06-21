#include "libft.h"

/*
**	Applies the function ’f’ to each character of the string ’s’, and passing 
**	its index as first argument to create a new string with ft_calloc 
**	resulting from successive applications of ’f’.
**
**	The ft_strmapi() returns the string created from the successive applications.
**	of 'f'. Returns NULL if the allocation fails.
*/
char	*ft_strmapi(char const *s, char (*f)(unsigned int, char))
{
	char			*array;
	unsigned int	i;

	i = 0;
	if (s == NULL)
		return (NULL);
	array = ft_calloc((ft_strlen(s) + 1), sizeof(char));
	if (!array)
		return (NULL);
	while (s[i] != '\0')
	{
		array[i] = f(i, s[i]);
		++i;
	}
	array[i] = '\0';
	return (array);
}
