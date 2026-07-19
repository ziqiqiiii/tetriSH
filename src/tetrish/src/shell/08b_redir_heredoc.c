#include "minishell.h"

static int	heredoc_parent(pid_t child_pid, char *delim);
static void	heredoc_child(t_root *sh, char *delim, int heredoc_fd);

/**
 * @brief Handles the heredoc redirection by checking if the "infile" exists,
 *        then returns the file descriptor or exits with an error message.
 *
 * @param node_value Path from node->value.
 * @param sh Shell state.
 * @return File descriptor of the infile or error code.
 */
int	heredoc_fd(char *node_value, t_root *sh)
{
	char	*delim;
	int		fd;
	pid_t	child;

	delim = find_file(node_value);
	if (delim == NULL)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		printf("minishell: syntax error near unexpected token `newline'\n");
		return (-1);
	}
	if (access(".here_doc_tmp", F_OK & X_OK) == 0)
		unlink(".here_doc_tmp");
	fd = ft_open(".here_doc_tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	child = ft_fork();
	if (child == 0)
		heredoc_child(sh, delim, fd);
	else
		fd = heredoc_parent(child, delim);
	return (fd);
}

/**
 * @brief Waits for the child process to handle input and create the
 *        temporary file, then opens the file and returns its file descriptor.
 *
 * @param child_pid Process ID of the child handling the input.
 * @param delim Delimiter string used for the heredoc.
 * @return File descriptor of the temporary heredoc file or -1 if an error
 *         occurred.
 */
static int	heredoc_parent(pid_t child_pid, char *delim)
{
	int					fd;
	struct 	sigaction	sa_old;

	/* ignore while waiting */
	sigint_ignore(&sa_old);	
	waitpid(child_pid, &g_exit_status, WUNTRACED);
	ft_kill(child_pid);
	sigint_restore(&sa_old);

	g_exit_status = exit_status(g_exit_status);

	free(delim);

	if (g_exit_status != 0)
		return (-1);
	if (access(".here_doc_tmp", F_OK & X_OK) != 0)
	{
		printf("Error: %s: %s\n", strerror(errno), ".here_doc_tmp");
		return (-1);
	}

	fd = ft_open(".here_doc_tmp", O_RDONLY, 0666);
	return (fd);
}

static char	*heredoc_input(t_root *sh, char *delim);

/**
 * @brief Handles reading input lines for the heredoc, expanding variables,
 *        and writing to the temporary heredoc file. Exits when the delimiter
 *        is matched.
 *
 * @param sh Pointer to the shell root structure.
 * @param delim Delimiter string used for the heredoc.
 * @param heredoc_fd File descriptor of the temporary heredoc file.
 */
static void	heredoc_child(t_root *sh, char *delim, int heredoc_fd)
{
	char	*line;
	char	*tmp;

	heredoc_restore_signals();

	while (TRUE)
	{
		line = heredoc_input(sh, delim);
		if (ft_strncmp(line, delim, ft_strlen(delim) + 1) == 0)
		{
			free(line);
			ft_close(heredoc_fd);
			break ;
		}
		tmp = line;
		line = expand(line, &sh->env_list);
		free(tmp);
		ft_putstr_fd(line, heredoc_fd);
		ft_putstr_fd("\n", heredoc_fd);
		free(line);
	}
	free(delim);
	exit(EXIT_SUCCESS);
}

/**
 * @brief Reads a line of input for the heredoc, handling appropriate
 *        redirections and shell variables.
 *
 * @param sh Pointer to the shell root structure.
 * @param delim Delimiter string used for the heredoc.
 * @return Read line of input, or NULL if the end of file is reached.
 */
static char	*heredoc_input(t_root *sh, char *delim)
{
	char	*line;

	if (sh->pipe[1] != 0)
	{
		ft_dup2(sh->stdout_tmp, STDOUT_FILENO);
		line = readline("> ");
		ft_dup2(sh->pipe[1], STDOUT_FILENO);
	}
	else
		line = readline("> ");
	if (line == NULL)
	{
		free(delim);
		free(line);
		exit(EXIT_SUCCESS);
	}
	return (line);
}
