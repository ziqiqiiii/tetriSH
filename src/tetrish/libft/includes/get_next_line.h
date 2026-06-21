#ifndef GET_NEXT_LINE_H
# define GET_NEXT_LINE_H

# include "libft.h"
# include <fcntl.h>

# define BUFFER_SIZE 1000

void		ft_free(char **str);

char		*gnl_find_nl(char *buffer, int nbyte, char *txtread, int fd);
char		*gnl_join(char *s1, char *s2);
char		*gnl_write(char *buffer, char **res);

char		*get_next_line(int fd);

#endif
