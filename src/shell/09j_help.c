#include "minishell.h"

static const t_help g_help[];

/**
 * @brief Lists all built-in commands with a one-line description each.
 *
 * Iterates the g_help table and prints each built-in name and its short
 * description, then reminds the user to run 'usage <builtin>' for details.
 *
 * @return EXIT_SUCCESS.
 */
int	help(void)
{
    int i;

    printf("\nBuilt-in commands:\n");
    i = 0;
    while (g_help[i].name)
    {
        printf("  %-10s  %s\n", g_help[i].name, g_help[i].desc);
        i++;
    }
    printf("\nRun 'usage <builtin>' for detailed help.\n\n");
    return (EXIT_SUCCESS);
}

static const t_help g_help[] = {
    {"echo",    "print args to stdout"},
    {"cd",      "change working directory"},
    {"pwd",     "print current working directory"},
    {"export",  "set environment variables"},
    {"unset",   "remove environment variables"},
    {"env",     "print all environment variables"},
    {"history", "print command history"},
    {"exit",    "exit the shell"},
    {"help",    "list all built-in commands"},
    {"usage",   "show detailed help for a built-in"},
    {"setenv",  "set environment variables (alias for export)"},
    {"unsetenv", "remove environment variables (alias for unset)"},
    {NULL, NULL}
};
