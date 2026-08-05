#include "harness.h"

// Static Functions
static int	ensure_tmp_root(void);

/**
 * @brief Creates a private directory and the paths a daemon needs inside it.
 *
 * The socket and the log file both live in the same throwaway directory, so a
 * suite that crashes leaves one directory behind rather than a stale socket
 * in the repository's runtime tree.
 *
 * @param fx Fixture to fill.
 * @return 0 on success, -1 when the directory could not be created.
 */
int	fx_make(t_fixture *fx)
{
	if (fx == NULL)
		return (-1);
	memset(fx, 0, sizeof(*fx));
	snprintf(fx->dir, sizeof(fx->dir), "tests/tmp/logdXXXXXX");
	if (ensure_tmp_root() != 0 || mkdtemp(fx->dir) == NULL)
		return (-1);
	snprintf(fx->sock_path, sizeof(fx->sock_path), "%s/logd.sock", fx->dir);
	snprintf(fx->file_path, sizeof(fx->file_path), "%s/logd.log", fx->dir);
	snprintf(fx->pid_path, sizeof(fx->pid_path), "%s/logd.pid", fx->dir);
	snprintf(fx->cfg.sock_path, TL_PATH_MAX, "%s", fx->sock_path);
	snprintf(fx->cfg.file_path, TL_PATH_MAX, "%s", fx->file_path);
	snprintf(fx->cfg.pid_path, TL_PATH_MAX, "%s", fx->pid_path);
	snprintf(fx->cfg.err_path, TL_PATH_MAX, "%s/logd.err", fx->dir);
	snprintf(fx->cfg.rc_path, TL_PATH_MAX, "%s", "./" TL_RC_NAME);
	return (0);
}

/**
 * @brief Removes the fixture's directory and everything in it.
 *
 * @param fx Fixture to destroy; safe on one that was never made.
 */
void	fx_destroy(t_fixture *fx)
{
	char	cmd[256];

	if (fx == NULL || fx->dir[0] == '\0')
		return ;
	snprintf(cmd, sizeof(cmd), "rm -rf %s", fx->dir);
	if (system(cmd) != 0)
		fprintf(stderr, "harness: could not remove %s\n", fx->dir);
	fx->dir[0] = '\0';
}

/**
 * @brief Opens a sending socket aimed at the fixture's logger.
 *
 * This is the same call tetrisd's shipper thread makes, so the tests exercise
 * the real datagram path rather than a stub.
 *
 * @param fx Fixture holding the socket path.
 * @return A connected fd on success, -1 on failure.
 */
int	fx_producer(const t_fixture *fx)
{
	if (fx == NULL)
		return (-1);
	return (us_dgram_open(fx->sock_path));
}

/**
 * @brief Sends one well-formed log record.
 *
 * @param fd Producer fd from fx_producer.
 * @param level Severity to stamp on the record.
 * @param msg Message text.
 * @return 0 on success, -1 on failure.
 */
int	fx_send(int fd, t_log_level level, const char *msg)
{
	t_log_record	rec;

	if (lr_make(&rec, level, 1700000000000ULL, (uint32_t)getpid(),
			"tetrisd", msg) != 0)
		return (-1);
	return (us_dgram_send_nb(fd, &rec, sizeof(rec)));
}

/**
 * @brief Sends arbitrary bytes, so a suite can post a malformed datagram.
 *
 * @param fd Producer fd from fx_producer.
 * @param buf Bytes to send.
 * @param len Number of bytes.
 * @return 0 on success, -1 on failure.
 */
int	fx_send_raw(int fd, const void *buf, size_t len)
{
	return (us_dgram_send_nb(fd, buf, len));
}

/**
 * @brief Reads a whole file into a caller-supplied buffer.
 *
 * @param path File to read.
 * @param out Destination, always NUL-terminated on success.
 * @param cap Capacity of out in bytes.
 * @return Bytes read, or -1 when the file cannot be opened.
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
 * @brief Reports whether a file contains a substring.
 *
 * @param path File to search.
 * @param needle Text to look for.
 * @return 1 when present, 0 when absent or unreadable.
 */
int	fx_contains(const char *path, const char *needle)
{
	char	buf[8192];

	if (fx_slurp(path, buf, sizeof(buf)) < 0)
		return (0);
	return (strstr(buf, needle) != NULL);
}

/**
 * @brief Counts the newline-terminated lines in a file.
 *
 * @param path File to count.
 * @return Number of lines, or -1 when the file cannot be read.
 */
int	fx_line_count(const char *path)
{
	char	buf[8192];
	ssize_t	n;
	int		lines;
	int		i;

	n = fx_slurp(path, buf, sizeof(buf));
	if (n < 0)
		return (-1);
	lines = 0;
	i = 0;
	while (i < n)
	{
		if (buf[i] == '\n')
			lines++;
		i++;
	}
	return (lines);
}

/**
 * @brief Makes sure tests/tmp exists before mkdtemp is asked to use it.
 *
 * @return 0 on success, -1 on failure.
 */
static int	ensure_tmp_root(void)
{
	if (mkdir("tests/tmp", TL_DIR_MODE) != 0 && errno != EEXIST)
		return (-1);
	return (0);
}
