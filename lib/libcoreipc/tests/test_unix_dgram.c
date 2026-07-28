#include "coreipc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Static Functions
static void	tmp_path(char *out, size_t cap, const char *name);
static void	setup_dir(void);
static void	teardown_dir(void);

// Static Variables
static char	g_dir[64];

void	test_bind_creates_socket_with_mode(void)
{
	char		path[128];
	struct stat	st;
	int			fd;

	tmp_path(path, sizeof(path), "logd.sock");
	fd = us_dgram_bind(path, 0600);
	assert(fd >= 0);
	assert(stat(path, &st) == 0);
	assert((st.st_mode & 0777) == 0600);
	assert(us_close_unlink(fd, path) == 0);
	printf("PASS test_bind_creates_socket_with_mode\n");
}

void	test_datagram_roundtrip(void)
{
	char	path[128];
	char	buf[64];
	int		rx;
	int		tx;

	tmp_path(path, sizeof(path), "logd.sock");
	rx = us_dgram_bind(path, 0600);
	assert(rx >= 0);
	tx = us_dgram_open(path);
	assert(tx >= 0);
	assert(us_dgram_send_nb(tx, "record-one", 10) == 0);
	assert(us_dgram_send_nb(tx, "record-two", 10) == 0);
	memset(buf, 0, sizeof(buf));
	assert(us_dgram_recv(rx, buf, sizeof(buf)) == 10);
	assert(memcmp(buf, "record-one", 10) == 0);
	assert(us_dgram_recv(rx, buf, sizeof(buf)) == 10);
	assert(memcmp(buf, "record-two", 10) == 0);
	assert(close(tx) == 0);
	assert(us_close_unlink(rx, path) == 0);
	printf("PASS test_datagram_roundtrip\n");
}

void	test_recv_on_empty_socket_returns_eagain(void)
{
	char	path[128];
	char	buf[64];
	int		rx;

	tmp_path(path, sizeof(path), "logd.sock");
	rx = us_dgram_bind(path, 0600);
	assert(rx >= 0);
	assert(us_dgram_recv(rx, buf, sizeof(buf)) == -1);
	assert(errno == EAGAIN || errno == EWOULDBLOCK);
	assert(us_close_unlink(rx, path) == 0);
	printf("PASS test_recv_on_empty_socket_returns_eagain\n");
}

void	test_open_on_missing_path_fails(void)
{
	char	path[128];

	tmp_path(path, sizeof(path), "absent.sock");
	assert(us_dgram_open(path) == -1);
	assert(errno == ENOENT);
	printf("PASS test_open_on_missing_path_fails\n");
}

void	test_send_without_receiver_is_a_reported_drop(void)
{
	char	path[128];
	int		rx;
	int		tx;

	tmp_path(path, sizeof(path), "logd.sock");
	rx = us_dgram_bind(path, 0600);
	assert(rx >= 0);
	tx = us_dgram_open(path);
	assert(tx >= 0);
	assert(close(rx) == 0);
	assert(us_dgram_send_nb(tx, "orphan", 6) == -1);
	assert(errno == ECONNREFUSED);
	assert(close(tx) == 0);
	assert(unlink(path) == 0);
	printf("PASS test_send_without_receiver_is_a_reported_drop\n");
}

void	test_rebind_over_stale_socket_file(void)
{
	char	path[128];
	int		first;
	int		second;

	tmp_path(path, sizeof(path), "logd.sock");
	first = us_dgram_bind(path, 0600);
	assert(first >= 0);
	assert(close(first) == 0);
	assert(access(path, F_OK) == 0);
	second = us_dgram_bind(path, 0600);
	assert(second >= 0);
	assert(us_close_unlink(second, path) == 0);
	printf("PASS test_rebind_over_stale_socket_file\n");
}

void	test_oversized_datagram_is_truncated(void)
{
	char	path[128];
	char	big[128];
	char	small[16];
	int		rx;
	int		tx;

	tmp_path(path, sizeof(path), "logd.sock");
	rx = us_dgram_bind(path, 0600);
	assert(rx >= 0);
	tx = us_dgram_open(path);
	assert(tx >= 0);
	memset(big, 'x', sizeof(big));
	assert(us_dgram_send_nb(tx, big, sizeof(big)) == 0);
	assert(us_dgram_recv(rx, small, sizeof(small)) == (ssize_t)sizeof(small));
	assert(us_dgram_recv(rx, small, sizeof(small)) == -1);
	assert(close(tx) == 0);
	assert(us_close_unlink(rx, path) == 0);
	printf("PASS test_oversized_datagram_is_truncated\n");
}

int	main(void)
{
	setup_dir();
	test_bind_creates_socket_with_mode();
	test_datagram_roundtrip();
	test_recv_on_empty_socket_returns_eagain();
	test_open_on_missing_path_fails();
	test_send_without_receiver_is_a_reported_drop();
	test_rebind_over_stale_socket_file();
	test_oversized_datagram_is_truncated();
	teardown_dir();
	return (0);
}

static void	tmp_path(char *out, size_t cap, const char *name)
{
	int	n;

	n = snprintf(out, cap, "%s/%s", g_dir, name);
	assert(n > 0 && (size_t)n < cap);
}

static void	setup_dir(void)
{
	strcpy(g_dir, "/tmp/coreipc-dgram.XXXXXX");
	assert(mkdtemp(g_dir) != NULL);
}

static void	teardown_dir(void)
{
	assert(rmdir(g_dir) == 0);
}
