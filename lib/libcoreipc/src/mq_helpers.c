#include "coreipc.h"

#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <time.h>

// Static Functions
static void	deadline_from_now(struct timespec *ts, int timeout_ms);
static int	remaining_ms(const struct timespec *deadline);
static int	wait_readable(mqd_t q, int timeout_ms);

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
	struct mq_attr	attr;

	if (!name || name[0] != '/' || maxmsg <= 0 || msgsize <= 0)
	{
		errno = EINVAL;
		return ((mqd_t)-1);
	}
	memset(&attr, 0, sizeof(attr));
	attr.mq_flags = O_NONBLOCK;
	attr.mq_maxmsg = maxmsg;
	attr.mq_msgsize = msgsize;
	return (mq_open(name, O_CREAT | O_RDWR | O_NONBLOCK, mode, &attr));
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
	if (!msg)
	{
		errno = EINVAL;
		return (-1);
	}
	return (mq_send(q, (const char *)msg, len, 0));
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
	ssize_t	n;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	n = mq_receive(q, (char *)buf, buflen, NULL);
	while (n == -1 && errno == EINTR)
		n = mq_receive(q, (char *)buf, buflen, NULL);
	return (n);
}

/**
 * @brief Receive one message, waiting up to timeout_ms for one to arrive.
 *
 * The only blocking call in the library: no lock may be held across it, and
 * callers holding a room mutex use mqh_recv_nb instead. The wait is on the
 * descriptor rather than in mq_timedreceive, because mqh_open leaves the
 * queue non-blocking - a timed receive on it would return at once. Toggling
 * O_NONBLOCK instead would race with any concurrent mqh_recv_nb. The deadline
 * is absolute, so an interrupted wait never extends it.
 *
 * @param q An open queue descriptor.
 * @param buf Destination; must be at least the queue's mq_msgsize.
 * @param buflen Capacity of buf in bytes.
 * @param timeout_ms Milliseconds to wait; 0 polls once.
 * @return Bytes received, or -1 with errno set (ETIMEDOUT when none arrived).
 */
ssize_t	mqh_recv_timed(mqd_t q, void *buf, size_t buflen, int timeout_ms)
{
	struct timespec	deadline;
	ssize_t			n;
	int				ready;

	if (!buf)
	{
		errno = EINVAL;
		return (-1);
	}
	deadline_from_now(&deadline, timeout_ms);
	while (1)
	{
		ready = wait_readable(q, remaining_ms(&deadline));
		if (ready == -1)
			return (-1);
		if (ready == 0)
			continue ;
		n = mq_receive(q, (char *)buf, buflen, NULL);
		if (n >= 0)
			return (n);
		if (errno != EAGAIN && errno != EINTR)
			return (-1);
	}
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
	return (mq_close(q));
}

/**
 * @brief Remove a queue name from the system.
 *
 * A POSIX queue is kernel-persistent, so a daemon that exits without
 * unlinking leaves messages behind for its next start to consume. Removing a
 * name that is already gone is reported as -1 with ENOENT, so a caller can
 * tell "I removed it" from "it was not there".
 *
 * @param name The POSIX queue name to remove.
 * @return 0 on success, -1 with errno set on failure.
 */
int	mqh_unlink(const char *name)
{
	if (!name)
	{
		errno = EINVAL;
		return (-1);
	}
	return (mq_unlink(name));
}

/**
 * @brief Compute an absolute monotonic deadline timeout_ms from now.
 *
 * @param ts Filled with the absolute deadline.
 * @param timeout_ms Milliseconds from now; negative is clamped to 0.
 */
static void	deadline_from_now(struct timespec *ts, int timeout_ms)
{
	long	extra_ns;

	if (timeout_ms < 0)
		timeout_ms = 0;
	clock_gettime(CLOCK_MONOTONIC, ts);
	ts->tv_sec += timeout_ms / 1000;
	extra_ns = (long)(timeout_ms % 1000) * 1000000L;
	ts->tv_nsec += extra_ns;
	if (ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}

/**
 * @brief Measure how long is left before an absolute deadline.
 *
 * @param deadline The absolute deadline from deadline_from_now.
 * @return Milliseconds remaining, never negative.
 */
static int	remaining_ms(const struct timespec *deadline)
{
	struct timespec	now;
	long			ms;

	clock_gettime(CLOCK_MONOTONIC, &now);
	ms = (deadline->tv_sec - now.tv_sec) * 1000L;
	ms += (deadline->tv_nsec - now.tv_nsec) / 1000000L;
	if (ms < 0)
		return (0);
	return ((int)ms);
}

/**
 * @brief Wait until a queue has a message readable or the deadline passes.
 *
 * A POSIX queue descriptor is pollable on Linux, which is what lets the wait
 * happen outside the queue's own non-blocking receive.
 *
 * @param q An open queue descriptor.
 * @param timeout_ms Milliseconds to wait; 0 polls once.
 * @return 1 when readable, 0 when interrupted (retry), -1 with errno set
 *         (ETIMEDOUT when the deadline passed).
 */
static int	wait_readable(mqd_t q, int timeout_ms)
{
	struct pollfd	pfd;
	int				n;

	pfd.fd = (int)q;
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, timeout_ms);
	if (n == 0)
	{
		errno = ETIMEDOUT;
		return (-1);
	}
	if (n == -1 && errno == EINTR)
		return (0);
	return (n);
}
