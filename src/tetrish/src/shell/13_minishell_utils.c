#include "minishell.h"

/**
 * @brief Wraps the tcgetattr function, providing error handling.
 *
 * This function retrieves the parameters associated with the 
 * terminal referred to by the given file descriptor and stores
 * them in the termios structure.
 *
 * @param fd The file descriptor of the terminal.
 * @param termios_p Pointer to the termios structure.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int	ft_tcgetattr(int fd, struct termios *termios_p)
{
	if (!isatty(fd))
		return (EXIT_SUCCESS);
	if (tcgetattr(fd, termios_p) == -1)
	{
		perror("tcgetattr failed");
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Wraps the tcsetattr function, providing error handling.
 *
 * This function sets the terminal parameters associated with 
 * the terminal referred to by the given file descriptor.
 *
 * @param fd The file descriptor of the terminal.
 * @param optional_actions Specifies when the changes take effect.
 * @param termios_p Pointer to the termios structure.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int	ft_tcsetattr(int fd, int optional_actions, struct termios *termios_p)
{
	if (!isatty(fd))
		return (EXIT_SUCCESS);
	if (tcsetattr(fd, optional_actions, termios_p) == -1)
	{
		perror("tcsetattr failed");
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

/**
 * @brief Prints the elements of a command array.
 *
 * This function prints the elements of a null-terminated array of strings, 
 * representing command arguments.
 *
 * @param cmd Pointer to the array of command arguments.
 */
void	print_exec_cmd(char **cmd)
{
	int	i;

	i = 0;
	while (cmd[i] != NULL)
	{
		printf("argv[%d]: |%s|\n", i, cmd[i]);
		i++;
	}
}
