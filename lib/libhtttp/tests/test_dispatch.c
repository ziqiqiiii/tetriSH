#include "htttp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int	handle_join(const t_htttp_message *message, void *context)
{
	int	*count;

	count = context;
	assert(strcmp(message->method, "JOIN") == 0);
	(*count)++;
	return (201);
}

static int	handle_first_pause(const t_htttp_message *message, void *context)
{
	int	*count;

	count = context;
	assert(strcmp(message->method, "PAUSE") == 0);
	(*count)++;
	return (201);
}

static int	handle_pause(const t_htttp_message *message, void *context)
{
	int	*count;

	count = context;
	assert(strcmp(message->method, "PAUSE") == 0);
	(*count)++;
	return (202);
}

static int	handle_move(const t_htttp_message *message, void *context)
{
	int	*count;

	count = context;
	assert(strcmp(message->method, "MOVE") == 0);
	(*count)++;
	return (203);
}

static int	handle_null_context(const t_htttp_message *message, void *context)
{
	assert(strcmp(message->method, "STATE") == 0);
	assert(context == NULL);
	return (-7);
}

static void	test_dispatch_parsed_extension_method(void)
{
	static const t_htttp_route	routes[] = {
		{"JOIN", 0u, handle_join},
		{"PAUSE", 0u, handle_pause}
	};
	static const unsigned char	input[] =
		"PAUSE /room/a HTTTP/1.0\r\n\r\n";
	t_htttp_message				message;
	t_htttp_message				before;
	int						count;
	int						handler_result;

	htttp_message_init(&message);
	assert(htttp_parse(input, sizeof(input) - 1u, &message) == HTTTP_OK);
	before = message;
	count = 0;
	handler_result = 0;
	assert(htttp_dispatch(&message, routes, 2u, &count, &handler_result)
		== HTTTP_OK);
	assert(count == 1);
	assert(handler_result == 202);
	assert(message.type == before.type);
	assert(message.method == before.method && strcmp(message.method, "PAUSE") == 0);
	assert(message.path == before.path && strcmp(message.path, "/room/a") == 0);
	assert(message.headers == before.headers);
	assert(message.body == before.body);
	htttp_message_free(&message);
	printf("PASS test_dispatch_parsed_extension_method\n");
}

static void	test_dispatch_uses_first_exact_match(void)
{
	static const t_htttp_route	routes[] = {
		{"pause", 0u, handle_pause},
		{"PAUSE", 0u, handle_first_pause},
		{"PAUSE", 0u, handle_pause}
	};
	t_htttp_message	message;
	int				count;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "PAUSE", "/room/a")
		== HTTTP_OK);
	count = 0;
	handler_result = 0;
	assert(htttp_dispatch(&message, routes, 3u, &count, &handler_result)
		== HTTTP_OK);
	assert(count == 1);
	assert(handler_result == 201);
	htttp_message_free(&message);
	printf("PASS test_dispatch_uses_first_exact_match\n");
}

static void	test_dispatch_supports_null_context(void)
{
	static const t_htttp_route	routes[] = {
		{"STATE", 0u, handle_null_context}
	};
	t_htttp_message	message;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "STATE", "/room/a")
		== HTTTP_OK);
	handler_result = 0;
	assert(htttp_dispatch(&message, routes, 1u, NULL, &handler_result)
		== HTTTP_OK);
	assert(handler_result == -7);
	htttp_message_free(&message);
	printf("PASS test_dispatch_supports_null_context\n");
}

static void	test_unknown_method_and_empty_table(void)
{
	static const t_htttp_route	routes[] = {
		{"JOIN", 0u, handle_join}
	};
	t_htttp_message	message;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 1u, NULL, &handler_result)
		== HTTTP_ERR_NO_HANDLER);
	assert(handler_result == 0);
	handler_result = 99;
	assert(htttp_dispatch(&message, NULL, 0u, NULL, &handler_result)
		== HTTTP_ERR_NO_HANDLER);
	assert(handler_result == 0);
	htttp_message_free(&message);
	printf("PASS test_unknown_method_and_empty_table\n");
}

static void	test_reject_invalid_dispatch_arguments(void)
{
	t_htttp_route	routes[2];
	t_htttp_message	message;
	int				handler_result;
	int				count;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	routes[0].method = "JOIN";
	routes[0].validation_flags = 0u;
	routes[0].handler = handle_join;
	routes[1].method = "PAUSE";
	routes[1].validation_flags = 0u;
	routes[1].handler = NULL;
	handler_result = 99;
	assert(htttp_dispatch(NULL, routes, 1u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(handler_result == 0);
	handler_result = 99;
	assert(htttp_dispatch(&message, NULL, 1u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(handler_result == 0);
	assert(htttp_dispatch(&message, routes, 1u, NULL, NULL)
		== HTTTP_ERR_INVALID_ARGUMENT);
	count = 0;
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 2u, &count, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(count == 0);
	assert(handler_result == 0);
	routes[1].method = NULL;
	routes[1].handler = handle_pause;
	assert(htttp_dispatch(&message, routes, 2u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	routes[1].method = "";
	assert(htttp_dispatch(&message, routes, 2u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	routes[1].method = "PAUSE";
	routes[1].validation_flags = 0x02u;
	count = 0;
	assert(htttp_dispatch(&message, routes, 2u, &count, &handler_result)
		== HTTTP_ERR_INVALID_ARGUMENT);
	assert(count == 0);
	assert(handler_result == 0);
	htttp_message_free(&message);
	printf("PASS test_reject_invalid_dispatch_arguments\n");
}

static void	test_reject_response_and_missing_method(void)
{
	static const t_htttp_route	routes[] = {
		{"JOIN", 0u, handle_join}
	};
	t_htttp_message	message;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_response(&message, 200u, "OK") == HTTTP_OK);
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 1u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_MESSAGE);
	assert(handler_result == 0);
	htttp_message_free(&message);
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 1u, NULL, &handler_result)
		== HTTTP_ERR_INVALID_MESSAGE);
	assert(handler_result == 0);
	printf("PASS test_reject_response_and_missing_method\n");
}

static void	test_reject_malformed_request_before_handler(void)
{
	static const t_htttp_route	routes[] = {
		{"JOIN", 0u, handle_join}
	};
	t_htttp_message	message;
	int				count;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	free(message.path);
	message.path = NULL;
	count = 0;
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 1u, &count, &handler_result)
		== HTTTP_ERR_INVALID_MESSAGE);
	assert(count == 0);
	assert(handler_result == 0);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "JOIN", "/room/a") == HTTTP_OK);
	assert(htttp_message_set_body(&message, "x", 1u) == HTTTP_OK);
	handler_result = 99;
	assert(htttp_dispatch(&message, routes, 1u, &count, &handler_result)
		== HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(count == 0);
	assert(handler_result == 0);
	htttp_message_free(&message);
	printf("PASS test_reject_malformed_request_before_handler\n");
}

static void	test_route_flags_enforce_player_id(void)
{
	static const t_htttp_route	authenticated[] = {
		{"MOVE", HTTTP_VALIDATE_AUTHENTICATED_REQUEST, handle_move}
	};
	static const t_htttp_route	public[] = {
		{"MOVE", 0u, handle_move}
	};
	t_htttp_message	message;
	int				count;
	int				handler_result;

	htttp_message_init(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	count = 0;
	handler_result = 99;
	assert(htttp_dispatch(&message, authenticated, 1u, &count, &handler_result)
		== HTTTP_ERR_MISSING_REQUIRED_HEADER);
	assert(count == 0);
	assert(handler_result == 0);
	assert(htttp_message_set_header(&message, "Player-Id", "p17") == HTTTP_OK);
	assert(htttp_dispatch(&message, authenticated, 1u, &count, &handler_result)
		== HTTTP_OK);
	assert(count == 1);
	assert(handler_result == 203);
	htttp_message_free(&message);
	assert(htttp_message_make_request(&message, "MOVE", "/room/a") == HTTTP_OK);
	count = 0;
	assert(htttp_dispatch(&message, public, 1u, &count, &handler_result)
		== HTTTP_OK);
	assert(count == 1);
	htttp_message_free(&message);
	printf("PASS test_route_flags_enforce_player_id\n");
}

int	main(void)
{
	test_dispatch_parsed_extension_method();
	test_dispatch_uses_first_exact_match();
	test_dispatch_supports_null_context();
	test_unknown_method_and_empty_table();
	test_reject_invalid_dispatch_arguments();
	test_reject_response_and_missing_method();
	test_reject_malformed_request_before_handler();
	test_route_flags_enforce_player_id();
	return (0);
}
