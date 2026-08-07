#include "tetrisd.h"

// Static Functions
static void	dispatch(t_server *srv, struct epoll_event *event);
static void	accept_ready(t_server *srv);
static void	wake_ready(t_server *srv);
static void	take_handshakes(t_server *srv);
static void	sweep(t_server *srv);
static void	wind_down(t_server *srv);

/**
 * @brief The event loop, and the single owner of every established connection.
 *
 * One pass is: wait, handle every descriptor the kernel reported, write out
 * whatever a room ticker enqueued while we were waiting, then free the clients
 * this pass ended. That last step is the ordering rule the whole design rests
 * on - a batch may carry several events for the same client, so nothing may be
 * released until every event in it has been looked at (docs/adr/0008).
 *
 * The wait is unbounded unless a handshake is in flight, in which case it ends
 * at that handshake's deadline. Gravity is not what wakes this loop yet; the
 * room tickers are still threads and poke the pipe (step 4 changes that).
 *
 * It returns only once it has torn the server down, because it is the thread
 * that owns what has to be torn down.
 *
 * @param srv Server to run; must be fully brought up.
 */
void	reactor_run(t_server *srv)
{
	struct epoll_event	events[TETRISD_EPOLL_BATCH];
	int					ready;
	int					i;

	while (atomic_load(&srv->running))
	{
		ready = epoll_wait(srv->epoll_fd, events, TETRISD_EPOLL_BATCH,
				handshake_pool_expire(&srv->pool));
		if (ready < 0 && errno == EINTR)
			continue ;
		if (ready < 0)
		{
			logger_emit(&srv->log, COREIPC_LOG_ERROR, "epoll_wait failed: %s",
				strerror(errno));
			break ;
		}
		i = 0;
		while (i < ready)
			dispatch(srv, &events[i++]);
		if (srv->sweep_due)
			sweep(srv);
		client_reap(srv);
	}
	wind_down(srv);
}

/**
 * @brief Routes one epoll event to whatever the kernel handed back.
 *
 * The tag is read before the pointer is used as a client, which is the whole
 * reason data.ptr can carry three different kinds of object. A client killed
 * earlier in this same batch is skipped rather than touched - it is still
 * allocated, but it no longer owns anything.
 *
 * @param srv Server the event belongs to.
 * @param event One ready event from epoll_wait.
 */
static void	dispatch(t_server *srv, struct epoll_event *event)
{
	t_event_tag	*tag;
	t_client	*cli;

	tag = event->data.ptr;
	if (tag->source == EVENT_LISTENER)
		return (accept_ready(srv));
	if (tag->source == EVENT_WAKE)
		return (wake_ready(srv));
	cli = event->data.ptr;
	if (cli->dead)
		return ;
	if (event->events & EPOLLOUT)
		client_flush(cli);
	if (!cli->dead && (event->events & (EPOLLIN | EPOLLHUP | EPOLLERR)))
		client_readable(cli);
}

/**
 * @brief Accepts every connection currently pending on the listener.
 *
 * @param srv Server whose listener is ready.
 */
static void	accept_ready(t_server *srv)
{
	int	fd;

	fd = listener_accept(srv->listen_fd);
	while (fd >= 0)
	{
		if (client_spawn(srv, fd) != 0)
			logger_emit(&srv->log, COREIPC_LOG_WARNING,
				"refused a connection: client limit reached");
		if (!atomic_load(&srv->running))
			return ;
		fd = listener_accept(srv->listen_fd);
	}
}

/**
 * @brief Handles everything that reaches the loop through the wake pipe.
 *
 * Signals, finished handshakes and enqueued STATE pushes share it because all
 * three are "something happened off this thread"; one drain covers however
 * many of them arrived. It is also the only way bytes reach an outbox without
 * this thread putting them there, which is why it, and nothing else, arms the
 * sweep.
 *
 * @param srv Server that was woken.
 */
static void	wake_ready(t_server *srv)
{
	selfpipe_drain(srv->wake[SELFPIPE_READ]);
	srv->sweep_due = true;
	if (signals_take_stop())
		atomic_store(&srv->running, false);
	if (signals_take_reload())
		server_reload(srv);
	if (signals_take_state_dump())
		server_state_dump(srv);
	take_handshakes(srv);
}

/**
 * @brief Adopts every connection whose handshake has finished.
 *
 * @param srv Server whose pool is drained.
 */
static void	take_handshakes(t_server *srv)
{
	t_client	*cli;

	while (handshake_pool_take(&srv->pool, &cli) == 0)
	{
		if (cli->handshake_ok && atomic_load(&srv->running))
			client_adopt(cli);
		else
			client_kill(cli);
	}
}

/**
 * @brief Writes out whatever was enqueued from off the loop, and ends overflows.
 *
 * Room tickers still run on their own threads until step 4, so bytes can
 * appear in an outbox without any descriptor becoming ready. The ticker pokes
 * the wake pipe and this sweep is what turns that into a write. A client whose
 * response queue overflowed is ended here rather than left to be noticed later
 * - it cannot keep up, and buffering more would let it exhaust the server.
 *
 * It runs only after a wake, never after ordinary client traffic: a reply this
 * thread produced was already written by client_readable, so sweeping every
 * batch would walk the whole registry for nothing.
 *
 * @param srv Server whose clients are swept.
 */
static void	sweep(t_server *srv)
{
	t_client	*cli;
	size_t		count;
	size_t		i;

	srv->sweep_due = false;
	count = registry_snapshot(&srv->reg, srv->sweep,
			(size_t)srv->cfg.max_clients);
	i = 0;
	while (i < count)
	{
		cli = srv->sweep[i++];
		if (cli->dead || !cli->watched)
			continue ;
		if (atomic_load(&cli->outbox.overflowed))
			client_kill(cli);
		else if (!outbox_idle(&cli->outbox))
			client_flush(cli);
	}
}

/**
 * @brief Tears the whole server down, on the thread that owns all of it.
 *
 * The order is the safety argument. The handshake pool stops first, so no new
 * client can arrive; the room tickers stop next, so nothing is still
 * enqueueing into an outbox; only then are the connections ended and freed.
 * Doing it the other way round - the order the threaded server used - would
 * leave a ticker holding a pointer to a client that had just been released.
 *
 * @param srv Server to wind down.
 */
static void	wind_down(t_server *srv)
{
	t_client	*cli;
	size_t		count;
	size_t		i;

	atomic_store(&srv->stopping, true);
	atomic_store(&srv->running, false);
	handshake_pool_stop(&srv->pool);
	while (handshake_pool_take(&srv->pool, &cli) == 0)
		client_kill(cli);
	server_room_stop_all(srv);
	count = registry_snapshot(&srv->reg, srv->sweep,
			(size_t)srv->cfg.max_clients);
	i = 0;
	while (i < count)
		client_kill(srv->sweep[i++]);
	client_reap(srv);
}
