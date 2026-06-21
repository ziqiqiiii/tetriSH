#include "libft.h"

/*
**	Iterates the list 'lst' and applies the function 'f' on the content of 
**	each node.
*/
void	ft_lstiter(t_list *lst, void (*f)(void *))
{
	if (!lst)
		return ;
	while (lst)
	{
		f(lst->content);
		lst = lst->next;
	}
}
