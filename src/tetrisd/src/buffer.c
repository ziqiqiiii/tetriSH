#include "tetrisd.h"

/**
 * @brief Grows a buffer so it can hold at least cap bytes.
 *
 * Capacity doubles rather than tracking each request exactly, so a connection
 * that talks in small frames stops reallocating almost immediately. Nothing
 * ever shrinks: the ceiling is one frame, and a client that has sent one large
 * frame will very likely send another.
 *
 * @param b Buffer to grow; a NULL pointer is rejected.
 * @param cap Capacity the caller needs in bytes.
 * @return 0 when the buffer holds cap bytes, -1 on allocation failure.
 */
int	buffer_reserve(t_buffer *b, size_t cap)
{
	unsigned char	*grown;
	size_t			want;

	if (b == NULL)
		return (-1);
	if (b->cap >= cap)
		return (0);
	want = b->cap;
	if (want < TETRISD_READ_CHUNK_BYTES)
		want = TETRISD_READ_CHUNK_BYTES;
	while (want < cap)
		want *= 2;
	grown = realloc(b->data, want);
	if (grown == NULL)
		return (-1);
	b->data = grown;
	b->cap = want;
	return (0);
}

/**
 * @brief Drops the bytes already consumed, moving the remainder to the front.
 *
 * @param b Buffer to compact; a NULL pointer is ignored.
 */
void	buffer_compact(t_buffer *b)
{
	if (b == NULL || b->used == 0)
		return ;
	if (b->used < b->len)
		memmove(b->data, b->data + b->used, b->len - b->used);
	b->len -= b->used;
	b->used = 0;
}

/**
 * @brief Writes the big-endian length prefix libtetrissh puts before a frame.
 *
 * @param p Four bytes to write into.
 * @param value The frame length to encode.
 */
void	buffer_put_u32(unsigned char *p, uint32_t value)
{
	p[0] = (unsigned char)(value >> 24);
	p[1] = (unsigned char)((value >> 16) & 0xFFu);
	p[2] = (unsigned char)((value >> 8) & 0xFFu);
	p[3] = (unsigned char)(value & 0xFFu);
}

/**
 * @brief Reads the big-endian length prefix libtetrissh puts before a frame.
 *
 * @param p The four prefix bytes.
 * @return The frame length they encode.
 */
uint32_t	buffer_get_u32(const unsigned char *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
		| ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

/**
 * @brief Releases a buffer's storage and blanks it.
 *
 * @param b Buffer to release; a NULL pointer is ignored.
 */
void	buffer_free(t_buffer *b)
{
	if (b == NULL)
		return ;
	free(b->data);
	b->data = NULL;
	b->cap = 0;
	b->len = 0;
	b->used = 0;
}
