#include "minishell.h"

/**
 * @brief Temporarily ignores SIGINT and saves the previous handler.
 *
 * Used by the parent shell while waiting for a child process so that
 * Ctrl-C is delivered to the child rather than interrupting the wait.
 *
 * @param old Pointer to a sigaction struct that receives the previous handler,
 *            which can later be passed to sigint_restore().
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	sigint_ignore(struct sigaction *old)
{
	struct sigaction	ign;

	ign.sa_handler = SIG_IGN;
	sigemptyset(&ign.sa_mask);
	ign.sa_flags = 0;
	sigaction(SIGINT, &ign, old);
}

/**
 * @brief Restores the SIGINT handler saved by sigint_ignore().
 *
 * @param old Pointer to the sigaction struct previously filled by sigint_ignore().
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	sigint_restore(struct sigaction *old)
{
	sigaction(SIGINT, old, NULL);
}
