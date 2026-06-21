# ifndef SYSTEM_PROGRAM_H
# define SYSTEM_PROGRAM_H

# include <dirent.h>
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
# include <dirent.h>        /* "readdir" etc. are defined here. */
# include <ctype.h>
# include <fcntl.h>
# include <limits.h>
# include <pwd.h>
# include <signal.h>
# include <sys/resource.h>
# include <sys/stat.h>
# include <sys/utsname.h>
# include <syslog.h>
# include <time.h>
# include <libgen.h>
# include <pthread.h>
# include <sys/file.h>
# include <stdarg.h>
# include <sys/sysinfo.h>

# include "libft.h"
# include "get_next_line.h"
# include "common.h"

/* PATH_MAX comes from <limits.h> on Linux/glibc and from
 * <sys/syslimits.h> (pulled in by <limits.h>) on macOS. POSIX allows it
 * to be left undefined when there is no fixed limit, so provide a sane
 * fallback to keep the system programs portable. */
# ifndef PATH_MAX
#  define PATH_MAX 4096
# endif

# define SHELL_BUFFERSIZE 256
# define SHELL_INPUT_DELIM " \t\r\n\a"
# define SHELL_OPT_DELIM "-"
# define MAX_DAEMONS 64

# define MAX_LINES 32
# define INFO_WIDTH 80
# define MAX_LOGO_LINES 64
# define MAX_LOGO_WIDTH 256

typedef struct {
        char name[64];
        pid_t pid;
        char timestamp[128];
} DaemonInfo;

#endif