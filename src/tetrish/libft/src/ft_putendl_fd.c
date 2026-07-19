#include "libft.h"

/*
**	Outputs the string 's' to the given file descriptor followed by a newline.
*/
void	ft_putendl_fd(char *s, int fd)
{
	if (s == NULL)
		return ;
	while (*s != '\0')
		write(fd, s++, 1);
	write(fd, "\n", 1);
}
