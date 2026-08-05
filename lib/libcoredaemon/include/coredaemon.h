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
** Both sides of the same agreement live here on purpose. The daemon detaches,
** claims a locked pidfile and reports itself ready; tetrisctl reads that same
** pidfile, signals the process and waits for the lock to come free. The file
** format is one decimal pid and a newline, and neither side has to be told
** that separately because neither side writes it twice (docs/adr/0007).
**
** Two things this library deliberately does not do. It never logs - a daemon
** that cannot start has to say so on the stderr it still shares with the
** terminal, and a library printing over that would bury it. And it never
** forks behind a start function: cd_detach belongs in main() alone, or an
** in-process test suite would begin forking the moment it booted a daemon.
*/

# define CD_PATH_MAX		1024
# define CD_FILE_MODE		0644
# define CD_DIR_MODE		0755

/*
** The byte a daemon writes to say it booted. Its value is irrelevant and its
** presence is everything: the parent reads one byte for success and end-of-
** file for a child that died, which is the whole difference between this and
** the shell's quiet-pipe (it closes on both paths and cannot tell them apart).
*/
# define CD_READY_BYTE		'1'

/* how often cd_pid_wait retries the lock while waiting for an exit */
# define CD_WAIT_STEP_MS	20

/*
** A held pidfile: the descriptor whose flock is the single-instance guard,
** the pid written into it, and the path both are named by. fd is -1 when
** nothing is held, which is the state cd_pid_release is safe on.
**
** The lock rather than the file is the guard, because a file can be read
** after its writer is gone and a lock cannot be held after it. One mechanism
** answers which pid to signal, whether a second instance may start, and
** whether a pidfile left behind is stale.
*/
typedef struct s_pidfile
{
	int		fd;
	pid_t	pid;
	char	path[CD_PATH_MAX];
}	t_pidfile;

/* DETACH.C - daemon side */
int		cd_detach(int *ready_fd);
void	cd_ready(int ready_fd);
int		cd_stderr_redirect(const char *path);

/* PIDFILE.C - daemon side */
void	cd_pid_blank(t_pidfile *pf);
int		cd_pid_claim(t_pidfile *pf, const char *path);
void	cd_pid_release(t_pidfile *pf);

/* PROBE.C - tetrisctl side */
int		cd_pid_read(const char *path, pid_t *out);
int		cd_pid_probe(const char *path, pid_t *out);
int		cd_pid_wait(const char *path, int timeout_ms);

/* PATHS.C */
int		cd_mkdir_p(const char *path);
int		cd_mkdir_parent(const char *path);

# endif
