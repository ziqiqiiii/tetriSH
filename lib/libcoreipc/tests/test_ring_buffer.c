#include "coreipc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Static Functions
static void	fill(t_ring_buffer *rb, int from, int count);

void	test_init_rejects_zero_arguments(void)
{
	t_ring_buffer	rb;

	assert(rb_init(&rb, 0, 8) == -1);
	assert(errno == EINVAL);
	assert(rb_init(&rb, sizeof(int), 0) == -1);
	assert(errno == EINVAL);
	printf("PASS test_init_rejects_zero_arguments\n");
}

void	test_push_pop_preserves_fifo_order(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(rb_init(&rb, sizeof(int), 8) == 0);
	fill(&rb, 10, 3);
	assert(rb_pop(&rb, &out) == 0 && out == 10);
	assert(rb_pop(&rb, &out) == 0 && out == 11);
	assert(rb_pop(&rb, &out) == 0 && out == 12);
	assert(rb_pop(&rb, &out) == -1);
	rb_destroy(&rb);
	printf("PASS test_push_pop_preserves_fifo_order\n");
}

void	test_capacity_is_exact(void)
{
	t_ring_buffer	rb;
	int				v;

	assert(rb_init(&rb, sizeof(int), 4) == 0);
	fill(&rb, 0, 4);
	v = 99;
	assert(rb_push(&rb, &v) == -1);
	rb_destroy(&rb);
	printf("PASS test_capacity_is_exact\n");
}

void	test_full_push_drops_and_counts(void)
{
	t_ring_buffer	rb;
	int				v;

	assert(rb_init(&rb, sizeof(int), 4) == 0);
	assert(rb_drops(&rb) == 0);
	fill(&rb, 0, 4);
	v = 99;
	assert(rb_push(&rb, &v) == -1);
	assert(rb_push(&rb, &v) == -1);
	assert(rb_drops(&rb) == 2);
	rb_destroy(&rb);
	printf("PASS test_full_push_drops_and_counts\n");
}

void	test_wraparound_past_capacity(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(rb_init(&rb, sizeof(int), 4) == 0);
	fill(&rb, 0, 4);
	assert(rb_pop(&rb, &out) == 0 && out == 0);
	assert(rb_pop(&rb, &out) == 0 && out == 1);
	fill(&rb, 4, 2);
	assert(rb_pop(&rb, &out) == 0 && out == 2);
	assert(rb_pop(&rb, &out) == 0 && out == 3);
	assert(rb_pop(&rb, &out) == 0 && out == 4);
	assert(rb_pop(&rb, &out) == 0 && out == 5);
	assert(rb_pop(&rb, &out) == -1);
	assert(rb_drops(&rb) == 0);
	rb_destroy(&rb);
	printf("PASS test_wraparound_past_capacity\n");
}

void	test_pop_empty_returns_minus_one(void)
{
	t_ring_buffer	rb;
	int				out;

	assert(rb_init(&rb, sizeof(int), 8) == 0);
	assert(rb_pop(&rb, &out) == -1);
	assert(rb_drops(&rb) == 0);
	rb_destroy(&rb);
	printf("PASS test_pop_empty_returns_minus_one\n");
}

void	test_drain_batches_in_order(void)
{
	t_ring_buffer	rb;
	int				batch[8];
	size_t			n;

	assert(rb_init(&rb, sizeof(int), 8) == 0);
	assert(rb_drain(&rb, batch, 8) == 0);
	fill(&rb, 100, 5);
	n = rb_drain(&rb, batch, 3);
	assert(n == 3);
	assert(batch[0] == 100 && batch[1] == 101 && batch[2] == 102);
	n = rb_drain(&rb, batch, 8);
	assert(n == 2);
	assert(batch[0] == 103 && batch[1] == 104);
	assert(rb_drain(&rb, batch, 8) == 0);
	rb_destroy(&rb);
	printf("PASS test_drain_batches_in_order\n");
}

void	test_destroy_is_safe_on_zeroed_struct(void)
{
	t_ring_buffer	rb;

	memset(&rb, 0, sizeof(rb));
	rb_destroy(&rb);
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
		assert(rb_push(rb, &v) == 0);
		i++;
	}
}
