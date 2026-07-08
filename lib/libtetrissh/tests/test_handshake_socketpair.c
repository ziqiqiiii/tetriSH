#include "tetrissh.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_server_args
{
	int			fd;
	t_session	sess;
	int			result;
} t_server_args;

static void	*server_thread(void *arg)
{
	t_server_args	*server;
	char			buf[64];

	server = arg;
	server->result = session_handshake_server(server->fd, &server->sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key");
	if (server->result == 0)
	{
		assert(session_recv(&server->sess, buf, sizeof(buf)) == 5);
		assert(memcmp(buf, "hello", 5) == 0);
		assert(session_send(&server->sess, "world", 5) == 5);
	}
	return (NULL);
}

static void	test_handshake_and_frames(void)
{
	int				fds[2];
	t_session		client;
	t_server_args	server;
	pthread_t		thread;
	char			buf[64];

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));
	server.fd = fds[1];
	assert(pthread_create(&thread, NULL, server_thread, &server) == 0);
	assert(session_handshake_client(fds[0], &client,
		"tests/tmp/certs/ca.crt") == 0);
	assert(session_send(&client, "hello", 5) == 5);
	assert(session_recv(&client, buf, sizeof(buf)) == 5);
	assert(memcmp(buf, "world", 5) == 0);
	assert(pthread_join(thread, NULL) == 0);
	assert(server.result == 0);
	session_close(&client);
	session_close(&server.sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_handshake_and_frames\n");
}

static void	*server_fail_thread(void *arg)
{
	t_server_args	*server;

	server = arg;
	server->result = session_handshake_server(server->fd, &server->sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key");
	return (NULL);
}

static void	test_invalid_ca_fails_client(void)
{
	int				fds[2];
	t_session		client;
	t_server_args	server;
	pthread_t		thread;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));
	server.fd = fds[1];
	assert(pthread_create(&thread, NULL, server_fail_thread, &server) == 0);
	assert(session_handshake_client(fds[0], &client,
		"tests/tmp/certs/wrong_ca.crt") == -1);
	shutdown(fds[0], SHUT_RDWR);
	shutdown(fds[1], SHUT_RDWR);
	assert(pthread_join(thread, NULL) == 0);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_invalid_ca_fails_client\n");
}

int	main(void)
{
	test_handshake_and_frames();
	test_invalid_ca_fails_client();
	return (0);
}
