#include "coreipc.h"

// Static Functions
static void		*slot_at(t_ring_buffer *rb, size_t index);
static size_t	advance(const t_ring_buffer *rb, size_t index);

/**
 * @brief Initialise a ring buffer for records of a fixed size.
 *
 * Allocates one slot more than the capacity so a full ring stays
 * distinguishable from an empty one without a separate count.
 *
 * @param rb The buffer to initialise (caller-owned storage).
 * @param record_size Size in bytes of one record; must be non-zero.
 * @param capacity Number of records the buffer holds; must be non-zero.
 * @return 0 on success, -1 with errno set (EINVAL, ENOMEM) on failure.
 */
int	ring_init(t_ring_buffer *rb, size_t record_size, size_t capacity)
{
	if (!rb || record_size == 0 || capacity == 0)
	{
		errno = EINVAL;
		return (-1);
	}
	rb->slots = calloc(capacity + 1, record_size);
	if (!rb->slots)
	{
		errno = ENOMEM;
		return (-1);
	}
	if (pthread_mutex_init(&rb->mutex, NULL) != 0)
	{
		free(rb->slots);
		rb->slots = NULL;
		errno = ENOMEM;
		return (-1);
	}
	rb->record_size = record_size;
	rb->capacity = capacity;
	rb->head = 0;
	rb->tail = 0;
	atomic_init(&rb->drops, 0);
	return (0);
}

/**
 * @brief Copy one record into the ring, or drop it if the ring is full.
 *
 * O(1), never blocks, never retries. Callable from any thread.
 *
 * @param rb The initialised buffer.
 * @param record Source of record_size bytes, copied by value.
 * @return 0 when the record was stored, -1 when the ring was full (dropped).
 */
int	ring_push(t_ring_buffer *rb, const void *record)
{
	size_t	next;

	if (!rb || !rb->slots || !record)
	{
		errno = EINVAL;
		return (-1);
	}
	pthread_mutex_lock(&rb->mutex);
	next = advance(rb, rb->tail);
	if (next == rb->head)
	{
		pthread_mutex_unlock(&rb->mutex);
		atomic_fetch_add(&rb->drops, 1);
		return (-1);
	}
	memcpy(slot_at(rb, rb->tail), record, rb->record_size);
	rb->tail = next;
	pthread_mutex_unlock(&rb->mutex);
	return (0);
}

/**
 * @brief Remove the oldest record from the ring.
 *
 * Single-consumer only: one thread may call ring_pop or ring_drain on a buffer.
 *
 * @param rb The initialised buffer.
 * @param out Destination for record_size bytes.
 * @return 0 when a record was copied out, -1 when the ring was empty.
 */
int	ring_pop(t_ring_buffer *rb, void *out)
{
	if (!rb || !rb->slots || !out)
	{
		errno = EINVAL;
		return (-1);
	}
	pthread_mutex_lock(&rb->mutex);
	if (rb->head == rb->tail)
	{
		pthread_mutex_unlock(&rb->mutex);
		return (-1);
	}
	memcpy(out, slot_at(rb, rb->head), rb->record_size);
	rb->head = advance(rb, rb->head);
	pthread_mutex_unlock(&rb->mutex);
	return (0);
}

/**
 * @brief Remove up to max_records records in one locked batch.
 *
 * @param rb The initialised buffer.
 * @param out Destination of at least max_records * record_size bytes.
 * @param max_records Ceiling on how many records to copy.
 * @return The number of records copied, 0 when the ring was empty.
 */
size_t	ring_drain(t_ring_buffer *rb, void *out, size_t max_records)
{
	size_t	n;

	if (!rb || !rb->slots || !out)
		return (0);
	n = 0;
	pthread_mutex_lock(&rb->mutex);
	while (n < max_records && rb->head != rb->tail)
	{
		memcpy((unsigned char *)out + n * rb->record_size, slot_at(rb, rb->head), rb->record_size);
		rb->head = advance(rb, rb->head);
		n++;
	}
	pthread_mutex_unlock(&rb->mutex);
	return (n);
}

/**
 * @brief Read the running total of records dropped by ring_push.
 *
 * Takes no lock, so the tetrisctl dropped-logs path never contends with the
 * producers. Monotonic: never reset for the lifetime of the buffer.
 *
 * @param rb The initialised buffer.
 * @return Total records dropped since ring_init.
 */
uint64_t	ring_dropped_count(const t_ring_buffer *rb)
{
	if (!rb)
		return (0);
	return ((uint64_t)atomic_load(&rb->drops));
}

/**
 * @brief Release the ring's storage and mutex.
 *
 * Safe on a zeroed struct. The caller must have stopped every producer and
 * the consumer first; this does not synchronise with them.
 *
 * @param rb The buffer to destroy.
 */
void	ring_destroy(t_ring_buffer *rb)
{
	if (!rb || !rb->slots)
		return ;
	free(rb->slots);
	pthread_mutex_destroy(&rb->mutex);
	memset(rb, 0, sizeof(*rb));
}

/**
 * @brief Address the slot holding the record at a ring index.
 *
 * @param rb The initialised buffer.
 * @param index A slot index in [0, capacity].
 * @return Pointer to the first byte of that slot.
 */
static void	*slot_at(t_ring_buffer *rb, size_t index)
{
	return (rb->slots + index * rb->record_size);
}

/**
 * @brief Step a ring index forward one slot, wrapping at the end.
 *
 * @param rb The initialised buffer.
 * @param index The index to advance.
 * @return The next index, modulo capacity + 1.
 */
static size_t	advance(const t_ring_buffer *rb, size_t index)
{
	if (index + 1 > rb->capacity)
		return (0);
	return (index + 1);
}
