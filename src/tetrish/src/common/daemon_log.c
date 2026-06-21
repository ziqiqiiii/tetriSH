#include "common.h"

/**
 * @brief Appends a timestamped message to the daemon log file.
 *
 * Writes to <project_root>/tmp/dspawn.log, taking an exclusive flock(2) while
 * appending so concurrent daemons do not interleave their lines.
 *
 * @param project_root Absolute path of the project root.
 * @param msg          Message string to record in the log.
 */
void	daemon_log(const char *project_root, const char *msg)
{
	char	log_path[PATH_MAX];
	int		fd;
	time_t	now;

	strncpy(log_path, project_root, sizeof(log_path) - 1);
	log_path[sizeof(log_path) - 1] = '\0';
	strncat(log_path, "/tmp/dspawn.log",
		sizeof(log_path) - strlen(log_path) - 1);
	fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd == -1)
	{
		perror("log open");
		return ;
	}
	now = time(NULL);
	flock(fd, LOCK_EX);
	dprintf(fd, "%sLogging dspawn daemon [%d] message: %s.\n", ctime(&now),
		getpid(), msg);
	close(fd);
}
