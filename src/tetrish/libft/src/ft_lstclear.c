#include "libft.h"

/**
 * ft_lstclear - Deletes and frees the given element and all successors
 * from the list.
 * @param lst: A double pointer to the first element of the list to be cleared.
 * @param del: A pointer to a function that deletes the node's content.
 *
 * @returns 
 * Nothing. After this function is called, all pointers to the elements
 * of 'lst' and 'lst' itself are no longer valid and should not be used.
 */
void	ft_lstclear(t_list **lst, void (*del)(void *))
{
	t_list	*temp;
	t_list	*current;

	if (!lst)
		return ;
	current = *lst;
	while (current)
	{
		temp = current;
		current = current->next;
		ft_lstdelone(temp, del);
	}
	*lst = NULL;
}
