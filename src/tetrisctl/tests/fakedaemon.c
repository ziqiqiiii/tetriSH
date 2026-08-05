/* ************************************************************************** */
/*                                                                            */
/*   fakedaemon.c - a real daemon, standing in for the ones tetrisctl runs    */
/*                                                                            */
/*   The suites need something tetrisctl can genuinely fork, exec, wait on,   */
/*   signal, and watch let go of a pidfile. A stub would prove none of that,  */
/*   so this is the real sequence out of libcoredaemon - detach, claim,       */
/*   report ready, wait for SIGTERM, release - and nothing else.              */
/*                                                                            */
/*   The Makefile installs it twice under tests/bin/, named after each        */
/*   managed daemon, because tetrisctl resolves what to run through PATH      */
/*   exactly as dspawn did. It reads its pidfile from the same key its        */
/*   namesake publishes, so both ends agree without a private channel.        */
/*                                                                            */
/*   FAKE_FAIL makes it die during boot without ever reporting ready, which   */
/*   is the case dspawn could not observe and tetrisctl must. FAKE_TRACE      */
/*   names a file each instance appends to on its way out, which is how a     */
/*   test sees that teardown ran in the reverse of launch order.              */
/*                                                                            */
/* ************************************************************************** */

#include "coredaemon.h"
#include <signal.h>
#include <time.h>

// Static Variables
static volatile sig_atomic_t	g_stop = 0;

// Static Functions
static const char	*whoami(const char *argv0);
static const char	*pid_key_for(const char *name);
static void			on_term(int sig);
static void			wait_for_term(void);
static void			trace(const char *name);

int	main(int argc, char **argv)
{
	t_pidfile	pf;
	const char	*name;
	const char	*pid_path;
	int			ready;

	(void)argc;
	name = whoami(argv[0]);
	pid_path = getenv(pid_key_for(name));
	if (pid_path == NULL || pid_path[0] == '\0')
	{
		fprintf(stderr, "%s: %s is not set\n", name, pid_key_for(name));
		return (EXIT_FAILURE);
	}
	if (cd_detach(&ready) != 0)
		return (EXIT_FAILURE);
	if (getenv("FAKE_FAIL") != NULL)
	{
		fprintf(stderr, "%s: refusing to boot\n", name);
		return (EXIT_FAILURE);
	}
	cd_pid_blank(&pf);
	if (cd_pid_claim(&pf, pid_path) != 0)
	{
		fprintf(stderr, "%s: already running\n", name);
		return (EXIT_FAILURE);
	}
	signal(SIGTERM, on_term);
	cd_ready(ready);
	wait_for_term();
	trace(name);
	cd_pid_release(&pf);
	return (EXIT_SUCCESS);
}

/**
 * @brief Reports which daemon this copy is standing in for.
 *
 * @param argv0 The name it was exec'd under.
 * @return The basename of argv0.
 */
static const char	*whoami(const char *argv0)
{
	const char	*slash;

	slash = strrchr(argv0, '/');
	if (slash != NULL)
		return (slash + 1);
	return (argv0);
}

/**
 * @brief Reports which .tetrishrc key names this daemon's pidfile.
 *
 * The two prefixes spell it differently, and reproducing that here is
 * deliberate: a fake that read one agreed key would not exercise the mapping
 * tetrisctl actually has to get right.
 *
 * @param name Daemon name.
 * @return The environment variable holding its pidfile path.
 */
static const char	*pid_key_for(const char *name)
{
	if (strcmp(name, "tetrisd") == 0)
		return ("TETRISD_PID_PATH");
	return ("TETRISLOGD_PID");
}

/**
 * @brief Records that a stop was asked for.
 *
 * @param sig Signal number (unused).
 */
static void	on_term(int sig)
{
	(void)sig;
	g_stop = 1;
}

/**
 * @brief Sleeps until SIGTERM arrives, then returns.
 *
 * A daemon that exited on its own would let a stop test pass without the
 * signal ever being delivered, so this waits indefinitely and only the signal
 * ends it.
 */
static void	wait_for_term(void)
{
	struct timespec	ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 10000000L;
	while (g_stop == 0)
		nanosleep(&ts, NULL);
}

/**
 * @brief Appends this daemon's name to the trace file, if one was named.
 *
 * Written before the pidfile is released, never after: releasing the lock is
 * what unblocks tetrisctl's wait, so a trace line written afterwards could
 * land once the next daemon had already stopped and the order would be a lie.
 *
 * @param name Daemon name to record.
 */
static void	trace(const char *name)
{
	const char	*path;
	FILE		*f;

	path = getenv("FAKE_TRACE");
	if (path == NULL || path[0] == '\0')
		return ;
	f = fopen(path, "a");
	if (f == NULL)
		return ;
	fprintf(f, "%s\n", name);
	fclose(f);
}
