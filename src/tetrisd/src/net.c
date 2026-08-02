#include "tetrisd.h"

// Static Functions
static int	bind_any(int fd, int port);

/**
 * @brief Opens the TCP listening socket the game server accepts clients on.
 *
 * Port 0 is deliberately supported: the kernel then assigns a free port,
 * which is how integration tests run several servers at once without
 * colliding. The chosen port is reported back through out_port.
 *
 * @param port Port to bind, or 0 to let the kernel choose.
 * @param out_port Receives the bound port (may be NULL).
 * @return The listening descriptor, or -1 on failure.
 */
int	net_listen(int port, int *out_port)
{
	struct sockaddr_in	addr;
	socklen_t			len;
	int					fd;
	int					on;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return (-1);
	on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind_any(fd, port) != 0 || listen(fd, SOMAXCONN) != 0
		|| us_set_nonblock(fd) != 0)
	{
		close(fd);
		return (-1);
	}
	len = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	if (out_port != NULL && getsockname(fd, (struct sockaddr *)&addr,
			&len) == 0)
		*out_port = (int)ntohs(addr.sin_port);
	return (fd);
}

/**
 * @brief Accepts one pending connection, if any is ready.
 *
 * The listener is non-blocking, so an empty backlog is an ordinary outcome
 * rather than an error; the caller simply polls again.
 *
 * @param listen_fd The listening descriptor.
 * @return The connected descriptor, or -1 when nothing was pending.
 */
int	net_accept(int listen_fd)
{
	int	fd;
	int	on;

	fd = accept(listen_fd, NULL, NULL);
	if (fd < 0)
		return (-1);
	on = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	return (fd);
}

/**
 * @brief Creates a directory and every missing parent along its path.
 *
 * Runtime paths come from .tetrishrc, so the directories they name may not
 * exist yet on a fresh clone; an already-existing directory is success.
 *
 * @param path Directory path to create.
 * @return 0 on success, -1 on failure.
 */
int	net_mkdir_p(const char *path)
{
	char	buf[TD_PATH_MAX];
	size_t	i;

	if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof(buf))
		return (-1);
	snprintf(buf, sizeof(buf), "%s", path);
	i = 1;
	while (buf[i] != '\0')
	{
		if (buf[i] == '/')
		{
			buf[i] = '\0';
			if (mkdir(buf, 0755) != 0 && errno != EEXIST)
				return (-1);
			buf[i] = '/';
		}
		i++;
	}
	if (mkdir(buf, 0755) != 0 && errno != EEXIST)
		return (-1);
	return (0);
}

/**
 * @brief Returns wall-clock milliseconds, for log record timestamps.
 *
 * @return Milliseconds since the epoch.
 */
uint64_t	net_now_ms(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (0);
	return ((uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000));
}

/**
 * @brief Measures elapsed monotonic milliseconds and rearms the marker.
 *
 * Gravity accumulates real elapsed time rather than assuming the ticker slept
 * exactly its period, so a descheduled thread does not slow the game down.
 *
 * @param last Marker holding the previous reading; updated to now.
 * @return Milliseconds since the previous reading, never negative.
 */
int	net_elapsed_ms(struct timespec *last)
{
	struct timespec	now;
	long			ms;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return (0);
	ms = (now.tv_sec - last->tv_sec) * 1000
		+ (now.tv_nsec - last->tv_nsec) / 1000000;
	*last = now;
	if (ms < 0)
		return (0);
	return ((int)ms);
}

/**
 * @brief Binds a socket to every interface on the given port.
 *
 * @param fd Socket to bind.
 * @param port Port number, 0 for kernel-assigned.
 * @return 0 on success, -1 on failure.
 */
static int	bind_any(int fd, int port)
{
	struct sockaddr_in	addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)port);
	return (bind(fd, (struct sockaddr *)&addr, sizeof(addr)));
}
