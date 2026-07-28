#include "coreipc.h"

/**
 * @brief Initialise a ring buffer for records of a fixed size.
 *
 * @param rb The buffer to initialise (caller-owned storage).
 * @param record_size Size in bytes of one record; must be non-zero.
 * @param capacity Number of records the buffer holds; must be non-zero.
 * @return 0 on success, -1 with errno set (EINVAL, ENOMEM) on failure.
 */
int	rb_init(t_ring_buffer *rb, size_t record_size, size_t capacity)
{
	/* TODO: validate -> EINVAL; calloc capacity + 1 slots -> ENOMEM;
	   pthread_mutex_init; atomic_init(&rb->drops, 0); head = tail = 0. */
	(void)rb;
	(void)record_size;
	(void)capacity;
	errno = ENOSYS;
	return (-1);
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
int	rb_push(t_ring_buffer *rb, const void *record)
{
	/* TODO: lock; full -> unlock, atomic_fetch_add(&rb->drops, 1), -1;
	   else memcpy slot[tail], advance tail, unlock. */
	(void)rb;
	(void)record;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Remove the oldest record from the ring.
 *
 * Single-consumer only: one thread may call rb_pop or rb_drain on a buffer.
 *
 * @param rb The initialised buffer.
 * @param out Destination for record_size bytes.
 * @return 0 when a record was copied out, -1 when the ring was empty.
 */
int	rb_pop(t_ring_buffer *rb, void *out)
{
	/* TODO: lock; head == tail -> unlock, -1; else memcpy out from
	   slot[head], advance head, unlock. */
	(void)rb;
	(void)out;
	errno = ENOSYS;
	return (-1);
}

/**
 * @brief Remove up to max_records records in one locked batch.
 *
 * @param rb The initialised buffer.
 * @param out Destination of at least max_records * record_size bytes.
 * @param max_records Ceiling on how many records to copy.
 * @return The number of records copied, 0 when the ring was empty.
 */
size_t	rb_drain(t_ring_buffer *rb, void *out, size_t max_records)
{
	/* TODO: lock once; copy while head != tail && n < max_records;
	   advance head per record; unlock; return n. */
	(void)rb;
	(void)out;
	(void)max_records;
	return (0);
}

/**
 * @brief Read the running total of records dropped by rb_push.
 *
 * Takes no lock, so the tetrisctl dropped-logs path never contends with the
 * producers. Monotonic: never reset for the lifetime of the buffer.
 *
 * @param rb The initialised buffer.
 * @return Total records dropped since rb_init.
 */
uint64_t	rb_drops(const t_ring_buffer *rb)
{
	/* TODO: atomic_load(&rb->drops); NULL rb -> 0. */
	(void)rb;
	return (0);
}

/**
 * @brief Release the ring's storage and mutex.
 *
 * Safe on a zeroed struct. The caller must have stopped every producer and
 * the consumer first; this does not synchronise with them.
 *
 * @param rb The buffer to destroy.
 */
void	rb_destroy(t_ring_buffer *rb)
{
	/* TODO: free(rb->slots); pthread_mutex_destroy; zero the struct. */
	(void)rb;
}
