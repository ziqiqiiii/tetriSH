#include "tetrisd.h"

/**
 * @brief Runs tetrisd: read .tetrishrc, start the server, wait, stop cleanly.
 *
 * Deliberately thin. Everything the daemon does lives behind
 * server_start()/server_stop(), which is what lets the integration suite run
 * a real server in-process instead of forking one. No daemonising happens
 * here either - dspawn already did that before exec'ing this binary.
 *
 * @param argc Number of command-line arguments.
 * @param argv Arguments; argv[1] optionally overrides the rc file path.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE when boot failed.
 */
int	main(int argc, char **argv)
{
	t_server	*srv;
	t_cfg		cfg;

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
	if (server_start(&cfg, &srv) != 0)
	{
		fprintf(stderr, "tetrisd: failed to start on port %d\n", cfg.port);
		return (EXIT_FAILURE);
	}
	signals_install(srv);
	server_wait(srv);
	signals_restore();
	server_stop(srv);
	return (EXIT_SUCCESS);
}
