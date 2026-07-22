#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define HARDENING_ITERATIONS 256u

static const unsigned char	g_request[] =
	"MOVE /room/main/player/p17 HTTTP/1.0\r\n"
	"Trace: room:a:join\r\n"
	"Content-Type: application/tetris-command\r\n"
	"Content-Length: 4\r\n"
	"\r\n"
	"\0\r\nX";

static const unsigned char	g_response[] =
	"HTTTP/1.0 409 INVALID_MOVE\r\n"
	"Date: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
	"Trace: room:a:join\r\n"
	"\r\n";

static void	assert_empty(const t_htttp_message *message)
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

static void	assert_parse_failure(const unsigned char *data, size_t data_len)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_parse(data, data_len, &message) != HTTTP_OK);
	assert_empty(&message);
	htttp_message_free(&message);
}

static void	assert_all_truncations_fail(const unsigned char *fixture,
		size_t fixture_len)
{
	unsigned char	*prefix;
	unsigned char	zero_length_data;
	size_t	prefix_len;

	/* AI-assisted: exact-size nonzero prefixes put ASan's redzone at each parser
	 * boundary; zero length uses a valid pointer to test data_len semantics. */
	zero_length_data = 0u;
	prefix_len = 0u;
	while (prefix_len < fixture_len)
	{
		if (prefix_len == 0u)
			assert_parse_failure(&zero_length_data, prefix_len);
		else
		{
			prefix = malloc(prefix_len);
			assert(prefix != NULL);
			memcpy(prefix, fixture, prefix_len);
			assert_parse_failure(prefix, prefix_len);
			free(prefix);
		}
		prefix_len++;
	}
}

static void	assert_delimiter_mutations_fail(const unsigned char *fixture,
		size_t fixture_len, size_t body_start)
{
	unsigned char	*mutation;
	size_t			i;

	mutation = malloc(fixture_len);
	assert(mutation != NULL);
	memcpy(mutation, fixture, fixture_len);
	i = 0u;
	while (i < body_start)
	{
		if (mutation[i] == '\r' || mutation[i] == '\n')
		{
			mutation[i] = 'X';
			assert_parse_failure(mutation, fixture_len);
			mutation[i] = fixture[i];
		}
		i++;
	}
	free(mutation);
}

static void	assert_round_trip(const unsigned char *fixture, size_t fixture_len)
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

static void	test_all_shorter_prefixes_fail_atomically(void)
{
	assert_all_truncations_fail(g_request, sizeof(g_request) - 1u);
	assert_all_truncations_fail(g_response, sizeof(g_response) - 1u);
	printf("PASS test_all_shorter_prefixes_fail_atomically\n");
}

static void	test_all_line_delimiter_mutations_fail(void)
{
	assert_delimiter_mutations_fail(g_request, sizeof(g_request) - 1u,
		sizeof(g_request) - 1u - 4u);
	assert_delimiter_mutations_fail(g_response, sizeof(g_response) - 1u,
		sizeof(g_response) - 1u);
	printf("PASS test_all_line_delimiter_mutations_fail\n");
}

static void	test_repeated_parse_serialize_cleanup(void)
{
	size_t	i;

	i = 0u;
	while (i < HARDENING_ITERATIONS)
	{
		assert_round_trip(g_request, sizeof(g_request) - 1u);
		assert_round_trip(g_response, sizeof(g_response) - 1u);
		i++;
	}
	printf("PASS test_repeated_parse_serialize_cleanup\n");
}

static void	test_header_value_preserves_later_colons(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_parse(g_request, sizeof(g_request) - 1u, &message)
		== HTTTP_OK);
	assert(strcmp(htttp_message_get_header(&message, "trace"),
			"room:a:join") == 0);
	htttp_message_free(&message);
	printf("PASS test_header_value_preserves_later_colons\n");
}

static void	test_every_result_has_diagnostic_text(void)
{
	int	result;

	result = HTTTP_OK;
	while (result <= HTTTP_ERR_NO_HANDLER)
	{
		assert(htttp_result_string((t_htttp_result)result) != NULL);
		assert(htttp_result_string((t_htttp_result)result)[0] != '\0');
		result++;
	}
	assert(htttp_result_string((t_htttp_result)-1) == NULL);
	assert(htttp_result_string((t_htttp_result)(HTTTP_ERR_NO_HANDLER + 1))
		== NULL);
	printf("PASS test_every_result_has_diagnostic_text\n");
}

int	main(void)
{
	test_all_shorter_prefixes_fail_atomically();
	test_all_line_delimiter_mutations_fail();
	test_repeated_parse_serialize_cleanup();
	test_header_value_preserves_later_colons();
	test_every_result_has_diagnostic_text();
	return (0);
}
