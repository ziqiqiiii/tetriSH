#include "common.h"
#include <sys/wait.h>

extern int	g_exit_status;

/**
 * @brief Sends SIGKILL to a stopped child process and waits for it to terminate.
 *
 * Checks if the child indicated by g_exit_status was stopped (WIFSTOPPED).
 * If so, kills it with SIGKILL and waits for the process to fully exit.
 *
 * @param pid The PID of the child process to kill.
 */
void	ft_kill(int pid)
{
	if (WIFSTOPPED(g_exit_status))
	{
		kill(pid, SIGKILL);
		waitpid(pid, &g_exit_status, 0);
	}
}
