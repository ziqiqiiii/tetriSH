#include "coreipc.h"

/**
 * @brief Open or create a non-blocking POSIX message queue.
 *
 * @param name POSIX queue name, leading '/' and no other slashes
 *             (from .tetrishrc, never defaulted).
 * @param maxmsg Maximum messages the queue holds before sends fail.
 * @param msgsize Maximum size of one message, in bytes.
 * @param mode Permission bits for the created queue.
 * @return The open descriptor, or (mqd_t)-1 with errno set on failure.
 */
mqd_t	mqh_open(const char *name, long maxmsg, long msgsize, mode_t mode)
{
	/* TODO: fill mq_attr { O_NONBLOCK, mq_maxmsg, mq_msgsize }; mq_open with
	   O_CREAT | O_RDWR | O_NONBLOCK. */
	(void)name;
	(void)maxmsg;
	(void)msgsize;
	(void)mode;
	errno = ENOSYS;
	return ((mqd_t)-1);
}

/**
 * @brief Send one message without ever blocking.
 *
 * A full queue is EAGAIN, and the caller counts that drop.
 *
 * @param q An open queue descriptor.
 * @param msg The message to send.
 * @param len Length of msg; must not exceed the queue's msgsize.
 * @return 0 when the message was queued, -1 with errno set otherwise.
 */
int	mqh_send_nb(mqd_t q, const void *msg, size_t len)
{
	/* TODO: mq_send(q, msg, len, 0) — priority is always 0, so garbage
	   events stay strictly FIFO. */
	(void)q;
	(void)msg;
	(void)len;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Receive one message, returning immediately when the queue is empty.
 *
 * @param q An open queue descriptor.
 * @param buf Destination; must be at least the queue's mq_msgsize, or the
 *            call fails with EMSGSIZE rather than truncating.
 * @param buflen Capacity of buf in bytes.
 * @return Bytes received, or -1 with errno set (EAGAIN when empty).
 */
ssize_t	mqh_recv_nb(mqd_t q, void *buf, size_t buflen)
{
	/* TODO: mq_receive(q, buf, buflen, NULL); retry on EINTR; let EAGAIN
	   through as "queue empty". */
	(void)q;
	(void)buf;
	(void)buflen;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Receive one message, waiting up to timeout_ms for one to arrive.
 *
 * The only blocking call in the library: no lock may be held across it, and
 * callers holding a room mutex use mqh_recv_nb instead.
 *
 * @param q An open queue descriptor.
 * @param buf Destination; must be at least the queue's mq_msgsize.
 * @param buflen Capacity of buf in bytes.
 * @param timeout_ms Milliseconds to wait; 0 polls once.
 * @return Bytes received, or -1 with errno set (ETIMEDOUT when none arrived).
 */
ssize_t	mqh_recv_timed(mqd_t q, void *buf, size_t buflen, int timeout_ms)
{
	/* TODO: CLOCK_REALTIME + timeout_ms, normalising nsec; mq_timedreceive;
	   on EINTR retry against the same deadline, never an extended one. */
	(void)q;
	(void)buf;
	(void)buflen;
	(void)timeout_ms;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Close a queue descriptor.
 *
 * The queue itself outlives every descriptor until mqh_unlink removes it.
 *
 * @param q The descriptor to close.
 * @return 0 on success, -1 with errno set on failure.
 */
int	mqh_close(mqd_t q)
{
	/* TODO: mq_close(q). */
	(void)q;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Remove a queue name from the system.
 *
 * A POSIX queue is kernel-persistent, so a daemon that exits without
 * unlinking leaves messages behind for its next start to consume.
 *
 * @param name The POSIX queue name to remove.
 * @return 0 on success, -1 with errno set on failure.
 */
int	mqh_unlink(const char *name)
{
	/* TODO: mq_unlink(name), treating ENOENT as already gone. */
	(void)name;
	errno = ENOSYS;
	return (-1);
}
