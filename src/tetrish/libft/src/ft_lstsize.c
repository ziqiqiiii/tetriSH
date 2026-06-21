#include "libft.h"

/*
**	Counts the number of nodes in a list.
**
**	Returns the length of the list.
*/
int	ft_lstsize(t_list *lst)
{
	unsigned int	count;

	count = 0;
	if (!lst)
		return (0);
	while (lst)
	{
		lst = lst->next;
		++count;
	}
	return (count);
}
