#ifndef COMMON_H
# define COMMON_H

# include <sys/stat.h>		/* mode_t, S_ISDIR, S_IRUSR, ... */
# include <limits.h>		/* PATH_MAX */
# include <string.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <libgen.h>
# include <signal.h>
# include <fcntl.h>
# include <time.h>
# include <sys/file.h>
# include <sys/stat.h>

# include "libft.h"

/*
** Helpers shared across the standalone system programs (bin/).
** Linked into each program through libcommon.a.
*/

/* ANSI colour escapes (CL_ prefix avoids clashing with curses' COLOR_*) */
# define CL_RED "\x1b[31m"
# define CL_GREEN "\x1b[32m"
# define CL_YELLOW "\x1b[33m"
# define CL_BLUE "\x1b[34m"
# define CL_MAGENTA "\x1b[35m"
# define CL_CYAN "\x1b[36m"
# define CL_RESET "\x1b[0m"

/* Renders a stat(2) mode into an "ls -l" style permission string. */
void	perms_to_string(mode_t mode, char str[11]);

/* Resolves the project root from /proc/self/exe; returns a malloc'd string. */
char	*resolve_project_root(void);

/* Daemonization + logging for the long-running system programs. */
void	daemon_spawn(void);
void	daemon_log(const char *project_root, const char *msg);

/* Filesystem helpers: create a dir/file only if it is missing. */
int		create_dir_if_missing(const char *path, mode_t mode);
int		create_file_if_missing(const char *path, mode_t mode);

/* Ensures <project_root>/tmp and the daemon registry/log files exist. */
void	ensure_daemon_files(const char *project_root);

/* Ensures <project_root>/archive exists (for backup tarballs). */
int		ensure_archive_dir(const char *project_root);

/* Checks a path: 0 = dir exists, -1 = exists but not a dir, 1 = missing. */
int		ft_stat(const char *path);

/* Wraps mkdir(2): 0 on success, -1 on failure (reported via perror). */
int		ft_mkdir(const char *path, mode_t mode);

/* Syscall wrappers that exit on failure (shared by shell + system programs). */
int		ft_pipe(int p[2]);
int		ft_dup2(int new_fd, int old_fd);
int		ft_open(const char *file, int flags, int permission);
FILE	*ft_fopen(const char *file, const char *mode);
int		ft_close(int fd);
int		ft_fork(void);
void	ft_kill(int pid);

#endif
