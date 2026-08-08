#include "tetrisd.h"

// Static Functions
static void	free_msg(t_outbound_message *msg);
static bool	take_next(t_outbox *ob, t_outbound_message *out);

/**
 * @brief Prepares an empty outbox for one client.
 *
 * @param ob Outbox to initialise.
 * @return 0 on success, -1 when ob is NULL.
 */
int	outbox_init(t_outbox *ob)
{
	if (ob == NULL)
		return (-1);
	memset(ob, 0, sizeof(*ob));
	return (0);
}

/**
 * @brief Queues one serialised response for the reactor to seal and write.
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
	if (ob->closed || ob->count == TETRISD_OUTBOX_CAPACITY)
	{
		if (!ob->closed)
			ob->overflowed = true;
		return (-1);
	}
	slot = (ob->head + ob->count) % TETRISD_OUTBOX_CAPACITY;
	ob->slots[slot].bytes = bytes;
	ob->slots[slot].len = len;
	ob->count++;
	return (0);
}

/**
 * @brief Leaves the latest STATE snapshot in the client's one-slot mailbox.
 *
 * A snapshot supersedes the one before it, so a slow client loses
 * intermediate frames rather than holding up the tick that produced them.
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
	if (ob->closed)
		return (-1);
	free_msg(&ob->state);
	ob->state.bytes = bytes;
	ob->state.len = len;
	ob->state_pending = true;
	return (0);
}

/**
 * @brief Takes the next message to send, or reports that there is none.
 *
 * Queued responses go out before the STATE mailbox: a request_reply is part of
 * a request the client is waiting on, while a snapshot is only ever the latest
 * truth. An empty outbox and a closed one are the same answer. The caller owns
 * the returned bytes.
 *
 * @param ob Outbox to take from.
 * @param out Receives the message.
 * @return 0 when a message was taken, -1 when nothing was waiting.
 */
int	outbox_pop(t_outbox *ob, t_outbound_message *out)
{
	if (ob == NULL || out == NULL)
		return (-1);
	if (!take_next(ob, out))
		return (-1);
	return (0);
}

/**
 * @brief Reports whether an outbox has nothing waiting to go out.
 *
 * The reactor sweeps every client after a tick, and asking is far cheaper than
 * sealing a frame to discover there was nothing to seal.
 *
 * @param ob Outbox to inspect.
 * @return true when neither a response nor a snapshot is pending.
 */
bool	outbox_idle(t_outbox *ob)
{
	bool	idle;

	if (ob == NULL)
		return (true);
	idle = ob->count == 0 && !ob->state_pending;
	return (idle);
}

/**
 * @brief Closes the outbox and drops whatever it was still holding.
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
}

/**
 * @brief Releases any bytes the outbox was still holding.
 *
 * @param ob Outbox to destroy.
 */
void	outbox_destroy(t_outbox *ob)
{
	if (ob == NULL)
		return ;
	outbox_close(ob);
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
 * @brief Moves the next message out of the outbox.
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
