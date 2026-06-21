#include "minishell.h"

static void	pipe_heredoc(t_tree *node, char **envp, t_root *sh);
static void	pipe_child_process(t_tree *node, char **envp, t_root *sh);

/**
 * @brief Handles the piping and heredoc execution within the shell.
 *
 * This function processes the pipe operator and calls appropriate functions
 * to handle child processes and heredoc input. It also handles errors
 * related to the pipe syntax.
 *
 * @param node Pointer to the syntax tree node containing the command.
 * @param envp Array of environment variable strings.
 * @param sh Pointer to the shell root structure.
 */
void	pipe_handler(t_tree *node, char **envp, t_root *sh)
{
	int	i;

	i = 0;
	while (node->value[i] == PIPE_OP[0])
		++i;
	if (i > 1)
	{
		ft_putstr_fd("minishell: syntax error near unexpected token `|'\n", 2);
		g_exit_status = 258;
		return ;
	}
	if (node->left == NULL)
	{
		printf("minishell: syntax error near unexpected token `|'\n");
		return ;
	}
	if (node->right == NULL)
	{
		printf("minishell: syntax error: unexpected end of file\n");
		return ;
	}
	ft_pipe(sh->pipe);
	pipe_heredoc(node, envp, sh);
	pipe_child_process(node, envp, sh);
}

/**
 * @brief Handles the heredoc input within the pipe.
 *
 * This function checks if the left or right child of the current node
 * is a heredoc token and performs the necessary operations to handle
 * the heredoc within a pipe.
 *
 * @param node Pointer to the syntax tree node containing the command.
 * @param envp Array of environment variable strings.
 * @param sh Pointer to the shell root structure.
 */
static void	pipe_heredoc(t_tree *node, char **envp, t_root *sh)
{
	if (node->left->token == HEREDOC)
	{
		ft_dup2(sh->pipe[1], STDOUT_FILENO);
		recurse_bst(node->left, envp, sh);
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
	}
	if (node->right->token == HEREDOC)
	{
		ft_dup2(sh->pipe[0], STDIN_FILENO);
		recurse_bst(node->right, envp, sh);
		ft_dup2(sh->stdin_tmp, STDIN_FILENO);
	}
}

static void	left_child(int	*pipe, t_tree *node, char **envp, t_root *sh);
static void	right_child(int *pipe, t_tree *node, char **envp, t_root *sh);

/**
 * @brief Manages the child processes during piping.
 *
 * This function creates child processes to handle the left and right
 * sides of the pipe, executing the corresponding functions and managing
 * the exit statuses.
 *
 * @param node Pointer to the syntax tree node containing the command.
 * @param envp Array of environment variable strings.
 * @param sh Pointer to the shell root structure.
 */
static void	pipe_child_process(t_tree *node, char **envp, t_root *sh)
{
	pid_t				children[2];
	struct sigaction	sa_old;

	children[0] = ft_fork();
	if (children[0] == 0)
	{
		child_restore_signals();
		if (node->left->token != HEREDOC)
			left_child(sh->pipe, node, envp, sh);
		_exit(g_exit_status);
	}
	
	children[1] = ft_fork();
	if (children[1] == 0)
	{
		child_restore_signals();
		if (node->right->token != HEREDOC)
			right_child(sh->pipe, node, envp, sh);
		_exit(g_exit_status);
	}

	ft_close(sh->pipe[1]);
	ft_close(sh->pipe[0]);
	sh->pipe[1] = 0;
	sh->pipe[0] = 0;

	/* ignore while waiting */
	sigint_ignore(&sa_old);
	waitpid(children[0], &g_exit_status, WUNTRACED);
	ft_kill(children[0]);
	waitpid(children[1], &g_exit_status, WUNTRACED);
	ft_kill(children[1]);
	sigint_restore(&sa_old);

	g_exit_status = exit_status(g_exit_status);
}

/**
 * @brief Executes the left child node of the pipe command.
 *
 * This function duplicates the STDOUT file descriptor to the pipe's write end
 * and then recursively processes the left child node of the current pipe
 * command.
 *
 * @param pipe Array of pipe file descriptors.
 * @param node Pointer to the syntax tree node containing the command.
 * @param envp Array of environment variable strings.
 * @param sh Pointer to the shell root structure.
 */
static void	left_child(int	*pipe, t_tree *node, char **envp, t_root *sh)
{
	ft_dup2(pipe[1], STDOUT_FILENO);
	ft_close(sh->pipe[0]);
	sh->pipe[0] = 0;
	recurse_bst(node->left, envp, sh);
	ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
}

/**
 * @brief Executes the right child node of the pipe command.
 *
 * This function duplicates the STDIN file descriptor to the pipe's read end
 * and then recursively processes the right child node of the current pipe
 * command.
 *
 * @param pipe Array of pipe file descriptors.
 * @param node Pointer to the syntax tree node containing the command.
 * @param envp Array of environment variable strings.
 * @param sh Pointer to the shell root structure.
 */
static void	right_child(int *pipe, t_tree *node, char **envp, t_root *sh)
{
	ft_dup2(pipe[0], STDIN_FILENO);
	ft_close(sh->pipe[1]);
	sh->pipe[1] = 0;
	recurse_bst(node->right, envp, sh);
	ft_dup2(sh->stdin_tmp, STDIN_FILENO);
}
