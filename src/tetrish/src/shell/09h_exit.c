#include "minishell.h"

/**
 * @brief Gets the exit status of a terminated child process.
 *
 * This function examines the status returned by wait() to determine
 * how the child process terminated.
 *
 * @param status The status code returned by wait().
 * @return The exit status if exited normally, or termination signal + 128
 * if terminated by a signal.
 */
int	exit_status(int status)
{
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		return (WTERMSIG(status) + 128);
	return (0);
}

/**
 * @brief Implements the exit command of the shell.
 *
 * This function handles the exit command, determining the appropriate
 * exit status, and handling error cases like non-numeric arguments
 * or too many arguments.
 *
 * @param cmd The command line arguments.
 * @param sh Pointer to the shell's root structure.
 * @return The exit status code.
 */
int	exit_command(char **cmd, t_root *sh)
{
	int	i;

	i = 0;
	sh->exit_cmd_flag = 1;
	if (ft_array2d_len(cmd) == 1)
		return (g_exit_status);
	while (ft_isdigit(cmd[1][i]) || cmd[1][i] == '-' || cmd[1][i] == '+')
		++i;
	if (ft_isdigit(cmd[1][i]) == 0 && cmd[1][i] != '\0')
	{
		ft_putstr_fd("exit\nminishell: exit: ", 2);
		ft_putstr_fd(cmd[1], 2);
		ft_putstr_fd(": numeric argument required\n", 2);
		return (EXIT_RANGE);
	}
	else
	{
		if (ft_array2d_len(cmd) > 2)
		{
			ft_putstr_fd("exit\nminishell: exit: too many arguments\n", 2);
			sh->exit_cmd_flag = 0;
			return (EXIT_FAILURE);
		}
	}
	return (ft_atoi(cmd[1]));
}

/**
 * @brief Finalizes the exit process for the shell.
 *
 * If the exit flag is set, this function performs necessary cleanup
 * before terminating the shell, such as closing file descriptors,
 * clearing history, and freeing memory.
 *
 * @param sh Pointer to the shell's root structure.
 */
void	exit_prompt(t_root *sh)
{
	ft_close(sh->stdin_tmp);
	ft_close(sh->stdout_tmp);
	history_clear(&sh->history);
	ft_lstclear(&sh->env_list, del_data);
	free(sh->pipe);
	free(sh->current_dir);
}
