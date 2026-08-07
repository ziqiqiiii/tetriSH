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
	return (0);
}

/**
 * @brief Publishes a client so it can be found by player and by walk.
 *
 * A client is registered as soon as it is accepted - before its handshake
 * finishes - so the connection limit is enforced before any crypto is spent on
 * it, and so a shutdown can reach a peer that never completes one.
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
	i = 0;
	while (i < rg->cap && rg->slots[i] != NULL)
		i++;
	if (i == rg->cap)
		return (-1);
	rg->slots[i] = cli;
	cli->index = (int)i;
	rg->count++;
	return (0);
}

/**
 * @brief Unlinks a client so nobody can start using it again.
 *
 * This says nothing about the client's memory, which is released later, from
 * the zombie list, once the batch of events that might still name it is over.
 *
 * @param rg Registry to remove from.
 * @param cli Client to unlink.
 */
void	registry_remove(t_registry *rg, t_client *cli)
{
	if (rg == NULL || cli == NULL || cli->index < 0)
		return ;
	if (rg->slots[cli->index] == cli)
	{
		rg->slots[cli->index] = NULL;
		rg->count--;
	}
	cli->index = -1;
}

/**
 * @brief Hands a serialised message to one player's outbox.
 *
 * Addressing a player rather than a connection is the point: the caller knows
 * whose game it is describing and not which socket that player is on. A client
 * whose response queue overflows is shut down - it cannot keep up, and
 * buffering more would let it exhaust the server.
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
	cli = find_by_player(rg, pid);
	if (cli == NULL)
		return (-1);
	if (is_state)
		rc = outbox_push_state(&cli->outbox, bytes, len);
	else
		rc = outbox_push(&cli->outbox, bytes, len);
	if (rc != 0 && cli->outbox.overflowed)
		shutdown(cli->fd, SHUT_RDWR);
	return (rc);
}

/**
 * @brief Publishes a connection's identity.
 *
 * A player id and the CLI_AUTHED state together are what every lookup matches
 * on, so they are set together and in that order - a connection is never
 * findable as a player it is not yet acting as.
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
	cli->player_id = pid;
	snprintf(cli->username, sizeof(cli->username), "%s", username);
	cli->state = CLI_AUTHED;
}

/**
 * @brief Moves a connection to a new state.
 *
 * @param rg Registry guarding the client.
 * @param cli Client whose state changes.
 * @param state The state to move to.
 */
void	registry_mark_state(t_registry *rg, t_client *cli, t_client_state state)
{
	if (rg == NULL || cli == NULL)
		return ;
	cli->state = state;
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
	i = 0;
	while (i < rg->cap && found == NULL)
	{
		if (rg->slots[i] != NULL && rg->slots[i] != keep
			&& rg->slots[i]->player_id == pid
			&& rg->slots[i]->state == CLI_AUTHED)
			found = rg->slots[i];
		i++;
	}
	return (found);
}

/**
 * @brief Copies every registered client into a caller-owned array.
 *
 * Copying the pointers out first is what lets the walk unlink clients as it
 * goes - flushing may end a connection, and so does shutdown - without
 * mutating the array it is iterating.
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
	i = 0;
	while (i < rg->cap && written < cap)
	{
		if (rg->slots[i] != NULL)
			out[written++] = rg->slots[i];
		i++;
	}
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
	online = find_by_player(rg, pid) != NULL;
	return (online);
}

/**
 * @brief Releases the registry's storage.
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
}

/**
 * @brief Finds the connection bound to a player.
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
