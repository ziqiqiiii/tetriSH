#include "system_program.h"

/**
 * @brief Lists the contents of the current directory with permissions.
 *
 * With no options, prints each non-hidden entry with its permission string
 * color-coded in red (permissions) and green (name). If args[1] is "-r",
 * delegates to the ldr binary for a recursive listing. Any other option
 * prints an error message.
 *
 * @param args Array where args[0] is the command name and args[1] is an
 *             optional flag (e.g., "-r").
 * @return EXIT_SUCCESS on success, 1 if execvp fails or the flag is invalid.
 */
int execute(char **args) {
        if (args[1] != NULL) {
                char *token = strtok(args[1], SHELL_OPT_DELIM);
                // printf("Token is %s\n", token);

                if (token != NULL) {
                        if (strcmp(token, "r") == 0) {
                                // call listdirall,
                                // execvp still need the ./bin because this was
                                // called by a process that was at the ..
                                // directory
                                if (execvp("./bin/ldr", args) == -1) {
                                        perror("Failed to execute, command is "
                                               "invalid.");
                                }
                                return 1;
                        } else {
                                printf("Invalid option. Use -r to display all "
                                       "files within the current directory and "
                                       "its subdirectories.\n");
                                return EXIT_SUCCESS;
                        }
                }
        }

        // print out all the contents of the directory using opendir() function
        DIR *d;
        struct dirent *dir;
        struct stat st;
        char permissions[11];
        d = opendir(".");
        if (d) {
                while ((dir = readdir(d)) != NULL) {
                        // Skip dotfiles
                        if (dir->d_name[0] != '.') {
                                if (stat(dir->d_name, &st) == 0) {
                                        perms_to_string(st.st_mode,
                                                        permissions);
                                        printf(CL_RED "%s " CL_GREEN
                                                         "%s\n" CL_RESET,
                                               permissions, dir->d_name);
                                } else {
                                        perror("stat failed");
                                }
                        }
                }
                closedir(d);
        } else {
                printf("Directory doesn't exist. \n");
        }

        return EXIT_SUCCESS;
}

/**
 * @brief Entry point for the ld utility.
 *
 * Passes the argument vector directly to execute() for directory listing.
 *
 * @param argc Number of command-line arguments (unused).
 * @param args Array of command-line arguments forwarded to execute().
 * @return The return value of execute().
 */
int main(int argc, char **args) {
        (void)argc;
        return execute(args);
}
