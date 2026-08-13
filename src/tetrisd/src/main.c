#include "tetrisd.h"

// Static Functions
static int	go_background(const t_config *cfg, t_pidfile *pf, int *ready);

/**
 * @brief Runs tetrisd: read .tetrishrc, detach, start the server, stop cleanly.
 *
 * The fork lives here, not in server_start(), so suites can boot a real
 * server in-process. Boot owns the terminal: failures up to daemon_ready hit
 * stderr and exit non-zero; stderr moves to the error file only once nothing
 * is left to fail. That costs one window in which a record from the running
 * log shipper lands on the terminal - redirecting earlier would hide boot
 * failures. tzset() resolves $TZ once, before anything can log.
 *
 * @param argc Number of command-line arguments.
 * @param argv Arguments; argv[1] optionally overrides the rc file path.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE when boot failed.
 */
int	main(int argc, char **argv)
{
	t_pidfile	pf;
	t_server	*srv;
	t_config	cfg;
	int			ready;

	tzset();
	if (config_load(&cfg, argc > 1 ? argv[1] : NULL) != 0)
	{
		fprintf(stderr, "tetrisd: %s holds an invalid setting\n", cfg.rc_path);
		return (EXIT_FAILURE);
	}
	if (config_validate(&cfg) != 0)
	{
		fprintf(stderr, "tetrisd: cannot read %s and %s - run `make certs`\n", cfg.cert_path, cfg.key_path);
		return (EXIT_FAILURE);
	}
	if (go_background(&cfg, &pf, &ready) != 0)
		return (EXIT_FAILURE);
	if (server_start(&cfg, &srv) != 0)
	{
		/* Every way server_start can fail has already said which one it was,
		** and said it to the logger - so name where that reason is rather
		** than guessing at it here. Naming the port was worse than saying
		** nothing: a store that would not open reported a port that was
		** never in use, and the one thing the boot had got right is what
		** the person who typed the command went and looked at. */
		fprintf(stderr, "tetrisd: failed to start - the reason was logged"
			" (tetrislogd's sink, or %s)\n", cfg.err_path);
		daemon_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	if (daemon_stderr_redirect(cfg.err_path) != 0)
	{
		fprintf(stderr, "tetrisd: cannot open %s: %s\n", cfg.err_path, strerror(errno));
		server_stop(srv);
		daemon_pid_release(&pf);
		return (EXIT_FAILURE);
	}
	signals_install(srv);
	daemon_ready(ready);
	server_wait(srv);
	signals_restore();
	server_stop(srv);
	daemon_pid_release(&pf);
	return (EXIT_SUCCESS);
}

/**
 * @brief Detaches into the background and claims the pidfile.
 *
 * Claiming comes after the fork: the pid written and the lock held both have
 * to belong to the process that stays around.
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
		fprintf(stderr, "%s: cannot detach: %s\n", TETRISD_COMPONENT_NAME, strerror(errno));
		return (-1);
	}
	if (daemon_pid_claim(pf, cfg->pid_path) != 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			fprintf(stderr, "%s: already running (%s)\n", TETRISD_COMPONENT_NAME, cfg->pid_path);
		else
			fprintf(stderr, "%s: cannot claim %s: %s\n", TETRISD_COMPONENT_NAME, cfg->pid_path, strerror(errno));
		return (-1);
	}
	return (0);
}
