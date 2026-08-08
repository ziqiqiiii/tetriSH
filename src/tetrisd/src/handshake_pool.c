#include "tetrisd.h"

// Static Functions
static int		spawn_workers(t_handshake_pool *pool, int wanted);
static void		*worker_main(void *arg);
static t_client	*take_queued(t_handshake_pool *pool);
static void		run_handshake(t_handshake_pool *pool, t_client *cli, bool live);
static int		set_recv_deadline(int fd, int timeout_ms);
static int		expire_one(t_client *cli, uint64_t now, int soonest);

/**
 * @brief Starts the pool of workers that run the secure handshake.
 *
 * The handshake is the one thing in tetrisd that genuinely blocks: six ordered
 * I/O steps around two RSA operations, waiting on a nonce from a peer that has
 * not authenticated itself yet. Putting it on the reactor would stutter every
 * running game, so it stays on threads that are allowed to wait (ADR-0008).
 *
 * The queues are sized to the client limit because a client is registered
 * before it is submitted, so no more than that many can ever be in flight.
 *
 * @param pool Pool to start.
 * @param srv Server owning the credentials, the logger, and the wake pipe.
 * @return 0 when at least one worker is running, -1 otherwise.
 */
int	handshake_pool_start(t_handshake_pool *pool, t_server *srv)
{
	if (pool == NULL || srv == NULL)
		return (-1);
	memset(pool, 0, sizeof(*pool));
	pool->srv = srv;
	pool->cap = (size_t)srv->cfg.max_clients;
	if (pthread_mutex_init(&pool->mutex, NULL) != 0)
		return (-1);
	if (pthread_cond_init(&pool->cond, NULL) != 0)
	{
		pthread_mutex_destroy(&pool->mutex);
		return (-1);
	}
	pool->ready = true;
	pool->queue = calloc(pool->cap, sizeof(*pool->queue));
	pool->done = calloc(pool->cap, sizeof(*pool->done));
	if (pool->queue == NULL || pool->done == NULL)
		return (-1);
	pool->running = true;
	return (spawn_workers(pool, srv->cfg.handshake_workers));
}

/**
 * @brief Hands an accepted client to the next free worker.
 *
 * @param pool Pool to submit to.
 * @param cli Client whose handshake has not run yet.
 * @return 0 when queued, -1 when the pool is stopping or its queue is full.
 */
int	handshake_pool_submit(t_handshake_pool *pool, t_client *cli)
{
	int	rc;

	if (pool == NULL || cli == NULL || !pool->ready)
		return (-1);
	rc = -1;
	pthread_mutex_lock(&pool->mutex);
	if (pool->running && pool->queue_count < pool->cap)
	{
		pool->queue[(pool->queue_head + pool->queue_count) % pool->cap] = cli;
		pool->queue_count++;
		pthread_cond_signal(&pool->cond);
		rc = 0;
	}
	pthread_mutex_unlock(&pool->mutex);
	return (rc);
}

/**
 * @brief Collects one client whose handshake has finished, either way.
 *
 * A failed handshake comes back too, with handshake_ok clear: the reactor
 * allocated the client and the reactor ends it, so a worker never frees one.
 *
 * @param pool Pool to take from.
 * @param out Receives the client.
 * @return 0 when a client was taken, -1 when none had finished.
 */
int	handshake_pool_take(t_handshake_pool *pool, t_client **out)
{
	if (pool == NULL || out == NULL || !pool->ready)
		return (-1);
	pthread_mutex_lock(&pool->mutex);
	if (pool->done_count == 0)
	{
		pthread_mutex_unlock(&pool->mutex);
		return (-1);
	}
	*out = pool->done[pool->done_head];
	pool->done_head = (pool->done_head + 1) % pool->cap;
	pool->done_count--;
	pthread_mutex_unlock(&pool->mutex);
	return (0);
}

/**
 * @brief Shuts down any handshake that has run out of time, and says when next.
 *
 * The socket deadline alone is not enough. SO_RCVTIMEO bounds one recv, and
 * libtetrissh loops until it has the bytes it asked for, so a peer that sends
 * one byte just before every timeout keeps a worker forever without ever
 * tripping it. Shutting the socket down is what makes the worker's next read
 * return: the worker is blocked inside the handshake and cannot check a clock.
 *
 * The budget runs from the moment a worker picks the connection up, not from
 * when it was accepted. Charging queue time to the client would mean that
 * whoever queues behind an attacker inherits whatever the attacker left over -
 * so a full pool would start refusing honest logins, which is the outcome this
 * exists to prevent. A queued client holds no worker; the pool's own bounded
 * handshakes are what bound its wait.
 *
 * The returned wait is what the reactor gives epoll_wait, so a server with
 * nothing handshaking still sleeps indefinitely.
 *
 * @param pool Pool to sweep.
 * @return Milliseconds until the earliest remaining deadline, or -1 when no
 *         handshake is running.
 */
int	handshake_pool_expire(t_handshake_pool *pool)
{
	uint64_t	now;
	int			soonest;
	int			i;

	if (pool == NULL || !pool->ready)
		return (-1);
	soonest = -1;
	now = clock_now_ms();
	pthread_mutex_lock(&pool->mutex);
	i = 0;
	while (i < pool->worker_count)
	{
		if (pool->busy[i] != NULL)
			soonest = expire_one(pool->busy[i], now, soonest);
		i++;
	}
	pthread_mutex_unlock(&pool->mutex);
	return (soonest);
}

/**
 * @brief Stops every worker and waits for each to finish.
 *
 * Sockets are shut down before the workers are joined, so a peer that
 * connected and went quiet does not hold shutdown open for its whole deadline.
 * A worker that finds the pool stopped still drains what is queued - it
 * reports each as failed without touching the socket - so nothing is left
 * behind for the caller to find.
 *
 * @param pool Pool to stop; safe when it never started.
 */
void	handshake_pool_stop(t_handshake_pool *pool)
{
	int	i;

	if (pool == NULL || !pool->ready)
		return ;
	pthread_mutex_lock(&pool->mutex);
	pool->running = false;
	i = 0;
	while (i < pool->worker_count)
	{
		if (pool->busy[i] != NULL)
			shutdown(pool->busy[i]->fd, SHUT_RDWR);
		i++;
	}
	i = 0;
	while ((size_t)i < pool->queue_count)
	{
		shutdown(pool->queue[(pool->queue_head + (size_t)i)
			% pool->cap]->fd, SHUT_RDWR);
		i++;
	}
	pthread_cond_broadcast(&pool->cond);
	pthread_mutex_unlock(&pool->mutex);
	i = 0;
	while (i < pool->worker_count)
		pthread_join(pool->workers[i++], NULL);
	pool->worker_count = 0;
}

/**
 * @brief Releases the pool's queues and synchronisation primitives.
 *
 * @param pool Pool to destroy; must already have been stopped and drained.
 */
void	handshake_pool_destroy(t_handshake_pool *pool)
{
	if (pool == NULL || !pool->ready)
		return ;
	handshake_pool_stop(pool);
	free(pool->queue);
	free(pool->done);
	pool->queue = NULL;
	pool->done = NULL;
	pthread_cond_destroy(&pool->cond);
	pthread_mutex_destroy(&pool->mutex);
	pool->ready = false;
}

/**
 * @brief Creates the worker threads, stopping at the first refusal.
 *
 * @param pool Pool the workers belong to.
 * @param wanted How many workers the configuration asked for.
 * @return 0 when at least one worker started, -1 when none did.
 */
static int	spawn_workers(t_handshake_pool *pool, int wanted)
{
	int	i;

	if (wanted > TETRISD_HANDSHAKE_WORKERS_MAX)
		wanted = TETRISD_HANDSHAKE_WORKERS_MAX;
	i = 0;
	while (i < wanted)
	{
		pool->seats[i].pool = pool;
		pool->seats[i].index = i;
		if (pthread_create(&pool->workers[i], NULL, worker_main,
				&pool->seats[i]) != 0)
			break ;
		i++;
		pool->worker_count = i;
	}
	if (pool->worker_count == 0)
		return (-1);
	return (0);
}

/**
 * @brief Worker thread: one handshake at a time, then hand the client back.
 *
 * @param arg This worker's seat in the pool.
 * @return Always NULL.
 */
static void	*worker_main(void *arg)
{
	t_handshake_seat	*seat;
	t_handshake_pool	*pool;
	t_client			*cli;
	bool				live;

	seat = arg;
	pool = seat->pool;
	while (true)
	{
		pthread_mutex_lock(&pool->mutex);
		while (pool->running && pool->queue_count == 0)
			pthread_cond_wait(&pool->cond, &pool->mutex);
		if (pool->queue_count == 0)
			break ;
		live = pool->running;
		cli = take_queued(pool);
		cli->handshake_deadline_ms = clock_now_ms()
			+ (uint64_t)pool->srv->cfg.handshake_timeout_ms;
		cli->handshake_expired = false;
		pool->busy[seat->index] = cli;
		pthread_mutex_unlock(&pool->mutex);
		run_handshake(pool, cli, live);
		pthread_mutex_lock(&pool->mutex);
		pool->busy[seat->index] = NULL;
		pool->done[(pool->done_head + pool->done_count) % pool->cap] = cli;
		pool->done_count++;
		pthread_mutex_unlock(&pool->mutex);
		server_wake(pool->srv);
	}
	pthread_mutex_unlock(&pool->mutex);
	return (NULL);
}

/**
 * @brief Removes the client at the head of the queue, under the caller's lock.
 *
 * @param pool Pool to take from; its queue must not be empty.
 * @return The client that waited longest.
 */
static t_client	*take_queued(t_handshake_pool *pool)
{
	t_client	*cli;

	cli = pool->queue[pool->queue_head];
	pool->queue_head = (pool->queue_head + 1) % pool->cap;
	pool->queue_count--;
	return (cli);
}

/**
 * @brief Runs one handshake against a deadline and records whether it worked.
 *
 * @param pool Pool holding the server's credentials and logger.
 * @param cli Client to authenticate to.
 * @param live false when the pool is already stopping, so nothing is attempted.
 */
static void	run_handshake(t_handshake_pool *pool, t_client *cli, bool live)
{
	cli->handshake_ok = false;
	if (!live)
		return ;
	if (set_recv_deadline(cli->fd, pool->srv->cfg.handshake_timeout_ms) != 0)
		return ;
	if (session_handshake_server(cli->fd, &cli->sess,
			pool->srv->credentials) != 0)
	{
		logger_emit(&pool->srv->log, COREIPC_LOG_WARNING,
			"handshake failed on fd %d", cli->fd);
		return ;
	}
	cli->handshake_ok = true;
}

/**
 * @brief Puts a per-read and per-write deadline on a socket being handshaken.
 *
 * This bounds one blocking call, which is the common case: a peer that
 * connects and says nothing releases its worker here without the reactor
 * having to intervene. It is not the whole guarantee - handshake_pool_expire
 * is - but it keeps the ordinary case off the reactor's plate.
 *
 * @param fd Socket the handshake will run over.
 * @param timeout_ms How long a single read or write may take.
 * @return 0 on success, -1 when the option could not be set.
 */
static int	set_recv_deadline(int fd, int timeout_ms)
{
	struct timeval	tv;

	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
		return (-1);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Expires one in-flight handshake, or folds it into the earliest wait.
 *
 * A connection is shut down once, and then ignored: its worker is already on
 * its way to reporting the failure, so keeping it in the wait would spin the
 * reactor on a deadline that can never move.
 *
 * @param cli Client whose handshake is being timed.
 * @param now The current monotonic-ish millisecond reading.
 * @param soonest The earliest wait found so far, or -1.
 * @return The earliest wait including this client.
 */
static int	expire_one(t_client *cli, uint64_t now, int soonest)
{
	int	remaining;

	if (cli->handshake_expired)
		return (soonest);
	if (now >= cli->handshake_deadline_ms)
	{
		cli->handshake_expired = true;
		shutdown(cli->fd, SHUT_RDWR);
		return (soonest);
	}
	remaining = (int)(cli->handshake_deadline_ms - now);
	if (soonest < 0 || remaining < soonest)
		return (remaining);
	return (soonest);
}
