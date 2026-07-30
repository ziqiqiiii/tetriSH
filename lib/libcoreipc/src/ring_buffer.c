#include "coreipc.h"

#include <stdlib.h>
#include <string.h>

int	rb_init(t_ring_buffer *rb, size_t record_size, size_t capacity)
{
	if (record_size == 0 || capacity == 0)
	{
		errno = EINVAL;
		return (-1);
	}
	rb->slots = calloc(capacity + 1, record_size);
	if (rb->slots == NULL)
		return (-1);
	rb->record_size = record_size;
	rb->capacity = capacity;
	rb->head = 0;
	rb->tail = 0;
	if (pthread_mutex_init(&rb->mutex, NULL) != 0)
	{
		free(rb->slots);
		rb->slots = NULL;
		return (-1);
	}
	atomic_init(&rb->drops, 0);
	return (0);
}

int	rb_push(t_ring_buffer *rb, const void *record)
{
	size_t	next;

	pthread_mutex_lock(&rb->mutex);
	next = (rb->tail + 1) % (rb->capacity + 1);
	if (next == rb->head)
	{
		pthread_mutex_unlock(&rb->mutex);
		atomic_fetch_add(&rb->drops, 1);
		return (-1);
	}
	memcpy(rb->slots + rb->tail * rb->record_size, record, rb->record_size);
	rb->tail = next;
	pthread_mutex_unlock(&rb->mutex);
	return (0);
}

int	rb_pop(t_ring_buffer *rb, void *out)
{
	pthread_mutex_lock(&rb->mutex);
	if (rb->head == rb->tail)
	{
		pthread_mutex_unlock(&rb->mutex);
		return (-1);
	}
	memcpy(out, rb->slots + rb->head * rb->record_size, rb->record_size);
	rb->head = (rb->head + 1) % (rb->capacity + 1);
	pthread_mutex_unlock(&rb->mutex);
	return (0);
}

size_t	rb_drain(t_ring_buffer *rb, void *out, size_t max_records)
{
	size_t	n;

	n = 0;
	pthread_mutex_lock(&rb->mutex);
	while (rb->head != rb->tail && n < max_records)
	{
		memcpy((unsigned char *)out + n * rb->record_size,
			rb->slots + rb->head * rb->record_size, rb->record_size);
		rb->head = (rb->head + 1) % (rb->capacity + 1);
		n++;
	}
	pthread_mutex_unlock(&rb->mutex);
	return (n);
}

uint64_t	rb_drops(const t_ring_buffer *rb)
{
	if (rb == NULL)
		return (0);
	return (atomic_load(&rb->drops));
}

void	rb_destroy(t_ring_buffer *rb)
{
	if (rb->slots != NULL)
	{
		free(rb->slots);
		pthread_mutex_destroy(&rb->mutex);
		memset(rb, 0, sizeof(*rb));
	}
}
