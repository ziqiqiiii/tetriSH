# ifndef HARNESS_H
# define HARNESS_H

# include "coredaemon.h"


# include <dirent.h>
# include <sys/wait.h>
# include <time.h>

/*
** The shared fixture for the libcoredaemon suites.
**
** Every case that touches the filesystem works inside its own mkdtemp
** directory, because the thing under test is a lock over a path and two
** suites sharing one path would pass or fail together for the wrong reason.
**
** fx_hold is the piece worth explaining. A single-instance guard cannot be
** tested from one process: flock is held per open file description, so the
** same process re-locking its own pidfile succeeds and proves nothing. So the
** fixture forks a real second process, has it claim the file for real, and
** tells the test when it has - which is exactly the shape of a second daemon
** losing the race.
*/

/*
** Test-side buffer sizes. Deliberately smaller than DAEMON_PATH_MAX: a mkdtemp
** directory plus a short leaf is all any case builds, and sizing these from
** the library's own maximum makes every snprintf here look to the compiler
** like it might truncate.
*/
# define FX_DIR_MAX		96
# define FX_PATH_MAX	256

typedef struct s_holder
{
	pid_t	pid;
	int		up[2];
	int		go[2];
}	t_holder;

/* HARNESS.C */
int		fx_tmpdir(char *out, size_t cap);
void	fx_rmtree(const char *path);
int		fx_hold(t_holder *h, const char *path);
void	fx_stop(t_holder *h);
int		fx_reap(pid_t pid);
ssize_t	fx_slurp(const char *path, char *out, size_t cap);
int		fx_wait_file(const char *path, int timeout_ms);

# endif
