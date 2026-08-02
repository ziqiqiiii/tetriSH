#include "tetrisd.h"

// Static Functions
static void	*shipper_main(void *arg);
static void	drain_once(t_logger *lg);
static void	ship_one(t_logger *lg, const t_log_record *rec);
static void	to_stderr(t_logger *lg, const t_log_record *rec);
static void	reconnect(t_logger *lg);

/**
 * @brief Puts a zeroed logger into the "nothing is open yet" state.
 *
 * Descriptors default to 0, which is stdin - so a logger that was never
 * initialised would have log_shutdown close the caller's standard input. Boot
 * can fail before log_init runs, and teardown is shared between that path and
 * an ordinary stop, so the safe state has to exist before anything can fail.
 *
 * @param lg Logger to blank.
 */
void	log_blank(t_logger *lg)
{
	if (lg == NULL)
		return ;
	memset(lg, 0, sizeof(*lg));
	lg->sock_fd = -1;
	lg->wake[SP_READ] = -1;
	lg->wake[SP_WRITE] = -1;
}

/**
 * @brief Brings up the log path: ring buffer, logger socket, shipper thread.
 *
 * A missing or unreachable tetrislogd is not a startup failure - records
 * simply fall back to stderr until the logger appears, because a game server
 * that refuses to run without its logger is worse than one that prints.
 *
 * @param lg Logger to initialise.
 * @param cfg Configuration supplying the IPC path and the level filter.
 * @return 0 on success, -1 when the ring or the shipper could not start.
 */
int	log_init(t_logger *lg, const t_cfg *cfg)
{
	if (lg == NULL || cfg == NULL)
		return (-1);
	log_blank(lg);
	atomic_store(&lg->level, cfg->log_level);
	snprintf(lg->ipc_path, TD_PATH_MAX, "%s", cfg->log_ipc);
	if (rb_init(&lg->ring, sizeof(t_log_record), TD_LOG_RING_CAP) != 0)
		return (-1);
	if (sp_pipe(lg->wake) != 0)
	{
		rb_destroy(&lg->ring);
		return (-1);
	}
	lg->sock_fd = us_dgram_open(lg->ipc_path);
	atomic_store(&lg->fallback, lg->sock_fd < 0);
	atomic_store(&lg->running, true);
	if (pthread_create(&lg->shipper, NULL, shipper_main, lg) != 0)
	{
		atomic_store(&lg->running, false);
		log_shutdown(lg);
		return (-1);
	}
	lg->shipper_started = true;
	return (0);
}

/**
 * @brief Records one event without ever blocking the caller.
 *
 * The record is copied into the ring and handed to the shipper thread; when
 * the ring is full the record is dropped and counted instead of waiting, so
 * logging can never stall a game (the counter is what tetrisctl reports).
 *
 * @param lg Logger, or NULL to discard the event.
 * @param level Severity; anything below the configured level is skipped.
 * @param fmt printf-style format for the message text.
 */
void	log_emit(t_logger *lg, t_log_level level, const char *fmt, ...)
{
	t_log_record	rec;
	char			msg[CIPC_LOG_MSG_MAX];
	va_list			ap;

	if (lg == NULL || fmt == NULL || (int)level < atomic_load(&lg->level))
		return ;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (lr_make(&rec, level, net_now_ms(), (uint32_t)getpid(),
			TD_COMPONENT, msg) != 0)
		return ;
	rb_push(&lg->ring, &rec);
}

/**
 * @brief Reports how many records were dropped under pressure.
 *
 * @param lg Logger to query, or NULL.
 * @return Total records dropped since start-up, 0 when lg is NULL.
 */
uint64_t	log_dropped(const t_logger *lg)
{
	if (lg == NULL)
		return (0);
	return (rb_drops(&lg->ring));
}

/**
 * @brief Stops the shipper, flushes what is left, and releases the ring.
 *
 * The final drain runs on the caller's thread so records emitted during
 * shutdown still reach the logger (or stderr) rather than dying in the ring.
 *
 * @param lg Logger to shut down; safe on a partially initialised logger.
 */
void	log_shutdown(t_logger *lg)
{
	if (lg == NULL)
		return ;
	atomic_store(&lg->running, false);
	if (lg->wake[SP_WRITE] >= 0)
		sp_notify(lg->wake[SP_WRITE]);
	if (lg->shipper_started)
		pthread_join(lg->shipper, NULL);
	lg->shipper_started = false;
	drain_once(lg);
	if (lg->sock_fd >= 0)
		close(lg->sock_fd);
	lg->sock_fd = -1;
	if (lg->wake[SP_READ] >= 0)
		close(lg->wake[SP_READ]);
	if (lg->wake[SP_WRITE] >= 0)
		close(lg->wake[SP_WRITE]);
	lg->wake[SP_READ] = -1;
	lg->wake[SP_WRITE] = -1;
	rb_destroy(&lg->ring);
}

/**
 * @brief Shipper thread: drains the ring and datagrams records to the logger.
 *
 * It waits on the self-pipe with a short timeout rather than sleeping blindly,
 * so shutdown is immediate and an idle server does no work. Reconnection to
 * tetrislogd is retried periodically while the socket is down.
 *
 * @param arg The logger.
 * @return Always NULL.
 */
static void	*shipper_main(void *arg)
{
	struct pollfd	pfd;
	t_logger		*lg;
	int				ticks;

	lg = arg;
	ticks = 0;
	while (atomic_load(&lg->running))
	{
		pfd.fd = lg->wake[SP_READ];
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, TD_SHIPPER_WAIT_MS) > 0 && (pfd.revents & POLLIN))
			sp_drain(lg->wake[SP_READ]);
		drain_once(lg);
		ticks++;
		if (lg->sock_fd < 0 && ticks % 50 == 0)
			reconnect(lg);
	}
	return (NULL);
}

/**
 * @brief Moves every record currently in the ring out to the logger.
 *
 * Records are copied out in one locked batch and sent afterwards, so the
 * ring's mutex is never held across a syscall.
 *
 * @param lg Logger whose ring is drained.
 */
static void	drain_once(t_logger *lg)
{
	t_log_record	batch[TD_LOG_DRAIN_MAX];
	size_t			got;
	size_t			i;

	got = rb_drain(&lg->ring, batch, TD_LOG_DRAIN_MAX);
	while (got > 0)
	{
		i = 0;
		while (i < got)
		{
			ship_one(lg, &batch[i]);
			i++;
		}
		got = rb_drain(&lg->ring, batch, TD_LOG_DRAIN_MAX);
	}
}

/**
 * @brief Sends one record to tetrislogd, or to stderr when that fails.
 *
 * @param lg Logger holding the datagram socket.
 * @param rec Record to ship.
 */
static void	ship_one(t_logger *lg, const t_log_record *rec)
{
	if (lg->sock_fd >= 0
		&& us_dgram_send_nb(lg->sock_fd, rec, sizeof(*rec)) == 0)
	{
		atomic_store(&lg->fallback, false);
		return ;
	}
	if (lg->sock_fd >= 0 && errno != EAGAIN)
	{
		close(lg->sock_fd);
		lg->sock_fd = -1;
	}
	to_stderr(lg, rec);
}

/**
 * @brief Prints one record on stderr in the logger's own line format.
 *
 * This is the bring-up fallback: while tetrislogd is unreachable the records
 * are still visible rather than silently lost.
 *
 * @param lg Logger, whose fallback flag is raised.
 * @param rec Record to print.
 */
static void	to_stderr(t_logger *lg, const t_log_record *rec)
{
	char	line[CIPC_LOG_MSG_MAX + 128];

	atomic_store(&lg->fallback, true);
	if (lr_format_line(rec, line, sizeof(line)) > 0)
		fprintf(stderr, "%s\n", line);
}

/**
 * @brief Retries the connection to tetrislogd's datagram socket.
 *
 * @param lg Logger to reconnect.
 */
static void	reconnect(t_logger *lg)
{
	lg->sock_fd = us_dgram_open(lg->ipc_path);
	if (lg->sock_fd >= 0)
		atomic_store(&lg->fallback, false);
}
