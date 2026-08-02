#include "tetrislogd.h"

/**
 * @brief Entry point: a shim over logd_start, the loop, and logd_stop.
 *
 * Every decision lives behind those three calls, so the tests drive the real
 * daemon in-process and this function stays the one piece of the program no
 * test exercises. tetrislogd does not daemonise itself - dspawn already did
 * that before exec'ing it.
 *
 * @param argc Number of command-line arguments.
 * @param argv Optional argv[1]: the .tetrishrc to read.
 * @return EXIT_SUCCESS on a clean shutdown, EXIT_FAILURE when boot failed.
 */
int	main(int argc, char **argv)
{
	t_logd		lg;
	t_cfg		cfg;
	const char	*rc;

	rc = NULL;
	if (argc > 1)
		rc = argv[1];
	if (cfg_load(&cfg, rc) != 0)
	{
		fprintf(stderr, "%s: cannot load configuration: %s\n",
			TL_COMPONENT, strerror(errno));
		return (EXIT_FAILURE);
	}
	if (logd_start(&lg, &cfg) != 0)
	{
		fprintf(stderr, "%s: cannot start: %s\n",
			TL_COMPONENT, strerror(errno));
		return (EXIT_FAILURE);
	}
	while (lg.running)
	{
		if (logd_run_once(&lg) != 0)
		{
			fprintf(stderr, "%s: loop failed: %s\n",
				TL_COMPONENT, strerror(errno));
			logd_stop(&lg);
			return (EXIT_FAILURE);
		}
	}
	logd_stop(&lg);
	return (EXIT_SUCCESS);
}
