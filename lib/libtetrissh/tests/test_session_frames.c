#include "tetrissh.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static void	init_pair(t_session *client, t_session *server, int fds[2])
{
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	memset(client, 0, sizeof(*client));
	memset(server, 0, sizeof(*server));
	client->fd = fds[0];
	client->role = TETRISSH_ROLE_CLIENT;
	client->established = 1;
	server->fd = fds[1];
	server->role = TETRISSH_ROLE_SERVER;
	server->established = 1;
	for (size_t i = 0; i < TETRISSH_KEY_LEN; ++i)
	{
		client->aes_key[i] = (unsigned char)(i + 1u);
		server->aes_key[i] = (unsigned char)(i + 1u);
	}
}

static void	test_client_to_server_round_trip(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];
	const char	*msg;
	ssize_t		len;

	msg = "JOIN /room/a HTTTP/1.0\r\n\r\n";
	len = (ssize_t)strlen(msg);
	init_pair(&client, &server, fds);
	assert(session_send(&client, msg, strlen(msg)) == len);
	assert(session_recv(&server, buf, sizeof(buf)) == len);
	buf[len] = '\0';
	assert(strcmp(buf, msg) == 0);
	assert(client.send_seq == 1);
	assert(server.recv_seq == 1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_client_to_server_round_trip\n");
}

static void	test_server_to_client_round_trip(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];
	const char	*msg;
	ssize_t		len;

	msg = "HTTTP/1.0 200 OK\r\n\r\n";
	len = (ssize_t)strlen(msg);
	init_pair(&client, &server, fds);
	assert(session_send(&server, msg, strlen(msg)) == len);
	assert(session_recv(&client, buf, sizeof(buf)) == len);
	buf[len] = '\0';
	assert(strcmp(buf, msg) == 0);
	assert(server.send_seq == 1);
	assert(client.recv_seq == 1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_server_to_client_round_trip\n");
}

static void	test_oversized_plaintext_rejected(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		too_big[TETRISSH_MAX_PLAINTEXT + 1u];

	init_pair(&client, &server, fds);
	memset(too_big, 'A', sizeof(too_big));
	assert(session_send(&client, too_big, sizeof(too_big)) == -1);
	assert(client.send_seq == 0);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_oversized_plaintext_rejected\n");
}

static void	test_replay_same_frame_fails(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	char		buf[64];

	init_pair(&client, &server, fds);
	assert(session_send(&client, "first", 5) == 5);
	assert(session_recv(&server, buf, sizeof(buf)) == 5);
	assert(session_send(&client, "second", 6) == 6);
	server.recv_seq = 0;
	assert(session_recv(&server, buf, sizeof(buf)) == -1);
	close(fds[0]);
	close(fds[1]);
	printf("PASS test_replay_same_frame_fails\n");
}

/* AI-assisted: isolate default SIGPIPE handling in a child so the regression
 * can prove session_send() reports EPIPE without killing its caller. */
static void	test_closed_peer_returns_error_without_sigpipe(void)
{
	t_session	client;
	t_session	server;
	int			fds[2];
	pid_t		pid;
	int			status;

	init_pair(&client, &server, fds);
	close(fds[1]);
	pid = fork();
	assert(pid >= 0);
	if (pid == 0)
	{
		signal(SIGPIPE, SIG_DFL);
		_exit(session_send(&client, "x", 1) == -1 ? 0 : 1);
	}
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	close(fds[0]);
	printf("PASS test_closed_peer_returns_error_without_sigpipe\n");
}

int	main(void)
{
	test_client_to_server_round_trip();
	test_server_to_client_round_trip();
	test_oversized_plaintext_rejected();
	test_replay_same_frame_fails();
	test_closed_peer_returns_error_without_sigpipe();
	return (0);
}
