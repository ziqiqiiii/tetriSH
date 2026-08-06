#include "harness.h"

// Static Functions
static void	holder_body(t_holder *h, const char *path);
static void	nap_ms(int ms);

/**
 * @brief Creates a private temporary directory for one test case.
 *
 * @param out Buffer receiving the directory path.
 * @param cap Size of out.
 * @return 0 on success, -1 on failure.
 */
int	fx_tmpdir(char *out, size_t cap)
{
	if (out == NULL || cap < 32)
		return (-1);
	snprintf(out, cap, "/tmp/cdaemonXXXXXX");
	if (mkdtemp(out) == NULL)
		return (-1);
	return (0);
}

/**
 * @brief Removes a directory and everything under it.
 *
 * @param path Directory to remove.
 */
void	fx_rmtree(const char *path)
{
	DIR				*d;
	struct dirent	*e;
	char			child[DAEMON_PATH_MAX];

	d = opendir(path);
	if (d == NULL)
	{
		unlink(path);
		return ;
	}
	e = readdir(d);
	while (e != NULL)
	{
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
		{
			snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
			fx_rmtree(child);
		}
		e = readdir(d);
	}
	closedir(d);
	rmdir(path);
}

/**
 * @brief Forks a process that claims a pidfile and holds it until told to stop.
 *
 * Returns only once the child has actually taken the lock, so a test that
 * expects the next claim to fail is not racing the child's own open().
 *
 * @param h Holder to fill.
 * @param path Pidfile the child claims.
 * @return 0 once the child holds the lock, -1 if it could not take it.
 */
int	fx_hold(t_holder *h, const char *path)
{
	char	byte;

	if (pipe(h->up) != 0 || pipe(h->go) != 0)
		return (-1);
	h->pid = fork();
	if (h->pid < 0)
		return (-1);
	if (h->pid == 0)
		holder_body(h, path);
	close(h->up[1]);
	close(h->go[0]);
	if (read(h->up[0], &byte, 1) != 1)
		return (-1);
	return (0);
}

/**
 * @brief Tells the holder to release the pidfile, then reaps it.
 *
 * @param h Holder started by fx_hold.
 */
void	fx_stop(t_holder *h)
{
	char	byte;

	byte = 'x';
	if (h->pid <= 0)
		return ;
	if (write(h->go[1], &byte, 1) != 1)
		kill(h->pid, SIGKILL);
	close(h->go[1]);
	close(h->up[0]);
	fx_reap(h->pid);
	h->pid = -1;
}

/**
 * @brief Waits for one child and reports how it exited.
 *
 * @param pid Child to wait for.
 * @return The child's exit status, or -1 when it did not exit normally.
 */
int	fx_reap(pid_t pid)
{
	int	status;

	while (waitpid(pid, &status, 0) < 0)
	{
		if (errno != EINTR)
			return (-1);
	}
	if (!WIFEXITED(status))
		return (-1);
	return (WEXITSTATUS(status));
}

/**
 * @brief Reads a whole file into a NUL-terminated buffer.
 *
 * @param path File to read.
 * @param out Buffer receiving the contents.
 * @param cap Size of out.
 * @return Bytes read, or -1 when the file could not be opened.
 */
ssize_t	fx_slurp(const char *path, char *out, size_t cap)
{
	ssize_t	n;
	int		fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);
	n = read(fd, out, cap - 1);
	close(fd);
	if (n < 0)
		return (-1);
	out[n] = '\0';
	return (n);
}

/**
 * @brief Waits for a path to exist, for asserting on a detached process.
 *
 * A daemon is reparented away, so the only honest way to see it ran is to
 * watch for something it made.
 *
 * @param path Path to watch for.
 * @param timeout_ms How long to keep looking.
 * @return 0 once the path exists, -1 on timeout.
 */
int	fx_wait_file(const char *path, int timeout_ms)
{
	struct stat	st;
	int			waited;

	waited = 0;
	while (waited < timeout_ms)
	{
		if (stat(path, &st) == 0)
			return (0);
		nap_ms(10);
		waited += 10;
	}
	return (-1);
}

/**
 * @brief Child half of fx_hold: claim, announce, block, release, exit.
 *
 * @param h Holder carrying both pipes.
 * @param path Pidfile to claim.
 */
static void	holder_body(t_holder *h, const char *path)
{
	t_pidfile	pf;
	char		byte;

	close(h->up[0]);
	close(h->go[1]);
	daemon_pid_blank(&pf);
	if (daemon_pid_claim(&pf, path) != 0)
		_exit(1);
	byte = 'u';
	if (write(h->up[1], &byte, 1) != 1)
		_exit(1);
	while (read(h->go[0], &byte, 1) < 0 && errno == EINTR)
		;
	daemon_pid_release(&pf);
	_exit(0);
}

/**
 * @brief Sleeps for a few milliseconds without pulling in a wider API.
 *
 * @param ms Milliseconds to sleep.
 */
static void	nap_ms(int ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
