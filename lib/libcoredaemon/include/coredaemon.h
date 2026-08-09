# ifndef COREDAEMON_H
# define COREDAEMON_H

# include <errno.h>
# include <fcntl.h>
# include <signal.h>
# include <stdbool.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/file.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <time.h>
# include <unistd.h>

/*
** libcoredaemon - becoming a daemon, and finding one that already is.
**
** Both sides of one agreement live here on purpose. The daemon detaches,
** claims a locked pidfile and reports itself ready; tetrisctl reads that same
** pidfile, signals the process and waits for the lock to come free.
**
** Two things this library deliberately does not do. It never logs, because a
** daemon that cannot start says so on the stderr it still shares with the
** terminal, and a library printing over that would bury it; the
** daemon_report_* functions print only what a caller asked for, and nothing
** here calls them. And it never forks behind a start function: daemon_detach
** belongs in main() alone, or in-process test suites would begin forking.
*/

# define DAEMON_PATH_MAX		1024
# define DAEMON_FILE_MODE		0644
# define DAEMON_DIR_MODE		0755

# define DAEMON_CL_RED			"\x1b[31m"
# define DAEMON_CL_GREEN		"\x1b[32m"
# define DAEMON_CL_YELLOW		"\x1b[33m"
# define DAEMON_CL_BLUE			"\x1b[34m"
# define DAEMON_CL_DIM			"\x1b[2m"
# define DAEMON_CL_BOLD			"\x1b[1m"
# define DAEMON_CL_RESET		"\x1b[0m"

# define DAEMON_NAME_COL_MIN	14

# define DAEMON_VERB_COL_MIN	11

/*
** The byte a daemon writes to say it booted. Its value is irrelevant, its
** presence everything: the parent reads one byte for success and end-of-file
** for a child that died. The shell's quiet-pipe closes on both paths and so
** cannot tell them apart.
*/
# define DAEMON_READY_BYTE		'1'

/* how often daemon_pid_wait retries the lock while waiting for an exit */
# define DAEMON_WAIT_STEP_MS	20

typedef struct s_pidfile
{
	int		fd;
	pid_t	pid;
	char	path[DAEMON_PATH_MAX];
}	t_pidfile;

typedef enum e_report_state
{
	DAEMON_REPORT_STOPPED,
	DAEMON_REPORT_RUNNING,
	DAEMON_REPORT_UNKNOWN
}	t_report_state;

typedef enum e_notice_kind
{
	DAEMON_NOTICE_UP,
	DAEMON_NOTICE_DOWN,
	DAEMON_NOTICE_IDLE
}	t_notice_kind;

/* DETACH.C - daemon side */
int		daemon_detach(int *ready_fd);
void	daemon_ready(int ready_fd);
int		daemon_stderr_redirect(const char *path);

/* PIDFILE.C - daemon side */
void	daemon_pid_blank(t_pidfile *pf);
int		daemon_pid_claim(t_pidfile *pf, const char *path);
void	daemon_pid_release(t_pidfile *pf);

/* PROBE.C - tetrisctl side */
int		daemon_pid_read(const char *path, pid_t *out);
int		daemon_pid_probe(const char *path, pid_t *out);
int		daemon_pid_wait(const char *path, int timeout_ms);
int		daemon_pid_uptime(pid_t pid, long *seconds);

/* PATHS.C */
int		daemon_mkdir_p(const char *path);
int		daemon_mkdir_parent(const char *path);

/* REPORT.C - tetrisctl side, output formatting */
void	daemon_report_colour(bool on);
int		daemon_report_width(const char *names, int count, size_t stride);
void	daemon_report_header(const char *indent, int name_width, int detail_width);
void	daemon_report_row(const char *indent, int name_width, const char *name, t_report_state state, pid_t pid, long uptime, const char *detail);
void	daemon_report_uptime(long seconds, char *out, size_t cap);
void	daemon_report_break(void);
void	daemon_report_notice(t_notice_kind kind, const char *verb, const char *name, pid_t pid);
void	daemon_report_footer(const char *label, int count);
void	daemon_report_error(const char *component, const char *subject, const char *reason);

# endif
