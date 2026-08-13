/*
** Frame codec suite: session_frame_seal / session_frame_open over byte arrays.
**
** These two functions are the pure core beneath session_send / session_recv -
** they perform no I/O, so every case here runs on a session whose fd is -1.
** That is the property the reactor depends on: it owns the socket
** and the read buffer, and the library only turns bytes into bytes.
*/

#include "tetrissh.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void	init_pair(t_session *client, t_session *server)
{
	size_t	i;

	memset(client, 0, sizeof(*client));
	memset(server, 0, sizeof(*server));
	client->fd = -1;
	client->role = TETRISSH_ROLE_CLIENT;
	client->established = 1;
	server->fd = -1;
	server->role = TETRISSH_ROLE_SERVER;
	server->established = 1;
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		client->aes_key[i] = (unsigned char)(i + 1u);
		server->aes_key[i] = (unsigned char)(i + 1u);
		i++;
	}
}

static void	test_client_to_server_round_trip(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	const char		*msg;
	ssize_t			frame_len;

	msg = "JOIN /room/a HTTTP/1.0\r\n\r\n";
	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, msg, strlen(msg),
			frame, sizeof(frame));
	assert(frame_len == (ssize_t)(strlen(msg) + TETRISSH_FRAME_OVERHEAD));
	assert(client.send_seq == 1);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == (ssize_t)strlen(msg));
	assert(server.recv_seq == 1);
	assert(memcmp(plain, msg, strlen(msg)) == 0);
	printf("PASS test_client_to_server_round_trip\n");
}

static void	test_server_to_client_round_trip(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	const char		*msg;
	ssize_t			frame_len;

	msg = "HTTTP/1.0 200 OK\r\n\r\n";
	init_pair(&client, &server);
	frame_len = session_frame_seal(&server, msg, strlen(msg),
			frame, sizeof(frame));
	assert(frame_len > 0);
	assert(session_frame_open(&client, frame, (size_t)frame_len,
			plain, sizeof(plain)) == (ssize_t)strlen(msg));
	assert(memcmp(plain, msg, strlen(msg)) == 0);
	printf("PASS test_server_to_client_round_trip\n");
}

static void	test_sequence_advances_across_many_frames(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[64];
	char			plain[64];
	ssize_t			frame_len;
	int				i;

	init_pair(&client, &server);
	for (i = 0; i < 8; ++i)
	{
		frame_len = session_frame_seal(&client, "tick", 4,
				frame, sizeof(frame));
		assert(frame_len == 4 + (ssize_t)TETRISSH_FRAME_OVERHEAD);
		assert(session_frame_open(&server, frame, (size_t)frame_len,
				plain, sizeof(plain)) == 4);
	}
	assert(client.send_seq == 8);
	assert(server.recv_seq == 8);
	printf("PASS test_sequence_advances_across_many_frames\n");
}

static void	test_empty_plaintext_round_trips(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[64];
	char			plain[64];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, NULL, 0, frame, sizeof(frame));
	assert(frame_len == (ssize_t)TETRISSH_FRAME_OVERHEAD);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == 0);
	assert(server.recv_seq == 1);
	printf("PASS test_empty_plaintext_round_trips\n");
}

static void	test_max_plaintext_round_trips(void)
{
	t_session		client;
	t_session		server;
	unsigned char	*frame;
	unsigned char	*message;
	unsigned char	*plain;
	ssize_t			frame_len;

	init_pair(&client, &server);
	message = malloc(TETRISSH_MAX_PLAINTEXT);
	plain = malloc(TETRISSH_MAX_PLAINTEXT);
	frame = malloc(TETRISSH_MAX_FRAME);
	assert(message != NULL && plain != NULL && frame != NULL);
	memset(message, 0xa5, TETRISSH_MAX_PLAINTEXT);
	frame_len = session_frame_seal(&client, message, TETRISSH_MAX_PLAINTEXT,
			frame, TETRISSH_MAX_FRAME);
	assert(frame_len == (ssize_t)TETRISSH_MAX_FRAME);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, TETRISSH_MAX_PLAINTEXT) == (ssize_t)TETRISSH_MAX_PLAINTEXT);
	assert(memcmp(plain, message, TETRISSH_MAX_PLAINTEXT) == 0);
	free(message);
	free(plain);
	free(frame);
	printf("PASS test_max_plaintext_round_trips\n");
}

static void	test_seal_rejects_oversized_plaintext(void)
{
	t_session		client;
	t_session		server;
	unsigned char	*message;
	unsigned char	*frame;

	init_pair(&client, &server);
	message = calloc(TETRISSH_MAX_PLAINTEXT + 1, 1);
	frame = malloc(TETRISSH_MAX_FRAME + 1);
	assert(message != NULL && frame != NULL);
	assert(session_frame_seal(&client, message, TETRISSH_MAX_PLAINTEXT + 1,
			frame, TETRISSH_MAX_FRAME + 1) == -1);
	assert(client.send_seq == 0);
	free(message);
	free(frame);
	printf("PASS test_seal_rejects_oversized_plaintext\n");
}

static void	test_seal_rejects_short_frame_buffer(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[TETRISSH_FRAME_OVERHEAD + 3];

	init_pair(&client, &server);
	assert(session_frame_seal(&client, "four", 4, frame, sizeof(frame)) == -1);
	assert(client.send_seq == 0);
	printf("PASS test_seal_rejects_short_frame_buffer\n");
}

static void	test_open_rejects_short_plain_buffer(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[3];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, "four", 4, frame, sizeof(frame));
	assert(frame_len > 0);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_open_rejects_short_plain_buffer\n");
}

static void	test_open_rejects_undersized_frame(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[TETRISSH_FRAME_OVERHEAD];
	char			plain[64];

	init_pair(&client, &server);
	memset(frame, 0, sizeof(frame));
	assert(session_frame_open(&server, frame, TETRISSH_FRAME_OVERHEAD - 1,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_open_rejects_undersized_frame\n");
}

static void	test_open_rejects_oversized_frame(void)
{
	t_session	client;
	t_session	server;
	char		plain[64];

	init_pair(&client, &server);
	assert(session_frame_open(&server, plain, TETRISSH_MAX_FRAME + 1,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_open_rejects_oversized_frame\n");
}

static void	test_tampered_ciphertext_rejected(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, "payload", 7,
			frame, sizeof(frame));
	assert(frame_len > 0);
	frame[TETRISSH_FRAME_OVERHEAD] ^= 0x01u;
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_tampered_ciphertext_rejected\n");
}

static void	test_tampered_tag_rejected(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, "payload", 7,
			frame, sizeof(frame));
	assert(frame_len > 0);
	frame[TETRISSH_FRAME_OVERHEAD - 1] ^= 0x80u;
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_tampered_tag_rejected\n");
}

static void	test_replayed_frame_rejected(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&client, "once", 4, frame, sizeof(frame));
	assert(frame_len > 0);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == 4);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 1);
	printf("PASS test_replayed_frame_rejected\n");
}

static void	test_reflected_frame_rejected(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	ssize_t			frame_len;

	init_pair(&client, &server);
	frame_len = session_frame_seal(&server, "mine", 4, frame, sizeof(frame));
	assert(frame_len > 0);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	assert(server.recv_seq == 0);
	printf("PASS test_reflected_frame_rejected\n");
}

static void	test_wrong_key_rejected(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];
	ssize_t			frame_len;

	init_pair(&client, &server);
	server.aes_key[0] ^= 0xffu;
	frame_len = session_frame_seal(&client, "secret", 6, frame, sizeof(frame));
	assert(frame_len > 0);
	assert(session_frame_open(&server, frame, (size_t)frame_len,
			plain, sizeof(plain)) == -1);
	printf("PASS test_wrong_key_rejected\n");
}

static void	test_rejects_unestablished_session(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];

	init_pair(&client, &server);
	client.established = 0;
	server.established = 0;
	assert(session_frame_seal(&client, "x", 1, frame, sizeof(frame)) == -1);
	assert(session_frame_open(&server, frame, TETRISSH_FRAME_OVERHEAD + 1,
			plain, sizeof(plain)) == -1);
	printf("PASS test_rejects_unestablished_session\n");
}

static void	test_rejects_roleless_session(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];

	init_pair(&client, &server);
	client.role = TETRISSH_ROLE_NONE;
	server.role = TETRISSH_ROLE_NONE;
	assert(session_frame_seal(&client, "x", 1, frame, sizeof(frame)) == -1);
	assert(session_frame_open(&server, frame, TETRISSH_FRAME_OVERHEAD + 1,
			plain, sizeof(plain)) == -1);
	printf("PASS test_rejects_roleless_session\n");
}

static void	test_rejects_null_arguments(void)
{
	t_session		client;
	t_session		server;
	unsigned char	frame[128];
	char			plain[128];

	init_pair(&client, &server);
	assert(session_frame_seal(NULL, "x", 1, frame, sizeof(frame)) == -1);
	assert(session_frame_seal(&client, "x", 1, NULL, sizeof(frame)) == -1);
	assert(session_frame_seal(&client, NULL, 1, frame, sizeof(frame)) == -1);
	assert(session_frame_open(NULL, frame, sizeof(frame), plain,
			sizeof(plain)) == -1);
	assert(session_frame_open(&server, NULL, sizeof(frame), plain,
			sizeof(plain)) == -1);
	assert(session_frame_open(&server, frame, sizeof(frame), NULL,
			sizeof(plain)) == -1);
	assert(client.send_seq == 0 && server.recv_seq == 0);
	printf("PASS test_rejects_null_arguments\n");
}

static void	test_nonce_differs_between_frames(void)
{
	t_session		client;
	t_session		server;
	unsigned char	first[128];
	unsigned char	second[128];

	init_pair(&client, &server);
	assert(session_frame_seal(&client, "same", 4, first, sizeof(first)) > 0);
	assert(session_frame_seal(&client, "same", 4, second, sizeof(second)) > 0);
	assert(memcmp(first, second, TETRISSH_FRAME_OVERHEAD) != 0);
	printf("PASS test_nonce_differs_between_frames\n");
}

int	main(void)
{
	test_client_to_server_round_trip();
	test_server_to_client_round_trip();
	test_sequence_advances_across_many_frames();
	test_empty_plaintext_round_trips();
	test_max_plaintext_round_trips();
	test_seal_rejects_oversized_plaintext();
	test_seal_rejects_short_frame_buffer();
	test_open_rejects_short_plain_buffer();
	test_open_rejects_undersized_frame();
	test_open_rejects_oversized_frame();
	test_tampered_ciphertext_rejected();
	test_tampered_tag_rejected();
	test_replayed_frame_rejected();
	test_reflected_frame_rejected();
	test_wrong_key_rejected();
	test_rejects_unestablished_session();
	test_rejects_roleless_session();
	test_rejects_null_arguments();
	test_nonce_differs_between_frames();
	return (0);
}
