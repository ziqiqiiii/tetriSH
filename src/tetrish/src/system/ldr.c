#include "system_program.h"

/**
 * @brief Prints a file path with directory separators highlighted in yellow.
 *
 * Iterates over each character; '/' is printed in yellow and the following
 * segment is printed in green. All other characters are printed as-is.
 *
 * @param path Null-terminated path string to print.
 */
void print_path_with_colored_slash(const char *path) {
        const char *p = path;
        while (*p) {
                if (*p == '/') {
                        printf(CL_YELLOW "/%s" CL_RESET, CL_GREEN);
                } else {
                        putchar(*p);
                }
                p++;
        }
}

/**
 * @brief Recursively lists all non-hidden files under basePath with permissions.
 *
 * For each non-dotfile entry, prints the permission string (in red) followed
 * by the full path (with colored slashes). Recurses into subdirectories.
 *
 * @param basePath Root directory to start listing from.
 */
void list_directory(const char *basePath) {
        char path[1000];
        struct dirent *dp;
        DIR *dir = opendir(basePath);
        if (!dir)
                return; // Unable to open directory

        while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, ".") != 0 &&
                    strcmp(dp->d_name, "..") != 0) {
                        // Construct new path
                        snprintf(path, sizeof(path), "%s/%s", basePath,
                                 dp->d_name);

                        // Skip dotfiles
                        if (dp->d_name[0] != '.') {
                                struct stat st;
                                char permissions[11] = {0};
                                if (stat(path, &st) == 0) {
                                        perms_to_string(st.st_mode,
                                                        permissions);
                                        printf(CL_RED "%s " CL_RESET,
                                               permissions);
                                        print_path_with_colored_slash(path);
                                        printf("\n");
                                }

                                // Recursively list directories
                                if (S_ISDIR(st.st_mode)) {
                                        list_directory(path);
                                }
                        }
                }
        }
        closedir(dir);
}

/**
 * @brief Entry point for the ldr utility.
 *
 * Recursively lists all non-hidden files in the current directory (".") with
 * their permission strings and color-coded paths.
 *
 * @return EXIT_SUCCESS.
 */
int main(void) {
        // printf("Recursively listing all visible files under the current
        // directory with permissions:\n");
        list_directory(".");

        return EXIT_SUCCESS;
}
