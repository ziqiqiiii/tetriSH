#include "internal.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_peer_args
{
	int	fd;
} t_peer_args;

static void	seed_session(t_session *sess)
{
	memset(sess, 0xa5, sizeof(*sess));
	sess->fd = 42;
	sess->role = TETRISSH_ROLE_CLIENT;
	sess->send_seq = 7;
	sess->recv_seq = 9;
	sess->established = 1;
}

static void	assert_session_reset(const t_session *sess)
{
	size_t	i;

	assert(sess->fd == -1);
	assert(sess->role == TETRISSH_ROLE_NONE);
	assert(sess->send_seq == 0);
	assert(sess->recv_seq == 0);
	assert(sess->established == 0);
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		assert(sess->aes_key[i] == 0);
		i++;
	}
}

static void	test_invalid_arguments_reset_session(void)
{
	t_session	sess;

	seed_session(&sess);
	assert(session_handshake_client(-1, &sess, NULL) == -1);
	assert_session_reset(&sess);
	seed_session(&sess);
	assert(session_handshake_server(-1, &sess, NULL, "unused") == -1);
	assert_session_reset(&sess);
	printf("PASS test_invalid_arguments_reset_session\n");
}

/* AI-assisted: one-byte bodies distinguish immediate length rejection from
 * allocating the claimed blob and reading until EOF. */
static void	*oversized_cert_peer(void *arg)
{
	t_peer_args		*peer;
	unsigned char	nonce[TSH_NONCE_LEN];

	peer = arg;
	assert(tsh_read_exact(peer->fd, nonce, sizeof(nonce)) == TSH_IO_OK);
	assert(tsh_write_u32(peer->fd, TETRISSH_MAX_PLAINTEXT + 1u) == 0);
	assert(tsh_write_exact(peer->fd, "C", 1) == 0);
	assert(shutdown(peer->fd, SHUT_WR) == 0);
	return (NULL);
}

static void	test_oversized_certificate_rejected_before_body(void)
{
	int			fds[2];
	t_peer_args	peer;
	pthread_t	thread;
	t_session	sess;
	char		leftover;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	peer.fd = fds[1];
	assert(pthread_create(&thread, NULL, oversized_cert_peer, &peer) == 0);
	assert(session_handshake_client(fds[0], &sess, "unused-ca") == -1);
	assert(pthread_join(thread, NULL) == 0);
	/* The peer has joined and shut down its write side, so this cannot block;
	 * flags=0 keeps the test portable where MSG_DONTWAIT is not exposed. */
	assert(recv(fds[0], &leftover, 1, 0) == 1);
	assert(leftover == 'C');
	assert_session_reset(&sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_oversized_certificate_rejected_before_body\n");
}

static unsigned char	*read_fixture(const char *path, uint32_t *len)
{
	FILE			*fp;
	long			size;
	unsigned char	*bytes;

	fp = fopen(path, "rb");
	assert(fp != NULL);
	assert(fseek(fp, 0, SEEK_END) == 0);
	size = ftell(fp);
	assert(size > 0 && (unsigned long)size <= UINT32_MAX);
	rewind(fp);
	bytes = malloc((size_t)size);
	assert(bytes != NULL);
	assert(fread(bytes, 1, (size_t)size, fp) == (size_t)size);
	assert(fclose(fp) == 0);
	*len = (uint32_t)size;
	return (bytes);
}

static void	*wrong_signature_length_peer(void *arg)
{
	t_peer_args		*peer;
	unsigned char	nonce[TSH_NONCE_LEN];
	unsigned char	*cert;
	uint32_t		cert_len;

	peer = arg;
	assert(tsh_read_exact(peer->fd, nonce, sizeof(nonce)) == TSH_IO_OK);
	cert = read_fixture("tests/tmp/certs/server.crt", &cert_len);
	assert(tsh_write_u32(peer->fd, cert_len) == 0);
	assert(tsh_write_exact(peer->fd, cert, cert_len) == 0);
	free(cert);
	/* Test fixture's 1024-bit RSA signature must be exactly 128 bytes. */
	assert(tsh_write_u32(peer->fd, 129u) == 0);
	assert(tsh_write_exact(peer->fd, "S", 1) == 0);
	assert(shutdown(peer->fd, SHUT_WR) == 0);
	return (NULL);
}

static void	test_wrong_signature_length_rejected_before_body(void)
{
	int			fds[2];
	t_peer_args	peer;
	pthread_t	thread;
	t_session	sess;
	char		leftover;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	peer.fd = fds[1];
	assert(pthread_create(&thread, NULL,
		wrong_signature_length_peer, &peer) == 0);
	assert(session_handshake_client(fds[0], &sess,
		"tests/tmp/certs/ca.crt") == -1);
	assert(pthread_join(thread, NULL) == 0);
	assert(recv(fds[0], &leftover, 1, 0) == 1);
	assert(leftover == 'S');
	assert_session_reset(&sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_wrong_signature_length_rejected_before_body\n");
}

static void	read_discard_blob(int fd)
{
	uint32_t		len;
	unsigned char	*buf;

	assert(tsh_read_u32(fd, &len) == TSH_IO_OK);
	buf = malloc(len);
	assert(buf != NULL);
	assert(tsh_read_exact(fd, buf, len) == TSH_IO_OK);
	free(buf);
}

static void	*wrong_wrapped_length_peer(void *arg)
{
	t_peer_args		*peer;
	unsigned char	nonce[TSH_NONCE_LEN];

	peer = arg;
	memset(nonce, 0x5a, sizeof(nonce));
	assert(tsh_write_exact(peer->fd, nonce, sizeof(nonce)) == 0);
	read_discard_blob(peer->fd);
	read_discard_blob(peer->fd);
	/* Test fixtures use 1024-bit RSA, whose wrapped block is exactly 128 bytes. */
	assert(tsh_write_u32(peer->fd, 129u) == 0);
	assert(tsh_write_exact(peer->fd, "W", 1) == 0);
	assert(shutdown(peer->fd, SHUT_WR) == 0);
	return (NULL);
}

static void	test_wrong_wrapped_length_rejected_before_body(void)
{
	int			fds[2];
	t_peer_args	peer;
	pthread_t	thread;
	t_session	sess;
	char		leftover;

	assert(system("sh ./scripts/generate_test_certs.sh tests/tmp/certs") == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	peer.fd = fds[0];
	assert(pthread_create(&thread, NULL, wrong_wrapped_length_peer, &peer) == 0);
	assert(session_handshake_server(fds[1], &sess,
		"tests/tmp/certs/server.crt", "tests/tmp/certs/server.key") == -1);
	assert(pthread_join(thread, NULL) == 0);
	assert(recv(fds[1], &leftover, 1, 0) == 1);
	assert(leftover == 'W');
	assert_session_reset(&sess);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_wrong_wrapped_length_rejected_before_body\n");
}

int	main(void)
{
	test_invalid_arguments_reset_session();
	test_oversized_certificate_rejected_before_body();
	test_wrong_signature_length_rejected_before_body();
	test_wrong_wrapped_length_rejected_before_body();
	return (0);
}
