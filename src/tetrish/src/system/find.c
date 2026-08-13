#include "system_program.h"


static int  execute(char **args);

/**
 * @brief Entry point for the find utility.
 *
 * Passes the argument vector directly to execute(), which treats args[0] as
 * the command name / starting directory and args[1] as the search keyword.
 *
 * @param argc Number of command-line arguments (unused).
 * @param args Array of command-line arguments forwarded to execute().
 * @return The return value of execute().
 */
int main(int argc, char **args) {
        (void)  argc;
        
        return execute(args);
}

/**
 * @brief Recursively search a directory tree for matching filenames.
 *
 * args[0] is the current directory ("find" maps to the current directory),
 * and args[1] is the keyword. Prints every entry whose name contains the
 * keyword, then recurses into each subdirectory (skipping "." and "..").
 *
 * @param args Argument vector: args[0] directory, args[1] keyword.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on usage error or if the
 *         directory cannot be opened.
 */
static int execute(char **args) {

        if (args[1] == NULL) {
            printf("Usage: find [keyword], to find any matching filename "
                    "in this directory or its children\n");
            return (EXIT_FAILURE);
        }

        char	*dir_name = args[0];
        char	*to_match = args[1];
        char	local_dir_name[SHELL_BUFFERSIZE];
        DIR		*d;

        /* Open the directory specified by "dir_name". */
        if (strcmp(dir_name, "find") == 0)
		{
			d = opendir(".");
			// getcwd(local_dir_name, SHELL_BUFFERSIZE);
			sprintf(local_dir_name, ".");
			dir_name = local_dir_name;
        }
		else
			d = opendir(dir_name);

        /* Check it was opened. */
        if (!d)
		{
			fprintf(stderr, "Cannot open directory '%s': %s\n", dir_name,
					strerror(errno));
			return (EXIT_FAILURE);
        }

        while (1)
		{
			struct dirent *entry;
			const char *d_name;

			/* "Readdir" gets subsequent entries from "d". */
			entry = readdir(d);
			if (!entry)
			{
				/* There are no more entries in this directory, so break
					out of the while loop. */
				break;
			}
			d_name = entry->d_name;

			/* Print the name of the file and directory if it matches the
				* keyword */
			if (strstr(d_name, to_match) != NULL) {
				printf("%s/%s\n", dir_name, d_name);
			}

			if (entry->d_type & DT_DIR)
			{
				/* Check that the directory is not "d" or d's parent. */
				if (strcmp(d_name, "..") != 0 && strcmp(d_name, ".") != 0)
				{
					int		path_length;
					char	path[PATH_MAX];

					path_length = snprintf(path, 1024, "%s/%s", dir_name, d_name);

					if (path_length >= PATH_MAX)
					{
						fprintf(stderr, "Path length has got too long.\n");
						exit(EXIT_FAILURE);
					}
					/* Recursively call "list_dir" with the new
						* path. */
					args[0] = path;
					execute(args);
				}
			}
        }

        /* After going through all the entries, close the directory. */
        if (closedir(d))
            fprintf(stderr, "Could not close '%s': %s\n", dir_name, strerror(errno));

        return (EXIT_SUCCESS);
}
