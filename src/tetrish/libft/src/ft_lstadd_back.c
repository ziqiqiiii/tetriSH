#include "libft.h"

/*
**	Adds the node 'new' at the end of the list.
*/
void	ft_lstadd_back(t_list **lst, t_list *new)
{
	t_list	*temp;

	if (!new)
		return ;
	if (!*lst)
		*lst = new;
	else
	{
		temp = *lst;
		temp = ft_lstlast(temp);
		temp->next = new;
	}
}
