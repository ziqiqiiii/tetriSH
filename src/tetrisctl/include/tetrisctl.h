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
# include "coreipc.h"
# include "htttp.h"
# include "statusbody.h"

/*
** tetrisctl - the admin CLI that owns the game daemons' lifecycle.
**
** This first version drives both daemons by pidfile and signal: start forks
** and execs the binary and reports what its readiness pipe says, status reads
** the pidfile lock, stop sends SIGTERM and waits for that lock to come free.
** tetrisd's control socket lands as a second step, buying an
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
** The Control channel, from the asking end.
**
** TETRISCTL_CONTROL_MS bounds the whole exchange - connect, write, read - and
** is short because the channel is a local socket answered on the reactor: a
** server that is up answers in microseconds, and one that does not answer
** quickly is one an operator wants told about rather than waited on.
**
** A reply is one length-prefixed plaintext HTTTP message. There is no session
** here, so nothing is encrypted and nothing is negotiated; reachability of a
** 0600 socket is the whole of the authorisation.
*/
# define TETRISCTL_CONTROL_MS		3000
# define TETRISCTL_LENGTH_PREFIX_BYTES	4
# define TETRISCTL_CONTROL_ROUTE	"/admin"
# define TETRISCTL_CONTROL_BODY_MAX	32768
/*
** Rows a `rooms` listing will decode into. LOBBY_MAX_ROOMS is tetrisd's, and
** tetrisctl does not link libtetrisroom to read it - so this is stated here
** and is a ceiling on what this program will print, not on what the server
** will send. A server that grew its lobby past this would have its extra rows
** refused by the decoder rather than silently dropped.
*/
# define TETRISCTL_MAX_ROOM_ROWS	64

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
** its pidfile under.
*/
typedef struct s_known_daemon
{
	const char	*name;
	const char	*pid_key;
	/*
	** The .tetrishrc key naming this daemon's Control channel, or NULL when it
	** has none. Which daemons expose one is knowledge about the programs
	** rather than about a deployment of them, so it is compiled in beside the
	** pidfile key rather than configured: tetrisd serves a channel and
	** tetrislogd does not, and that is not a site's choice to make.
	*/
	const char	*control_key;
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
	/*
	** Empty for a daemon with no Control channel, and empty too when the key
	** was simply not set. A missing path is not an error at load time the way
	** a missing pidfile is: the pidfile is how every verb works, while the
	** channel is how four of them work, and a stack managed purely by signal
	** should still start and stop.
	*/
	char	control_path[TETRISCTL_FILESYSTEM_PATH_MAX];
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
	char		control_paths[TETRISCTL_MAX_DAEMONS][TETRISCTL_FILESYSTEM_PATH_MAX];
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

/* CONTROL.C */
int				control_ask(const t_managed *d, const char *method, char *body, size_t cap, size_t *body_len);

/* COMMANDS_ADMIN.C */
int				rooms_command(const t_ctl *ctl, const char *only);
int				players_command(const t_ctl *ctl, const char *only);
int				dropped_command(const t_ctl *ctl, const char *only);
int				health_report(const t_ctl *ctl, const char *only);

# endif
