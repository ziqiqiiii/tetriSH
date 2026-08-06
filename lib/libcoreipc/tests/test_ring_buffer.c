#include "coreipc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Static Functions
static void	fill(t_ring_buffer *rb, int from, int count);

void	test_init_rejects_zero_arguments(void)
{
	t_ring_buffer	rb;

	assert(ring_init(&rb, 0, 8) == -1);
	assert(errno == EINVAL);
	assert(ring_init(&rb, sizeof(int), 0) == -1);
	assert(errno == EINVAL);
	printf("PASS test_init_rejects_zero_arguments\n");
}

void	test_push_pop_preserves_fifo_order(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(ring_init(&rb, sizeof(int), 8) == 0);
	fill(&rb, 10, 3);
	assert(ring_pop(&rb, &out) == 0 && out == 10);
	assert(ring_pop(&rb, &out) == 0 && out == 11);
	assert(ring_pop(&rb, &out) == 0 && out == 12);
	assert(ring_pop(&rb, &out) == -1);
	ring_destroy(&rb);
	printf("PASS test_push_pop_preserves_fifo_order\n");
}

void	test_capacity_is_exact(void)
{
	t_ring_buffer	rb;
	int				v;

	assert(ring_init(&rb, sizeof(int), 4) == 0);
	fill(&rb, 0, 4);
	v = 99;
	assert(ring_push(&rb, &v) == -1);
	ring_destroy(&rb);
	printf("PASS test_capacity_is_exact\n");
}

void	test_full_push_drops_and_counts(void)
{
	t_ring_buffer	rb;
	int				v;

	assert(ring_init(&rb, sizeof(int), 4) == 0);
	assert(ring_dropped_count(&rb) == 0);
	fill(&rb, 0, 4);
	v = 99;
	assert(ring_push(&rb, &v) == -1);
	assert(ring_push(&rb, &v) == -1);
	assert(ring_dropped_count(&rb) == 2);
	ring_destroy(&rb);
	printf("PASS test_full_push_drops_and_counts\n");
}

void	test_wraparound_past_capacity(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(ring_init(&rb, sizeof(int), 4) == 0);
	fill(&rb, 0, 4);
	assert(ring_pop(&rb, &out) == 0 && out == 0);
	assert(ring_pop(&rb, &out) == 0 && out == 1);
	fill(&rb, 4, 2);
	assert(ring_pop(&rb, &out) == 0 && out == 2);
	assert(ring_pop(&rb, &out) == 0 && out == 3);
	assert(ring_pop(&rb, &out) == 0 && out == 4);
	assert(ring_pop(&rb, &out) == 0 && out == 5);
	assert(ring_pop(&rb, &out) == -1);
	assert(ring_dropped_count(&rb) == 0);
	ring_destroy(&rb);
	printf("PASS test_wraparound_past_capacity\n");
}

void	test_pop_empty_returns_minus_one(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(ring_init(&rb, sizeof(int), 8) == 0);
	assert(ring_pop(&rb, &out) == -1);
	assert(ring_dropped_count(&rb) == 0);
	ring_destroy(&rb);
	printf("PASS test_pop_empty_returns_minus_one\n");
}

void	test_drain_batches_in_order(void)
{
	t_ring_buffer	rb;
	int				batch[8];
	size_t			n;

	assert(ring_init(&rb, sizeof(int), 8) == 0);
	assert(ring_drain(&rb, batch, 8) == 0);
	fill(&rb, 100, 5);
	n = ring_drain(&rb, batch, 3);
	assert(n == 3);
	assert(batch[0] == 100 && batch[1] == 101 && batch[2] == 102);
	n = ring_drain(&rb, batch, 8);
	assert(n == 2);
	assert(batch[0] == 103 && batch[1] == 104);
	assert(ring_drain(&rb, batch, 8) == 0);
	ring_destroy(&rb);
	printf("PASS test_drain_batches_in_order\n");
}

void	test_destroy_is_safe_on_zeroed_struct(void)
{
	t_ring_buffer	rb;

	memset(&rb, 0, sizeof(rb));
	ring_destroy(&rb);
	printf("PASS test_destroy_is_safe_on_zeroed_struct\n");
}

int	main(void)
{
	test_init_rejects_zero_arguments();
	test_push_pop_preserves_fifo_order();
	test_capacity_is_exact();
	test_full_push_drops_and_counts();
	test_wraparound_past_capacity();
	test_pop_empty_returns_minus_one();
	test_drain_batches_in_order();
	test_destroy_is_safe_on_zeroed_struct();
	return (0);
}

static void	fill(t_ring_buffer *rb, int from, int count)
{
	int	i;
	int	v;

	i = 0;
	while (i < count)
	{
		v = from + i;
		assert(ring_push(rb, &v) == 0);
		i++;
	}
}
