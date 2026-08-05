#include "tetrisd.h"

// Static Functions
static int	go_background(const t_cfg *cfg, t_pidfile *pf, int *ready);

/**
 * @brief Runs tetrisd: read .tetrishrc, detach, start the server, stop cleanly.
 *
 * The fork lives here, never behind server_start(), so the test suites can
 * boot a real server in-process without forking (docs/adr/0007).
 *
 * Boot owns the terminal: everything up to cd_ready reports failure on
 * stderr and exits non-zero, so the operator who typed the command sees it.
 * stderr only moves to the error file once nothing is left to fail - which
 * costs one window, where a record from the already-running log shipper
 * lands on the terminal. Redirecting earlier would hide boot failures.
 *
 * @param argc Number of command-line arguments.
 * @param argv Arguments; argv[1] optionally overrides the rc file path.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE when boot failed.
 */
int	main(int argc, char **argv)
{
	t_pidfile	pf;
	t_server	*srv;
	t_cfg		cfg;
	int			ready;

	if (cfg_load(&cfg, argc > 1 ? argv[1] : NULL) != 0)
	{
		fprintf(stderr, "tetrisd: %s holds an invalid setting\n", cfg.rc_path);
		return (EXIT_FAILURE);
	}
	if (cfg_validate(&cfg) != 0)
	{
		fprintf(stderr, "tetrisd: cannot read %s and %s - run `make certs`\n",
			cfg.cert_path, cfg.key_path);
		return (EXIT_FAILURE);
	}
	if (go_background(&cfg, &pf, &ready) != 0)
		return (EXIT_FAILURE);
	if (server_start(&cfg, &srv) != 0)
	{
		fprintf(stderr, "tetrisd: failed to start on port %d\n", cfg.port);
		cd_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	if (cd_stderr_redirect(cfg.err_path) != 0)
	{
		fprintf(stderr, "tetrisd: cannot open %s: %s\n",
			cfg.err_path, strerror(errno));
		server_stop(srv);
		cd_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	signals_install(srv);
	cd_ready(ready);
	server_wait(srv);
	signals_restore();
	server_stop(srv);
	cd_pid_release(&pf);
	return (EXIT_SUCCESS);
}

/**
 * @brief Detaches into the background and claims the pidfile.
 *
 * Claiming comes after the fork: the pid written has to be the detached
 * process's, and the lock has to be held by the process that stays around.
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
			TD_COMPONENT, strerror(errno));
		return (-1);
	}
	if (cd_pid_claim(pf, cfg->pid_path) != 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			fprintf(stderr, "%s: already running (%s)\n",
				TD_COMPONENT, cfg->pid_path);
		else
			fprintf(stderr, "%s: cannot claim %s: %s\n",
				TD_COMPONENT, cfg->pid_path, strerror(errno));
		return (-1);
	}
	return (0);
}
