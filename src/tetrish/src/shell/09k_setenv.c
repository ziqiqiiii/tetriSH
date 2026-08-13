#include "minishell.h"

/**
 * @brief Sets or updates an environment variable.
 *
 * Delegates to the export built-in to add a new variable or update an
 * existing one in the shell's environment list. Accepts the full command
 * array so that export can process one or more NAME=VALUE arguments.
 *
 * @param cmd  Array of strings where cmd[0] is "setenv" and subsequent
 *             elements are NAME=VALUE pairs to set.
 * @param env_list Pointer to the linked list representing the environment.
 * @return 0 on success, 1 on error (e.g. invalid identifier).
 */
int	set_env(char **cmd, t_list **env_list)
{
	return export(cmd, env_list);
}
