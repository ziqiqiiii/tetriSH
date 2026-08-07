#include "tetrisd.h"

// Static Functions
static t_client	*find_by_player(t_registry *rg, t_player_id pid);

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
 * This is the whole reason the registry still holds a lock: once the write
 * lock is released the client is unreachable from a room ticker, so any
 * enqueuer that was mid-push has already finished under the read lock. It says
 * nothing about the client's memory, which the reactor frees later, on its own
 * thread, from the zombie list.
 *
 * @param rg Registry to remove from.
 * @param cli Client to unlink.
 */
void	registry_remove(t_registry *rg, t_client *cli)
{
	if (rg == NULL || cli == NULL || cli->index < 0)
		return ;
	pthread_rwlock_wrlock(&rg->lock);
	if (rg->slots[cli->index] == cli)
	{
		rg->slots[cli->index] = NULL;
		rg->count--;
	}
	cli->index = -1;
	pthread_rwlock_unlock(&rg->lock);
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
 * A player id and the CLI_AUTHED state are what a room ticker matches on when
 * it looks for a connection, so they are written under the same lock those
 * readers hold. Doing it any other way is a data race: a ticker could see
 * CLI_AUTHED while the player id was still zero.
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
 * @brief Finds another connection already acting as a player.
 *
 * A player has at most one connection, so a fresh LOGIN takes the identity
 * back instead of being refused - a client that died without closing its
 * socket would otherwise lock its own account out until TCP gave up on it,
 * which can be hours (ADR-0004).
 *
 * Only the answer is given here. The reactor owns both connections, so it ends
 * the old one itself, synchronously, before the new one binds - which is why
 * the wait that used to be needed between two client threads is gone rather
 * than ported (docs/adr/0008).
 *
 * @param rg Registry to search.
 * @param pid Player being claimed.
 * @param keep The claiming connection, which is never returned.
 * @return The other connection holding that player, or NULL when there is none.
 */
t_client	*registry_find_other(t_registry *rg, t_player_id pid,
		const t_client *keep)
{
	t_client	*found;
	size_t		i;

	if (rg == NULL)
		return (NULL);
	found = NULL;
	pthread_rwlock_rdlock(&rg->lock);
	i = 0;
	while (i < rg->cap && found == NULL)
	{
		if (rg->slots[i] != NULL && rg->slots[i] != keep
			&& rg->slots[i]->player_id == pid
			&& rg->slots[i]->state == CLI_AUTHED)
			found = rg->slots[i];
		i++;
	}
	pthread_rwlock_unlock(&rg->lock);
	return (found);
}

/**
 * @brief Copies every registered client into a caller-owned array.
 *
 * The reactor needs to walk every connection - to flush what a room ticker
 * enqueued, or to end them all at shutdown - and neither can happen under the
 * read lock, because both may unlink a client. Copying the pointers out first
 * is safe because only the reactor adds and removes them, so the list cannot
 * go stale while it is being used.
 *
 * @param rg Registry to snapshot.
 * @param out Array receiving the clients.
 * @param cap Length of out.
 * @return Number of clients written.
 */
size_t	registry_snapshot(t_registry *rg, t_client **out, size_t cap)
{
	size_t	written;
	size_t	i;

	if (rg == NULL || out == NULL)
		return (0);
	written = 0;
	pthread_rwlock_rdlock(&rg->lock);
	i = 0;
	while (i < rg->cap && written < cap)
	{
		if (rg->slots[i] != NULL)
			out[written++] = rg->slots[i];
		i++;
	}
	pthread_rwlock_unlock(&rg->lock);
	return (written);
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
