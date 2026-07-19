#include "libft.h"

/*
**	Allocates (with malloc(3)) and returns a new node. The member variable 
**	'content' is initialized with the value of the parameter 'content'. 
**	The variable 'next' is initialized to NULL.
*/
t_list	*ft_lstnew(void *content)
{
	t_list	*list;

	list = ft_calloc(1, sizeof(t_list));
	if (!list)
		return (NULL);
	list->content = content;
	list->next = NULL;
	return (list);
}
