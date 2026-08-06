#include "coreipc.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BIG_LEN	(100 * 1024)

// Static Functions
static void	*client_sends_big(void *arg);
static void	*client_closes_early(void *arg);
static void	tmp_path(char *out, size_t cap, const char *name);
static void	setup_dir(void);
static void	teardown_dir(void);

// Static Variables
static char	g_dir[64];

void	test_listen_applies_mode(void)
{
	char		path[128];
	struct stat	st;
	int			fd;

	tmp_path(path, sizeof(path), "ctl.sock");
	fd = unixsock_stream_listen(path, 4, 0600);
	assert(fd >= 0);
	assert(stat(path, &st) == 0);
	assert((st.st_mode & 0777) == 0600);
	assert(unixsock_close_unlink(fd, path) == 0);
	printf("PASS test_listen_applies_mode\n");
}

void	test_connect_to_missing_path_fails(void)
{
	char	path[128];

	tmp_path(path, sizeof(path), "absent.sock");
	assert(unixsock_stream_connect(path) == -1);
	assert(errno == ENOENT);
	printf("PASS test_connect_to_missing_path_fails\n");
}

void	test_accept_connect_roundtrip(void)
{
	char		path[128];
	pthread_t	client;
	char		*got;
	int			listen_fd;
	int			conn;

	tmp_path(path, sizeof(path), "ctl.sock");
	listen_fd = unixsock_stream_listen(path, 4, 0600);
	assert(listen_fd >= 0);
	assert(pthread_create(&client, NULL, client_sends_big, path) == 0);
	conn = unixsock_stream_accept(listen_fd);
	assert(conn >= 0);
	got = malloc(BIG_LEN);
	assert(got != NULL);
	assert(unixsock_recv_all(conn, got, BIG_LEN) == 0);
	assert(got[0] == 'A' && got[BIG_LEN - 1] == 'A');
	assert(memchr(got, 0, BIG_LEN) == NULL);
	assert(pthread_join(client, NULL) == 0);
	free(got);
	assert(close(conn) == 0);
	assert(unixsock_close_unlink(listen_fd, path) == 0);
	printf("PASS test_accept_connect_roundtrip\n");
}

void	test_send_all_survives_short_writes(void)
{
	char		path[128];
	pthread_t	client;
	char		*payload;
	char		*got;
	int			listen_fd;
	int			conn;

	// 100 KiB exceeds the socket buffer, so the kernel forces short writes
	// on the sending side and short reads on the receiving side.
	tmp_path(path, sizeof(path), "ctl.sock");
	listen_fd = unixsock_stream_listen(path, 4, 0600);
	assert(listen_fd >= 0);
	payload = malloc(BIG_LEN);
	got = malloc(BIG_LEN);
	assert(payload != NULL && got != NULL);
	memset(payload, 'A', BIG_LEN);
	assert(pthread_create(&client, NULL, client_sends_big, path) == 0);
	conn = unixsock_stream_accept(listen_fd);
	assert(conn >= 0);
	assert(unixsock_recv_all(conn, got, BIG_LEN) == 0);
	assert(memcmp(payload, got, BIG_LEN) == 0);
	assert(pthread_join(client, NULL) == 0);
	free(payload);
	free(got);
	assert(close(conn) == 0);
	assert(unixsock_close_unlink(listen_fd, path) == 0);
	printf("PASS test_send_all_survives_short_writes\n");
}

void	test_recv_all_reports_early_close(void)
{
	char		path[128];
	pthread_t	client;
	char		got[64];
	int			listen_fd;
	int			conn;

	tmp_path(path, sizeof(path), "ctl.sock");
	listen_fd = unixsock_stream_listen(path, 4, 0600);
	assert(listen_fd >= 0);
	assert(pthread_create(&client, NULL, client_closes_early, path) == 0);
	conn = unixsock_stream_accept(listen_fd);
	assert(conn >= 0);
	assert(unixsock_recv_all(conn, got, sizeof(got)) == -1);
	assert(errno == EPIPE);
	assert(pthread_join(client, NULL) == 0);
	assert(close(conn) == 0);
	assert(unixsock_close_unlink(listen_fd, path) == 0);
	printf("PASS test_recv_all_reports_early_close\n");
}

void	test_send_all_to_closed_peer_fails(void)
{
	char		path[128];
	pthread_t	client;
	char		payload[64];
	int			listen_fd;
	int			conn;

	tmp_path(path, sizeof(path), "ctl.sock");
	listen_fd = unixsock_stream_listen(path, 4, 0600);
	assert(listen_fd >= 0);
	assert(pthread_create(&client, NULL, client_closes_early, path) == 0);
	conn = unixsock_stream_accept(listen_fd);
	assert(conn >= 0);
	assert(pthread_join(client, NULL) == 0);
	memset(payload, 'B', sizeof(payload));
	// The first send may land in the kernel buffer of an already-closed peer;
	// the second must fail with EPIPE rather than raising SIGPIPE.
	unixsock_send_all(conn, payload, sizeof(payload));
	assert(unixsock_send_all(conn, payload, sizeof(payload)) == -1);
	assert(errno == EPIPE || errno == ECONNRESET);
	assert(close(conn) == 0);
	assert(unixsock_close_unlink(listen_fd, path) == 0);
	printf("PASS test_send_all_to_closed_peer_fails\n");
}

int	main(void)
{
	setup_dir();
	test_listen_applies_mode();
	test_connect_to_missing_path_fails();
	test_accept_connect_roundtrip();
	test_send_all_survives_short_writes();
	test_recv_all_reports_early_close();
	test_send_all_to_closed_peer_fails();
	teardown_dir();
	return (0);
}

static void	*client_sends_big(void *arg)
{
	char	*payload;
	int		fd;

	fd = unixsock_stream_connect((const char *)arg);
	assert(fd >= 0);
	payload = malloc(BIG_LEN);
	assert(payload != NULL);
	memset(payload, 'A', BIG_LEN);
	assert(unixsock_send_all(fd, payload, BIG_LEN) == 0);
	free(payload);
	assert(close(fd) == 0);
	return (NULL);
}

static void	*client_closes_early(void *arg)
{
	int	fd;

	fd = unixsock_stream_connect((const char *)arg);
	assert(fd >= 0);
	assert(unixsock_send_all(fd, "half", 4) == 0);
	assert(close(fd) == 0);
	return (NULL);
}

static void	tmp_path(char *out, size_t cap, const char *name)
{
	int	n;

	n = snprintf(out, cap, "%s/%s", g_dir, name);
	assert(n > 0 && (size_t)n < cap);
}

static void	setup_dir(void)
{
	strcpy(g_dir, "/tmp/coreipc-stream.XXXXXX");
	assert(mkdtemp(g_dir) != NULL);
}

static void	teardown_dir(void)
{
	assert(rmdir(g_dir) == 0);
}
