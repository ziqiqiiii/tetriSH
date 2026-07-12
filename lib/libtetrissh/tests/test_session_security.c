#include "../src/internal.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_recv_args
{
	t_session		*session;
	unsigned char	*out;
	ssize_t			result;
} t_recv_args;

static void	init_session(t_session *sess, int fd, t_tetrissh_role role)
{
	size_t	i;

	memset(sess, 0, sizeof(*sess));
	sess->fd = fd;
	sess->role = role;
	sess->established = 1;
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		sess->aes_key[i] = (unsigned char)(i + 1u);
		i++;
	}
}

static unsigned char	*capture_frame(int fd, uint32_t *len)
{
	unsigned char	*frame;

	assert(tsh_read_u32(fd, len) == TSH_IO_OK);
	frame = malloc(*len);
	assert(frame != NULL);
	assert(tsh_read_exact(fd, frame, *len) == TSH_IO_OK);
	return (frame);
}

static void	inject_frame(int fd, const unsigned char *frame, uint32_t len)
{
	assert(tsh_write_u32(fd, len) == 0);
	assert(tsh_write_exact(fd, frame, len) == 0);
}

static void	*recv_maximum_frame(void *arg)
{
	t_recv_args	*recv_args;

	recv_args = arg;
	recv_args->result = session_recv(recv_args->session, recv_args->out,
		TETRISSH_MAX_PLAINTEXT);
	return (NULL);
}

/* AI-assisted: capture and reinject real encrypted frames so tests alter wire
 * bytes without exposing test-only crypto hooks from production code. */
static void	test_tampered_tag_rejected(void)
{
	int				source[2];
	int				target[2];
	t_session		sender;
	t_session		receiver;
	unsigned char	*frame;
	uint32_t		len;
	char			out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, source) == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, target) == 0);
	init_session(&sender, source[0], TETRISSH_ROLE_CLIENT);
	init_session(&receiver, target[1], TETRISSH_ROLE_SERVER);
	assert(session_send(&sender, "tag", 3) == 3);
	frame = capture_frame(source[1], &len);
	frame[TSH_GCM_NONCE_LEN] ^= 0x01u;
	inject_frame(target[0], frame, len);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	assert(receiver.recv_seq == 0);
	free(frame);
	close(source[0]);
	close(source[1]);
	close(target[0]);
	close(target[1]);
	printf("PASS test_tampered_tag_rejected\n");
}

static void	test_reflected_direction_rejected(void)
{
	int				source[2];
	int				target[2];
	t_session		sender;
	t_session		receiver;
	unsigned char	*frame;
	uint32_t		len;
	char			out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, source) == 0);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, target) == 0);
	init_session(&sender, source[0], TETRISSH_ROLE_CLIENT);
	init_session(&receiver, target[1], TETRISSH_ROLE_CLIENT);
	assert(session_send(&sender, "dir", 3) == 3);
	frame = capture_frame(source[1], &len);
	inject_frame(target[0], frame, len);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	assert(receiver.recv_seq == 0);
	free(frame);
	close(source[0]);
	close(source[1]);
	close(target[0]);
	close(target[1]);
	printf("PASS test_reflected_direction_rejected\n");
}

static void	test_zero_and_maximum_frames(void)
{
	int				fds[2];
	t_session		client;
	t_session		server;
	unsigned char	*plain;
	unsigned char	*out;
	size_t			i;
	char			empty;
	t_recv_args		recv_args;
	pthread_t		thread;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&client, fds[0], TETRISSH_ROLE_CLIENT);
	init_session(&server, fds[1], TETRISSH_ROLE_SERVER);
	assert(session_send(&client, NULL, 0) == 0);
	assert(session_recv(&server, &empty, 0) == 0);
	assert(client.send_seq == 1 && server.recv_seq == 1);
	plain = malloc(TETRISSH_MAX_PLAINTEXT);
	out = malloc(TETRISSH_MAX_PLAINTEXT);
	assert(plain != NULL && out != NULL);
	i = 0;
	while (i < TETRISSH_MAX_PLAINTEXT)
	{
		plain[i] = (unsigned char)(i & 0xffu);
		i++;
	}
	recv_args.session = &server;
	recv_args.out = out;
	recv_args.result = -1;
	assert(pthread_create(&thread, NULL, recv_maximum_frame, &recv_args) == 0);
	assert(session_send(&client, plain, TETRISSH_MAX_PLAINTEXT)
		== (ssize_t)TETRISSH_MAX_PLAINTEXT);
	assert(pthread_join(thread, NULL) == 0);
	assert(recv_args.result == (ssize_t)TETRISSH_MAX_PLAINTEXT);
	assert(memcmp(plain, out, TETRISSH_MAX_PLAINTEXT) == 0);
	free(plain);
	free(out);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_zero_and_maximum_frames\n");
}

static void	test_malformed_lengths_rejected(void)
{
	int			fds[2];
	t_session	receiver;
	char		out[8];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&receiver, fds[1], TETRISSH_ROLE_SERVER);
	assert(tsh_write_u32(fds[0], TSH_FRAME_OVERHEAD - 1u) == 0);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	close(fds[0]);
	close(fds[1]);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	init_session(&receiver, fds[1], TETRISSH_ROLE_SERVER);
	assert(tsh_write_u32(fds[0],
		TSH_FRAME_OVERHEAD + TETRISSH_MAX_PLAINTEXT + 1u) == 0);
	assert(session_recv(&receiver, out, sizeof(out)) == -1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_malformed_lengths_rejected\n");
}

static void	test_invalid_state_and_close(void)
{
	int			fds[2];
	t_session	sess;
	char		byte;
	size_t		i;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(&sess, 0x7f, sizeof(sess));
	sess.fd = fds[0];
	sess.role = TETRISSH_ROLE_NONE;
	sess.established = 0;
	assert(session_send(&sess, "x", 1) == -1);
	assert(session_recv(&sess, &byte, 1) == -1);
	session_close(&sess);
	assert(sess.fd == -1 && sess.role == TETRISSH_ROLE_NONE);
	assert(sess.send_seq == 0 && sess.recv_seq == 0 && sess.established == 0);
	i = 0;
	while (i < TETRISSH_KEY_LEN)
	{
		assert(sess.aes_key[i] == 0);
		i++;
	}
	assert(send(fds[0], "Z", 1, MSG_NOSIGNAL) == 1);
	assert(recv(fds[1], &byte, 1, 0) == 1 && byte == 'Z');
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_invalid_state_and_close\n");
}

int	main(void)
{
	test_tampered_tag_rejected();
	test_reflected_direction_rejected();
	test_zero_and_maximum_frames();
	test_malformed_lengths_rejected();
	test_invalid_state_and_close();
	return (0);
}
