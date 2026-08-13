#include "minishell.h"

/**
 * @brief Checks if the redirect-in file exists and returns its descriptor.
 *
 * If the "infile" does not exist, it exits with an error message.
 * Otherwise, it returns the file descriptor of the file.
 *
 * @param node_value Path from node->value.
 * @param sh Shell state.
 * @return File descriptor of the infile or error code.
 */
int	rdin_fd(char *node_value, t_root *sh)
{
	char	*file;
	int		fd;

	file = find_file(node_value);
	if (file == NULL)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		printf("minishell: syntax error near unexpected token `newline'\n");
		return (-1);
	}
	if (access(file, F_OK & X_OK) != 0)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		printf("Error: %s: %s\n", strerror(errno), file);
		free(file);
		return (-2);
	}
	fd = ft_open(file, O_RDONLY, 0666);
	free(file);
	return (fd);
}

/**
 * @brief Checks or creates the redirect-out file and returns its descriptor.
 *
 * If the "outfile" does not exist, it creates the file.
 *
 * @param node_value Path from node->value.
 * @param sh Shell state.
 * @return File descriptor of the outfile.
 */
int	rdout_fd(char *node_value, t_root *sh)
{
	char	*file;
	int		fd;

	file = find_file(node_value);
	if (file == NULL)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		printf("minishell: syntax error near unexpected token `newline'\n");
		return (-1);
	}
	fd = ft_open(file, O_CREAT | O_RDWR | O_TRUNC, 0666);
	free(file);
	return (fd);
}

/**
 * @brief Checks or creates the redirect-append file and returns its descriptor.
 *
 * Similar to rdout_fd, but opens the file in append mode.
 *
 * @param node_value Append node value.
 * @param sh Shell state.
 * @return File descriptor of the appended file.
 */
int	rdapp_fd(char *node_value, t_root *sh)
{
	char	*file;
	int		fd;

	file = find_file(node_value);
	if (file == NULL)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		printf("minishell: syntax error near unexpected token `newline'\n");
		return (-1);
	}
	fd = ft_open(file, O_CREAT | O_RDWR | O_APPEND, 0666);
	free(file);
	return (fd);
}

/**
 * @brief Removes redirection and spaces from the path to find the file.
 *
 * It parses the value from a node, skipping redirection symbols
 * and spaces to find the file path.
 *
 * @param value The value that is stored in the node.
 * @return File path as a string.
 */
char	*find_file(char *value)
{
	char	**result;
	char	*file;

	if ((*value != '<' && *value != '>') && *value != '\0')
		return (NULL);
	while ((*value == '<' || *value == '>' || *value == SPACE) \
			&& *value != '\0')
		++value;
	if (*value == '\0')
		return (NULL);
	result = cmd_quote_handler(value, SPACE);
	file = ft_strdup(result[0]);
	free_2d(result);
	return (file);
}
