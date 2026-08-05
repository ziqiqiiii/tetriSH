#include "coredaemon.h"

// Static Functions
static void	parent_wait(int ready[2]);
static void	second_fork(void);
static void	shed_descriptors(int keep_fd);

/**
 * @brief Detaches into the background and hands back the readiness pipe.
 *
 * Returns in the daemon only. The originating process blocks inside this call
 * until the daemon either reports itself ready (cd_ready) or dies, and exits
 * with the corresponding status, so `tetrisctl start` learns whether the boot
 * it asked for actually happened.
 *
 * stderr is deliberately left pointing at whatever the caller had - usually a
 * terminal. Everything between here and cd_ready is boot, and a boot that
 * fails has to say why somewhere a person is looking; cd_stderr_redirect
 * moves the stream to its configured file once there is nothing left to fail.
 *
 * @param ready_fd Receives the write end of the readiness pipe, to be passed
 * to cd_ready once the daemon is up.
 * @return 0 in the detached daemon, -1 with errno set when it could not fork.
 */
int	cd_detach(int *ready_fd)
{
	pid_t	pid;
	int		ready[2];
	int		saved;

	if (ready_fd == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	*ready_fd = -1;
	if (pipe(ready) != 0)
		return (-1);
	fflush(NULL);
	pid = fork();
	if (pid < 0)
	{
		saved = errno;
		close(ready[0]);
		close(ready[1]);
		errno = saved;
		return (-1);
	}
	if (pid > 0)
		parent_wait(ready);
	close(ready[0]);
	if (setsid() < 0)
		_exit(EXIT_FAILURE);
	second_fork();
	umask(0);
	shed_descriptors(ready[1]);
	*ready_fd = ready[1];
	return (0);
}

/**
 * @brief Reports a successful boot, releasing the process that launched us.
 *
 * The byte is the whole point. The shell's daemon_spawn closes its pipe on
 * success and on death alike, so its parent cannot tell them apart and always
 * exits EXIT_SUCCESS; here the parent reads one byte for success and
 * end-of-file for a daemon that died before it got this far.
 *
 * @param ready_fd Descriptor from cd_detach, or -1 to do nothing.
 */
void	cd_ready(int ready_fd)
{
	char	byte;
	ssize_t	n;

	if (ready_fd < 0)
		return ;
	byte = CD_READY_BYTE;
	n = write(ready_fd, &byte, 1);
	while (n < 0 && errno == EINTR)
		n = write(ready_fd, &byte, 1);
	close(ready_fd);
}

/**
 * @brief Points stderr at a file, for a daemon that has finished booting.
 *
 * Not cosmetic: tetrisd's stderr is the last-resort copy of records the
 * logger could not take, and tetrislogd's is where Degraded records go. A
 * daemon that left it on /dev/null would throw those away precisely when the
 * log path is the thing that is broken.
 *
 * @param path File to append stderr to; its parents are created.
 * @return 0 on success, -1 with errno set on failure.
 */
int	cd_stderr_redirect(const char *path)
{
	int	fd;
	int	saved;

	if (path == NULL || path[0] == '\0' || strlen(path) >= CD_PATH_MAX)
	{
		errno = EINVAL;
		return (-1);
	}
	if (cd_mkdir_parent(path) != 0)
		return (-1);
	fd = open(path, O_WRONLY | O_APPEND | O_CREAT, CD_FILE_MODE);
	if (fd < 0)
		return (-1);
	if (dup2(fd, STDERR_FILENO) < 0)
	{
		saved = errno;
		close(fd);
		errno = saved;
		return (-1);
	}
	if (fd != STDERR_FILENO)
		close(fd);
	return (0);
}

/**
 * @brief Holds the originating process until the daemon reports, then exits.
 *
 * read() returning 0 covers every way the daemon can fail to come up,
 * including dying before it reached any error message of its own, because the
 * last writer closing the pipe is what end-of-file means here.
 *
 * @param ready Both ends of the readiness pipe; never returns.
 */
static void	parent_wait(int ready[2])
{
	char	byte;
	ssize_t	n;

	close(ready[1]);
	n = read(ready[0], &byte, 1);
	while (n < 0 && errno == EINTR)
		n = read(ready[0], &byte, 1);
	close(ready[0]);
	if (n == 1)
		_exit(EXIT_SUCCESS);
	_exit(EXIT_FAILURE);
}

/**
 * @brief Forks once more so the daemon is not a session leader.
 *
 * A session leader can acquire a controlling terminal by opening one; the
 * process that returns from here cannot, which is what stops a daemon picking
 * up a terminal by accident and being killed with it.
 *
 * SIGHUP is ignored only across the fork and put straight back, so a daemon
 * that uses it for something - tetrislogd rotates its sink on SIGHUP -
 * installs its own handler onto an untouched disposition.
 */
static void	second_fork(void)
{
	pid_t	pid;

	signal(SIGHUP, SIG_IGN);
	pid = fork();
	if (pid < 0)
		_exit(EXIT_FAILURE);
	if (pid > 0)
		_exit(EXIT_SUCCESS);
	signal(SIGHUP, SIG_DFL);
}

/**
 * @brief Closes inherited descriptors and takes stdin and stdout to /dev/null.
 *
 * stderr is left alone; see cd_detach. The working directory is left alone
 * too, deliberately: every path in .tetrishrc is relative to where the daemon
 * was launched, so the customary chdir("/") would resolve all of them
 * somewhere else.
 *
 * @param keep_fd The readiness pipe, the one inherited descriptor that stays.
 */
static void	shed_descriptors(int keep_fd)
{
	long	max_fd;
	int		null_fd;
	int		fd;

	max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd < 0 || max_fd > 4096)
		max_fd = 4096;
	fd = STDERR_FILENO + 1;
	while (fd < (int)max_fd)
	{
		if (fd != keep_fd)
			close(fd);
		fd++;
	}
	null_fd = open("/dev/null", O_RDWR);
	if (null_fd < 0)
		return ;
	dup2(null_fd, STDIN_FILENO);
	dup2(null_fd, STDOUT_FILENO);
	if (null_fd > STDERR_FILENO)
		close(null_fd);
}
