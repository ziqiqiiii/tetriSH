/* ************************************************************************** */
/*                                                                            */
/*   test_buffer.c - the growable byte buffer the reactor reads and writes    */
/*                                                                            */
/*   Two cursors and a length are all this is, but they are the two cursors   */
/*   that decide whether a frame split across TCP segments is reassembled or  */
/*   corrupted, so the arithmetic is pinned here rather than only through a   */
/*   socket.                                                                  */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

#include <assert.h>

// Static Functions
static void	test_reserve_grows_and_never_shrinks(void);
static void	test_compact_drops_only_what_was_used(void);
static void	test_compact_of_a_fully_used_buffer_empties_it(void);
static void	test_u32_round_trips_big_endian(void);
static void	test_free_leaves_a_reusable_buffer(void);

int	main(void)
{
	test_reserve_grows_and_never_shrinks();
	test_compact_drops_only_what_was_used();
	test_compact_of_a_fully_used_buffer_empties_it();
	test_u32_round_trips_big_endian();
	test_free_leaves_a_reusable_buffer();
	return (0);
}

static void	test_reserve_grows_and_never_shrinks(void)
{
	t_buffer	b;
	size_t		grown;

	memset(&b, 0, sizeof(b));
	assert(buffer_reserve(&b, 1) == 0);
	assert(b.cap >= TETRISD_READ_CHUNK_BYTES);
	grown = b.cap;
	assert(buffer_reserve(&b, TETRISD_RECV_BUFFER_MAX) == 0);
	assert(b.cap >= TETRISD_RECV_BUFFER_MAX);
	grown = b.cap;
	assert(buffer_reserve(&b, 8) == 0);
	assert(b.cap == grown);
	buffer_free(&b);
	printf("PASS test_reserve_grows_and_never_shrinks\n");
}

/*
** The case a partly-received frame lands in: some bytes consumed, a tail that
** is the start of the next frame. That tail has to survive at offset zero.
*/
static void	test_compact_drops_only_what_was_used(void)
{
	t_buffer	b;

	memset(&b, 0, sizeof(b));
	assert(buffer_reserve(&b, 16) == 0);
	memcpy(b.data, "ABCDEFGH", 8);
	b.len = 8;
	b.used = 3;
	buffer_compact(&b);
	assert(b.len == 5);
	assert(b.used == 0);
	assert(memcmp(b.data, "DEFGH", 5) == 0);
	buffer_free(&b);
	printf("PASS test_compact_drops_only_what_was_used\n");
}

static void	test_compact_of_a_fully_used_buffer_empties_it(void)
{
	t_buffer	b;

	memset(&b, 0, sizeof(b));
	assert(buffer_reserve(&b, 16) == 0);
	memcpy(b.data, "ABCD", 4);
	b.len = 4;
	b.used = 4;
	buffer_compact(&b);
	assert(b.len == 0 && b.used == 0);
	buffer_compact(&b);
	assert(b.len == 0 && b.used == 0);
	buffer_free(&b);
	printf("PASS test_compact_of_a_fully_used_buffer_empties_it\n");
}

/*
** The length prefix is written by tetrisd and read by tetrisu, so the byte
** order is a wire contract, not an internal convention.
*/
static void	test_u32_round_trips_big_endian(void)
{
	unsigned char	raw[TETRISD_LENGTH_PREFIX_BYTES];

	buffer_put_u32(raw, 0x01020304u);
	assert(raw[0] == 0x01 && raw[1] == 0x02 && raw[2] == 0x03 && raw[3] == 0x04);
	assert(buffer_get_u32(raw) == 0x01020304u);
	buffer_put_u32(raw, 0u);
	assert(buffer_get_u32(raw) == 0u);
	buffer_put_u32(raw, TETRISSH_MAX_FRAME);
	assert(buffer_get_u32(raw) == TETRISSH_MAX_FRAME);
	printf("PASS test_u32_round_trips_big_endian\n");
}

static void	test_free_leaves_a_reusable_buffer(void)
{
	t_buffer	b;

	memset(&b, 0, sizeof(b));
	assert(buffer_reserve(&b, 64) == 0);
	b.len = 10;
	b.used = 4;
	buffer_free(&b);
	assert(b.data == NULL && b.cap == 0 && b.len == 0 && b.used == 0);
	assert(buffer_reserve(&b, 64) == 0);
	buffer_free(&b);
	printf("PASS test_free_leaves_a_reusable_buffer\n");
}
