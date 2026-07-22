#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * Differential / round-trip fuzz suite.
 *
 * Locks in the invariants the manual review verified by hand:
 *   1. htttp_parse() never crashes and always fails atomically -- a non-OK
 *      result leaves the output message empty (no partial ownership, no leak).
 *   2. Any message that parses is serialisable into canonical wire bytes.
 *   3. Those canonical bytes are a fixed point: parse -> serialize -> parse ->
 *      serialize reproduces byte-identical output.
 *
 * Run natively for speed and under `make memcheck FILTER=fuzz` to exercise the
 * allocation and cleanup paths for leaks.
 */

#define FUZZ_ITERATIONS 20000u
#define FUZZ_MAX_LEN 512u

static const unsigned char	g_valid_request[] =
	"MOVE /room/main/player/p17 HTTTP/1.0\r\n"
	"Player-Id: p17\r\n"
	"Content-Type: application/tetris-command\r\n"
	"Content-Length: 4\r\n"
	"\r\n"
	"LEFT";

static const unsigned char	g_valid_response[] =
	"HTTTP/1.0 200 OK\r\n"
	"Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
	"\r\n";

/* Bytes that steer the generator toward protocol structure (delimiters,
 * colons, digits, letters) so header and body states are actually reached. */
static const unsigned char	g_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	"/-_: \r\n.\t";

/* Deterministic xorshift so any failure reproduces from a fixed seed. */
static unsigned int	g_rng_state = 0x1234abcdu;

static unsigned int	next_random(void)
{
	g_rng_state ^= g_rng_state << 13;
	g_rng_state ^= g_rng_state >> 17;
	g_rng_state ^= g_rng_state << 5;
	return (g_rng_state);
}

static void	assert_message_empty(const t_htttp_message *message)
{
	assert(message->type == HTTTP_MESSAGE_REQUEST);
	assert(message->method == NULL);
	assert(message->path == NULL);
	assert(message->status_code == 0u);
	assert(message->reason == NULL);
	assert(message->headers == NULL);
	assert(message->header_count == 0u);
	assert(message->body == NULL);
	assert(message->body_len == 0u);
}

static void	fuzz_one(const unsigned char *data, size_t len)
{
	t_htttp_message	message;
	t_htttp_message	reparsed;
	unsigned char	*wire;
	unsigned char	*wire2;
	size_t			wire_len;
	size_t			wire2_len;

	htttp_message_init(&message);
	if (htttp_parse(data, len, &message) != HTTTP_OK)
	{
		assert_message_empty(&message);
		htttp_message_free(&message);
		return;
	}
	wire = NULL;
	wire_len = 0u;
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_OK);
	assert(wire != NULL && wire_len > 0u && wire_len <= HTTTP_MAX_MESSAGE_SIZE);
	htttp_message_init(&reparsed);
	assert(htttp_parse(wire, wire_len, &reparsed) == HTTTP_OK);
	wire2 = NULL;
	wire2_len = 0u;
	assert(htttp_serialize(&reparsed, &wire2, &wire2_len) == HTTTP_OK);
	assert(wire2_len == wire_len);
	assert(memcmp(wire2, wire, wire_len) == 0);
	free(wire);
	free(wire2);
	htttp_message_free(&reparsed);
	htttp_message_free(&message);
}

static void	assert_fixed_point(const unsigned char *fixture, size_t fixture_len)
{
	t_htttp_message	message;
	unsigned char	*wire;
	size_t			wire_len;

	htttp_message_init(&message);
	assert(htttp_parse(fixture, fixture_len, &message) == HTTTP_OK);
	wire = NULL;
	wire_len = 0u;
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_OK);
	assert(wire_len == fixture_len);
	assert(memcmp(wire, fixture, fixture_len) == 0);
	free(wire);
	htttp_message_free(&message);
}

static void	test_known_fixtures_round_trip(void)
{
	assert_fixed_point(g_valid_request, sizeof(g_valid_request) - 1u);
	assert_fixed_point(g_valid_response, sizeof(g_valid_response) - 1u);
	printf("PASS test_known_fixtures_round_trip\n");
}

static void	test_fuzz_random_bytes(void)
{
	unsigned char	buffer[FUZZ_MAX_LEN];
	size_t			iteration;
	size_t			len;
	size_t			i;

	iteration = 0u;
	while (iteration < FUZZ_ITERATIONS)
	{
		len = next_random() % (FUZZ_MAX_LEN + 1u);
		i = 0u;
		while (i < len)
		{
			buffer[i] = (unsigned char)(next_random() & 0xffu);
			i++;
		}
		fuzz_one(buffer, len);
		iteration++;
	}
	printf("PASS test_fuzz_random_bytes\n");
}

static void	test_fuzz_protocol_alphabet(void)
{
	unsigned char	buffer[FUZZ_MAX_LEN];
	size_t			alphabet_len;
	size_t			iteration;
	size_t			len;
	size_t			i;

	alphabet_len = sizeof(g_alphabet) - 1u;
	iteration = 0u;
	while (iteration < FUZZ_ITERATIONS)
	{
		len = next_random() % (FUZZ_MAX_LEN + 1u);
		i = 0u;
		while (i < len)
		{
			buffer[i] = g_alphabet[next_random() % alphabet_len];
			i++;
		}
		fuzz_one(buffer, len);
		iteration++;
	}
	printf("PASS test_fuzz_protocol_alphabet\n");
}

static void	fuzz_mutations(const unsigned char *fixture, size_t fixture_len)
{
	unsigned char	buffer[FUZZ_MAX_LEN];
	size_t			iteration;
	size_t			flips;
	size_t			f;
	size_t			pos;

	assert(fixture_len > 0u && fixture_len <= FUZZ_MAX_LEN);
	iteration = 0u;
	while (iteration < FUZZ_ITERATIONS)
	{
		memcpy(buffer, fixture, fixture_len);
		flips = 1u + (next_random() % 4u);
		f = 0u;
		while (f < flips)
		{
			pos = next_random() % fixture_len;
			buffer[pos] = (unsigned char)(next_random() & 0xffu);
			f++;
		}
		fuzz_one(buffer, fixture_len);
		iteration++;
	}
}

static void	test_fuzz_valid_fixture_mutations(void)
{
	fuzz_mutations(g_valid_request, sizeof(g_valid_request) - 1u);
	fuzz_mutations(g_valid_response, sizeof(g_valid_response) - 1u);
	printf("PASS test_fuzz_valid_fixture_mutations\n");
}

int	main(void)
{
	test_known_fixtures_round_trip();
	test_fuzz_random_bytes();
	test_fuzz_protocol_alphabet();
	test_fuzz_valid_fixture_mutations();
	return (0);
}
