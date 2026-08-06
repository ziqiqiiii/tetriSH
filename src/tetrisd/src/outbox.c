#include "tetrisd.h"

// Static Functions
static void	free_msg(t_outbound_message *msg);
static bool	take_next(t_outbox *ob, t_outbound_message *out);

/**
 * @brief Prepares an empty outbox for one client.
 *
 * @param ob Outbox to initialise.
 * @return 0 on success, -1 when the mutex or condition variable failed.
 */
int	outbox_init(t_outbox *ob)
{
	if (ob == NULL)
		return (-1);
	memset(ob, 0, sizeof(*ob));
	if (pthread_mutex_init(&ob->mutex, NULL) != 0)
		return (-1);
	if (pthread_cond_init(&ob->cond, NULL) != 0)
	{
		pthread_mutex_destroy(&ob->mutex);
		return (-1);
	}
	return (0);
}

/**
 * @brief Queues one serialised response for the client's writer thread.
 *
 * The queue is bounded on purpose: a client that stops reading must not be
 * able to grow the server's memory, so overflow is reported and the caller
 * closes the connection. Ownership of the bytes transfers only on success.
 *
 * @param ob Outbox to push into.
 * @param bytes Serialised message; freed by the outbox once accepted.
 * @param len Length of bytes.
 * @return 0 when queued, -1 when the outbox is closed or full.
 */
int	outbox_push(t_outbox *ob, unsigned char *bytes, size_t len)
{
	size_t	slot;

	if (ob == NULL || bytes == NULL)
		return (-1);
	pthread_mutex_lock(&ob->mutex);
	if (ob->closed || ob->count == TETRISD_OUTBOX_CAPACITY)
	{
		if (!ob->closed)
			atomic_store(&ob->overflowed, true);
		pthread_mutex_unlock(&ob->mutex);
		return (-1);
	}
	slot = (ob->head + ob->count) % TETRISD_OUTBOX_CAPACITY;
	ob->slots[slot].bytes = bytes;
	ob->slots[slot].len = len;
	ob->count++;
	pthread_cond_signal(&ob->cond);
	pthread_mutex_unlock(&ob->mutex);
	return (0);
}

/**
 * @brief Leaves the latest STATE snapshot in the client's one-slot mailbox.
 *
 * A snapshot supersedes the one before it, so a slow client loses
 * intermediate frames rather than making a room's ticker wait - and the
 * ticker never blocks on anybody's socket.
 *
 * @param ob Outbox to mail into.
 * @param bytes Serialised STATE push; freed by the outbox once accepted.
 * @param len Length of bytes.
 * @return 0 when mailed, -1 when the outbox is closed.
 */
int	outbox_push_state(t_outbox *ob, unsigned char *bytes, size_t len)
{
	if (ob == NULL || bytes == NULL)
		return (-1);
	pthread_mutex_lock(&ob->mutex);
	if (ob->closed)
	{
		pthread_mutex_unlock(&ob->mutex);
		return (-1);
	}
	free_msg(&ob->state);
	ob->state.bytes = bytes;
	ob->state.len = len;
	ob->state_pending = true;
	pthread_cond_signal(&ob->cond);
	pthread_mutex_unlock(&ob->mutex);
	return (0);
}

/**
 * @brief Waits for the next message to send, blocking while the outbox is idle.
 *
 * Queued responses go out before the STATE mailbox: a request_reply is part of a
 * request the client is waiting on, while a snapshot is only ever the latest
 * truth. The caller owns the returned bytes.
 *
 * @param ob Outbox to take from.
 * @param out Receives the message.
 * @return 0 when a message was taken, -1 once the outbox is closed and empty.
 */
int	outbox_pop(t_outbox *ob, t_outbound_message *out)
{
	if (ob == NULL || out == NULL)
		return (-1);
	pthread_mutex_lock(&ob->mutex);
	while (!ob->closed && ob->count == 0 && !ob->state_pending)
		pthread_cond_wait(&ob->cond, &ob->mutex);
	if (!take_next(ob, out))
	{
		pthread_mutex_unlock(&ob->mutex);
		return (-1);
	}
	pthread_mutex_unlock(&ob->mutex);
	return (0);
}

/**
 * @brief Closes the outbox and wakes the writer thread.
 *
 * Everything still queued is dropped: the connection is going away, so the
 * bytes have nowhere to go and holding them would leak.
 *
 * @param ob Outbox to close.
 */
void	outbox_close(t_outbox *ob)
{
	size_t	i;

	if (ob == NULL)
		return ;
	pthread_mutex_lock(&ob->mutex);
	ob->closed = true;
	i = 0;
	while (i < ob->count)
	{
		free_msg(&ob->slots[(ob->head + i) % TETRISD_OUTBOX_CAPACITY]);
		i++;
	}
	ob->count = 0;
	ob->head = 0;
	free_msg(&ob->state);
	ob->state_pending = false;
	pthread_cond_broadcast(&ob->cond);
	pthread_mutex_unlock(&ob->mutex);
}

/**
 * @brief Releases the outbox's lock, condition variable, and any bytes left.
 *
 * @param ob Outbox to destroy; must have no waiters left.
 */
void	outbox_destroy(t_outbox *ob)
{
	if (ob == NULL)
		return ;
	outbox_close(ob);
	pthread_cond_destroy(&ob->cond);
	pthread_mutex_destroy(&ob->mutex);
}

/**
 * @brief Frees one message's bytes and blanks the slot.
 *
 * @param msg Message slot to clear.
 */
static void	free_msg(t_outbound_message *msg)
{
	free(msg->bytes);
	msg->bytes = NULL;
	msg->len = 0;
}

/**
 * @brief Moves the next message out of the outbox, under the caller's lock.
 *
 * @param ob Outbox to take from.
 * @param out Receives the message.
 * @return true when a message was taken, false when there was nothing left.
 */
static bool	take_next(t_outbox *ob, t_outbound_message *out)
{
	if (ob->count > 0)
	{
		*out = ob->slots[ob->head];
		ob->slots[ob->head].bytes = NULL;
		ob->slots[ob->head].len = 0;
		ob->head = (ob->head + 1) % TETRISD_OUTBOX_CAPACITY;
		ob->count--;
		return (true);
	}
	if (ob->state_pending)
	{
		*out = ob->state;
		ob->state.bytes = NULL;
		ob->state.len = 0;
		ob->state_pending = false;
		return (true);
	}
	return (false);
}
