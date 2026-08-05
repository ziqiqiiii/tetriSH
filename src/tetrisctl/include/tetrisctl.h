# ifndef TETRISCTL_H
# define TETRISCTL_H

# include <errno.h>
# include <signal.h>
# include <stdbool.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

# include "coredaemon.h"

/*
** tetrisctl - the admin CLI that owns the game daemons' lifecycle.
**
** This is the first version, and it drives both daemons by pidfile and
** signal: start forks and execs the binary and reports what the binary's own
** readiness pipe says, status reads the pidfile lock, stop sends SIGTERM and
** blocks until that lock comes free (docs/adr/0007). Nothing here waits on a
** protocol being built - tetrisd's control socket lands as a second step and
** buys something a signal cannot, an admin channel that still answers while
** the public port is flooded.
**
** Two things are deliberately not compiled in. The set of daemons and the
** order they start in come from TETRISCTL_DAEMONS in .tetrishrc; teardown is
** that order reversed, because stopping the logger first would push tetrisd's
** entire shutdown into its error file instead of the log.
*/

# define TC_COMPONENT		"tetrisctl"
# define TC_KEY_PREFIX		"TETRISCTL_"
# define TC_RC_NAME			".tetrishrc"
# define TC_PATH_MAX		1024
# define TC_LINE_MAX		2048
# define TC_NAME_MAX		32
# define TC_MAX_DAEMONS		8

/* launch order used when .tetrishrc names none: the logger, then the server */
# define TC_DEF_DAEMONS		"tetrislogd tetrisd"

/*
** How long stop waits for a daemon to finish tearing down. Generous on
** purpose: the wait ends the moment the pidfile lock comes free, so the only
** thing this number decides is when to give up and say so.
*/
# define TC_STOP_MS			10000

/*
** What tetrisctl can tell about a daemon. TC_UNKNOWN is not a third kind of
** running - it means the pidfile could not be inspected, which is a different
** report to make and a different exit code to return.
*/
typedef enum e_state
{
	TC_STOPPED,
	TC_RUNNING,
	TC_UNKNOWN
}	t_state;

/*
** One managed daemon: the name .tetrishrc listed and the pidfile that name
** resolved to. The pidfile is the whole handle - it is what gets signalled,
** what gets waited on, and what says whether a second start may proceed.
*/
typedef struct s_daemon
{
	char	name[TC_NAME_MAX];
	char	pid_path[TC_PATH_MAX];
}	t_daemon;

/*
** The roster, resolved once from .tetrishrc.
**
** `order` and `paths` are kept raw until cfg_resolve runs, because a start-up
** file may name the daemons before or after it names their pidfiles and
** neither ordering should change the answer.
*/
typedef struct s_ctl
{
	t_daemon	daemons[TC_MAX_DAEMONS];
	int			count;
	char		order[TC_LINE_MAX];
	char		paths[TC_MAX_DAEMONS][TC_PATH_MAX];
	char		rc_path[TC_PATH_MAX];
	int			stop_ms;
}	t_ctl;

/* CFG.C */
void			cfg_defaults(t_ctl *ctl);
int				cfg_set(t_ctl *ctl, const char *key, const char *value);
int				cfg_parse_line(t_ctl *ctl, const char *line);
int				cfg_resolve(t_ctl *ctl);
int				cfg_load(t_ctl *ctl, const char *rc_override);
int				cfg_resolve_rc(const char *override, char *out, size_t cap);
const t_daemon	*ctl_find(const t_ctl *ctl, const char *name);

/* DAEMON.C */
t_state			d_state(const t_daemon *d, pid_t *pid);
int				d_start(const t_daemon *d, const char *rc_path);
int				d_stop(const t_daemon *d, int timeout_ms);

/* CMD.C */
int				cmd_start(const t_ctl *ctl, const char *only);
int				cmd_status(const t_ctl *ctl, const char *only);
int				cmd_stop(const t_ctl *ctl, const char *only);
int				cmd_restart(const t_ctl *ctl, const char *only);

# endif
