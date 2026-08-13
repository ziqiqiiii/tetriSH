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
 * initialised would have logger_shutdown close the caller's standard input. Boot
 * can fail before logger_init runs, and teardown is shared between that path and
 * an ordinary stop, so the safe state has to exist before anything can fail.
 *
 * @param lg Logger to blank.
 */
void	logger_blank(t_logger *lg)
{
	if (lg == NULL)
		return ;
	memset(lg, 0, sizeof(*lg));
	lg->sock_fd = -1;
	lg->wake[SELFPIPE_READ] = -1;
	lg->wake[SELFPIPE_WRITE] = -1;
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
int	logger_init(t_logger *lg, const t_config *cfg)
{
	if (lg == NULL || cfg == NULL)
		return (-1);
	logger_blank(lg);
	atomic_store(&lg->level, cfg->log_level);
	snprintf(lg->ipc_path, TETRISD_FILESYSTEM_PATH_MAX, "%s", cfg->log_ipc);
	if (ring_init(&lg->ring, sizeof(t_log_record), TETRISD_LOG_RING_CAPACITY) != 0)
		return (-1);
	if (selfpipe_open(lg->wake) != 0)
	{
		ring_destroy(&lg->ring);
		return (-1);
	}
	lg->sock_fd = unixsock_dgram_open(lg->ipc_path);
	atomic_store(&lg->fallback, lg->sock_fd < 0);
	atomic_store(&lg->running, true);
	if (pthread_create(&lg->shipper, NULL, shipper_main, lg) != 0)
	{
		atomic_store(&lg->running, false);
		logger_shutdown(lg);
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
void	logger_emit(t_logger *lg, t_log_level level, const char *fmt, ...)
{
	t_log_record	rec;
	char			msg[COREIPC_LOG_MSG_MAX];
	va_list			ap;

	if (lg == NULL || fmt == NULL || (int)level < atomic_load(&lg->level))
		return ;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (logrecord_make(&rec, level, clock_now_ms(), (uint32_t)getpid(), TETRISD_COMPONENT_NAME, msg) != 0)
		return ;
	ring_push(&lg->ring, &rec);
}

/**
 * @brief Reports how many records were dropped under pressure.
 *
 * @param lg Logger to query, or NULL.
 * @return Total records dropped since start-up, 0 when lg is NULL.
 */
uint64_t	logger_dropped_count(const t_logger *lg)
{
	if (lg == NULL)
		return (0);
	return (ring_dropped_count(&lg->ring));
}

/**
 * @brief Reports whether records are reaching the Sink or falling back.
 *
 * This is not a count of anything. Dropped, Rejected and Degraded are three
 * different quantities, and this is none of them - it is only which way this
 * producer's records are currently going.
 *
 * @param lg Logger to query, or NULL.
 * @return true when records are going to tetrislogd, false when they are
 *         falling back to this daemon's own stderr; false when lg is NULL.
 */
bool	logger_sink_reaching(const t_logger *lg)
{
	if (lg == NULL)
		return (false);
	return (!atomic_load(&lg->fallback));
}

/**
 * @brief Stops the shipper, flushes what is left, and releases the ring.
 *
 * The final drain runs on the caller's thread so records emitted during
 * shutdown still reach the logger (or stderr) rather than dying in the ring.
 *
 * @param lg Logger to shut down; safe on a partially initialised logger.
 */
void	logger_shutdown(t_logger *lg)
{
	if (lg == NULL)
		return ;
	atomic_store(&lg->running, false);
	if (lg->wake[SELFPIPE_WRITE] >= 0)
		selfpipe_notify(lg->wake[SELFPIPE_WRITE]);
	if (lg->shipper_started)
		pthread_join(lg->shipper, NULL);
	lg->shipper_started = false;
	drain_once(lg);
	if (lg->sock_fd >= 0)
		close(lg->sock_fd);
	lg->sock_fd = -1;
	if (lg->wake[SELFPIPE_READ] >= 0)
		close(lg->wake[SELFPIPE_READ]);
	if (lg->wake[SELFPIPE_WRITE] >= 0)
		close(lg->wake[SELFPIPE_WRITE]);
	lg->wake[SELFPIPE_READ] = -1;
	lg->wake[SELFPIPE_WRITE] = -1;
	ring_destroy(&lg->ring);
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
		pfd.fd = lg->wake[SELFPIPE_READ];
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, TETRISD_LOG_SHIPPER_WAIT_MS) > 0 && (pfd.revents & POLLIN))
			selfpipe_drain(lg->wake[SELFPIPE_READ]);
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
	t_log_record	batch[TETRISD_LOG_DRAIN_MAX];
	size_t			got;
	size_t			i;

	got = ring_drain(&lg->ring, batch, TETRISD_LOG_DRAIN_MAX);
	while (got > 0)
	{
		i = 0;
		while (i < got)
		{
			ship_one(lg, &batch[i]);
			i++;
		}
		got = ring_drain(&lg->ring, batch, TETRISD_LOG_DRAIN_MAX);
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
		&& unixsock_dgram_send_nonblock(lg->sock_fd, rec, sizeof(*rec)) == 0)
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
	char	line[COREIPC_LOG_MSG_MAX + 128];

	atomic_store(&lg->fallback, true);
	if (logrecord_format_line(rec, line, sizeof(line)) > 0)
		fprintf(stderr, "%s\n", line);
}

/**
 * @brief Retries the connection to tetrislogd's datagram socket.
 *
 * @param lg Logger to reconnect.
 */
static void	reconnect(t_logger *lg)
{
	lg->sock_fd = unixsock_dgram_open(lg->ipc_path);
	if (lg->sock_fd >= 0)
		atomic_store(&lg->fallback, false);
}
