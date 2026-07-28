#include "coreipc.h"

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Static Functions
static void	on_sigusr1(int sig);
static int	readable(int fd);
static void	tmp_path(char *out, size_t cap, const char *name);
static void	setup_dir(void);
static void	teardown_dir(void);

// Static Variables
static int	g_sp[2];
static char	g_dir[64];

void	test_set_nonblock_preserves_existing_flags(void)
{
	int	fds[2];
	int	before;
	int	after;

	assert(pipe(fds) == 0);
	assert(fcntl(fds[0], F_SETFL, O_APPEND) == 0);
	before = fcntl(fds[0], F_GETFL);
	assert(before != -1 && (before & O_APPEND));
	assert(us_set_nonblock(fds[0]) == 0);
	after = fcntl(fds[0], F_GETFL);
	assert(after & O_NONBLOCK);
	assert(after & O_APPEND);
	assert(close(fds[0]) == 0 && close(fds[1]) == 0);
	printf("PASS test_set_nonblock_preserves_existing_flags\n");
}

void	test_close_unlink_removes_socket_file(void)
{
	char	path[128];
	int		fd;

	tmp_path(path, sizeof(path), "gone.sock");
	fd = us_dgram_bind(path, 0600);
	assert(fd >= 0);
	assert(access(path, F_OK) == 0);
	assert(us_close_unlink(fd, path) == 0);
	assert(access(path, F_OK) == -1);
	assert(errno == ENOENT);
	printf("PASS test_close_unlink_removes_socket_file\n");
}

void	test_pipe_ends_are_nonblocking_and_cloexec(void)
{
	int	fds[2];

	assert(sp_pipe(fds) == 0);
	assert(fcntl(fds[SP_READ], F_GETFL) & O_NONBLOCK);
	assert(fcntl(fds[SP_WRITE], F_GETFL) & O_NONBLOCK);
	assert(fcntl(fds[SP_READ], F_GETFD) & FD_CLOEXEC);
	assert(fcntl(fds[SP_WRITE], F_GETFD) & FD_CLOEXEC);
	assert(close(fds[SP_READ]) == 0 && close(fds[SP_WRITE]) == 0);
	printf("PASS test_pipe_ends_are_nonblocking_and_cloexec\n");
}

void	test_drain_on_quiet_pipe_succeeds(void)
{
	int	fds[2];

	assert(sp_pipe(fds) == 0);
	assert(readable(fds[SP_READ]) == 0);
	assert(sp_drain(fds[SP_READ]) == 0);
	assert(close(fds[SP_READ]) == 0 && close(fds[SP_WRITE]) == 0);
	printf("PASS test_drain_on_quiet_pipe_succeeds\n");
}

void	test_notify_wakes_poll_then_drain_clears(void)
{
	int	fds[2];

	assert(sp_pipe(fds) == 0);
	sp_notify(fds[SP_WRITE]);
	assert(readable(fds[SP_READ]) == 1);
	assert(sp_drain(fds[SP_READ]) == 0);
	assert(readable(fds[SP_READ]) == 0);
	assert(close(fds[SP_READ]) == 0 && close(fds[SP_WRITE]) == 0);
	printf("PASS test_notify_wakes_poll_then_drain_clears\n");
}

void	test_repeated_notifies_coalesce_to_one_wakeup(void)
{
	int	fds[2];
	int	i;

	assert(sp_pipe(fds) == 0);
	i = 0;
	while (i < 64)
	{
		sp_notify(fds[SP_WRITE]);
		i++;
	}
	assert(readable(fds[SP_READ]) == 1);
	assert(sp_drain(fds[SP_READ]) == 0);
	assert(readable(fds[SP_READ]) == 0);
	assert(close(fds[SP_READ]) == 0 && close(fds[SP_WRITE]) == 0);
	printf("PASS test_repeated_notifies_coalesce_to_one_wakeup\n");
}

void	test_notify_from_a_real_signal_handler(void)
{
	struct sigaction	sa;
	int					saved;

	assert(sp_pipe(g_sp) == 0);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_sigusr1;
	assert(sigaction(SIGUSR1, &sa, NULL) == 0);
	errno = EEXIST;
	assert(raise(SIGUSR1) == 0);
	saved = errno;
	// sp_notify saves and restores errno, so the interrupted code must not
	// observe a change across handler entry.
	assert(saved == EEXIST);
	assert(readable(g_sp[SP_READ]) == 1);
	assert(sp_drain(g_sp[SP_READ]) == 0);
	assert(close(g_sp[SP_READ]) == 0 && close(g_sp[SP_WRITE]) == 0);
	printf("PASS test_notify_from_a_real_signal_handler\n");
}

int	main(void)
{
	setup_dir();
	test_set_nonblock_preserves_existing_flags();
	test_close_unlink_removes_socket_file();
	test_pipe_ends_are_nonblocking_and_cloexec();
	test_drain_on_quiet_pipe_succeeds();
	test_notify_wakes_poll_then_drain_clears();
	test_repeated_notifies_coalesce_to_one_wakeup();
	test_notify_from_a_real_signal_handler();
	teardown_dir();
	return (0);
}

static void	on_sigusr1(int sig)
{
	(void)sig;
	sp_notify(g_sp[SP_WRITE]);
}

static int	readable(int fd)
{
	struct pollfd	pfd;
	int				n;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	n = poll(&pfd, 1, 0);
	assert(n >= 0);
	return (n);
}

static void	tmp_path(char *out, size_t cap, const char *name)
{
	int	n;

	n = snprintf(out, cap, "%s/%s", g_dir, name);
	assert(n > 0 && (size_t)n < cap);
}

static void	setup_dir(void)
{
	strcpy(g_dir, "/tmp/coreipc-fdsig.XXXXXX");
	assert(mkdtemp(g_dir) != NULL);
}

static void	teardown_dir(void)
{
	assert(rmdir(g_dir) == 0);
}
