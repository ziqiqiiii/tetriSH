#include "minishell.h"

static const t_usage g_usage[];
static void print_general_usage(void);

/**
 * @brief Displays usage information for built-in commands.
 *
 * With no argument, prints a general usage summary. With an argument,
 * looks up the named built-in in the g_usage table and prints its
 * detailed help string.
 *
 * @param cmd Array of command strings; cmd[1] is the optional built-in name.
 * @return EXIT_SUCCESS on success, or EXIT_FAILURE if the named built-in
 *         is not found.
 */
int usage(char **cmd)
{
    int i;

    if (!cmd[1])
    {
        print_general_usage();
        return (EXIT_SUCCESS);
    }
    i = 0;
    while (g_usage[i].name)
    {
        if (ft_strncmp(cmd[1], g_usage[i].name, ft_strlen(g_usage[i].name) + 1) == 0)
        {
            puts(g_usage[i].detail);
            return (EXIT_SUCCESS);
        }
        i++;
    }
    ft_putstr_fd("usage: no such built-in: ", 2);
    ft_putstr_fd(cmd[1], 2);
    ft_putstr_fd("\n", 2);
    return (EXIT_FAILURE);
}

static const t_usage g_usage[] = {
    {"echo",    "\n\techo [-n] [arg...]\n"
                "\t\tPrint args to stdout. -n suppresses trailing newline.\n"},
    {"cd",      "\n\tcd [path]\n\tcd -\n"
                "\t\tChange working directory. Defaults to $HOME.\n"
                "\t\tcd - switches to $OLDPWD.\n"},
    {"pwd",     "\n\tpwd\n"
                "\t\tPrint current working directory.\n"},
    {"export",  "\n\texport [name=value]...\n"
                "\t\tSet environment variables.\n"},
    {"unset",   "\n\tunset [name]...\n"
                "\t\tRemove environment variables. Does nothing if not set.\n"},
    {"env",     "\n\tenv\n"
                "\t\tPrint all environment variables.\n"},
    {"history", "\n\thistory\n"
                "\t\tPrint command history.\n"},
    {"exit",    "\n\texit [n]\n"
                "\t\tExit the shell with status n.\n"},
    {"help",    "\n\thelp\n"
                "\t\tList all built-in commands.\n"},
    {"usage",   "\n\tusage [builtin]\n"
                "\t\tShow detailed help for a built-in command.\n"},
    {"setenv",  "\n\tsetenv [name=value]...\n"
                "\t\tSet environment variables. Alias for export.\n"},
    {"unsetenv", "\n\tunsetenv [name]...\n"
                "\t\tRemove environment variables. Alias for unset.\n"},
    {NULL, NULL}
};

/**
 * @brief Prints a summary of how to invoke minishell built-ins and programs.
 */
static void print_general_usage(void)
{
    printf("\nminishell usage:\n");
    printf("\t  help                 list built-in commands\n");
    printf("\t  usage <builtin>      detailed help for a command\n");
    printf("\t  <builtin> [args...]  run a built-in command\n");
    printf("\t  <cmd>     [args...]  run a program from $PATH\n\n");
}
