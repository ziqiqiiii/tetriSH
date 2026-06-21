#include "minishell.h"

/**
 * @brief Locate and execute the shell start-up (rc) file.
 *
 * Tries the project-local rc file first, then the one in $HOME. If either
 * is found it is run line by line; if neither exists, an empty project rc
 * file is created as a starter.
 *
 * @param sh Shell root state.
 * @param envp Environment array passed through to rc command execution.
 */
void	source_rc(t_root *sh, char **envp)
{
	char	rc_path[PATH_MAX];
	char	home_path[PATH_MAX];
	int		fd;

	get_rc_paths(sh, rc_path, home_path);
	// printf("%s \n %s \n", rc_path, home_path);
	fd = -1;
	if (rc_path[0])
		fd = open(rc_path, O_RDONLY);
	if (fd == -1 && home_path[0])
		fd = open(home_path, O_RDONLY);
	if (fd != -1)
	{
		run_rc(fd, sh, envp);
		close(fd);
		return ;
	}
	if (rc_path[0])
		create_empty_rc(rc_path);
}

/**
 * @brief Compute the candidate rc file paths.
 *
 * Resolves the project root into sh->current_dir and builds the project rc
 * path "<root>/.macminishellrc"; if $HOME is set, also builds the home rc
 * path. Either buffer is left empty if its source is unavailable.
 *
 * @param sh Shell root state (its current_dir is set here).
 * @param rc_path Output buffer (PATH_MAX) for the project rc path.
 * @param home_path Output buffer (PATH_MAX) for the home rc path.
 */
void	get_rc_paths(t_root *sh, char *rc_path, char *home_path)
{
	const char	*home;

	rc_path[0] = '\0';
	home_path[0] = '\0';
	sh->current_dir = resolve_project_root();
	snprintf(rc_path, PATH_MAX, "%s/.macminishellrc", sh->current_dir);
	home = getenv("HOME");
	if (home)
		snprintf(home_path, PATH_MAX, "%s/.macminishellrc", home);
}

/**
 * @brief Read an rc file line by line and act on each line.
 *
 * Strips trailing CR/LF, classifies each line, and either updates PATH or
 * executes the command accordingly. Empty and comment lines are ignored.
 *
 * @param fd Open file descriptor for the rc file.
 * @param sh Shell root state.
 * @param envp Environment array passed through to command execution.
 */
void	run_rc(int fd, t_root *sh, char **envp)
{
	char			*line;
	const char		*value;
	size_t			len;
	t_rc_line_type	type;

	line = get_next_line(fd);
	while (line)
	{
		len = ft_strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';
		if (len > 1 && line[len - 2] == '\r')
			line[len - 2] = '\0';
		type = classify_rc_line(line, &value);
		if (type == RC_LINE_PATH)
			set_path(sh, value);
		else if (type == RC_LINE_COMMAND)
			exec_rc_cmd(sh, envp, (char *)value);
		free(line);
		line = get_next_line(fd);
	}
}



/**
 * @brief Creates an empty .minishellrc file with a starter comment header.
 *
 * Uses O_EXCL so the file is only created if it does not already exist;
 * silently returns if the file is already present.
 *
 * @param path Absolute file path at which to create the rc file.
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	create_empty_rc(const char *path)
{
	int			fd;
	const char	*header;

	fd = ft_open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);
	header = "# MacMini Shell start-up file\n"
		"# Add one command per line; blank lines and # comments"
		" are ignored.\n"
		"\n# Manifesting Mac Mini\n";
	write(fd, header, ft_strlen(header));
	close(fd);
}
