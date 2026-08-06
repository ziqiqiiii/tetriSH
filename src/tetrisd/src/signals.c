#include "tetrisd.h"

/*
** Signal handlers can touch almost nothing safely, so they touch as little as
** possible: a flag and one byte down the self-pipe. Everything real happens
** on the main loop when it wakes. These three are file-private state rather
** than shared globals precisely because nothing outside this file may read
** them without going through the take_* calls below.
*/

// Static Variables
static volatile sig_atomic_t	g_wake_fd = -1;
static volatile sig_atomic_t	g_stop_pending = 0;
static volatile sig_atomic_t	g_reload_pending = 0;
static volatile sig_atomic_t	g_dump_pending = 0;

// Static Functions
static void	on_stop(int sig);
static void	on_reload(int sig);
static void	on_dump(int sig);

/**
 * @brief Installs the daemon's signal handling around a running server.
 *
 * SIGTERM and SIGINT ask for a clean shutdown, SIGHUP re-reads .tetrishrc,
 * SIGUSR1 dumps the whole server state to the log, and SIGPIPE is ignored so
 * a client that vanishes mid-send cannot kill the whole server instead of
 * just its own connection.
 *
 * @param srv Server whose self-pipe the handlers wake.
 */
void	signals_install(t_server *srv)
{
	struct sigaction	act;

	if (srv == NULL)
		return ;
	g_wake_fd = srv->wake[SP_WRITE];
	g_stop_pending = 0;
	g_reload_pending = 0;
	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = on_stop;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	act.sa_handler = on_reload;
	sigaction(SIGHUP, &act, NULL);
	act.sa_handler = on_dump;
	sigaction(SIGUSR1, &act, NULL);
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
}

/**
 * @brief Reports and clears a pending shutdown request.
 *
 * @return true when a stop signal arrived since the last call.
 */
bool	signals_take_stop(void)
{
	if (g_stop_pending == 0)
		return (false);
	g_stop_pending = 0;
	return (true);
}

/**
 * @brief Reports and clears a pending configuration reload request.
 *
 * @return true when SIGHUP arrived since the last call.
 */
bool	signals_take_reload(void)
{
	if (g_reload_pending == 0)
		return (false);
	g_reload_pending = 0;
	return (true);
}

/**
 * @brief Reports and clears a pending state-dump request.
 *
 * @return true when SIGUSR1 arrived since the last call.
 */
bool	signals_take_state_dump(void)
{
	if (g_dump_pending == 0)
		return (false);
	g_dump_pending = 0;
	return (true);
}

/**
 * @brief Restores default signal handling and forgets the self-pipe.
 */
void	signals_restore(void)
{
	struct sigaction	act;

	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_DFL;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGPIPE, &act, NULL);
	g_wake_fd = -1;
}

/**
 * @brief Handler for SIGTERM and SIGINT: flag, then wake the main loop.
 *
 * @param sig The delivered signal (unused).
 */
static void	on_stop(int sig)
{
	(void)sig;
	g_stop_pending = 1;
	if (g_wake_fd >= 0)
		sp_notify((int)g_wake_fd);
}

/**
 * @brief Handler for SIGHUP: flag a reload, then wake the main loop.
 *
 * @param sig The delivered signal (unused).
 */
static void	on_reload(int sig)
{
	(void)sig;
	g_reload_pending = 1;
	if (g_wake_fd >= 0)
		sp_notify((int)g_wake_fd);
}

/**
 * @brief Handler for SIGUSR1: flag a state dump, then wake the main loop.
 *
 * @param sig The delivered signal (unused).
 */
static void	on_dump(int sig)
{
	(void)sig;
	g_dump_pending = 1;
	if (g_wake_fd >= 0)
		sp_notify((int)g_wake_fd);
}
