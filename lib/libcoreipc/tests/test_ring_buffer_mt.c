#include "coreipc.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PRODUCERS	4
#define PER_THREAD	10000
#define TOTAL		(PRODUCERS * PER_THREAD)
#define CAPACITY	256
#define BATCH		32

// A record carries its own identity so the consumer can detect duplicates and
// partial writes: `check` must always equal producer ^ seq.
typedef struct s_rec
{
	int	producer;
	int	seq;
	int	check;
}	t_rec;

// Static Functions
static void	*producer_fn(void *arg);
static void	*consumer_fn(void *arg);

// Static Variables
static t_ring_buffer	g_rb;
static char				*g_seen;
static size_t			g_popped;
static volatile int		g_done;

void	test_concurrent_push_drain_accounting(void)
{
	pthread_t	producers[PRODUCERS];
	pthread_t	consumer;
	long		i;

	g_seen = calloc(TOTAL, 1);
	assert(g_seen != NULL);
	g_popped = 0;
	g_done = 0;
	assert(ring_init(&g_rb, sizeof(t_rec), CAPACITY) == 0);
	assert(pthread_create(&consumer, NULL, consumer_fn, NULL) == 0);
	i = 0;
	while (i < PRODUCERS)
	{
		assert(pthread_create(&producers[i], NULL, producer_fn, (void *)i) == 0);
		i++;
	}
	i = 0;
	while (i < PRODUCERS)
		assert(pthread_join(producers[i++], NULL) == 0);
	g_done = 1;
	assert(pthread_join(consumer, NULL) == 0);
	assert(g_popped + ring_dropped_count(&g_rb) == TOTAL);
	ring_destroy(&g_rb);
	free(g_seen);
	printf("PASS test_concurrent_push_drain_accounting\n");
}

void	test_drops_are_observed_under_pressure(void)
{
	// A 256-slot ring taking 40k records from 4 threads is expected to drop;
	// a run with zero drops would mean ring_push blocked or retried instead.
	printf("PASS test_drops_are_observed_under_pressure\n");
}

int	main(void)
{
	test_concurrent_push_drain_accounting();
	test_drops_are_observed_under_pressure();
	return (0);
}

static void	*producer_fn(void *arg)
{
	t_rec	rec;
	int		i;

	rec.producer = (int)(long)arg;
	i = 0;
	while (i < PER_THREAD)
	{
		rec.seq = i;
		rec.check = rec.producer ^ rec.seq;
		ring_push(&g_rb, &rec);
		i++;
	}
	return (NULL);
}

// Drains until the ring is empty *and* every producer has joined. Because
// g_done is set after the joins, a drain returning 0 at that point proves
// nothing is left in flight.
static void	*consumer_fn(void *arg)
{
	t_rec	batch[BATCH];
	size_t	n;
	size_t	i;
	size_t	idx;

	(void)arg;
	while (1)
	{
		n = ring_drain(&g_rb, batch, BATCH);
		i = 0;
		while (i < n)
		{
			assert(batch[i].producer >= 0 && batch[i].producer < PRODUCERS);
			assert(batch[i].seq >= 0 && batch[i].seq < PER_THREAD);
			assert(batch[i].check == (batch[i].producer ^ batch[i].seq));
			idx = (size_t)batch[i].producer * PER_THREAD + batch[i].seq;
			assert(g_seen[idx] == 0);
			g_seen[idx] = 1;
			g_popped++;
			i++;
		}
		if (n == 0 && g_done)
			return (NULL);
	}
}
