#include "tetrislogd.h"

// Static Functions
static int	go_background(const t_config *cfg, t_pidfile *pf, int *ready);
static int	run(t_logd *lg);

/**
 * @brief Entry point: detach, claim the pidfile, then loop until stopped.
 *
 * The fork lives here, never behind logd_start(), so the test suites can
 * boot the daemon in-process without forking.
 *
 * Boot owns the terminal: everything up to daemon_ready reports failure on
 * stderr and exits non-zero, so the operator who typed the command sees it.
 * stderr only moves to the error file once the daemon is listening.
 *
 * @param argc Number of command-line arguments.
 * @param argv Optional argv[1]: the .tetrishrc to read.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE when boot failed.
 */
int	main(int argc, char **argv)
{
	t_pidfile	pf;
	t_logd		lg;
	t_config		cfg;
	int			ready;
	int			status;

	if (config_load(&cfg, argc > 1 ? argv[1] : NULL) != 0)
	{
		fprintf(stderr, "%s: cannot load configuration: %s\n",
			TETRISLOGD_COMPONENT_NAME, strerror(errno));
		return (EXIT_FAILURE);
	}
	if (go_background(&cfg, &pf, &ready) != 0)
		return (EXIT_FAILURE);
	if (logd_start(&lg, &cfg) != 0)
	{
		fprintf(stderr, "%s: cannot start: %s\n",
			TETRISLOGD_COMPONENT_NAME, strerror(errno));
		daemon_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	if (daemon_stderr_redirect(cfg.err_path) != 0)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n",
			TETRISLOGD_COMPONENT_NAME, cfg.err_path, strerror(errno));
		logd_stop(&lg);
		daemon_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	daemon_ready(ready);
	status = run(&lg);
	logd_stop(&lg);
	daemon_pid_release(&pf);
	return (status);
}

/**
 * @brief Detaches into the background and claims the pidfile.
 *
 * The order is the whole single-instance guard. Claiming comes after the
 * fork, because the pid written has to be the detached process's; it comes
 * before logd_start, because unixsock_dgram_bind unlinks its socket path
 * unconditionally, so a second instance has to lose the race and leave
 * before it can steal a running logger's socket.
 *
 * @param cfg Configuration supplying the pidfile path.
 * @param pf Pidfile to claim.
 * @param ready Receives the readiness descriptor for daemon_ready.
 * @return 0 on success, -1 after reporting why on stderr.
 */
static int	go_background(const t_config *cfg, t_pidfile *pf, int *ready)
{
	daemon_pid_blank(pf);
	if (daemon_detach(ready) != 0)
	{
		fprintf(stderr, "%s: cannot detach: %s\n",
			TETRISLOGD_COMPONENT_NAME, strerror(errno));
		return (-1);
	}
	if (daemon_pid_claim(pf, cfg->pid_path) != 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			fprintf(stderr, "%s: already running (%s)\n",
				TETRISLOGD_COMPONENT_NAME, cfg->pid_path);
		else
			fprintf(stderr, "%s: cannot claim %s: %s\n",
				TETRISLOGD_COMPONENT_NAME, cfg->pid_path, strerror(errno));
		return (-1);
	}
	return (0);
}

/**
 * @brief Steps the daemon loop until a signal stops it or the loop fails.
 *
 * @param lg Started daemon.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE on a loop error.
 */
static int	run(t_logd *lg)
{
	while (lg->running)
	{
		if (logd_run_once(lg) != 0)
		{
			fprintf(stderr, "%s: loop failed: %s\n",
				TETRISLOGD_COMPONENT_NAME, strerror(errno));
			return (EXIT_FAILURE);
		}
	}
	return (EXIT_SUCCESS);
}
