#include "tetrisd.h"

// Static Functions
static void	*loop_main(void *arg);
static int	bring_up(t_server *srv, const t_config *cfg);
static int	open_reactor(t_server *srv);
static int	open_timer(t_server *srv);
static int	watch(t_server *srv, int fd, t_event_tag *tag);
static void	destroy(t_server *srv);

/**
 * @brief Boots a game server and returns as soon as it is accepting clients.
 *
 * This is the entry seam the whole test suite uses: the executable's main is
 * a thin shim over it, so an integration test can run a real server in-process
 * on a kernel-assigned port instead of forking a daemon.
 *
 * @param cfg Configuration to run with; copied, not retained.
 * @param out Receives the running server.
 * @return 0 on success, -1 when the configuration or a resource failed.
 */
int	server_start(const t_config *cfg, t_server **out)
{
	t_server	*srv;

	if (cfg == NULL || out == NULL)
		return (-1);
	if (config_validate(cfg) != 0)
		return (-1);
	srv = calloc(1, sizeof(*srv));
	if (srv == NULL)
		return (-1);
	if (bring_up(srv, cfg) != 0)
	{
		destroy(srv);
		return (-1);
	}
	atomic_store(&srv->running, true);
	if (pthread_create(&srv->loop, NULL, loop_main, srv) != 0)
	{
		atomic_store(&srv->running, false);
		destroy(srv);
		return (-1);
	}
	srv->loop_started = true;
	logger_emit(&srv->log, COREIPC_LOG_INFO, "tetrisd listening on port %d", srv->port);
	*out = srv;
	return (0);
}

/**
 * @brief Shuts the server down cleanly and releases everything it owns.
 *
 * All this does is ask and wait. The reactor owns every client, every room and
 * the handshake pool, so it is the thread that ends them - in the one order
 * that is safe - on its way out of the loop. Joining it is therefore the whole
 * of shutdown, and there is no window here in which two threads are taking the
 * same server apart.
 *
 * @param srv Server to stop; NULL is ignored.
 */
void	server_stop(t_server *srv)
{
	if (srv == NULL)
		return ;
	atomic_store(&srv->stopping, true);
	atomic_store(&srv->running, false);
	server_wake(srv);
	if (srv->loop_started)
		pthread_join(srv->loop, NULL);
	srv->loop_started = false;
	logger_emit(&srv->log, COREIPC_LOG_INFO, "tetrisd stopped (%llu logs dropped)",
		(unsigned long long)logger_dropped_count(&srv->log));
	destroy(srv);
}

/**
 * @brief Reports the port the server actually bound.
 *
 * @param srv Running server.
 * @return The bound port, or -1 when srv is NULL.
 */
int	server_port(const t_server *srv)
{
	if (srv == NULL)
		return (-1);
	return (srv->port);
}

/**
 * @brief Blocks until the server has been asked to stop.
 *
 * @param srv Running server.
 */
void	server_wait(t_server *srv)
{
	struct timespec	nap;

	if (srv == NULL)
		return ;
	nap.tv_sec = 0;
	nap.tv_nsec = 100 * 1000000L;
	while (atomic_load(&srv->running))
		nanosleep(&nap, NULL);
}

/**
 * @brief Asks a running server to stop, from any thread.
 *
 * @param srv Server to stop.
 */
void	server_request_stop(t_server *srv)
{
	if (srv == NULL)
		return ;
	atomic_store(&srv->running, false);
	server_wake(srv);
}

/**
 * @brief Wakes the reactor from any thread.
 *
 * Every thread beside the reactor - a handshake worker with a finished
 * session, a signal handler - reaches it the same way, and none of them may
 * block doing so. The pipe is non-blocking, so a full one is not an error: it
 * already means a wake-up is pending.
 *
 * @param srv Server to wake; NULL and an unopened pipe are both ignored.
 */
void	server_wake(t_server *srv)
{
	if (srv == NULL || srv->wake[SELFPIPE_WRITE] < 0)
		return ;
	selfpipe_notify(srv->wake[SELFPIPE_WRITE]);
}

/**
 * @brief Re-reads .tetrishrc after SIGHUP and applies what can change.
 *
 * The listening port, certificates, and data directory belong to resources
 * that are already open, so a reload adjusts only the settings that can
 * safely change under a running server.
 *
 * @param srv Server to reconfigure.
 */
void	server_reload(t_server *srv)
{
	t_config	fresh;

	if (srv == NULL)
		return ;
	if (config_load(&fresh, srv->cfg.rc_path) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_WARNING, "reload failed: %s has an invalid setting", srv->cfg.rc_path);
		return ;
	}
	srv->cfg.log_level = fresh.log_level;
	atomic_store(&srv->log.level, fresh.log_level);
	srv->cfg.tick_ms = fresh.tick_ms;
	srv->tick_ms = fresh.tick_ms;
	if (reactor_arm_timer(srv) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "gravity has stopped: the tick timer could not be retimed");
		return ;
	}
	logger_emit(&srv->log, COREIPC_LOG_INFO, "reloaded %s", srv->cfg.rc_path);
}

/**
 * @brief Thread entry point for the reactor.
 *
 * server_start creates a thread rather than running the loop inline because
 * every suite drives a real server in-process through that seam and then talks
 * to it with the blocking wrappers. tetrisd is not single-threaded; it is
 * single-owner, which is the property that matters.
 *
 * @param arg The server.
 * @return Always NULL.
 */
static void	*loop_main(void *arg)
{
	reactor_run(arg);
	return (NULL);
}

/**
 * @brief Opens everything a running server needs, in dependency order.
 *
 * @param srv Server being brought up.
 * @param cfg Configuration to run with.
 * @return 0 on success, -1 when any resource could not be opened.
 */
static int	bring_up(t_server *srv, const t_config *cfg)
{
	srv->cfg = *cfg;
	srv->listen_fd = -1;
	srv->epoll_fd = -1;
	srv->timer_fd = -1;
	srv->wake[SELFPIPE_READ] = -1;
	srv->wake[SELFPIPE_WRITE] = -1;
	srv->listener_tag.source = EVENT_LISTENER;
	srv->wake_tag.source = EVENT_WAKE;
	srv->timer_tag.source = EVENT_TIMER;
	srv->tick_ms = cfg->tick_ms;
	srv->started_ms = clock_now_ms();
	logger_blank(&srv->log);
	if (server_rooms_init(srv, cfg->br_slots) != 0)
		return (-1);
	if (daemon_mkdir_p(cfg->data_dir) != 0)
		return (-1);
	if (logger_init(&srv->log, &srv->cfg) != 0)
		return (-1);
	if (session_credentials_load(cfg->cert_path, cfg->key_path, &srv->credentials) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot load %s and %s", cfg->cert_path, cfg->key_path);
		return (-1);
	}
	if (db_open(cfg->data_dir, cfg->config_dir, &srv->db) != DB_OK)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot open the player store");
		return (-1);
	}
	if (registry_init(&srv->reg, (size_t)cfg->max_clients) != 0)
		return (-1);
	srv->scratch = malloc(TETRISSH_MAX_PLAINTEXT);
	srv->sweep = calloc((size_t)cfg->max_clients, sizeof(*srv->sweep));
	if (srv->scratch == NULL || srv->sweep == NULL)
		return (-1);
	if (selfpipe_open(srv->wake) != 0)
		return (-1);
	srv->listen_fd = listener_open(cfg->port, &srv->port);
	if (srv->listen_fd < 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot listen on port %d", cfg->port);
		return (-1);
	}
	return (open_reactor(srv));
}

/**
 * @brief Opens the epoll set, watches the listener and the wake pipe, and
 *        starts the handshake pool.
 *
 * @param srv Server being brought up; its listener and pipe must be open.
 * @return 0 on success, -1 when any of the three failed.
 */
static int	open_reactor(t_server *srv)
{
	srv->epoll_fd = epoll_create1(0);
	if (srv->epoll_fd < 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot open epoll: %s", strerror(errno));
		return (-1);
	}
	if (watch(srv, srv->listen_fd, &srv->listener_tag) != 0
		|| watch(srv, srv->wake[SELFPIPE_READ], &srv->wake_tag) != 0
		|| open_timer(srv) != 0)
		return (-1);
	if (handshake_pool_start(&srv->pool, srv) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot start the handshake pool");
		return (-1);
	}
	return (0);
}

/**
 * @brief Opens the gravity timer and puts it in the epoll set.
 *
 * One timer for the whole server, not one per room: t_game already accumulates
 * against each player's own gravity interval, so a uniform coarse tick still
 * produces per-player speeds.
 *
 * @param srv Server being brought up; its epoll set must be open.
 * @return 0 on success, -1 on failure.
 */
static int	open_timer(t_server *srv)
{
	srv->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (srv->timer_fd < 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot open the tick timer: %s", strerror(errno));
		return (-1);
	}
	if (watch(srv, srv->timer_fd, &srv->timer_tag) != 0)
		return (-1);
	clock_gettime(CLOCK_MONOTONIC, &srv->last_tick);
	return (reactor_arm_timer(srv));
}

/**
 * @brief Adds one of the server's own descriptors to the epoll set.
 *
 * @param srv Server holding the epoll descriptor.
 * @param fd Descriptor to watch for readability.
 * @param tag What the reactor should read the event back as.
 * @return 0 on success, -1 on failure.
 */
static int	watch(t_server *srv, int fd, t_event_tag *tag)
{
	struct epoll_event	ev;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = tag;
	if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, fd, &ev) != 0)
	{
		logger_emit(&srv->log, COREIPC_LOG_ERROR, "cannot watch fd %d: %s", fd, strerror(errno));
		return (-1);
	}
	return (0);
}

/**
 * @brief Releases every resource a server holds and frees it.
 *
 * Written to tolerate a partially built server, so start-up failure and
 * ordinary shutdown share one teardown path instead of two.
 *
 * @param srv Server to destroy.
 */
static void	destroy(t_server *srv)
{
	handshake_pool_destroy(&srv->pool);
	client_reap(srv);
	if (srv->timer_fd >= 0)
		close(srv->timer_fd);
	if (srv->epoll_fd >= 0)
		close(srv->epoll_fd);
	if (srv->listen_fd >= 0)
		close(srv->listen_fd);
	if (srv->wake[SELFPIPE_READ] >= 0)
		close(srv->wake[SELFPIPE_READ]);
	if (srv->wake[SELFPIPE_WRITE] >= 0)
		close(srv->wake[SELFPIPE_WRITE]);
	free(srv->scratch);
	free(srv->sweep);
	registry_destroy(&srv->reg);
	session_credentials_free(srv->credentials);
	if (srv->db != NULL)
		db_close(srv->db);
	logger_shutdown(&srv->log);
	free(srv);
}
