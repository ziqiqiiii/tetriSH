#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	test_required_reason_phrases();
	test_result_text();
	return (0);
}
