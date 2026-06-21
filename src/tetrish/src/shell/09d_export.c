#include "minishell.h"

static void	export_declare(t_list **env_list);
static void	modified_value(t_env *data_node, char *input);
static int	add_link_list(char	*input, t_list	**env_list);

/**
 * @brief Handles the export command in the shell, allowing for the
 *        modification and display of environment variables.
 *
 * This function takes the export command and arguments and either
 * displays the current environment variables or modifies them
 * based on the given input. The function also checks for invalid
 * identifiers.
 *
 * @param cmd Array of command strings, including "export" and its arguments.
 * @param env_list Pointer to the linked list of environment variables.
 * @return EXIT_SUCCESS if successful, or EXIT_FAILURE if an error occurs.
 */
int	export(char **cmd, t_list **env_list)
{
	char	**split;
	int		i;

	i = 1;
	split = cmd;
	if (split[1] == NULL)
	{
		export_declare(env_list);
		return (EXIT_SUCCESS);
	}
	else
	{
		while (split[i])
		{
			if (invalid_identifier(split[i]) == 1)
				return (EXIT_FAILURE);
			else
				if (add_link_list(split[i], env_list) == 1)
					return (EXIT_FAILURE);
			i++;
		}
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Prints declarations for all environment variables.
 *
 * This function iterates through the linked list of environment variables
 * and prints each key-value pair in declaration format.
 *
 * @param env_list Pointer to the linked list of environment variables.
 */
static void	export_declare(t_list **env_list)
{
	t_list	*tmp;
	t_env	*data;

	tmp = *env_list;
	while (tmp)
	{
		data = (t_env *)tmp->content;
		if (data->value == NULL)
			printf("declare -x %s\n", data->key);
		else
			printf("declare -x %s=\"%s\"\n", data->key, data->value);
		tmp = tmp->next;
	}
}

/**
 * @brief Modifies the value of an existing environment variable.
 *
 * This function takes a pointer to an environment variable node and
 * the new value to be assigned and modifies the value accordingly.
 *
 * @param data_node Pointer to the environment variable node.
 * @param input New value string to be assigned.
 */
static void	modified_value(t_env *data_node, char *input)
{
	char	*value;
	char	*tmp;
	char	*equal_ptr;

	equal_ptr = ft_strchr(input, EQUAL);
	value = ft_substr(input, equal_ptr - input + 1, \
						ft_strlen(input) - (equal_ptr - input));
	tmp = data_node->value;
	data_node->value = value;
	free(tmp);
}

/**
 * @brief Adds a new link to the environment variables list or modifies an
 *        existing one.
 *
 * This function takes a key-value pair input and either creates a new
 * environment variable or modifies an existing one based on the key.
 *
 * @param input Input key-value string.
 * @param env_list Pointer to the linked list of environment variables.
 * @return EXIT_SUCCESS if successful, or EXIT_FAILURE if an error occurs.
 */
static int	add_link_list(char	*input, t_list	**env_list)
{
	char	*key;
	t_env	*data;
	t_list	*tmp;
	char	*equal_ptr;
	int		i;

	key = key_check(input);
	if (key == NULL)
		return (EXIT_FAILURE);
	i = 1;
	tmp = *env_list;
	while (tmp)
	{
		data = (t_env *)tmp->content;
		i = ft_strncmp(data->key, key, ft_strlen(data->key) + 1);
		if (i == 0)
		{
			if (ft_strchr(input, EQUAL) != NULL)
				modified_value(data, input);
			break ;
		}
		tmp = tmp->next;
	}
	if (i != 0)
		creat_new_env_node(key, input, env_list);
	equal_ptr = ft_strchr(input, EQUAL);
	if (equal_ptr != NULL)
		setenv(key, equal_ptr + 1, 1);
	free(key);
	return (EXIT_SUCCESS);
}

/**
 * @brief Checks if the input string is a valid identifier for an environment
 *        variable.
 *
 * This function examines the input string to determine if it meets the
 * criteria for a valid environment variable identifier.
 *
 * @param input Input string to check.
 * @return EXIT_SUCCESS if it's a valid identifier, or EXIT_FAILURE if not.
 */
int	invalid_identifier(char *input)
{
	char	*start;
	char	*equal_ptr;

	start = input;
	equal_ptr = ft_strchr(input, EQUAL);
	if (start == equal_ptr || (ft_isalpha(*start) == 0 && *start != '_'))
	{
		printf("export 1: '%s': not a valid identifier\n", start);
		g_exit_status = 1;
		return (EXIT_FAILURE);
	}
	while (input < equal_ptr)
	{	
		if (ft_isalnum(*input) == 0 && *input != '_')
		{
			printf("export 1: '%s': not a valid identifier\n", start);
			g_exit_status = 1;
			return (EXIT_FAILURE);
		}
		++input;
	}
	return (EXIT_SUCCESS);
}
