#include "minishell.h"

/**
 * @brief Removes one or more environment variables.
 *
 * Delegates to the unset built-in, skipping cmd[0] ("unsetenv") so that
 * only the variable names are passed. Each named variable is removed from
 * the shell's environment list if it exists; non-existent names are silently
 * ignored.
 *
 * @param cmd  Array of strings where cmd[0] is "unsetenv" and subsequent
 *             elements are the variable names to remove.
 * @param env_list Pointer to the linked list representing the environment.
 * @return 0 on success, 1 on error (e.g. invalid identifier).
 */
int	unset_env(char **cmd, t_list **env_list)
{
	return (unset(cmd + 1, env_list));
}
