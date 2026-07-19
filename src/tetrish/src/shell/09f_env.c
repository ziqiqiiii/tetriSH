#include "minishell.h"

/**
 * @brief Prints the environment variables.
 *
 * Iterates through a linked list of environment variables, printing the key
 * and value for each non-NULL value.
 *
 * @param env_list A pointer to the linked list of environment variables.
 * @return EXIT_SUCCESS, indicating successful execution.
 */
int	get_env(t_list **env_list)
{
	t_env	*data;
	t_list	*tmp;

	tmp = *env_list;
	while (tmp)
	{
		data = (t_env *)tmp->content;
		if (data->value != NULL)
			printf("%s=%s\n", data->key, data->value);
		tmp = tmp->next;
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Initializes a linked list of environment variables.
 *
 * Creates a linked list of environment variables from the given array of 
 * strings. The array is expected to contain strings in the format "key=value".
 *
 * @param envp The array of environment variable strings.
 * @param env_list A pointer to the linked list of environment variables.
 * @return EXIT_SUCCESS on success, or EXIT_FAILURE if an allocation fails.
 */
int	env_link_list(char **envp, t_list **env_list)
{
	int		i;
	t_env	*content;
	t_list	*node;
	char	*equal_ptr;

	i = 0;
	while (envp[i])
	{
		content = ft_calloc(1, sizeof(t_env));
		if (!content)
			return (EXIT_FAILURE);
		equal_ptr = ft_strchr(envp[i], EQUAL);
		content->key = ft_substr(envp[i], 0, equal_ptr - envp[i]);
		content->value = ft_substr(envp[i], equal_ptr - envp[i] + 1, \
						ft_strlen(envp[i]) - (equal_ptr - envp[i]));
		node = ft_lstnew(content);
		ft_lstadd_back(env_list, node);
		i++;
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Retrieves the value of an environment variable.
 *
 * Searches the linked list of environment variables for a key and returns
 * its corresponding value.
 *
 * @param key The key of the environment variable to retrieve.
 * @param env_list A pointer to the linked list of environment variables.
 * @return The value of the environment variable if it exists, or NULL if not 
 * 			found.
 */
char	*existed_env(char *key, t_list **env_list)
{
	char	*value;
	t_env	*data;
	t_list	*tmp;

	if (key == NULL)
		return (NULL);
	value = NULL;
	tmp = *env_list;
	while (tmp)
	{
		data = (t_env *)tmp->content;
		if (ft_strncmp(data->key, key, ft_strlen(key) + 1) == 0)
		{
			value = data->value;
			break ;
		}
		tmp = tmp->next;
	}
	return (value);
}

/**
 * @brief Creates a new environment variable node.
 *
 * Allocates and initializes a new environment variable node with the given
 * key and input. The input is expected to be in the format "key=value".
 * The node is added to the end of the linked list.
 *
 * @param key The key for the new environment variable.
 * @param input The input string containing the key and value.
 * @param env_list A pointer to the linked list of environment variables.
 */
void	creat_new_env_node(char *key, char	*input, t_list **env_list)
{
	t_env	*data;
	t_list	*node;
	char	*equal_ptr;

	data = ft_calloc(1, sizeof(t_env));
	equal_ptr = ft_strchr(input, EQUAL);
	data->key = ft_substr(key, 0, ft_strlen(key));
	data->value = NULL;
	if (equal_ptr != NULL)
		data->value = ft_substr(input, equal_ptr - input + 1, \
							ft_strlen(input) - (equal_ptr - input));
	node = ft_lstnew(data);
	ft_lstadd_back(env_list, node);
}
