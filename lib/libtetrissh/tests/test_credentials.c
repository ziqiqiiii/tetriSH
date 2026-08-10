/*
** Server credentials suite: session_credentials_load / session_credentials_free.
**
** The certificate bytes and the parsed private key are loaded once, not once
** per accepted connection. The load-bearing case here is
** test_handshake_survives_certificate_removal: it deletes both files after the
** load and still completes two handshakes, which only passes if the handshake
** has stopped touching the disk.
*/

#include "tetrissh.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CERT_PATH			"tests/tmp/certs/server.crt"
#define KEY_PATH			"tests/tmp/certs/server.key"
#define CA_PATH				"tests/tmp/certs/ca.crt"
#define CONCURRENT_WORKERS	8

typedef struct s_server_args
{
	int								fd;
	const t_tetrissh_credentials	*credentials;
	t_session						sess;
	int								result;
}	t_server_args;

/* One concurrent worker's private slot: nothing here is shared but the key. */
typedef struct s_worker
{
	const t_tetrissh_credentials	*credentials;
	int								result;
}	t_worker;

static void	*server_thread(void *arg)
{
	t_server_args	*server;

	server = arg;
	server->result = session_handshake_server(server->fd, &server->sess,
			server->credentials);
	return (NULL);
}

/* Run one full handshake over a socketpair; returns 0 when both ends agree. */
static int	handshake_once(const t_tetrissh_credentials *credentials)
{
	int				fds[2];
	t_session		client;
	t_server_args	server;
	pthread_t		thread;
	int				client_result;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));
	server.fd = fds[1];
	server.credentials = credentials;
	assert(pthread_create(&thread, NULL, server_thread, &server) == 0);
	client_result = session_handshake_client(fds[0], &client, CA_PATH);
	shutdown(fds[0], SHUT_RDWR);
	shutdown(fds[1], SHUT_RDWR);
	assert(pthread_join(thread, NULL) == 0);
	session_close(&client);
	session_close(&server.sess);
	close(fds[0]);
	close(fds[1]);
	if (client_result != 0 || server.result != 0)
		return (-1);
	return (0);
}

static void	test_load_and_free(void)
{
	t_tetrissh_credentials	*credentials;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	credentials = NULL;
	assert(session_credentials_load(CERT_PATH, KEY_PATH, &credentials) == 0);
	assert(credentials != NULL);
	session_credentials_free(credentials);
	printf("PASS test_load_and_free\n");
}

static void	test_free_null_is_noop(void)
{
	session_credentials_free(NULL);
	printf("PASS test_free_null_is_noop\n");
}

static void	test_load_rejects_null_arguments(void)
{
	t_tetrissh_credentials	*credentials;

	credentials = NULL;
	assert(session_credentials_load(NULL, KEY_PATH, &credentials) == -1);
	assert(session_credentials_load(CERT_PATH, NULL, &credentials) == -1);
	assert(session_credentials_load(CERT_PATH, KEY_PATH, NULL) == -1);
	assert(credentials == NULL);
	printf("PASS test_load_rejects_null_arguments\n");
}

static void	test_load_rejects_missing_files(void)
{
	t_tetrissh_credentials	*credentials;

	credentials = (t_tetrissh_credentials *)0x1;
	assert(session_credentials_load("tests/tmp/certs/absent.crt", KEY_PATH,
			&credentials) == -1);
	assert(credentials == NULL);
	credentials = (t_tetrissh_credentials *)0x1;
	assert(session_credentials_load(CERT_PATH, "tests/tmp/certs/absent.key",
			&credentials) == -1);
	assert(credentials == NULL);
	printf("PASS test_load_rejects_missing_files\n");
}

/* A certificate is a readable PEM file that is not a private key. */
static void	test_load_rejects_unusable_key(void)
{
	t_tetrissh_credentials	*credentials;

	credentials = NULL;
	assert(session_credentials_load(CERT_PATH, CERT_PATH, &credentials) == -1);
	assert(credentials == NULL);
	printf("PASS test_load_rejects_unusable_key\n");
}

static void	test_handshake_rejects_null_credentials(void)
{
	t_session	sess;

	memset(&sess, 0xa5, sizeof(sess));
	assert(session_handshake_server(-1, &sess, NULL) == -1);
	assert(sess.established == 0);
	assert(sess.fd == -1);
	printf("PASS test_handshake_rejects_null_credentials\n");
}

static void	test_one_load_serves_many_handshakes(void)
{
	t_tetrissh_credentials	*credentials;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	credentials = NULL;
	assert(session_credentials_load(CERT_PATH, KEY_PATH, &credentials) == 0);
	assert(handshake_once(credentials) == 0);
	assert(handshake_once(credentials) == 0);
	assert(handshake_once(credentials) == 0);
	session_credentials_free(credentials);
	printf("PASS test_one_load_serves_many_handshakes\n");
}

static void	test_handshake_survives_certificate_removal(void)
{
	t_tetrissh_credentials	*credentials;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	credentials = NULL;
	assert(session_credentials_load(CERT_PATH, KEY_PATH, &credentials) == 0);
	assert(remove(CERT_PATH) == 0);
	assert(remove(KEY_PATH) == 0);
	assert(handshake_once(credentials) == 0);
	assert(handshake_once(credentials) == 0);
	session_credentials_free(credentials);
	printf("PASS test_handshake_survives_certificate_removal\n");
}

/*
** One credentials object is shared by every handshake worker, so the private
** key inside it is signed and decrypted with concurrently. This case is the
** one that would catch that being unsafe, which is why each worker writes only
** to its own result slot - a shared counter here would be the very race the
** case exists to detect.
*/
static void	*concurrent_handshake_thread(void *arg)
{
	t_worker	*worker;

	worker = arg;
	worker->result = handshake_once(worker->credentials);
	return (NULL);
}

static void	test_concurrent_handshakes_share_one_key(void)
{
	t_tetrissh_credentials	*credentials;
	pthread_t				threads[CONCURRENT_WORKERS];
	t_worker				workers[CONCURRENT_WORKERS];
	int						i;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	credentials = NULL;
	assert(session_credentials_load(CERT_PATH, KEY_PATH, &credentials) == 0);
	i = 0;
	while (i < CONCURRENT_WORKERS)
	{
		workers[i].credentials = credentials;
		workers[i].result = -1;
		assert(pthread_create(&threads[i], NULL, concurrent_handshake_thread,
				&workers[i]) == 0);
		i++;
	}
	i = 0;
	while (i < CONCURRENT_WORKERS)
	{
		assert(pthread_join(threads[i], NULL) == 0);
		i++;
	}
	i = 0;
	while (i < CONCURRENT_WORKERS)
	{
		assert(workers[i].result == 0);
		i++;
	}
	session_credentials_free(credentials);
	printf("PASS test_concurrent_handshakes_share_one_key\n");
}

int	main(void)
{
	test_load_and_free();
	test_free_null_is_noop();
	test_load_rejects_null_arguments();
	test_load_rejects_missing_files();
	test_load_rejects_unusable_key();
	test_handshake_rejects_null_credentials();
	test_one_load_serves_many_handshakes();
	test_handshake_survives_certificate_removal();
	test_concurrent_handshakes_share_one_key();
	return (0);
}
