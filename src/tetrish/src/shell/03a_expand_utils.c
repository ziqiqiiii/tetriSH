#include "minishell.h"

/**
 * @brief Handles a single quote within the command.
 *
 * This function takes care of the portion of the command enclosed in single
 * quotes. It updates data related to quote handling and command construction.
 *
 * @param data Pointer to a t_expand_var structure containing necessary 
 * 				data for quote handling.
 */
void	single_quote(t_expand_var *data)
{
	data->count = quote_count(data->single_quote_ptr);
	data->len = data->single_quote_ptr - data->start + data->count;
	if (data->substring != NULL)
	{
		free(data->substring);
		data->substring = ft_substr(data->start, 0, data->len);
	}
	data->n_cmd = sub_or_join(data->n_cmd, \
					data->start, data->len, data->substring);
	data->dollar_ptr = data->start;
	data->dollar_ptr += data->len;
}

/**
 * @brief Joins the segment pointed to by the dollar pointer and handles
 * environment variables replacement.
 *
 * This function calculates the length of the segment and replaces
 * environment variables found with their corresponding values.
 *
 * @param data Structure containing expansion-related variables.
 * @param env_list List of environment variables.
 */
void	join_dollar_ptr(t_expand_var *data, t_list **env_list)
{
	data->len = data->dollar_ptr - data->start;
	if (data->substring != NULL)
	{
		free(data->substring);
		data->substring = ft_substr(data->start, 0, data->len);
	}
	data->n_cmd = sub_or_join(data->n_cmd, \
									data->start, data->len, data->substring);
	data->dollar_ptr++;
	if (data->dollar_ptr[0] == '?')
		replace_exit_status(data);
	else
	{
		data->key = key_check(data->dollar_ptr);
		data->value = existed_env(data->key, env_list);
		if (data->value != NULL)
			data->n_cmd = sub_or_join(data->n_cmd, data->start, 0, data->value);
		if (data->key == NULL)
			data->n_cmd = sub_or_join(data->n_cmd, data->start, 0, "$");
		else if (data->key != NULL)
		{
			data->dollar_ptr += ft_strlen(data->key);
			free(data->key);
		}
	}
}

/**
 * @brief Replaces the exit status code within the command expansion.
 *
 * This function replaces the $? expression with the last exit status
 * in the command expansion.
 *
 * @param data Structure containing expansion-related variables.
 */
void	replace_exit_status(t_expand_var *data)
{
	char	*exit_status_str;

	exit_status_str = ft_itoa(g_exit_status);
	data->n_cmd = sub_or_join(data->n_cmd, \
										data->start, 0, exit_status_str);
	free(exit_status_str);
	data->dollar_ptr++;
}

/**
 * @brief Concatenates the provided substring with the command or creates
 * a new substring.
 *
 * This function either joins the given substring to the command string
 * or creates a new substring based on the parameters provided.
 *
 * @param cmd Command string.
 * @param start Starting position.
 * @param len Length of the substring.
 * @param substring The substring to concatenate.
 * @return The concatenated or new substring.
 */
char	*sub_or_join(char *cmd, char *start, int len, char *substring)
{
	char	*tmp;

	if (cmd == NULL)
		return (ft_substr(start, 0, len));
	tmp = cmd;
	cmd = ft_strjoin(cmd, substring);
	free(tmp);
	return (cmd);
}

/**
 * @brief Extracts a key from the input string.
 *
 * This function extracts a key that consists of alphanumeric characters
 * and underscores from the input string.
 *
 * @param input Input string.
 * @return The extracted key or NULL if not found.
 */
char	*key_check(char *input)
{
	int		i;
	char	*key;

	i = 0;
	while (ft_isalnum(input[i]) || input[i] == '_')
		i++;
	if (i == 0)
		key = NULL;
	else
		key = ft_substr(input, 0, i);
	return (key);
}
