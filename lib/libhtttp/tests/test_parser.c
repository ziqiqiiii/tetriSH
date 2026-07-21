#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void	*__real_malloc(size_t size);
void	*__real_realloc(void *ptr, size_t size);

static int		g_fail_malloc;
static size_t	g_mallocs_before_failure;
static int		g_fail_realloc;
static size_t	g_reallocs_before_failure;

void	*__wrap_malloc(size_t size)
{
	if (g_fail_malloc && g_mallocs_before_failure == 0u)
	{
		g_fail_malloc = 0;
		return (NULL);
	}
	if (g_fail_malloc)
		g_mallocs_before_failure--;
	return (__real_malloc(size));
}

void	*__wrap_realloc(void *ptr, size_t size)
{
	if (g_fail_realloc && g_reallocs_before_failure == 0u)
	{
		g_fail_realloc = 0;
		return (NULL);
	}
	if (g_fail_realloc)
		g_reallocs_before_failure--;
	return (__real_realloc(ptr, size));
}

static void	fail_malloc_after(size_t successful_calls)
{
	g_fail_malloc = 1;
	g_mallocs_before_failure = successful_calls;
}

static void	fail_realloc_after(size_t successful_calls)
{
	g_fail_realloc = 1;
	g_reallocs_before_failure = successful_calls;
}

static t_htttp_result	parse_text(const char *text, t_htttp_message *message)
{
	return (htttp_parse((const unsigned char *)text, strlen(text), message));
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

static void	assert_parse_error(const unsigned char *data, size_t data_len,
		t_htttp_result expected)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_parse(data, data_len, &message) == expected);
	assert_message_empty(&message);
	htttp_message_free(&message);
}

static unsigned char	*build_header_message(size_t header_count,
		size_t *message_len)
{
	unsigned char	*data;
	size_t			capacity;
	size_t			used;
	size_t			i;
	int				written;

	capacity = 4096u;
	data = malloc(capacity);
	assert(data != NULL);
	used = (size_t)snprintf((char *)data, capacity,
			"JOIN /room/a HTTTP/1.0\r\n");
	i = 0u;
	while (i < header_count)
	{
		written = snprintf((char *)data + used, capacity - used,
				"X-%zu: value\r\n", i);
		assert(written > 0 && (size_t)written < capacity - used);
		used += (size_t)written;
		i++;
	}
	assert(used + 2u <= capacity);
	memcpy(data + used, "\r\n", 2u);
	*message_len = used + 2u;
	return (data);
}

static void	assert_text_error(const char *text, t_htttp_result expected)
{
	assert_parse_error((const unsigned char *)text, strlen(text), expected);
}

static void	test_parse_request_and_extension_method(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(parse_text("JOIN /room/a HTTTP/1.0\r\n\r\n", &message) == HTTTP_OK);
	assert(message.type == HTTTP_MESSAGE_REQUEST);
	assert(strcmp(message.method, "JOIN") == 0);
	assert(strcmp(message.path, "/room/a") == 0);
	assert(message.header_count == 0u);
	htttp_message_free(&message);
	assert(parse_text("PAUSE /room/a HTTTP/1.0\r\n"
			"X-Extension: enabled\r\n\r\n", &message) == HTTTP_OK);
	assert(strcmp(message.method, "PAUSE") == 0);
	assert(strcmp(htttp_message_get_header(&message, "x-extension"),
			"enabled") == 0);
	htttp_message_free(&message);
	printf("PASS test_parse_request_and_extension_method\n");
}

static void	test_parse_response_and_header_values(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(parse_text("HTTTP/1.0 409 INVALID_MOVE\r\n"
			"Date: Thu, 01 Jan 1970 00:00:00 GMT\r\n\r\n", &message)
		== HTTTP_OK);
	assert(message.type == HTTTP_MESSAGE_RESPONSE);
	assert(message.status_code == 409u);
	assert(strcmp(message.reason, "INVALID_MOVE") == 0);
	assert(strcmp(htttp_message_get_header(&message, "date"),
			"Thu, 01 Jan 1970 00:00:00 GMT") == 0);
	htttp_message_free(&message);
	assert(parse_text("JOIN /room/a HTTTP/1.0\r\n"
			"Trace: \t room:a:join \t\r\n\r\n", &message) == HTTTP_OK);
	assert(strcmp(htttp_message_get_header(&message, "Trace"),
			"room:a:join") == 0);
	htttp_message_free(&message);
	printf("PASS test_parse_response_and_header_values\n");
}

static void	test_parsed_message_owns_input(void)
{
	t_htttp_message	message;
	unsigned char	input[] = "MOVE /room/r1/player/p17 HTTTP/1.0\r\n"
		"Player-Id: p17\r\n\r\n";

	htttp_message_init(&message);
	assert(htttp_parse(input, sizeof(input) - 1u, &message) == HTTTP_OK);
	memset(input, 'X', sizeof(input) - 1u);
	assert(strcmp(message.method, "MOVE") == 0);
	assert(strcmp(message.path, "/room/r1/player/p17") == 0);
	assert(strcmp(htttp_message_get_header(&message, "Player-Id"), "p17") == 0);
	htttp_message_free(&message);
	printf("PASS test_parsed_message_owns_input\n");
}

static void	test_reject_malformed_request_lines(void)
{
	static const unsigned char	nul_method[] =
		"JO\0IN /room/a HTTTP/1.0\r\n\r\n";
	static const unsigned char	control_path[] =
		"JOIN /room/\x01" "a HTTTP/1.0\r\n\r\n";
	static const unsigned char	nul_path[] =
		"JOIN /room/\0a HTTTP/1.0\r\n\r\n";
	static const unsigned char	nonascii_path[] =
		"JOIN /room/\x80 HTTTP/1.0\r\n\r\n";

	assert_text_error("join /room/a HTTTP/1.0\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("J@IN /room/a HTTTP/1.0\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN room/a HTTTP/1.0\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN /room a HTTTP/1.0\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN /room/a\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN /room/a HTTTP/2.0\r\n\r\n",
		HTTTP_ERR_UNSUPPORTED_VERSION);
	assert_text_error("JOIN /room/a HTTTP/1.0 EXTRA\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN /room/a HTTTP/1.0\n\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("JOIN /room/a HTTTP/1.0\rX\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_parse_error(nul_method, sizeof(nul_method) - 1u,
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_parse_error(control_path, sizeof(control_path) - 1u,
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_parse_error(nul_path, sizeof(nul_path) - 1u,
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_parse_error(nonascii_path, sizeof(nonascii_path) - 1u,
		HTTTP_ERR_MALFORMED_START_LINE);
	printf("PASS test_reject_malformed_request_lines\n");
}

static void	test_reject_malformed_status_lines(void)
{
	assert_text_error("HTTTP/2.0 200 OK\r\n\r\n", HTTTP_ERR_UNSUPPORTED_VERSION);
	assert_text_error("HTTTP/1.0 20 OK\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("HTTTP/1.0 099 Low\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("HTTTP/1.0 600 High\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("HTTTP/1.0 200\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	assert_text_error("HTTTP/1.0 200 O\tK\r\n\r\n",
		HTTTP_ERR_MALFORMED_START_LINE);
	printf("PASS test_reject_malformed_status_lines\n");
}

static void	test_reject_malformed_headers(void)
{
	static const unsigned char	nul_value[] =
		"JOIN /room/a HTTTP/1.0\r\nX-Test: A\0B\r\n\r\n";
	static const unsigned char	control_name[] =
		"JOIN /room/a HTTTP/1.0\r\nX\x01-Test: value\r\n\r\n";
	static const unsigned char	del_name[] =
		"JOIN /room/a HTTTP/1.0\r\nX\x7f-Test: value\r\n\r\n";
	static const unsigned char	nonascii_name[] =
		"JOIN /room/a HTTTP/1.0\r\nX\x80-Test: value\r\n\r\n";
	static const unsigned char	control_value[] =
		"JOIN /room/a HTTTP/1.0\r\nX-Test: A\x01" "B\r\n\r\n";
	static const unsigned char	del_value[] =
		"JOIN /room/a HTTTP/1.0\r\nX-Test: \x7f\r\n\r\n";
	static const unsigned char	nonascii_value[] =
		"JOIN /room/a HTTTP/1.0\r\nX-Test: \x80\r\n\r\n";

	assert_text_error("JOIN /room/a HTTTP/1.0\r\nMissing-Colon\r\n\r\n",
		HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\nName : value\r\n\r\n",
		HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\n: value\r\n\r\n",
		HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\nX-Test: value\r\n"
		" continued\r\n\r\n", HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\nX-Test: value\n\n",
		HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\nX-Test: value\rX\r\n",
		HTTTP_ERR_MALFORMED_HEADER);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\nPlayer-Id: p17\r\n"
		"player-id: p18\r\n\r\n", HTTTP_ERR_DUPLICATE_HEADER);
	assert_parse_error(nul_value, sizeof(nul_value) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(control_name, sizeof(control_name) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(del_name, sizeof(del_name) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(nonascii_name, sizeof(nonascii_name) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(control_value, sizeof(control_value) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(del_value, sizeof(del_value) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	assert_parse_error(nonascii_value, sizeof(nonascii_value) - 1u,
		HTTTP_ERR_MALFORMED_HEADER);
	printf("PASS test_reject_malformed_headers\n");
}

static void	test_header_count_boundary(void)
{
	t_htttp_message	message;
	unsigned char	*data;
	size_t			used;

	htttp_message_init(&message);
	data = build_header_message(HTTTP_MAX_HEADERS, &used);
	assert(htttp_parse(data, used, &message) == HTTTP_OK);
	assert(message.header_count == HTTTP_MAX_HEADERS);
	htttp_message_free(&message);
	free(data);
	data = build_header_message(HTTTP_MAX_HEADERS + 1u, &used);
	assert(htttp_parse(data, used, &message) == HTTTP_ERR_TOO_MANY_HEADERS);
	assert(message.headers == NULL);
	assert(message.header_count == 0u);
	htttp_message_free(&message);
	free(data);
	printf("PASS test_header_count_boundary\n");
}

static void	test_parser_allocation_failures_are_atomic(void)
{
	static const unsigned char	no_headers[] =
		"JOIN /room/a HTTTP/1.0\r\n\r\n";
	static const unsigned char	one_header[] =
		"JOIN /room/a HTTTP/1.0\r\nX-One: value\r\n\r\n";
	static const unsigned char	two_headers[] =
		"JOIN /room/a HTTTP/1.0\r\nX-One: value\r\nX-Two: value\r\n\r\n";
	t_htttp_message				message;

	htttp_message_init(&message);
	fail_malloc_after(1u);
	assert(htttp_parse(no_headers, sizeof(no_headers) - 1u, &message)
		== HTTTP_ERR_NO_MEMORY);
	assert_message_empty(&message);
	fail_malloc_after(5u);
	assert(htttp_parse(one_header, sizeof(one_header) - 1u, &message)
		== HTTTP_ERR_NO_MEMORY);
	assert_message_empty(&message);
	fail_realloc_after(1u);
	assert(htttp_parse(two_headers, sizeof(two_headers) - 1u, &message)
		== HTTTP_ERR_NO_MEMORY);
	assert_message_empty(&message);
	htttp_message_free(&message);
	printf("PASS test_parser_allocation_failures_are_atomic\n");
}

static void	test_parser_api_and_atomic_output(void)
{
	t_htttp_message	message;
	unsigned char	byte;
	const char		*valid;

	byte = 'X';
	valid = "JOIN /room/a HTTTP/1.0\r\n\r\n";
	htttp_message_init(&message);
	assert(htttp_parse(NULL, 1u, &message) == HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_parse(&byte, 0u, &message) == HTTTP_ERR_MALFORMED_START_LINE);
	assert(htttp_parse(&byte, HTTTP_MAX_MESSAGE_SIZE + 1u, &message)
		== HTTTP_ERR_TOO_LARGE);
	assert(htttp_parse((const unsigned char *)valid, strlen(valid), NULL)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	assert(htttp_parse((const unsigned char *)valid, strlen(valid), &message)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(strcmp(message.method, "MOVE") == 0);
	htttp_message_free(&message);
	assert_text_error("JOIN /room/a HTTTP/1.0\r\n",
		HTTTP_ERR_MALFORMED_HEADER);
	printf("PASS test_parser_api_and_atomic_output\n");
}

int	main(void)
{
	test_parse_request_and_extension_method();
	test_parse_response_and_header_values();
	test_parsed_message_owns_input();
	test_reject_malformed_request_lines();
	test_reject_malformed_status_lines();
	test_reject_malformed_headers();
	test_header_count_boundary();
	test_parser_allocation_failures_are_atomic();
	test_parser_api_and_atomic_output();
	return (0);
}
