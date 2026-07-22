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

static void	test_format_rfc1123_dates(void)
{
	char	date[HTTTP_DATE_BUFSIZE];

	memset(date, 'X', sizeof(date));
	assert(htttp_format_date((time_t)0, date) == HTTTP_OK);
	assert(strcmp(date, "Thu, 01 Jan 1970 00:00:00 GMT") == 0);
	assert(strlen(date) == HTTTP_DATE_BUFSIZE - 1u);
	assert(htttp_format_date((time_t)784111777, date) == HTTTP_OK);
	assert(strcmp(date, "Sun, 06 Nov 1994 08:49:37 GMT") == 0);
	assert(htttp_format_date((time_t)-1, date) == HTTTP_OK);
	assert(strcmp(date, "Wed, 31 Dec 1969 23:59:59 GMT") == 0);
	assert(htttp_format_date((time_t)-2208988800LL, date) == HTTTP_OK);
	assert(strcmp(date, "Mon, 01 Jan 1900 00:00:00 GMT") == 0);
	assert(htttp_format_date((time_t)253402300799LL, date) == HTTTP_OK);
	assert(strcmp(date, "Fri, 31 Dec 9999 23:59:59 GMT") == 0);
	memset(date, 'X', sizeof(date));
	assert(htttp_format_date((time_t)-2208988801LL, date)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(date[0] == '\0');
	memset(date, 'X', sizeof(date));
	assert(htttp_format_date((time_t)253402300800LL, date)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(date[0] == '\0');
	assert(htttp_format_date((time_t)0, NULL) == HTTTP_ERR_INVALID_ARGUMENT);
	printf("PASS test_format_rfc1123_dates\n");
}

static void	test_response_requires_date(void)
{
	static const char	*invalid_dates[] = {
		"Fry, 01 Jan 1970 00:00:00 GMT",
		"Thu, 01 Foo 1970 00:00:00 GMT",
		"Thu, 00 Jan 1970 00:00:00 GMT",
		"Thu, 32 Jan 1970 00:00:00 GMT",
		"Mon, 31 Feb 2025 00:00:00 GMT",
		"Sat, 29 Feb 2025 00:00:00 GMT",
		"Fri, 01 Jan 1970 00:00:00 GMT",
		"Thu, 01 Jan 1899 00:00:00 GMT",
		"Thu, 01 Jan 1970 24:00:00 GMT",
		"Thu, 01 Jan 1970 00:60:00 GMT",
		"Thu, 01 Jan 1970 00:00:61 GMT",
		"Thu, 01 Jan 1970 00:00:00 UTC"
	};
	t_htttp_message	message;
	char			date[HTTTP_DATE_BUFSIZE];
	size_t			i;

	htttp_message_init(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Date", "") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Date", "yesterday") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Date",
			"Thu; 01 Jan 1970 00:00:00 GMT") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Date",
			"Thu, 01 Jan 0000 00:00:00 GMT") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	i = 0u;
	while (i < sizeof(invalid_dates) / sizeof(invalid_dates[0]))
	{
		assert(htttp_message_set_header(&message, "Date", invalid_dates[i])
			== HTTTP_OK);
		assert(htttp_validate(&message, 0u)
			== HTTTP_ERR_MISSING_REQUIRED_HEADER);
		i++;
	}
	assert(htttp_message_set_header(&message, "Date",
			"Thu, 29 Feb 2024 00:00:00 GMT") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	assert(htttp_format_date((time_t)0, date) == HTTTP_OK);
	assert(htttp_message_set_header(&message, "date", date) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	htttp_message_free(&message);
	printf("PASS test_response_requires_date\n");
}

static void	test_command_body_requires_command_type(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_body(&message, "LEFT", 4u) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_STATE) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "content-type",
			HTTTP_CONTENT_TYPE_COMMAND) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Content-Type",
			"Application/Tetris-Command") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Content-Type",
			"application/tetris-command; charset=utf-8") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "PAUSE", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_body(&message, "NOW", 3u) == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_STATE) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_COMMAND) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	htttp_message_free(&message);
	printf("PASS test_command_body_requires_command_type\n");
}

static void	test_state_body_requires_state_type(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "STATE", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_body(&message, "map", 3u) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_COMMAND) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_STATE) == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	htttp_message_free(&message);
	printf("PASS test_state_body_requires_state_type\n");
}

static void	test_authenticated_request_requires_player_id(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Player-Id", "") == HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "Player-Id", " \t") == HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(htttp_message_set_header(&message, "player-id", "p17") == HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_OK);
	assert(htttp_message_set_header(&message, "Player-Id", " \t p18 \t")
		== HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_OK);
	htttp_message_free(&message);
	printf("PASS test_authenticated_request_requires_player_id\n");
}

static void	test_validation_arguments_and_immutability(void)
{
	t_htttp_message	message;
	t_htttp_message	before;
	const char		*player_id;

	htttp_message_init(&message);
	assert(htttp_validate(NULL, 0u) == HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Player-Id", "p17") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Content-Type",
			HTTTP_CONTENT_TYPE_COMMAND) == HTTTP_OK);
	assert(htttp_message_set_body(&message, "LEFT", 4u) == HTTTP_OK);
	player_id = htttp_message_get_header(&message, "Player-Id");
	before = message;
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_OK);
	assert(message.type == before.type);
	assert(message.method == before.method && strcmp(message.method, "MOVE") == 0);
	assert(message.path == before.path && strcmp(message.path, "/room/a") == 0);
	assert(message.status_code == before.status_code);
	assert(message.reason == before.reason);
	assert(message.headers == before.headers);
	assert(message.header_count == before.header_count);
	assert(message.headers[0].name == before.headers[0].name);
	assert(message.headers[0].value == before.headers[0].value);
	assert(strcmp(message.headers[0].name, "Player-Id") == 0);
	assert(strcmp(message.headers[0].value, "p17") == 0);
	assert(message.headers[1].name == before.headers[1].name);
	assert(message.headers[1].value == before.headers[1].value);
	assert(strcmp(message.headers[1].name, "Content-Type") == 0);
	assert(strcmp(message.headers[1].value, HTTTP_CONTENT_TYPE_COMMAND) == 0);
	assert(message.body == before.body);
	assert(message.body_len == before.body_len);
	assert(memcmp(message.body, "LEFT", 4u) == 0);
	assert(htttp_message_get_header(&message, "Player-Id") == player_id);
	assert(htttp_validate(&message, 0x80u) == HTTTP_ERR_INVALID_ARGUMENT);
	assert(htttp_validate(&message,
			HTTTP_VALIDATE_AUTHENTICATED_REQUEST | 0x80u)
		== HTTTP_ERR_INVALID_ARGUMENT);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_ERR_INVALID_ARGUMENT);
	htttp_message_free(&message);
	message.type = (t_htttp_message_type)99;
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_init(&message);
	printf("PASS test_validation_arguments_and_immutability\n");
}

static void	test_reject_malformed_message_shapes(void)
{
	t_htttp_message	message;

	htttp_message_init(&message);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	assert(htttp_message_make_request(&message, "", "/room/a") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a\nother")
		== HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "") == HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Bad\rName", "value")
		== HTTTP_OK);
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "Player-Id", "p17\nforged")
		== HTTTP_OK);
	assert(htttp_validate(&message, HTTTP_VALIDATE_AUTHENTICATED_REQUEST)
		== HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_header(&message, "X-Test", "one") == HTTTP_OK);
	append_raw_header(&message, "x-test", "two");
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.status_code = 200u;
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	message.status_code = 0u;
	message.reason = copy_text("contaminated");
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	message.method = copy_text("JOIN");
	message.path = copy_text("/room/a");
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.body_len = 1u;
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	message.body_len = 0u;
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	message.header_count = 1u;
	assert(htttp_validate(&message, 0u) == HTTTP_ERR_INVALID_MESSAGE);
	message.header_count = 0u;
	htttp_message_free(&message);
	printf("PASS test_reject_malformed_message_shapes\n");
}

int	main(void)
{
	test_format_rfc1123_dates();
	test_response_requires_date();
	test_command_body_requires_command_type();
	test_state_body_requires_state_type();
	test_authenticated_request_requires_player_id();
	test_validation_arguments_and_immutability();
	test_reject_malformed_message_shapes();
	return (0);
}
