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
	if (g_fail_realloc)
	{
		g_fail_realloc = 0;
		return (NULL);
	}
	return (__real_realloc(ptr, size));
}

static void	fail_malloc_after(size_t successful_calls)
{
	g_fail_malloc = 1;
	g_mallocs_before_failure = successful_calls;
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

static void	test_init_and_free_are_idempotent(void)
{
	t_htttp_message	message;

	memset(&message, 0xa5, sizeof(message));
	htttp_message_init(&message);
	assert(message.type == HTTTP_MESSAGE_REQUEST);
	assert(message.method == NULL);
	assert(message.path == NULL);
	assert(message.status_code == 0u);
	assert(message.reason == NULL);
	assert(message.headers == NULL);
	assert(message.header_count == 0u);
	assert(message.body == NULL);
	assert(message.body_len == 0u);
	htttp_message_free(&message);
	htttp_message_free(&message);
	htttp_message_init(NULL);
	htttp_message_free(NULL);
	printf("PASS test_init_and_free_are_idempotent\n");
}

static void	test_free_releases_partial_message(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	message.method = copy_text("MOVE");
	message.path = copy_text("/room/a");
	message.reason = copy_text("Conflict");
	message.headers = calloc(1u, sizeof(*message.headers));
	assert(message.headers != NULL);
	message.headers[0].name = copy_text("Player-Id");
	message.headers[0].value = copy_text("p17");
	message.header_count = 1u;
	message.body = malloc(3u);
	assert(message.body != NULL);
	memcpy(message.body, "ABC", 3u);
	message.body_len = 3u;
	htttp_message_free(&message);
	assert(message.method == NULL);
	assert(message.path == NULL);
	assert(message.reason == NULL);
	assert(message.headers == NULL);
	assert(message.header_count == 0u);
	assert(message.body == NULL);
	assert(message.body_len == 0u);
	printf("PASS test_free_releases_partial_message\n");
}

static void	test_request_and_response_are_owned(void)
{
	t_htttp_message	request;
	t_htttp_message	response;
	char			method[] = "MOVE";
	char			path[] = "/room/r1/player/p17";
	char			reason[] = "Conflict";

	htttp_message_init(&request);
	htttp_message_init(&response);
	assert(htttp_message_make_request(&request, method, path) == HTTTP_OK);
	method[0] = 'X';
	path[1] = 'X';
	assert(request.type == HTTTP_MESSAGE_REQUEST);
	assert(strcmp(request.method, "MOVE") == 0);
	assert(strcmp(request.path, "/room/r1/player/p17") == 0);
	assert(htttp_message_make_response(&response, 409u, reason) == HTTTP_OK);
	reason[0] = 'X';
	assert(response.type == HTTTP_MESSAGE_RESPONSE);
	assert(response.status_code == 409u);
	assert(strcmp(response.reason, "Conflict") == 0);
	htttp_message_free(&request);
	htttp_message_free(&response);
	printf("PASS test_request_and_response_are_owned\n");
}

static void	test_constructors_require_empty_output(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_message_make_request(NULL, "JOIN", "/room/a")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_make_request(&message, NULL, "/room/a")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_make_response(&message, 200u, NULL)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_make_response(&message, 200u, "OK")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(strcmp(message.method, "JOIN") == 0);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(message.type == HTTTP_MESSAGE_RESPONSE);
	assert(strcmp(message.reason, "OK") == 0);
	htttp_message_free(&message);
	assert(htttp_message_set_header(&message, "X-Test", "value") == HTTTP_OK);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(strcmp(htttp_message_get_header(&message, "X-Test"), "value") == 0);
	htttp_message_free(&message);
	assert(htttp_message_set_body(&message, "ABC", 3u) == HTTTP_OK);
	assert(htttp_message_make_response(&message, 200u, "OK")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(message.body_len == 3u);
	assert(memcmp(message.body, "ABC", 3u) == 0);
	htttp_message_free(&message);
	printf("PASS test_constructors_require_empty_output\n");
}

static void	test_headers_are_owned_and_case_insensitive(void)
{
	t_htttp_message	message;
	char			name[] = "Player-Id";
	char			value[] = "p17";
	char			replacement[] = "p18";

	htttp_message_init(&message);
	assert(htttp_message_set_header(&message, name, value) == HTTTP_OK);
	name[0] = 'X';
	value[0] = 'X';
	assert(strcmp(htttp_message_get_header(&message, "player-id"), "p17") == 0);
	assert(htttp_message_set_header(&message, "PLAYER-ID", replacement)
		== HTTTP_OK);
	replacement[0] = 'X';
	assert(message.header_count == 1u);
	assert(strcmp(htttp_message_get_header(&message, "Player-Id"), "p18") == 0);
	assert(htttp_message_get_header(&message, "Missing") == NULL);
	assert(htttp_message_get_header(NULL, "Player-Id") == NULL);
	assert(htttp_message_get_header(&message, NULL) == NULL);
	assert(htttp_message_set_header(&message, NULL, "value")
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_set_header(&message, "Name", NULL)
		== HTTTP_ERR_INVALID_ARGUMENT);
	htttp_message_free(&message);
	printf("PASS test_headers_are_owned_and_case_insensitive\n");
}

static void	test_header_limit_is_enforced(void)
{
	t_htttp_message	message;
	char			name[32];
	size_t			i;

	htttp_message_init(&message);
	i = 0;
	while (i < HTTTP_MAX_HEADERS)
	{
		assert(snprintf(name, sizeof(name), "X-Header-%zu", i) > 0);
		assert(htttp_message_set_header(&message, name, "value") == HTTTP_OK);
		i++;
	}
	assert(message.header_count == HTTTP_MAX_HEADERS);
	assert(htttp_message_set_header(&message, "x-header-0", "updated")
		== HTTTP_OK);
	assert(message.header_count == HTTTP_MAX_HEADERS);
	assert(strcmp(htttp_message_get_header(&message, "X-Header-0"),
			"updated") == 0);
	assert(htttp_message_set_header(&message, "X-Overflow", "value")
		== HTTTP_ERR_TOO_MANY_HEADERS);
	assert(message.header_count == HTTTP_MAX_HEADERS);
	htttp_message_free(&message);
	printf("PASS test_header_limit_is_enforced\n");
}

static void	test_body_copy_and_failure_are_atomic(void)
{
	t_htttp_message	message;
	unsigned char	body[] = {'A', '\0', 'B'};
	unsigned char	replacement[] = {'X', 'Y'};

	htttp_message_init(&message);
	assert(htttp_message_set_body(&message, body, sizeof(body)) == HTTTP_OK);
	body[0] = 'Z';
	assert(message.body_len == 3u);
	assert(message.body[0] == 'A');
	assert(message.body[1] == '\0');
	assert(message.body[2] == 'B');
	assert(htttp_message_set_body(&message, NULL, 1u)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(message.body_len == 3u);
	assert(message.body[0] == 'A');
	assert(htttp_message_set_body(&message, replacement, sizeof(replacement))
		== HTTTP_OK);
	replacement[0] = 'Z';
	assert(message.body_len == 2u);
	assert(message.body[0] == 'X');
	assert(htttp_message_set_body(&message, "x", HTTTP_MAX_MESSAGE_SIZE + 1u)
		== HTTTP_ERR_TOO_LARGE);
	assert(message.body_len == 2u);
	assert(htttp_message_set_body(&message, NULL, 0u) == HTTTP_OK);
	assert(message.body == NULL);
	assert(message.body_len == 0u);
	htttp_message_free(&message);
	printf("PASS test_body_copy_and_failure_are_atomic\n");
}

static void	test_allocation_failures_are_atomic(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	fail_malloc_after(1u);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a")
		== HTTTP_ERR_NO_MEMORY);
	assert(message.method == NULL);
	assert(message.path == NULL);
	fail_malloc_after(0u);
	assert(htttp_message_make_response(&message, 200u, "OK")
		== HTTTP_ERR_NO_MEMORY);
	assert(message.reason == NULL);
	assert(message.status_code == 0u);
	assert(htttp_message_set_header(&message, "X-First", "old") == HTTTP_OK);
	fail_malloc_after(0u);
	assert(htttp_message_set_header(&message, "x-first", "new")
		== HTTTP_ERR_NO_MEMORY);
	assert(strcmp(htttp_message_get_header(&message, "X-First"), "old") == 0);
	fail_malloc_after(1u);
	assert(htttp_message_set_header(&message, "X-Second", "value")
		== HTTTP_ERR_NO_MEMORY);
	assert(message.header_count == 1u);
	assert(htttp_message_get_header(&message, "X-Second") == NULL);
	g_fail_realloc = 1;
	assert(htttp_message_set_header(&message, "X-Third", "value")
		== HTTTP_ERR_NO_MEMORY);
	assert(message.header_count == 1u);
	assert(htttp_message_get_header(&message, "X-Third") == NULL);
	assert(htttp_message_set_body(&message, "old", 3u) == HTTTP_OK);
	fail_malloc_after(0u);
	assert(htttp_message_set_body(&message, "new", 3u)
		== HTTTP_ERR_NO_MEMORY);
	assert(message.body_len == 3u);
	assert(memcmp(message.body, "old", 3u) == 0);
	htttp_message_free(&message);
	printf("PASS test_allocation_failures_are_atomic\n");
}

static void	test_required_reason_phrases(void)
{
	assert(strcmp(htttp_reason_phrase(200u), "OK") == 0);
	assert(strcmp(htttp_reason_phrase(201u), "Created") == 0);
	assert(strcmp(htttp_reason_phrase(400u), "Bad Request") == 0);
	assert(strcmp(htttp_reason_phrase(401u), "Unauthorized") == 0);
	assert(strcmp(htttp_reason_phrase(403u), "Forbidden") == 0);
	assert(strcmp(htttp_reason_phrase(404u), "Not Found") == 0);
	assert(strcmp(htttp_reason_phrase(409u), "Conflict") == 0);
	assert(strcmp(htttp_reason_phrase(413u), "Payload Too Large") == 0);
	assert(strcmp(htttp_reason_phrase(429u), "Too Many Requests") == 0);
	assert(strcmp(htttp_reason_phrase(500u), "Internal Server Error") == 0);
	assert(htttp_reason_phrase(418u) == NULL);
	printf("PASS test_required_reason_phrases\n");
}

static void	test_result_text(void)
{
	int	result;

	result = HTTTP_OK;
	while (result <= HTTTP_ERR_NO_HANDLER)
	{
		assert(htttp_result_string((t_htttp_result)result) != NULL);
		result++;
	}
	assert(strcmp(htttp_result_string(HTTTP_ERR_TOO_LARGE),
			"message too large") == 0);
	assert(htttp_result_string((t_htttp_result)-1) == NULL);
	printf("PASS test_result_text\n");
}

int	main(void)
{
	test_init_and_free_are_idempotent();
	test_free_releases_partial_message();
	test_request_and_response_are_owned();
	test_constructors_require_empty_output();
	test_headers_are_owned_and_case_insensitive();
	test_header_limit_is_enforced();
	test_body_copy_and_failure_are_atomic();
	test_allocation_failures_are_atomic();
	test_required_reason_phrases();
	test_result_text();
	return (0);
}
