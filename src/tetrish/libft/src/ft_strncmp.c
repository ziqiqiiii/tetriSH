#include "libft.h"

/* 
	The ft_strncmp() function compares null-terminated strings s1 and s2 up to 
	not more than n characters.  Because ft_strncmp() is designed for comparing 
	strings rather than binary data, characters that appear after a `\0' 
	character are not compared.
	
	RETURN VALUE
	- Return 0 if the strings are identical
	- Return +ve if s1 is greater than s2
	- Return -ve if s1 is less than s2
*/
int	ft_strncmp(const char *s1, const char *s2, size_t n)
{
	if (n == 0)
		return (0);
	while (n-- > 1 && (*s1 != '\0' || *s2 != '\0') && (*s1 == *s2))
	{
		++s1;
		++s2;
	}
	return (*(unsigned char *)s1 - *(unsigned char *)s2);
}
