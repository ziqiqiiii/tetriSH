#include "tetrislogd.h"

// Static Functions
static int			bring_up(t_logd *lg, const t_cfg *cfg);
static void			unwind(t_logd *lg);
static void			apply_signals(t_logd *lg, int flags);
static void			drain_socket(t_logd *lg);
static void			on_idle_tick(t_logd *lg);
static uint64_t		now_ms(void);

/**
 * @brief Puts a zeroed daemon into the "nothing is open yet" state.
 *
 * Descriptors default to 0, which is stdin - so a daemon that was never
 * started would have logd_stop close the caller's standard input.
 *
 * @param lg Daemon to blank.
 */
void	logd_blank(t_logd *lg)
{
	if (lg == NULL)
		return ;
	memset(lg, 0, sizeof(*lg));
	sink_blank(&lg->sink);
	lg->sock_fd = -1;
	lg->wake[SP_READ] = -1;
	lg->wake[SP_WRITE] = -1;
	lg->idle_ms = TL_IDLE_MS;
	lg->running = false;
}

/**
 * @brief Brings the logger up: sink, lock, self-pipe, signals, socket.
 *
 * The sink is claimed before the socket is bound, and that order is the whole
 * single-instance guard: us_dgram_bind unlinks its path unconditionally, so a
 * second instance has to lose the lock race and leave before it can steal a
 * running logger's socket.
 *
 * @param lg Daemon to start.
 * @param cfg Configuration supplying both paths.
 * @return 0 on success, -1 when another instance holds the sink or the socket
 * cannot be bound.
 */
int	logd_start(t_logd *lg, const t_cfg *cfg)
{
	if (lg == NULL || cfg == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	logd_blank(lg);
	lg->cfg = *cfg;
	if (bring_up(lg, cfg) != 0)
	{
		unwind(lg);
		return (-1);
	}
	lg->running = true;
	logd_emit(lg, CIPC_LOG_INFO, "listening on %s, writing %s",
		lg->cfg.sock_path, lg->cfg.file_path);
	logd_report(lg, "boot");
	return (0);
}

/**
 * @brief Runs one iteration of the daemon loop.
 *
 * One call is one poll iteration because that is the seam the tests drive:
 * send a datagram or raise a signal, step the loop once, assert on the file.
 * Signals are acted on before records, so a pending rotation happens before
 * the next batch is written rather than after.
 *
 * @param lg Daemon to step.
 * @return 0 on success, -1 on an unrecoverable poll error.
 */
int	logd_run_once(t_logd *lg)
{
	struct pollfd	pfd[2];
	int				n;

	if (lg == NULL)
		return (-1);
	pfd[0].fd = lg->sock_fd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	pfd[1].fd = lg->wake[SP_READ];
	pfd[1].events = POLLIN;
	pfd[1].revents = 0;
	n = poll(pfd, 2, lg->idle_ms);
	if (n < 0 && errno != EINTR)
		return (-1);
	if (pfd[1].revents & POLLIN)
		sp_drain(lg->wake[SP_READ]);
	apply_signals(lg, sig_take());
	if (pfd[0].revents & POLLIN)
		drain_socket(lg);
	else if (n == 0)
		on_idle_tick(lg);
	return (0);
}

/**
 * @brief Drains what is left, reports, and releases everything.
 *
 * The final drain is not tidiness: those records are ones the kernel already
 * accepted on this process's behalf, so dropping them would lose exactly the
 * last few lines before a shutdown - the ones worth reading.
 *
 * @param lg Daemon to stop; safe on one that never started.
 */
void	logd_stop(t_logd *lg)
{
	if (lg == NULL)
		return ;
	if (lg->sock_fd >= 0)
	{
		drain_socket(lg);
		logd_report(lg, "exit");
	}
	sig_detach();
	sink_close(&lg->sink);
	if (lg->sock_fd >= 0)
		us_close_unlink(lg->sock_fd, lg->cfg.sock_path);
	lg->sock_fd = -1;
	if (lg->wake[SP_READ] >= 0)
		close(lg->wake[SP_READ]);
	if (lg->wake[SP_WRITE] >= 0)
		close(lg->wake[SP_WRITE]);
	lg->wake[SP_READ] = -1;
	lg->wake[SP_WRITE] = -1;
	lg->running = false;
}

/**
 * @brief Handles one received datagram, counting whichever fate it meets.
 *
 * A record that fails validation is discarded rather than written: the sink
 * is a file operators read line by line, and one malformed datagram must not
 * be able to corrupt a line of it. A valid record whose sink is unavailable
 * goes to stderr and counts as Degraded, which is still not a Dropped record:
 * Dropped means tetrisd never sent it. Under dspawn stderr is /dev/null, so a
 * degraded record is gone and its counter is the only evidence it existed.
 *
 * @param lg Daemon whose sink and counters are used.
 * @param buf Raw datagram bytes.
 * @param len Bytes received.
 * @return 0 when the record was written, -1 when it was rejected or degraded.
 */
int	logd_accept(t_logd *lg, const void *buf, size_t len)
{
	char	line[TL_LINE_MAX];
	int		n;

	if (lg == NULL)
		return (-1);
	n = -1;
	if (lr_validate(buf, len) == 0)
		n = lr_format_line((const t_log_record *)buf, line, sizeof(line));
	if (n <= 0)
	{
		lg->count.rejected++;
		return (-1);
	}
	if (sink_is_open(&lg->sink)
		&& sink_write(&lg->sink, line, (size_t)n) == 0)
	{
		lg->count.written++;
		return (0);
	}
	fprintf(stderr, "%s", line);
	lg->count.degraded++;
	return (-1);
}

/**
 * @brief Writes one of the logger's own records into its own sink.
 *
 * Self-authored records go straight to the sink and never through the socket:
 * a logger that logged to itself over IPC would be queuing behind its own
 * receive buffer.
 *
 * @param lg Daemon to log through.
 * @param level Severity of the event.
 * @param fmt printf-style format for the message text.
 */
void	logd_emit(t_logd *lg, t_log_level level, const char *fmt, ...)
{
	t_log_record	rec;
	char			msg[CIPC_LOG_MSG_MAX];
	va_list			ap;

	if (lg == NULL || fmt == NULL)
		return ;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (lr_make(&rec, level, now_ms(), (uint32_t)getpid(),
			TL_COMPONENT, msg) != 0)
		return ;
	logd_accept(lg, &rec, sizeof(rec));
}

/**
 * @brief Writes the counter line that stands in for a control channel.
 *
 * With no control socket in this milestone, these reports are the only way
 * the counters become observable: at boot and at exit, on rotation, on
 * SIGUSR1, and whenever the sink is lost or comes back.
 *
 * The line is itself a record, so the totals it carries exclude it.
 *
 * @param lg Daemon whose counters are reported.
 * @param event What prompted the report.
 */
void	logd_report(t_logd *lg, const char *event)
{
	if (lg == NULL || event == NULL)
		return ;
	logd_emit(lg, CIPC_LOG_INFO,
		"%s: %llu written, %llu rejected, %llu degraded", event,
		(unsigned long long)lg->count.written,
		(unsigned long long)lg->count.rejected,
		(unsigned long long)lg->count.degraded);
}

/**
 * @brief Opens everything logd_start needs, in the order that order matters.
 *
 * @param lg Daemon being started.
 * @param cfg Configuration supplying both paths.
 * @return 0 on success, -1 with errno set on the first failure.
 */
static int	bring_up(t_logd *lg, const t_cfg *cfg)
{
	if (sink_open(&lg->sink, cfg->file_path) != 0)
		return (-1);
	if (sp_pipe(lg->wake) != 0)
		return (-1);
	if (sig_install(lg->wake[SP_WRITE]) != 0)
		return (-1);
	if (cfg_mkdir_parent(cfg->sock_path) != 0)
		return (-1);
	lg->sock_fd = us_dgram_bind(cfg->sock_path, TL_SOCK_MODE);
	if (lg->sock_fd < 0)
		return (-1);
	return (0);
}

/**
 * @brief Releases whatever a failed start-up had already opened.
 *
 * It reports nothing and unlinks nothing. A second instance fails here, and
 * the socket on disk at that moment belongs to the logger that is still
 * running.
 *
 * @param lg Daemon to unwind.
 */
static void	unwind(t_logd *lg)
{
	sig_detach();
	sink_close(&lg->sink);
	if (lg->wake[SP_READ] >= 0)
		close(lg->wake[SP_READ]);
	if (lg->wake[SP_WRITE] >= 0)
		close(lg->wake[SP_WRITE]);
	lg->wake[SP_READ] = -1;
	lg->wake[SP_WRITE] = -1;
	lg->running = false;
}

/**
 * @brief Receives every datagram the socket is holding.
 *
 * Draining until EAGAIN rather than one record per wake-up is what keeps a
 * burst from filling the receive buffer, which would push the sender into its
 * stderr fallback for no reason.
 *
 * @param lg Daemon whose socket is drained.
 */
static void	drain_socket(t_logd *lg)
{
	unsigned char	buf[sizeof(t_log_record) + 1];
	ssize_t			n;

	if (lg->sock_fd < 0)
		return ;
	n = us_dgram_recv(lg->sock_fd, buf, sizeof(buf));
	while (n > 0)
	{
		logd_accept(lg, buf, (size_t)n);
		n = us_dgram_recv(lg->sock_fd, buf, sizeof(buf));
	}
}

/**
 * @brief Carries out whatever the signals that arrived asked for.
 *
 * @param lg Daemon to act on.
 * @param flags Bitmask from sig_take.
 */
static void	apply_signals(t_logd *lg, int flags)
{
	if (flags & TL_SIG_HUP)
	{
		if (sink_reopen(&lg->sink) == 0)
			logd_report(lg, "rotated");
		else
			logd_report(lg, "rotation failed, degrading to stderr");
	}
	if (flags & TL_SIG_DUMP)
		logd_report(lg, "dump");
	if (flags & TL_SIG_STOP)
		lg->running = false;
}

/**
 * @brief The work an otherwise idle iteration does.
 *
 * Both the flush and the sink retry ride the timeout, so neither costs
 * anything on a busy logger and a sink that came back is still picked up.
 *
 * @param lg Daemon to service.
 */
static void	on_idle_tick(t_logd *lg)
{
	sink_sync(&lg->sink);
	if (sink_retry(&lg->sink) == 0)
		logd_report(lg, "sink recovered");
}

/**
 * @brief Reads the wall clock in milliseconds.
 *
 * @return Milliseconds since the epoch.
 */
static uint64_t	now_ms(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (0);
	return ((uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000));
}
