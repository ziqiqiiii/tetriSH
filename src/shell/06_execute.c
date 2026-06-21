#include "minishell.h"

static void	exec_cmd(char *argv, char **envp, t_root *sh);
static char	**env_list_to_arr(t_list *env_list);

/**
 * @brief Recursively traverses the binary syntax tree.
 *
 * This function traverses the binary syntax tree and calls appropriate
 * handlers based on the token type of each node, including pipe handling,
 * input/output redirection, and command execution.
 *
 * @param node Current node in the binary syntax tree.
 * @param envp Environment variables array.
 * @param sh Root structure containing shell information.
 */
void	recurse_bst(t_tree *node, char **envp, t_root *sh)
{
	if (node == NULL)
		return ;
	if (node->token == PIPE)
		pipe_handler(node, envp, sh);
	else if (node->token == RDIN)
		rdin_handler(node, envp, sh);
	else if (node->token == RDOUT)
		rdout_handler(node, envp, sh);
	else if (node->token == RDAPP)
		rdapp_handler(node, envp, sh);
	else if (node->token == HEREDOC)
		heredoc_handler(node, envp, sh);
	else if (node->token == COMMAND)
		exec_cmd(node->value, envp, sh);
}

/**
 * @brief Executes a command.
 *
 * This function takes a command string, processes it into a command array,
 * and executes it. If the command is a built-in, it handles it accordingly.
 * It also sets the global exit status after execution.
 *
 * @param argv Command string.
 * @param envp Environment variables array.
 * @param sh Root structure containing shell information.
 */
static void	exec_cmd(char *argv, char **envp, t_root *sh)
{
	char				*path;
	char				**cmd;
	pid_t				child;
	struct 	sigaction	sa_old;

	(void)envp;
	cmd = cmd_quote_handler(argv, SPACE);
	if (sh->tree_arg_value != NULL)
		cmd = cmd_join(cmd, sh);
	if (builtin(cmd, sh) == 1)
		return ;
	path = get_exe_path(cmd[0], &sh->env_list);

	child = ft_fork();
	if (child == 0)
	{
		child_restore_signals();
		if (execve(path, cmd, env_list_to_arr(sh->env_list)) == -1)
		{
			ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
			ft_putstr_fd("Error: Command not found: ", STDOUT_FILENO);
			ft_putstr_fd(*cmd, STDOUT_FILENO);
			ft_putstr_fd(".\n", STDOUT_FILENO);
			_exit(EXIT_NO_CMD);
		}
	}

	/* ignore while waiting */
	sigint_ignore(&sa_old);
	waitpid(child, &g_exit_status, WUNTRACED);
	ft_kill(child);
	sigint_restore(&sa_old);

	g_exit_status = exit_status(g_exit_status);

	free(path);
	free_2d(cmd);
}

/**
 * @brief Builds an envp-style array from the environment linked list.
 *
 * Allocates a NULL-terminated array of "key=value" strings (or just "key"
 * for variables with no value) from the shell's environment list, so that
 * child processes receive variables set at runtime via export/setenv.
 *
 * @param env_list The linked list of environment variables.
 * @return A newly allocated NULL-terminated array, or NULL on allocation
 *         failure.
 */
static char	**env_list_to_arr(t_list *env_list)
{
	char	**arr;
	t_env	*data;
	char	*tmp;
	int		i;

	arr = ft_calloc(ft_lstsize(env_list) + 1, sizeof(char *));
	if (arr == NULL)
		return (NULL);
	i = 0;
	while (env_list)
	{
		data = (t_env *)env_list->content;
		if (data->value == NULL)
			arr[i] = ft_strdup(data->key);
		else
		{
			tmp = ft_strjoin(data->key, "=");
			arr[i] = ft_strjoin(tmp, data->value);
			free(tmp);
		}
		i++;
		env_list = env_list->next;
	}
	return (arr);
}
