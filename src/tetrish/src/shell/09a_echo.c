#include "minishell.h"

static int	loop_n(char *str);

/**
 * @brief Implements the echo command in the shell.
 *
 * This function handles the echo command, printing the provided
 * arguments to the standard output. The "-n" option is also handled,
 * which suppresses the trailing newline if specified.
 *
 * @param cmd Array of strings representing the command and its arguments.
 * @return EXIT_SUCCESS upon successful completion.
 */
int	echo_command(char **cmd)
{
	t_echo_var	echo;

	echo.flag = 0;
	echo.flag2 = 0;
	echo.i = 0;
	while (cmd[++echo.i])
	{
		if ((ft_strncmp(cmd[echo.i], "-n", ft_strlen("-n")) == 0) \
			&& (loop_n((cmd[echo.i] + 2)) == 1) \
			&& (echo.flag2 == 0))
		{
			echo.flag = 1;
			continue ;
		}
		else
		{
			write(1, cmd[echo.i], ft_strlen(cmd[echo.i]));
			if (cmd[echo.i + 1] != NULL)
				write(1, " ", 1);
			echo.flag2 = 1;
		}
	}
	if (echo.flag != 1)
		write(1, "\n", 1);
	return (EXIT_SUCCESS);
}

/**
 * @brief Checks if the given string consists only of the character 'n'.
 *
 * This function iterates through the provided string and checks if
 * all the characters in the string are 'n'. It's used to handle
 * the "-n" option in the echo command.
 *
 * @param str Input string to be checked.
 * @return 1 if all characters are 'n', 0 otherwise.
 */
static int	loop_n(char *str)
{
	int	i;

	i = 0;
	while (str[i])
	{
		if (str[i] != 'n')
			return (0);
		i++;
	}
	return (1);
}
