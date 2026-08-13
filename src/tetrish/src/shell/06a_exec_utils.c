#include "minishell.h"

static int	join_first_cmd(char **new_res, char **res);
static char	**join_second_cmd(char **new_res, char **add_arg, int i);

/**
 * @brief Joins two command arrays.
 *
 * This function concatenates two arrays of commands, the original command
 * array and additional commands processed by the cmd_quote_handler.
 *
 * @param res Original command array.
 * @param sh Root structure containing tree arguments.
 * @return The new concatenated command array.
 */
char	**cmd_join(char **res, t_root *sh)
{
	int		i;
	int		j;
	char	**new_res;
	char	**add_arg;

	i = 0;
	j = 0;
	add_arg = cmd_quote_handler(sh->tree_arg_value, SPACE);
	while (res[i] != NULL)
		++i;
	while (add_arg[j] != NULL)
		++j;
	new_res = ft_calloc(i + j + 1, sizeof(char *));
	i = join_first_cmd(new_res, res);
	free_2d(res);
	if (i == -1)
		return (NULL);
	return (join_second_cmd(new_res, add_arg, i));
}

/**
 * @brief Joins the first part of the command array.
 *
 * This function copies the original command array into the new array and
 * returns the index of the last element.
 *
 * @param new_res New command array.
 * @param res Original command array.
 * @return The index of the last element, or -1 on error.
 */
static int	join_first_cmd(char **new_res, char **res)
{
	int	i;

	i = 0;
	i = 0;
	while (res[i] != NULL)
	{
		new_res[i] = ft_strdup(res[i]);
		if (!new_res[i])
		{
			free_2d(new_res);
			return (-1);
		}
		++i;
	}
	return (i);
}

/**
 * @brief Joins the second part of the command array.
 *
 * This function copies the additional commands into the new array starting
 * at the given index, and returns the new concatenated command array.
 *
 * @param new_res New command array.
 * @param add_arg Additional commands array.
 * @param i Index to start copying at.
 * @return The new concatenated command array.
 */
static char	**join_second_cmd(char **new_res, char **add_arg, int i)
{
	int		j;

	j = 0;
	while (add_arg[j] != NULL)
	{
		new_res[i] = ft_strdup(add_arg[j]);
		if (!new_res[i])
		{
			free_2d(new_res);
			return (NULL);
		}
		++i;
		++j;
	}
	free_2d(add_arg);
	return (new_res);
}
