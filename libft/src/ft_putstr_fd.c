#include "libft.h"

/*
**	Outputs the string 's' to the given file descriptor.
*/
void	ft_putstr_fd(char *s, int fd)
{
	if (s == NULL)
		return ;
	while (*s != '\0')
		write(fd, s++, 1);
}
