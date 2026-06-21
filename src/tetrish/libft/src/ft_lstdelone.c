#include "libft.h"

/**
 * ft_lstdelone - Deletes a node from a linked list.
 * @param lst: The node to be deleted.
 * @param del: A pointer to a function that deletes the node's content.
 *
 * This function uses the 'del' function to delete the node's content, 
 * and then frees the memory of the node itself. After this function is 
 * called, the pointer 'lst' is no longer valid and should not be used.
 */
void	ft_lstdelone(t_list *lst, void (*del)(void *))
{
	if (!lst)
		return ;
	del(lst->content);
	free(lst);
}
