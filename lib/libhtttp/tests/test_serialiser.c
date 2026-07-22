#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void	*__real_malloc(size_t size);

static int	g_fail_malloc;
static int	g_track_malloc;
static size_t	g_last_malloc_size;

void	*__wrap_malloc(size_t size)
{
	if (g_fail_malloc)
	{
		g_fail_malloc = 0;
		return (NULL);
	}
	if (g_track_malloc)
		g_last_malloc_size = size;
	return (__real_malloc(size));
}

static char	*copy_text(const char *text)
{
	char	*copy;
	size_t	len;

	len = strlen(text) + 1u;
	copy = malloc(len);
	assert(copy != NULL);
	memcpy(copy, text, len);
	return (copy);
}

static void	append_raw_header(t_htttp_message *message,
		const char *name, const char *value)
{
	t_htttp_header	*headers;

	headers = realloc(message->headers,
			(message->header_count + 1u) * sizeof(*headers));
	assert(headers != NULL);
	message->headers = headers;
	message->headers[message->header_count].name = copy_text(name);
	message->headers[message->header_count].value = copy_text(value);
	message->header_count++;
}

static void	assert_wire(const t_htttp_message *message,
		const unsigned char *expected, size_t expected_len)
{
	unsigned char	*wire;
	size_t			wire_len;

	wire = (unsigned char *)1;
	wire_len = 1u;
	g_last_malloc_size = 0u;
	g_track_malloc = 1;
	assert(htttp_serialize(message, &wire, &wire_len) == HTTTP_OK);
	g_track_malloc = 0;
	assert(wire != NULL);
	assert(wire_len == expected_len);
	assert(g_last_malloc_size == expected_len);
	assert(memcmp(wire, expected, expected_len) == 0);
	free(wire);
}

static void	test_serialize_canonical_request(void)
{
	static const unsigned char	expected[] =
		"MOVE /room/r1/player/p17 HTTTP/1.0\r\n"
		"Content-Type: application/tetris-command\r\n"
		"Player-Id: p17\r\n"
		"Content-Length: 4\r\n"
		"\r\n"
		"LEFT";
	t_htttp_message			message;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "MOVE",
			"/room/r1/player/p17") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_COMMAND) == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Player-Id", "p17") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "content-length", "999")
		== HTTTP_OK);
	assert(htttp_message_set_body(&message, "LEFT", 4u) == HTTTP_OK);
	assert_wire(&message, expected, sizeof(expected) - 1u);
	htttp_message_free(&message);
	printf("PASS test_serialize_canonical_request\n");
}

static void	test_serialize_response_and_empty_body(void)
{
	static const unsigned char	expected[] =
		"HTTTP/1.0 409 INVALID_MOVE\r\n"
		"Date: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
		"\r\n";
	t_htttp_message			message;

	htttp_message_init(&message);
	assert(htttp_message_make_response(&message, 409u, "INVALID_MOVE")
		== HTTTP_OK);
	assert(htttp_message_set_header(&message, "Date",
			"Thu, 01 Jan 1970 00:00:00 GMT") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Content-Length", "123")
		== HTTTP_OK);
	assert_wire(&message, expected, sizeof(expected) - 1u);
	htttp_message_free(&message);
	printf("PASS test_serialize_response_and_empty_body\n");
}

static void	test_parse_serialize_parse_round_trip(void)
{
	static const unsigned char	body[] = {'A', '\0', 'B'};
	static const char			prefix[] =
		"STATE /room/a HTTTP/1.0\r\n"
		"Content-Type: application/tetris-state\r\n"
		"X-Trace: room:a:state\r\n"
		"Content-Length: 3\r\n\r\n";
	t_htttp_message				first;
	t_htttp_message				second;
	unsigned char				*input;
	unsigned char				*wire;
	size_t					wire_len;
	size_t					prefix_len;
	size_t					i;

	prefix_len = strlen(prefix);
	input = malloc(prefix_len + sizeof(body));
	assert(input != NULL);
	memcpy(input, prefix, prefix_len);
	memcpy(input + prefix_len, body, sizeof(body));
	htttp_message_init(&first);
	htttp_message_init(&second);
	assert(htttp_parse(input, prefix_len + sizeof(body), &first) == HTTTP_OK);
	wire = NULL;
	wire_len = 0u;
	assert(htttp_serialize(&first, &wire, &wire_len) == HTTTP_OK);
	assert(htttp_parse(wire, wire_len, &second) == HTTTP_OK);
	assert(second.type == first.type);
	assert(strcmp(second.method, first.method) == 0);
	assert(strcmp(second.path, first.path) == 0);
	assert(second.header_count == first.header_count);
	i = 0u;
	while (i < first.header_count)
	{
		assert(strcmp(second.headers[i].name, first.headers[i].name) == 0);
		assert(strcmp(second.headers[i].value, first.headers[i].value) == 0);
		i++;
	}
	assert(second.body_len == first.body_len);
	assert(memcmp(second.body, first.body, first.body_len) == 0);
	free(wire);
	free(input);
	htttp_message_free(&second);
	htttp_message_free(&first);
	printf("PASS test_parse_serialize_parse_round_trip\n");
}

static void	assert_invalid_message(t_htttp_message *message)
{
	unsigned char	*wire;
	size_t			wire_len;

	wire = (unsigned char *)1;
	wire_len = 1u;
	assert(htttp_serialize(message, &wire, &wire_len)
		== HTTTP_ERR_INVALID_MESSAGE);
	assert(wire == NULL);
	assert(wire_len == 0u);
}

static void	test_reject_invalid_structured_messages(void)
{
	t_htttp_message	message;
	char			nonascii_method[] = {'M', (char)0x80, '\0'};
	char			nonascii_path[] = {'/', 'r', (char)0x80, '\0'};
	char			nonascii_reason[] = {'B', (char)0x80, '\0'};
	char			nonascii_name[] = {'X', (char)0x80, '\0'};
	char			nonascii_value[] = {'v', (char)0x80, '\0'};

	htttp_message_init(&message);
	assert_invalid_message(&message);
	assert(htttp_message_make_request(&message, "", "/room/a") == HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MOVE", "") == HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, nonascii_method, "/room/a")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MO\rVE", "/room/a")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "") == HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, nonascii_reason)
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MOVE", nonascii_path)
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a\nother")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 600u, "Bad") == HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "Bad\rReason")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Name", nonascii_value)
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, nonascii_name, "value")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.status_code = 200u;
	assert_invalid_message(&message);
	message.status_code = 0u;
	message.reason = copy_text("contaminated");
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	message.method = copy_text("JOIN");
	message.path = copy_text("/room/a");
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "X-Test", "one") == HTTTP_OK);
	append_raw_header(&message, "x-test", "two");
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 99u, "Bad") == HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Bad\rName", "value")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Name", "bad\nvalue")
		== HTTTP_OK);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	message.type = (t_htttp_message_type)99;
	assert_invalid_message(&message);
	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.body_len = 1u;
	assert_invalid_message(&message);
	message.body_len = 0u;
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.body = malloc(1u);
	assert(message.body != NULL);
	assert_invalid_message(&message);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.header_count = 1u;
	assert_invalid_message(&message);
	message.header_count = 0u;
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.header_count = HTTTP_MAX_HEADERS + 1u;
	assert_invalid_message(&message);
	message.header_count = 0u;
	htttp_message_free(&message);
	printf("PASS test_reject_invalid_structured_messages\n");
}

static void	test_failure_outputs_and_allocation(void)
{
	t_htttp_message	message;
	unsigned char	*wire;
	size_t			wire_len;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	wire = (unsigned char *)1;
	wire_len = 1u;
	assert(htttp_serialize(NULL, &wire, &wire_len) == HTTTP_ERR_INVALID_ARGUMENT);
	assert(wire == NULL);
	assert(wire_len == 0u);
	assert(htttp_serialize(&message, NULL, &wire_len)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(wire_len == 0u);
	wire = (unsigned char *)1;
	assert(htttp_serialize(&message, &wire, NULL)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(wire == NULL);
	wire_len = 1u;
	g_fail_malloc = 1;
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_ERR_NO_MEMORY);
	assert(wire == NULL);
	assert(wire_len == 0u);
	htttp_message_free(&message);
	printf("PASS test_failure_outputs_and_allocation\n");
}

static void	test_complete_output_size_boundary(void)
{
	t_htttp_message	message;
	unsigned char	*body;
	unsigned char	*wire;
	size_t			wire_len;

	body = malloc(65487u);
	assert(body != NULL);
	memset(body, 'S', 65487u);
	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "STATE", "/room/a")
		== HTTTP_OK);
	assert(htttp_message_set_body(&message, body, 65486u) == HTTTP_OK);
	wire = NULL;
	wire_len = 0u;
	g_last_malloc_size = 0u;
	g_track_malloc = 1;
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_OK);
	g_track_malloc = 0;
	assert(wire_len == HTTTP_MAX_MESSAGE_SIZE);
	assert(g_last_malloc_size == HTTTP_MAX_MESSAGE_SIZE);
	assert(memcmp(wire + wire_len - 65486u, body, 65486u) == 0);
	free(wire);
	assert(htttp_message_set_body(&message, body, 65487u) == HTTTP_OK);
	wire = (unsigned char *)1;
	wire_len = 1u;
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_ERR_TOO_LARGE);
	assert(wire == NULL);
	assert(wire_len == 0u);
	htttp_message_free(&message);
	free(body);
	printf("PASS test_complete_output_size_boundary\n");
}

static void	add_numbered_headers(t_htttp_message *message, size_t count)
{
	char	name[32];
	size_t	i;

	i = 0u;
	while (i < count)
	{
		assert(snprintf(name, sizeof(name), "X-Header-%zu", i) > 0);
		assert(htttp_message_set_header(message, name, "value") == HTTTP_OK);
		i++;
	}
}

static void	test_generated_content_length_respects_header_limit(void)
{
	t_htttp_message	message;
	t_htttp_message	parsed;
	unsigned char	*wire;
	size_t			wire_len;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	add_numbered_headers(&message, HTTTP_MAX_HEADERS);
	assert(htttp_message_set_body(&message, "X", 1u) == HTTTP_OK);
	wire = (unsigned char *)1;
	wire_len = 1u;
	assert(htttp_serialize(&message, &wire, &wire_len)
		== HTTTP_ERR_TOO_MANY_HEADERS);
	assert(wire == NULL);
	assert(wire_len == 0u);
	htttp_message_free(&message);
	htttp_message_init(&message);
	htttp_message_init(&parsed);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	add_numbered_headers(&message, HTTTP_MAX_HEADERS - 1u);
	assert(htttp_message_set_body(&message, "X", 1u) == HTTTP_OK);
	assert(htttp_serialize(&message, &wire, &wire_len) == HTTTP_OK);
	assert(htttp_parse(wire, wire_len, &parsed) == HTTTP_OK);
	assert(parsed.header_count == HTTTP_MAX_HEADERS);
	free(wire);
	htttp_message_free(&parsed);
	htttp_message_free(&message);
	printf("PASS test_generated_content_length_respects_header_limit\n");
}

int	main(void)
{
	test_serialize_canonical_request();
	test_serialize_response_and_empty_body();
	test_parse_serialize_parse_round_trip();
	test_reject_invalid_structured_messages();
	test_failure_outputs_and_allocation();
	test_complete_output_size_boundary();
	test_generated_content_length_respects_header_limit();
	return (0);
}
