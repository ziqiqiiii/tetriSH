#include "minishell.h"

int	g_exit_status = EXIT_SUCCESS;

/**
 * @brief Main entry point of the shell.
 *
 * This function initializes the shell root structure, prints the banner,
 * and invokes the prompt loop.
 *
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @param envp Environment variables.
 * @return EXIT_SUCCESS if successful, or EXIT_FAILURE if an error occurs.
 */
int	main(int argc, char **argv, char **envp)
{
	t_root	sh;

	(void) argv;
	(void) argc;
	if (init_root(&sh, envp) == EXIT_FAILURE)
		return (EXIT_FAILURE);
	source_rc(&sh, envp);
	print_banner(&sh);
	prompt(&sh, envp);
	exit_prompt(&sh);
	return (g_exit_status);
}
