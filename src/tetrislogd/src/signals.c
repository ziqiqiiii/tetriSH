#include "tetrislogd.h"

// Static Functions
static void	on_signal(int signo);
static int	install_one(int signo);

// Static Variables
static volatile sig_atomic_t	g_pending = 0;
static volatile sig_atomic_t	g_wake_fd = -1;

/**
 * @brief Installs the daemon's signal handlers.
 *
 * SIGPIPE is ignored rather than handled: the degraded path writes to stderr,
 * and a closed pipe must not be able to kill the process that is recording
 * why everything else died.
 *
 * @param wake_fd Write end of the self-pipe the handler notifies.
 * @return 0 on success, -1 with errno set on failure.
 */
int	signals_install(int wake_fd)
{
	if (wake_fd < 0)
	{
		errno = EINVAL;
		return (-1);
	}
	g_wake_fd = wake_fd;
	g_pending = 0;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return (-1);
	if (install_one(SIGTERM) != 0 || install_one(SIGINT) != 0
		|| install_one(SIGHUP) != 0 || install_one(SIGUSR1) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Reports which signals arrived since the last call, and clears them.
 *
 * The loop asks once per wake-up, so two SIGHUPs between iterations coalesce
 * into one rotation - which is right, the file only needs reopening once.
 *
 * @return Bitmask of TETRISLOGD_SIGNAL_STOP, TETRISLOGD_SIGNAL_HUP and TETRISLOGD_SIGNAL_DUMP.
 */
int	signals_take(void)
{
	int	pending;

	pending = (int)g_pending;
	g_pending = 0;
	return (pending);
}

/**
 * @brief Forgets the self-pipe, leaving the handlers harmless.
 *
 * The daemon closes both ends of that pipe when it stops, and the number is
 * then free for the next open() to reuse. A signal arriving afterwards would
 * have the handler write a byte into whatever now owns that descriptor, so
 * the loop must disown it before the close rather than after.
 */
void	signals_detach(void)
{
	g_wake_fd = -1;
	g_pending = 0;
}

/**
 * @brief Records a signal and wakes the loop; does nothing else.
 *
 * Everything this signal means happens later, on the loop thread. A handler
 * that opened a file or wrote a record would be running code that is not
 * async-signal-safe between two arbitrary instructions.
 *
 * @param signo Signal that arrived.
 */
static void	on_signal(int signo)
{
	if (signo == SIGTERM || signo == SIGINT)
		g_pending |= TETRISLOGD_SIGNAL_STOP;
	else if (signo == SIGHUP)
		g_pending |= TETRISLOGD_SIGNAL_HUP;
	else if (signo == SIGUSR1)
		g_pending |= TETRISLOGD_SIGNAL_DUMP;
	if (g_wake_fd >= 0)
		selfpipe_notify((int)g_wake_fd);
}

/**
 * @brief Points one signal at the shared handler.
 *
 * Handlers are installed without SA_RESTART, so a signal arriving while the
 * loop is blocked in poll() interrupts it instead of being noticed a whole
 * idle tick later.
 *
 * @param signo Signal to install.
 * @return 0 on success, -1 with errno set on failure.
 */
static int	install_one(int signo)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	return (sigaction(signo, &sa, NULL));
}
