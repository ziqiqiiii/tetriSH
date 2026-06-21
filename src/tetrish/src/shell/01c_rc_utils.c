#include "minishell.h"

/**
 * @brief Classify a single line from the shell rc file.
 *
 * Skips leading spaces/tabs, then categorises the line as empty/comment, a
 * "PATH=" assignment, or a command. The relevant value (the text after
 * "PATH=", or the trimmed command) is returned via value_out.
 *
 * @param line Raw rc line to classify.
 * @param value_out Out-param set to the line's value, or NULL for empty lines.
 * @return The line type: RC_LINE_EMPTY, RC_LINE_PATH or RC_LINE_COMMAND.
 */
t_rc_line_type	classify_rc_line(const char *line, const char **value_out)
{
	const char	*s;

	s = line;
	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '\0' || *s == '#')
	{
		*value_out = NULL;
		return (RC_LINE_EMPTY);
	}
	if (ft_strncmp(s, "PATH=", 5) == 0)
	{
		*value_out = s + 5;
		return (RC_LINE_PATH);
	}
	*value_out = s;
	return (RC_LINE_COMMAND);
}

/**
 * @brief Prepend a directory to PATH from an rc "PATH=" line.
 *
 * Builds "<value>:<existing PATH>" (or just <value> if PATH is unset),
 * updates the process environment via setenv, and mirrors the change into
 * the shell's env list by running the export builtin.
 *
 * @param sh Shell root state holding the env list.
 * @param value Directory string to prepend to PATH.
 */
void	set_path(t_root *sh, const char *value)
{
	char		new_path[PATH_MAX * 2];
	char		arg[PATH_MAX * 2 + 6];
	const char	*old;
	char		*cmd[3];

	old = getenv("PATH");
	if (old)
		snprintf(new_path, sizeof(new_path), "%s:%s", value, old);
	else
		snprintf(new_path, sizeof(new_path), "%s", value);
	setenv("PATH", new_path, 1);
	snprintf(arg, sizeof(arg), "PATH=%s", new_path);
	cmd[0] = "export";
	cmd[1] = arg;
	cmd[2] = NULL;
	export(cmd, &sh->env_list);
}

/**
 * @brief Run a single command line from the rc file.
 *
 * Expands the line, lexes and parses it into a syntax tree, restores the
 * terminal attributes, executes the tree, and frees the intermediate data.
 *
 * @param sh Shell root state.
 * @param envp Environment array passed through to execution.
 * @param line Raw command line from the rc file.
 */
void	exec_rc_cmd(t_root *sh, char **envp, char *line)
{
	char	*cmd;
	t_list	*cmd_lexer;
	t_tree	*head;

	cmd = expand(line, &sh->env_list);
	cmd_lexer = lexer(cmd);
	if (cmd_lexer == NULL)
	{
		free(cmd);
		return ;
	}
	head = parser(cmd_lexer, ft_lstsize(cmd_lexer), sh);
	ft_tcsetattr(STDIN_FILENO, TCSANOW, &sh->previous);
	recurse_bst(head, envp, sh);
	reset_data(sh, &cmd_lexer, &head);
	free(cmd);
}
