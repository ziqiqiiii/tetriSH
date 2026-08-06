#include "tetrislogd.h"

// Static Functions
static int	go_background(const t_cfg *cfg, t_pidfile *pf, int *ready);
static int	run(t_logd *lg);

/**
 * @brief Entry point: detach, claim the pidfile, then loop until stopped.
 *
 * The fork lives here, never behind logd_start(), so the test suites can
 * boot the daemon in-process without forking (docs/adr/0007).
 *
 * Boot owns the terminal: everything up to cd_ready reports failure on
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
	t_cfg		cfg;
	int			ready;
	int			status;

	if (cfg_load(&cfg, argc > 1 ? argv[1] : NULL) != 0)
	{
		fprintf(stderr, "%s: cannot load configuration: %s\n",
			TL_COMPONENT, strerror(errno));
		return (EXIT_FAILURE);
	}
	if (go_background(&cfg, &pf, &ready) != 0)
		return (EXIT_FAILURE);
	if (logd_start(&lg, &cfg) != 0)
	{
		fprintf(stderr, "%s: cannot start: %s\n",
			TL_COMPONENT, strerror(errno));
		cd_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	if (cd_stderr_redirect(cfg.err_path) != 0)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n",
			TL_COMPONENT, cfg.err_path, strerror(errno));
		logd_stop(&lg);
		cd_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	cd_ready(ready);
	status = run(&lg);
	logd_stop(&lg);
	cd_pid_release(&pf);
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
 * @param ready Receives the readiness descriptor for cd_ready.
 * @return 0 on success, -1 after reporting why on stderr.
 */
static int	go_background(const t_cfg *cfg, t_pidfile *pf, int *ready)
{
	cd_pid_blank(pf);
	if (cd_detach(ready) != 0)
	{
		fprintf(stderr, "%s: cannot detach: %s\n",
			TL_COMPONENT, strerror(errno));
		return (-1);
	}
	if (cd_pid_claim(pf, cfg->pid_path) != 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			fprintf(stderr, "%s: already running (%s)\n",
				TL_COMPONENT, cfg->pid_path);
		else
			fprintf(stderr, "%s: cannot claim %s: %s\n",
				TL_COMPONENT, cfg->pid_path, strerror(errno));
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
				TL_COMPONENT, strerror(errno));
			return (EXIT_FAILURE);
		}
	}
	return (EXIT_SUCCESS);
}
