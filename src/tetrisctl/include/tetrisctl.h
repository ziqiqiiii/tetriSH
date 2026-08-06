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
** This first version drives both daemons by pidfile and signal: start forks
** and execs the binary and reports what its readiness pipe says, status reads
** the pidfile lock, stop sends SIGTERM and waits for that lock to come free
** (docs/adr/0007). tetrisd's control socket lands as a second step, buying an
** admin channel that still answers while the public port is flooded.
**
** No deployment detail is compiled in: the daemon set and start order come
** from TETRISCTL_DAEMONS in .tetrishrc, and each pidfile path from the key its
** own daemon publishes it under, so no path is written down twice. Teardown is
** that order reversed - stopping the logger first would push tetrisd's whole
** shutdown into its error file instead of the log.
**
** Compiled in is only the daemon name -> pidfile key mapping, which is
** knowledge about the programs rather than about a deployment of them.
*/

# define TETRISCTL_COMPONENT_NAME		"tetrisctl"
# define TETRISCTL_CONFIG_KEY_PREFIX		"TETRISCTL_"
# define TETRISCTL_RC_FILENAME			".tetrishrc"
# define TETRISCTL_FILESYSTEM_PATH_MAX		1024
# define TETRISCTL_CONFIG_LINE_MAX		2048
# define TETRISCTL_NAME_MAX		32
# define TETRISCTL_MAX_DAEMONS		8

/*
** How long stop waits for a daemon to finish tearing down. Generous on
** purpose: the wait ends the moment the pidfile lock comes free, so the only
** thing this number decides is when to give up and say so.
*/
# define TETRISCTL_STOP_MS			10000

/*
** What tetrisctl can tell about a daemon. MANAGED_UNKNOWN is not a third kind of
** running - it means the pidfile could not be inspected, which is a different
** report to make and a different exit code to return.
*/
typedef enum e_managed_state
{
	MANAGED_STOPPED,
	MANAGED_RUNNING,
	MANAGED_UNKNOWN
}	t_managed_state;

/*
** A daemon this build knows how to manage, and the .tetrishrc key it publishes
** its pidfile under. The key names differ between the two because each daemon
** keeps its own prefix's existing habit - tetrisd already had CERT_PATH and
** KEY_PATH, tetrislogd already had SOCK and FILE - and a daemon reading its
** own settings is the one place that consistency actually matters.
*/
typedef struct s_known_daemon
{
	const char	*name;
	const char	*pid_key;
}	t_known_daemon;

/*
** One managed daemon: the name .tetrishrc listed and the pidfile that name
** resolved to. The pidfile is the whole handle - it is what gets signalled,
** what gets waited on, and what says whether a second start may proceed.
*/
typedef struct s_managed
{
	char	name[TETRISCTL_NAME_MAX];
	char	pid_path[TETRISCTL_FILESYSTEM_PATH_MAX];
}	t_managed;

/*
** The roster, resolved once from .tetrishrc.
**
** `order` and `paths` are kept raw until config_resolve runs, because a start-up
** file may name the daemons before or after it names their pidfiles and
** neither ordering should change the answer. Both start empty: a roster that
** was never declared is an error, not a default, or the order this program
** exists to take from a file would be sitting in this one instead.
*/
typedef struct s_ctl
{
	t_managed	daemons[TETRISCTL_MAX_DAEMONS];
	int			count;
	char		order[TETRISCTL_CONFIG_LINE_MAX];
	char		paths[TETRISCTL_MAX_DAEMONS][TETRISCTL_FILESYSTEM_PATH_MAX];
	char		rc_path[TETRISCTL_FILESYSTEM_PATH_MAX];
	int			stop_ms;
}	t_ctl;

/* CONFIG.C */
void			config_defaults(t_ctl *ctl);
int				config_set(t_ctl *ctl, const char *key, const char *value);
int				config_parse_line(t_ctl *ctl, const char *line);
int				config_resolve(t_ctl *ctl);
int				config_load(t_ctl *ctl, const char *rc_override);
int				config_resolve_rc_path(const char *override, char *out, size_t cap);
const t_managed	*ctl_find_daemon(const t_ctl *ctl, const char *name);

/* MANAGED.C */
t_managed_state			managed_state(const t_managed *d, pid_t *pid);
int				managed_start(const t_managed *d, const char *rc_path);
int				managed_stop(const t_managed *d, int timeout_ms);

/* COMMANDS.C */
int				start_command(const t_ctl *ctl, const char *only);
int				status_command(const t_ctl *ctl, const char *only);
int				stop_command(const t_ctl *ctl, const char *only);
int				restart_command(const t_ctl *ctl, const char *only);

# endif
