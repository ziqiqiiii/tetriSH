#include "minishell.h"

static void	sigint_handler(int signum);
static void	heredoc_sigint_handler(int signum);

/**
 * @brief Configures signal handling for the interactive shell prompt.
 *
 * Installs sigint_handler for SIGINT so Ctrl-C reprints the prompt, and
 * ignores SIGQUIT and SIGTSTP so Ctrl-\ and Ctrl-Z have no effect.
 *
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	shell_ignore_signals(void)
{
    struct sigaction	sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;           /* restart interrupted syscalls */

    sigaction(SIGINT, &sa, NULL);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
}

/**
 * @brief Restores default signal handling in a child process.
 *
 * Resets SIGINT, SIGQUIT, and SIGTSTP to SIG_DFL so that the child
 * process behaves like a normal program rather than inheriting the
 * shell's custom handlers.
 *
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	child_restore_signals(void)
{
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
}

/**
 * @brief Configures signal handling for the heredoc child process.
 *
 * Installs heredoc_sigint_handler for SIGINT so Ctrl-C causes the heredoc
 * child to exit immediately, allowing the parent to detect cancellation.
 * SIGQUIT and SIGTSTP are ignored.
 *
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
void	heredoc_restore_signals(void)
{
    struct sigaction	sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = heredoc_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;           /* restart interrupted syscalls */

    sigaction(SIGINT, &sa, NULL);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
}

/**
 * @brief Handles SIGINT and SIGQUIT during normal prompt operation.
 *
 * On SIGINT, prints a newline and redraws the prompt. On SIGQUIT,
 * simply redraws the prompt without any output.
 *
 * @param signum The signal number received.
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
static void	sigint_handler(int signum)
{
	(void) signum;

	write(1, "\n", 1);
	rl_replace_line("", 0);
	rl_on_new_line();
	rl_redisplay();
	return ;
}

/**
 * @brief Handles SIGINT during heredoc input (mode 2).
 *
 * Immediately exits the heredoc child process so the parent can
 * detect cancellation via the child's exit status.
 *
 * @param signum The signal number received.
 * @see https://github.com/natalieagus/C-Shell-custom/tree/master
 */
static void	heredoc_sigint_handler(int signum)
{
	(void) signum;
	
	exit(EXIT_FAILURE);
}
