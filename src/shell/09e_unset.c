#include "minishell.h"

static void	unset_helper(char **key, t_list **env_list);

/**
 * @brief Removes one or more environment variables from the linked list.
 *
 * This function iterates over the provided key array and removes each
 * matching environment variable from the linked list.
 *
 * @param key Array of variable names to remove.
 * @param env_list Double pointer to the linked list of environment variables.
 * @return EXIT_SUCCESS upon completion.
 */
int	unset(char **key, t_list **env_list)
{
	if (*key == NULL)
		return (EXIT_SUCCESS);
	while (*key)
	{
		unset_helper(key, env_list);
		++key;
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Helper function to remove a specific environment variable.
 *
 * This function iterates through the linked list and searches for the
 * environment variable with the given key. Once found, it removes the node
 * from the linked list and deallocates the memory used by that node.
 *
 * @param key Pointer to the key of the environment variable to be removed.
 * @param env_list Double pointer to the linked list of environment variables.
 */
static void	unset_helper(char **key, t_list **env_list)
{
	t_list	*tmp;
	t_list	*prev;
	t_env	*data;

	tmp = *env_list;
	prev = NULL;
	while (tmp)
	{
		data = (t_env *)tmp->content;
		if (ft_strncmp(data->key, *key, ft_strlen(data->key) + 1) == 0)
		{
			if (prev == NULL)
				*env_list = tmp->next;
			else
				prev->next = tmp->next;
			ft_lstdelone(tmp, del_data);
			break ;
		}
		prev = tmp;
		tmp = tmp->next;
	}
}
