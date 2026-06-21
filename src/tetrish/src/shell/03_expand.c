#include "minishell.h"

static t_expand_var	*init_data(char *cmd);
static char			*join_remaining(t_expand_var *data, char *cmd);

/**
 * @brief Expands the command string, replacing variables with their values.
 *
 * This function processes the command string to replace variable references
 * with their corresponding values. It considers both single and double quotes,
 * handling them appropriately.
 *
 * @param cmd Original command string.
 * @param env_list List of environment variables.
 * @return Expanded command string with variable values replaced.
 */
char	*expand(char *cmd, t_list **env_list)
{
	t_expand_var	*data;

	data = init_data(cmd);
	while (data->dollar_ptr)
	{
		data->single_quote_ptr = ft_strchr(data->start, SINGLE_QUOTE);
		data->double_quote_ptr = ft_strchr(data->start, DOUBLE_QUOTE);
		if (data->double_quote_ptr == NULL)
			data->double_quote_ptr = data->single_quote_ptr + 1;
		if ((data->single_quote_ptr != NULL) && \
			(data->single_quote_ptr < data->dollar_ptr) && \
			(data->single_quote_ptr < data->double_quote_ptr))
			single_quote(data);
		else
			join_dollar_ptr(data, env_list);
		data->start = data->dollar_ptr;
		data->dollar_ptr = ft_strchr(data->start, DOLLAR);
		if (data->dollar_ptr == NULL)
			break ;
		if (data->substring != NULL)
			free(data->substring);
		data->substring = ft_substr(data->start, 0, \
									data->dollar_ptr - data->start);
	}
	return (join_remaining(data, cmd));
}

/**
 * @brief Initializes the data structure for command expansion.
 *
 * This function initializes the expansion-related variables, setting the
 * start pointer to the command and the dollar pointer to the first dollar
 * character found.
 *
 * @param data Structure containing expansion-related variables.
 * @param cmd Command string to be expanded.
 */
static t_expand_var	*init_data(char *cmd)
{
	t_expand_var	*data;

	data = ft_calloc(1, sizeof(t_expand_var));
	data->n_cmd = NULL;
	data->substring = NULL;
	data->dollar_ptr = ft_strchr(cmd, DOLLAR);
	data->single_quote_ptr = NULL;
	data->double_quote_ptr = NULL;
	data->exit_status_str = NULL;
	data->start = cmd;
	data->key = NULL;
	data->value = NULL;
	data->count = 0;
	data->len = 0;
	return (data);
}

/**
 * @brief Joins the remaining part of the command after expansion.
 *
 * This function creates a substring from the remaining part of the
 * command and joins it to the newly created command string, finalizing
 * the expansion process.
 *
 * @param data Structure containing expansion-related variables.
 * @param cmd Command string to be expanded.
 * @return Final expanded command string.
 */
static char	*join_remaining(t_expand_var *data, char *cmd)
{
	if (data->substring != NULL)
		free(data->substring);
	data->substring = ft_substr(data->start, 0, ft_strlen(data->start));
	cmd = sub_or_join(data->n_cmd, data->start, \
						ft_strlen(data->start), data->substring);
	free(data->substring);
	free(data);
	return (cmd);
}
