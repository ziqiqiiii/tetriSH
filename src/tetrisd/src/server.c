#include "tetrisd.h"

// Static Functions
static void	*loop_main(void *arg);
static void	accept_ready(t_server *srv);
static int	bring_up(t_server *srv, const t_cfg *cfg);
static void	stop_rooms(t_server *srv);
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
int	server_start(const t_cfg *cfg, t_server **out)
{
	t_server	*srv;

	if (cfg == NULL || out == NULL)
		return (-1);
	if (cfg_validate(cfg) != 0)
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
	log_emit(&srv->log, CIPC_LOG_INFO, "tetrisd listening on port %d",
		srv->port);
	*out = srv;
	return (0);
}

/**
 * @brief Shuts the server down cleanly and releases everything it owns.
 *
 * The order is what makes shutdown safe: stop accepting, shut every
 * connection down and wait for its thread to finish, and only then stop the
 * room tickers and close the store.
 *
 * Clients are drained before the rooms on purpose. A client thread can be
 * inside START - and therefore inside room_rt_begin - at any moment, so
 * stopping the tickers while clients still exist would race one thread
 * creating a ticker against another joining it. Once the registry is empty no
 * such thread remains, which removes the race rather than synchronising it.
 * The tickers left running meanwhile cost nothing: they see running=false and
 * an emptying room, and exit within a tick.
 *
 * @param srv Server to stop; NULL is ignored.
 */
void	server_stop(t_server *srv)
{
	if (srv == NULL)
		return ;
	atomic_store(&srv->stopping, true);
	atomic_store(&srv->running, false);
	if (srv->wake[SP_WRITE] >= 0)
		sp_notify(srv->wake[SP_WRITE]);
	if (srv->loop_started)
		pthread_join(srv->loop, NULL);
	srv->loop_started = false;
	reg_shutdown_all(&srv->reg);
	reg_wait_empty(&srv->reg);
	stop_rooms(srv);
	log_emit(&srv->log, CIPC_LOG_INFO, "tetrisd stopped (%llu logs dropped)",
		(unsigned long long)log_dropped(&srv->log));
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
	if (srv->wake[SP_WRITE] >= 0)
		sp_notify(srv->wake[SP_WRITE]);
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
	t_cfg	fresh;

	if (srv == NULL)
		return ;
	if (cfg_load(&fresh, srv->cfg.rc_path) != 0)
	{
		log_emit(&srv->log, CIPC_LOG_WARNING,
			"reload failed: %s has an invalid setting", srv->cfg.rc_path);
		return ;
	}
	srv->cfg.log_level = fresh.log_level;
	atomic_store(&srv->log.level, fresh.log_level);
	srv->cfg.tick_ms = fresh.tick_ms;
	atomic_store(&srv->tick_ms, fresh.tick_ms);
	log_emit(&srv->log, CIPC_LOG_INFO, "reloaded %s", srv->cfg.rc_path);
}

/**
 * @brief Main loop: waits on the listener and the self-pipe, nothing else.
 *
 * Accepting is the only thing this thread does, so a slow handshake or a
 * chatty client can never delay the next connection - that work belongs to
 * the per-client threads.
 *
 * @param arg The server.
 * @return Always NULL.
 */
static void	*loop_main(void *arg)
{
	struct pollfd	pfds[2];
	t_server		*srv;

	srv = arg;
	while (atomic_load(&srv->running))
	{
		pfds[0].fd = srv->listen_fd;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;
		pfds[1].fd = srv->wake[SP_READ];
		pfds[1].events = POLLIN;
		pfds[1].revents = 0;
		if (poll(pfds, 2, -1) < 0 && errno != EINTR)
			break ;
		if (pfds[1].revents & POLLIN)
		{
			sp_drain(srv->wake[SP_READ]);
			if (signals_take_stop())
				atomic_store(&srv->running, false);
			if (signals_take_reload())
				server_reload(srv);
			if (signals_take_dump())
				state_dump(srv);
		}
		if (pfds[0].revents & POLLIN)
			accept_ready(srv);
	}
	return (NULL);
}

/**
 * @brief Accepts every connection currently pending on the listener.
 *
 * @param srv Server whose listener is ready.
 */
static void	accept_ready(t_server *srv)
{
	int	fd;

	fd = net_accept(srv->listen_fd);
	while (fd >= 0)
	{
		if (cli_spawn(srv, fd) != 0)
			log_emit(&srv->log, CIPC_LOG_WARNING,
				"refused a connection: client limit reached");
		if (!atomic_load(&srv->running))
			return ;
		fd = net_accept(srv->listen_fd);
	}
}

/**
 * @brief Opens everything a running server needs, in dependency order.
 *
 * @param srv Server being brought up.
 * @param cfg Configuration to run with.
 * @return 0 on success, -1 when any resource could not be opened.
 */
static int	bring_up(t_server *srv, const t_cfg *cfg)
{
	srv->cfg = *cfg;
	srv->listen_fd = -1;
	srv->wake[SP_READ] = -1;
	srv->wake[SP_WRITE] = -1;
	atomic_store(&srv->tick_ms, cfg->tick_ms);
	srv->started_ms = net_now_ms();
	log_blank(&srv->log);
	pthread_mutex_init(&srv->lobby_mutex, NULL);
	lobby_init(&srv->lobby, LOBBY_MAX_ROOMS, cfg->br_slots);
	room_rt_init_all(srv);
	if (net_mkdir_p(cfg->data_dir) != 0)
		return (-1);
	if (log_init(&srv->log, &srv->cfg) != 0)
		return (-1);
	if (db_open(cfg->data_dir, cfg->config_dir, &srv->db) != DB_OK)
	{
		log_emit(&srv->log, CIPC_LOG_ERROR, "cannot open the player store");
		return (-1);
	}
	if (reg_init(&srv->reg, (size_t)cfg->max_clients) != 0)
		return (-1);
	if (sp_pipe(srv->wake) != 0)
		return (-1);
	srv->listen_fd = net_listen(cfg->port, &srv->port);
	if (srv->listen_fd < 0)
	{
		log_emit(&srv->log, CIPC_LOG_ERROR, "cannot listen on port %d",
			cfg->port);
		return (-1);
	}
	return (0);
}

/**
 * @brief Stops every room ticker and waits for each to finish.
 *
 * @param srv Server whose rooms are stopping.
 */
static void	stop_rooms(t_server *srv)
{
	int	i;

	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		room_rt_stop(&srv->rooms[i]);
		i++;
	}
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
	int	i;

	if (srv->listen_fd >= 0)
		close(srv->listen_fd);
	if (srv->wake[SP_READ] >= 0)
		close(srv->wake[SP_READ]);
	if (srv->wake[SP_WRITE] >= 0)
		close(srv->wake[SP_WRITE]);
	i = 0;
	while (i < LOBBY_MAX_ROOMS)
	{
		pthread_mutex_destroy(&srv->rooms[i].mutex);
		i++;
	}
	pthread_mutex_destroy(&srv->lobby_mutex);
	reg_destroy(&srv->reg);
	if (srv->db != NULL)
		db_close(srv->db);
	log_shutdown(&srv->log);
	free(srv);
}
