# ifndef TETRISLOGD_H
# define TETRISLOGD_H

# include <errno.h>
# include <fcntl.h>
# include <poll.h>
# include <signal.h>
# include <stdarg.h>
# include <stdbool.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/file.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <time.h>
# include <unistd.h>

# include "coredaemon.h"
# include "coreipc.h"

/*
** tetrislogd - the standalone logger daemon.
**
** One process, one thread, one loop: poll the bound AF_UNIX datagram socket
** and a self-pipe, receive one fixed-size t_log_record per datagram, validate
** it, format it, and write it to the sink. It keeps no internal queue - the
** kernel's socket receive buffer is the only queue in the design, so a full
** buffer pushes back on the sender rather than dropping records this process
** has already accepted (docs/adr/0005).
**
** A record has exactly three fates, and they are three different words:
**   Dropped   tetrisd's ring was full; the record never left tetrisd.
**   Rejected  it arrived here but failed lr_validate; discarded.
**   Degraded  it was valid but the sink was unavailable; written to stderr.
** Only the last two are counted here - Dropped belongs to tetrisd (rb_drops).
**
** The logger survives tetrisd restarts and never exits because of one: a
** producer that goes away simply stops sending.
**
** It detaches itself and publishes a locked pidfile, and tetrisctl starts,
** inspects and stops it through that file (docs/adr/0007). Both the fork and
** the claim live in main.c and nowhere else: logd_start must stay the seam
** the tests drive in-process, and a start function that forked would take
** every suite with it.
*/

# define TL_COMPONENT		"tetrislogd"
# define TL_KEY_PREFIX		"TETRISLOGD_"
# define TL_RC_NAME			".tetrishrc"
# define TL_PATH_MAX		1024
# define TL_LINE_MAX		2048

/* config defaults - all overridable from .tetrishrc */
# define TL_DEF_SOCK		"tmp/tetrisd/tetrislogd.sock"
# define TL_DEF_FILE		"tmp/tetrislogd/tetrislogd.log"
# define TL_DEF_PID			"tmp/tetrislogd/tetrislogd.pid"
# define TL_DEF_ERR			"tmp/tetrislogd/tetrislogd.err"

# define TL_SOCK_MODE		0600
# define TL_FILE_MODE		0644
# define TL_DIR_MODE		0755

/*
** Idle tick. The poll timeout doubles as the period of the fdatasync, of the
** sink-reopen retry, and of the check for the log file having been deleted or
** replaced, so none of the three ever happens on a loop that had records to
** handle. logd_start copies it into t_logd.idle_ms, which tests lower to keep
** the timeout path fast.
*/
# define TL_IDLE_MS			1000

/* signal flags reported by sig_take */
# define TL_SIG_STOP		0x1
# define TL_SIG_HUP			0x2
# define TL_SIG_DUMP		0x4

/*
** Every path the daemon touches, resolved once at boot from .tetrishrc.
** Config is cold: SIGHUP reopens the sink at the same path, it does not
** re-read this. Paths change by restarting the daemon.
*/
typedef struct s_cfg
{
	char	sock_path[TL_PATH_MAX];
	char	file_path[TL_PATH_MAX];
	char	pid_path[TL_PATH_MAX];
	char	err_path[TL_PATH_MAX];
	char	rc_path[TL_PATH_MAX];
}	t_cfg;

/*
** What the logger can honestly account for. `written` and `degraded` together
** are every valid record it handled; `rejected` is the malformed remainder.
*/
typedef struct s_counters
{
	uint64_t	written;
	uint64_t	rejected;
	uint64_t	degraded;
}	t_counters;

/*
** The log file. fd is -1 while the sink is unavailable, which is a working
** state and not a fatal one - records go to stderr until a retry on the idle
** tick gets the file back.
**
** No lock is taken here. The sink used to carry the single-instance guard as
** well, which conflated two things: deleting tmp/ took away the sink and the
** guard in one stroke, and the reclaim path had to restore both. The guard is
** the pidfile now (docs/adr/0007), so the sink is only about the sink.
**
** dev and ino are which file the descriptor actually holds, remembered so the
** daemon can notice the path now names a different one. An open descriptor
** outlives the unlink that took its name away, so `make reset` leaves a logger
** writing happily into an inode nothing can open - the writes succeed and the
** log is empty. Both are 0 when the sink has never been opened.
*/
typedef struct s_sink
{
	int		fd;
	bool	dirty;
	dev_t	dev;
	ino_t	ino;
	char	path[TL_PATH_MAX];
}	t_sink;

/*
** The whole daemon. main.c is a shim: logd_start, then logd_run_once until
** running goes false, then logd_stop. One call to logd_run_once is one poll
** iteration, which is what makes the loop testable in-process without a
** thread and without a fork.
*/
typedef struct s_logd
{
	t_cfg		cfg;
	t_sink		sink;
	t_counters	count;
	int			sock_fd;
	int			wake[2];
	int			idle_ms;
	bool		running;
}	t_logd;

/* CFG.C */
void	cfg_defaults(t_cfg *cfg);
int		cfg_set(t_cfg *cfg, const char *key, const char *value);
int		cfg_parse_line(t_cfg *cfg, const char *line);
int		cfg_load(t_cfg *cfg, const char *rc_override);
int		cfg_resolve_rc(const char *override, char *out, size_t cap);
int		cfg_mkdir_p(const char *path);
int		cfg_mkdir_parent(const char *path);

/* SINK.C */
void	sink_blank(t_sink *sk);
int		sink_open(t_sink *sk, const char *path);
int		sink_write(t_sink *sk, const char *line, size_t len);
int		sink_sync(t_sink *sk);
int		sink_reopen(t_sink *sk);
int		sink_retry(t_sink *sk);
void	sink_close(t_sink *sk);
bool	sink_is_open(const t_sink *sk);
bool	sink_is_stale(const t_sink *sk);

/* LOGD.C */
void	logd_blank(t_logd *lg);
int		logd_start(t_logd *lg, const t_cfg *cfg);
int		logd_run_once(t_logd *lg);
void	logd_stop(t_logd *lg);
int		logd_accept(t_logd *lg, const void *buf, size_t len);
void	logd_emit(t_logd *lg, t_log_level level, const char *fmt, ...);
void	logd_report(t_logd *lg, const char *event);

/* SIGNALS.C */
int		sig_install(int wake_fd);
int		sig_take(void);
void	sig_detach(void);

# endif
