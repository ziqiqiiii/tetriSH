#include "minishell.h"

static int	tilda_dash_helper(char **dir, t_list **env_list);
static void	set_old_pwd(char cwd[256], t_list **env_list);
static void	expand_tilda_only(char **dir, t_list **env_list);
static int	expand_dash(char **dir, t_list **env_list);
static void	expand_tilda_slash(char **dir, char *slash_ptr, t_list **env_list);

/**
 * @brief Changes the current working directory.
 *
 * Changes the working directory to the value specified. If no value is
 * provided, changes to the directory specified by the "HOME" environment
 * variable.
 *
 * @param value An array containing the command and its arguments.
 * @param env_list Pointer to the environment list.
 */
int	cd(char **value, t_list **env_list)
{
	char	cwd[256];
	char	**split;
	int		i;

	split = NULL;
	if (get_pwd(cwd))
		return (EXIT_FAILURE);
	if (value[1] == NULL)
	{
		if (existed_env("HOME", env_list) == NULL)
		{
			ft_putstr_fd("cd: HOME not set\n", 2);
			return (EXIT_FAILURE);
		}
		i = chdir(existed_env("HOME", env_list));
	}
	else
	{
		split = value;
		if (tilda_dash_helper(&split[1], env_list))
			return (EXIT_FAILURE);
		i = chdir(split[1]);
		if (i != 0)
		{
			perror("cd: ");
			return (EXIT_FAILURE);
		}
	}
	set_old_pwd(cwd, env_list);
	return (EXIT_SUCCESS);
}

/**
 * @brief Replaces a tilde (~) in the directory string with the value of HOME.
 *
 * The tilde is only replaced if it appears at the start of the string,
 * before any slash (/). If a tilde appears after a slash, it is not replaced.
 *
 * @param dir Pointer to the directory string.
 * @param env_list Pointer to the environment list.
 */
static int	tilda_dash_helper(char **dir, t_list **env_list)
{
	char	*tilda_ptr;
	char	*slash_ptr;
	char	*dash_ptr;

	slash_ptr = ft_strchr(*dir, '/');
	tilda_ptr = ft_strchr(*dir, '~');
	dash_ptr = ft_strchr(*dir, '-');
	if (tilda_ptr != NULL && slash_ptr == NULL)
		expand_tilda_only(dir, env_list);
	else if (dash_ptr != NULL && tilda_ptr == NULL && slash_ptr == NULL)
		return (expand_dash(dir, env_list));
	else if (tilda_ptr != NULL && tilda_ptr < slash_ptr)
		expand_tilda_slash(dir, slash_ptr, env_list);
	return (EXIT_SUCCESS);
}

/**
 * @brief Record the previous directory into OLDPWD.
 *
 * Builds "OLDPWD=<cwd>" and stores it in the environment via the export
 * builtin (using the setenv command form).
 *
 * @param cwd The working directory to remember as OLDPWD.
 * @param env_list Pointer to the environment list.
 */
static void	set_old_pwd(char cwd[256], t_list **env_list)
{
	char	*pair;
	char	*cmd[3];

	pair = ft_strjoin("OLDPWD=", cwd);
	cmd[0] = "setenv";
	cmd[1] = pair;
	cmd[2] = NULL;
	export(cmd, env_list);

	free(pair);
}

/**
 * @brief Replace a lone "~" argument with the HOME directory.
 *
 * @param dir Pointer to the directory string; freed and replaced with HOME.
 * @param env_list Pointer to the environment list.
 */
static void	expand_tilda_only(char **dir, t_list **env_list)
{
	free(*dir);
	*dir = ft_strdup(existed_env("HOME", env_list));
}

/**
 * @brief Replace a "-" argument with the OLDPWD directory.
 *
 * @param dir Pointer to the directory string; freed and replaced with OLDPWD.
 * @param env_list Pointer to the environment list.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE if OLDPWD is not set.
 */
static int	expand_dash(char **dir, t_list **env_list)
{
	char	*tmp;

	if (existed_env("OLDPWD", env_list) == NULL)
	{
		ft_putstr_fd("cd: OLDPWD not set\n", 2);
		return (EXIT_FAILURE);
	}
	tmp = *dir;
	*dir = ft_strdup(existed_env("OLDPWD", env_list));
	free(tmp);
	return (EXIT_SUCCESS);
}

/**
 * @brief Expand a leading "~/" path into "<HOME>/...".
 *
 * Joins HOME with the path starting at the first slash, replacing the
 * original string.
 *
 * @param dir Pointer to the directory string; freed and replaced.
 * @param slash_ptr Pointer to the first '/' within the original string.
 * @param env_list Pointer to the environment list.
 */
static void	expand_tilda_slash(char **dir, char *slash_ptr, t_list **env_list)
{
	char	*tmp;

	tmp = *dir;
	*dir = ft_strjoin(existed_env("HOME", env_list), slash_ptr);
	free(tmp);
}
