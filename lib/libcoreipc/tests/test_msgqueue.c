#include "coreipc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXMSG	4
#define MSGSIZE	32

// Static Functions
static mqd_t	fresh_queue(void);
static void		queue_name(char *out, size_t cap);
static int		mqueue_available(void);
static long		elapsed_ms(struct timespec *from, struct timespec *to);

// Static Variables
static char	g_name[64];

void	test_open_rejects_a_bad_name(void)
{
	assert(msgqueue_open("no-leading-slash", MAXMSG, MSGSIZE, 0600) == (mqd_t)-1);
	assert(errno == EINVAL);
	printf("PASS test_open_rejects_a_bad_name\n");
}

void	test_send_fills_then_drops_without_blocking(void)
{
	mqd_t	q;
	char	msg[MSGSIZE];
	int		i;

	q = fresh_queue();
	memset(msg, 'g', sizeof(msg));
	i = 0;
	while (i < MAXMSG)
	{
		assert(msgqueue_send_nonblock(q, msg, sizeof(msg)) == 0);
		i++;
	}
	assert(msgqueue_send_nonblock(q, msg, sizeof(msg)) == -1);
	assert(errno == EAGAIN);
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_send_fills_then_drops_without_blocking\n");
}

void	test_recv_drains_in_order(void)
{
	mqd_t	q;
	char	msg[MSGSIZE];
	char	got[MSGSIZE];
	int		i;

	q = fresh_queue();
	i = 0;
	while (i < MAXMSG)
	{
		memset(msg, 0, sizeof(msg));
		msg[0] = (char)('a' + i);
		assert(msgqueue_send_nonblock(q, msg, sizeof(msg)) == 0);
		i++;
	}
	i = 0;
	while (i < MAXMSG)
	{
		assert(msgqueue_recv_nonblock(q, got, sizeof(got)) == (ssize_t)sizeof(got));
		assert(got[0] == (char)('a' + i));
		i++;
	}
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_recv_drains_in_order\n");
}

void	test_recv_on_empty_queue_returns_eagain(void)
{
	mqd_t	q;
	char	got[MSGSIZE];

	q = fresh_queue();
	assert(msgqueue_recv_nonblock(q, got, sizeof(got)) == -1);
	assert(errno == EAGAIN);
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_recv_on_empty_queue_returns_eagain\n");
}

void	test_recv_rejects_undersized_buffer(void)
{
	mqd_t	q;
	char	got[MSGSIZE - 1];

	q = fresh_queue();
	assert(msgqueue_recv_nonblock(q, got, sizeof(got)) == -1);
	assert(errno == EMSGSIZE);
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_recv_rejects_undersized_buffer\n");
}

void	test_recv_timed_returns_a_queued_message(void)
{
	mqd_t	q;
	char	msg[MSGSIZE];
	char	got[MSGSIZE];

	q = fresh_queue();
	memset(msg, 'z', sizeof(msg));
	assert(msgqueue_send_nonblock(q, msg, sizeof(msg)) == 0);
	assert(msgqueue_recv_timed(q, got, sizeof(got), 500) == (ssize_t)sizeof(got));
	assert(memcmp(msg, got, sizeof(msg)) == 0);
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_recv_timed_returns_a_queued_message\n");
}

void	test_recv_timed_waits_then_times_out(void)
{
	struct timespec	start;
	struct timespec	end;
	mqd_t			q;
	char			got[MSGSIZE];
	long			waited;

	q = fresh_queue();
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(msgqueue_recv_timed(q, got, sizeof(got), 200) == -1);
	assert(errno == ETIMEDOUT);
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	// It must actually park rather than spin-return, but the deadline is
	// absolute, so a slow machine may overshoot; only the floor is asserted.
	waited = elapsed_ms(&start, &end);
	assert(waited >= 150);
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	printf("PASS test_recv_timed_waits_then_times_out\n");
}

void	test_unlink_removes_the_queue(void)
{
	mqd_t	q;

	q = fresh_queue();
	assert(msgqueue_close(q) == 0);
	assert(msgqueue_unlink(g_name) == 0);
	assert(msgqueue_unlink(g_name) == -1);
	assert(errno == ENOENT);
	printf("PASS test_unlink_removes_the_queue\n");
}

int	main(void)
{
	if (!mqueue_available())
	{
		printf("SKIP test_msgqueue (no /dev/mqueue on this host)\n");
		return (0);
	}
	queue_name(g_name, sizeof(g_name));
	msgqueue_unlink(g_name);
	test_open_rejects_a_bad_name();
	test_send_fills_then_drops_without_blocking();
	test_recv_drains_in_order();
	test_recv_on_empty_queue_returns_eagain();
	test_recv_rejects_undersized_buffer();
	test_recv_timed_returns_a_queued_message();
	test_recv_timed_waits_then_times_out();
	test_unlink_removes_the_queue();
	return (0);
}

static mqd_t	fresh_queue(void)
{
	mqd_t	q;

	msgqueue_unlink(g_name);
	q = msgqueue_open(g_name, MAXMSG, MSGSIZE, 0600);
	assert(q != (mqd_t)-1);
	return (q);
}

// Per-process name so concurrent runs never share a kernel-persistent queue.
static void	queue_name(char *out, size_t cap)
{
	int	n;

	n = snprintf(out, cap, "/coreipc-test-%d", (int)getpid());
	assert(n > 0 && (size_t)n < cap);
}

static int	mqueue_available(void)
{
	return (access("/dev/mqueue", F_OK) == 0);
}

static long	elapsed_ms(struct timespec *from, struct timespec *to)
{
	long	ms;

	ms = (to->tv_sec - from->tv_sec) * 1000;
	ms += (to->tv_nsec - from->tv_nsec) / 1000000;
	return (ms);
}
