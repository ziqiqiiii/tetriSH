#include "tetrisd.h"

// Static Functions
static t_client	*find_by_player(t_registry *rg, t_player_id pid);
static size_t	count_now(t_registry *rg);
static bool		still_present(t_registry *rg, t_player_id pid,
					const t_client *keep);
static void		deadline_in(struct timespec *ts, int ms);

/**
 * @brief Creates an empty client registry of a fixed capacity.
 *
 * The capacity is the configured client limit: connections beyond it are
 * refused at accept time rather than allowed to exhaust memory.
 *
 * @param rg Registry to initialise.
 * @param cap Maximum simultaneous clients.
 * @return 0 on success, -1 on invalid capacity or allocation failure.
 */
int	registry_init(t_registry *rg, size_t cap)
{
	if (rg == NULL || cap == 0)
		return (-1);
	memset(rg, 0, sizeof(*rg));
	rg->slots = calloc(cap, sizeof(*rg->slots));
	if (rg->slots == NULL)
		return (-1);
	rg->cap = cap;
	if (pthread_rwlock_init(&rg->lock, NULL) != 0)
	{
		free(rg->slots);
		rg->slots = NULL;
		return (-1);
	}
	pthread_mutex_init(&rg->empty_mutex, NULL);
	pthread_cond_init(&rg->empty_cond, NULL);
	return (0);
}

/**
 * @brief Publishes a client so other threads may reach its outbox.
 *
 * A client is registered as soon as it is accepted - before its handshake
 * finishes - so that a shutdown can interrupt a peer that never completes it.
 *
 * @param rg Registry to add to.
 * @param cli Client to publish; its index is recorded on success.
 * @return 0 on success, -1 when the registry is full.
 */
int	registry_add(t_registry *rg, t_client *cli)
{
	size_t	i;

	if (rg == NULL || cli == NULL)
		return (-1);
	pthread_rwlock_wrlock(&rg->lock);
	i = 0;
	while (i < rg->cap && rg->slots[i] != NULL)
		i++;
	if (i == rg->cap)
	{
		pthread_rwlock_unlock(&rg->lock);
		return (-1);
	}
	rg->slots[i] = cli;
	cli->index = (int)i;
	rg->count++;
	pthread_rwlock_unlock(&rg->lock);
	return (0);
}

/**
 * @brief Unlinks a client so nobody can start using it again.
 *
 * This is the first step of teardown and the whole reason the registry holds
 * a lock: once the write lock is released the client is unreachable, so any
 * enqueuer that was mid-push has already finished under the read lock.
 *
 * The count drops and the waiters are woken under one hold of empty_mutex.
 * Signalling after releasing it would let a waiter observe an empty registry,
 * return, and destroy the condition variable while this thread was still
 * about to touch it.
 *
 * @param rg Registry to remove from.
 * @param cli Client to unlink.
 */
void	registry_remove(t_registry *rg, t_client *cli)
{
	if (rg == NULL || cli == NULL || cli->index < 0)
		return ;
	pthread_mutex_lock(&rg->empty_mutex);
	pthread_rwlock_wrlock(&rg->lock);
	if (rg->slots[cli->index] == cli)
	{
		rg->slots[cli->index] = NULL;
		rg->count--;
	}
	cli->index = -1;
	pthread_rwlock_unlock(&rg->lock);
	pthread_cond_broadcast(&rg->empty_cond);
	pthread_mutex_unlock(&rg->empty_mutex);
}

/**
 * @brief Hands a serialised message to one player's outbox.
 *
 * The read lock is held across the push, which is what makes this safe from
 * any thread: the client cannot be freed while the lock is held, and the push
 * itself never blocks. A client whose response queue overflows is shut down -
 * it cannot keep up, and buffering more would let it exhaust the server.
 *
 * @param rg Registry to look in.
 * @param pid Player the message is addressed to.
 * @param bytes Serialised message; owned by the outbox once accepted.
 * @param len Length of bytes.
 * @param is_state true for a STATE snapshot (mailbox), false for a response.
 * @return 0 when queued, -1 when the player is gone or the queue overflowed.
 */
int	registry_enqueue(t_registry *rg, t_player_id pid, unsigned char *bytes,
		size_t len, bool is_state)
{
	t_client	*cli;
	int			rc;

	if (rg == NULL || bytes == NULL)
		return (-1);
	pthread_rwlock_rdlock(&rg->lock);
	cli = find_by_player(rg, pid);
	if (cli == NULL)
	{
		pthread_rwlock_unlock(&rg->lock);
		return (-1);
	}
	if (is_state)
		rc = outbox_push_state(&cli->outbox, bytes, len);
	else
		rc = outbox_push(&cli->outbox, bytes, len);
	if (rc != 0 && atomic_load(&cli->outbox.overflowed))
		shutdown(cli->fd, SHUT_RDWR);
	pthread_rwlock_unlock(&rg->lock);
	return (rc);
}

/**
 * @brief Publishes a connection's identity, under the write lock.
 *
 * A player id and the CLI_AUTHED state are what every other thread matches on
 * when it looks for a connection, so they are written under the same lock
 * those readers hold. Doing it any other way is a data race: a ticker could
 * see CLI_AUTHED while the player id was still zero.
 *
 * @param rg Registry guarding the client.
 * @param cli Client being bound.
 * @param pid Player the connection now acts as.
 * @param username That player's name.
 */
void	registry_bind(t_registry *rg, t_client *cli, t_player_id pid,
		const char *username)
{
	if (rg == NULL || cli == NULL)
		return ;
	pthread_rwlock_wrlock(&rg->lock);
	cli->player_id = pid;
	snprintf(cli->username, sizeof(cli->username), "%s", username);
	cli->state = CLI_AUTHED;
	pthread_rwlock_unlock(&rg->lock);
}

/**
 * @brief Moves a connection to a new state, under the write lock.
 *
 * @param rg Registry guarding the client.
 * @param cli Client whose state changes.
 * @param state The state to move to.
 */
void	registry_mark_state(t_registry *rg, t_client *cli, t_client_state state)
{
	if (rg == NULL || cli == NULL)
		return ;
	pthread_rwlock_wrlock(&rg->lock);
	cli->state = state;
	pthread_rwlock_unlock(&rg->lock);
}

/**
 * @brief Closes any other connection already acting as a player.
 *
 * A player has at most one connection, so a fresh LOGIN takes the identity
 * back instead of being refused - a client that died without closing its
 * socket would otherwise lock its own account out until TCP gave up on it,
 * which can be hours. The displaced connection is only asked to stop here;
 * it tears itself down on its own thread, as every other connection does.
 *
 * @param rg Registry to search.
 * @param pid Player being claimed.
 * @param keep The claiming connection, which is never displaced.
 * @return true when a connection was displaced.
 */
bool	registry_displace(t_registry *rg, t_player_id pid, const t_client *keep)
{
	bool	found;
	size_t	i;

	if (rg == NULL)
		return (false);
	found = false;
	pthread_rwlock_wrlock(&rg->lock);
	i = 0;
	while (i < rg->cap)
	{
		if (rg->slots[i] != NULL && rg->slots[i] != keep
			&& rg->slots[i]->player_id == pid
			&& rg->slots[i]->state == CLI_AUTHED)
		{
			rg->slots[i]->state = CLI_CLOSING;
			shutdown(rg->slots[i]->fd, SHUT_RDWR);
			outbox_close(&rg->slots[i]->outbox);
			found = true;
		}
		i++;
	}
	pthread_rwlock_unlock(&rg->lock);
	return (found);
}

/**
 * @brief Waits for a displaced connection to finish tearing itself down.
 *
 * The wait is what makes displacement safe rather than merely quick. A
 * connection forfeits its room *before* it unlinks from the registry, so
 * once it is gone from here its forfeit has already run and cannot reach
 * into whatever room the new connection goes on to join.
 *
 * @param rg Registry to watch.
 * @param pid Player whose old connection is going away.
 * @param keep The claiming connection, which is not waited for.
 * @param timeout_ms How long to wait before giving up.
 * @return 0 once no other connection holds the player, -1 on timeout.
 */
int	registry_wait_absent(t_registry *rg, t_player_id pid, const t_client *keep,
		int timeout_ms)
{
	struct timespec	deadline;
	int				rc;

	if (rg == NULL)
		return (-1);
	deadline_in(&deadline, timeout_ms);
	rc = 0;
	pthread_mutex_lock(&rg->empty_mutex);
	while (rc == 0 && still_present(rg, pid, keep))
		rc = pthread_cond_timedwait(&rg->empty_cond, &rg->empty_mutex,
				&deadline);
	if (rc != 0 && still_present(rg, pid, keep))
		rc = -1;
	else
		rc = 0;
	pthread_mutex_unlock(&rg->empty_mutex);
	return (rc);
}

/**
 * @brief Shuts every connection down so its reader thread can finish.
 *
 * shutdown() rather than close(): the descriptor stays valid until its owning
 * thread tears the client down, so no other thread can race onto a reused fd.
 *
 * @param rg Registry whose clients are being stopped.
 */
void	registry_shutdown_all(t_registry *rg)
{
	size_t	i;

	if (rg == NULL)
		return ;
	pthread_rwlock_rdlock(&rg->lock);
	i = 0;
	while (i < rg->cap)
	{
		if (rg->slots[i] != NULL)
		{
			shutdown(rg->slots[i]->fd, SHUT_RDWR);
			outbox_close(&rg->slots[i]->outbox);
		}
		i++;
	}
	pthread_rwlock_unlock(&rg->lock);
}

/**
 * @brief Blocks until every client has torn itself down.
 *
 * Client threads are detached and free themselves, so shutdown waits on the
 * registry emptying rather than joining threads it does not own.
 *
 * @param rg Registry to wait on.
 */
void	registry_wait_empty(t_registry *rg)
{
	if (rg == NULL)
		return ;
	pthread_mutex_lock(&rg->empty_mutex);
	while (count_now(rg) > 0)
		pthread_cond_wait(&rg->empty_cond, &rg->empty_mutex);
	pthread_mutex_unlock(&rg->empty_mutex);
}

/**
 * @brief Answers whether a player still has a live connection.
 *
 * This is the liveness probe the room domain asks for: libtetrisroom never
 * touches a socket, so the question is answered here instead.
 *
 * @param rg Registry to look in.
 * @param pid Player to look for.
 * @return true when that player is connected and authenticated.
 */
bool	registry_player_online(t_registry *rg, t_player_id pid)
{
	bool	online;

	if (rg == NULL)
		return (false);
	pthread_rwlock_rdlock(&rg->lock);
	online = find_by_player(rg, pid) != NULL;
	pthread_rwlock_unlock(&rg->lock);
	return (online);
}

/**
 * @brief Releases the registry's storage and locks.
 *
 * @param rg Registry to destroy; must already be empty.
 */
void	registry_destroy(t_registry *rg)
{
	if (rg == NULL)
		return ;
	free(rg->slots);
	rg->slots = NULL;
	rg->cap = 0;
	pthread_rwlock_destroy(&rg->lock);
	pthread_mutex_destroy(&rg->empty_mutex);
	pthread_cond_destroy(&rg->empty_cond);
}

/**
 * @brief Finds the connection bound to a player, under the caller's lock.
 *
 * @param rg Registry to scan.
 * @param pid Player to look for.
 * @return The client, or NULL when that player is not connected.
 */
static t_client	*find_by_player(t_registry *rg, t_player_id pid)
{
	size_t	i;

	i = 0;
	while (i < rg->cap)
	{
		if (rg->slots[i] != NULL && rg->slots[i]->state == CLI_AUTHED
			&& rg->slots[i]->player_id == pid)
			return (rg->slots[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Reads the live client count.
 *
 * @param rg Registry to count.
 * @return Number of registered clients.
 */
static size_t	count_now(t_registry *rg)
{
	size_t	n;

	pthread_rwlock_rdlock(&rg->lock);
	n = rg->count;
	pthread_rwlock_unlock(&rg->lock);
	return (n);
}

/**
 * @brief Reports whether any connection other than keep still holds a player.
 *
 * State is deliberately not considered: a connection on its way out still
 * owns the player until it unlinks itself, and that is exactly the window the
 * caller is waiting through.
 *
 * @param rg Registry to scan.
 * @param pid Player to look for.
 * @param keep Connection to ignore.
 * @return true while another connection still holds that player.
 */
static bool	still_present(t_registry *rg, t_player_id pid, const t_client *keep)
{
	bool	present;
	size_t	i;

	present = false;
	pthread_rwlock_rdlock(&rg->lock);
	i = 0;
	while (i < rg->cap && !present)
	{
		present = rg->slots[i] != NULL && rg->slots[i] != keep
			&& rg->slots[i]->player_id == pid;
		i++;
	}
	pthread_rwlock_unlock(&rg->lock);
	return (present);
}

/**
 * @brief Builds an absolute deadline the condition variable can wait against.
 *
 * @param ts Receives the deadline.
 * @param ms Milliseconds from now.
 */
static void	deadline_in(struct timespec *ts, int ms)
{
	clock_gettime(CLOCK_REALTIME, ts);
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (long)(ms % 1000) * 1000000L;
	if (ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}
