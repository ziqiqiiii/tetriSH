#include "tetrisctl.h"

// Static Functions
static int	dispatch(const t_ctl *ctl, const char *verb, const char *only);
static int	usage(void);

/**
 * @brief Entry point: read the roster, then run one command against it.
 *
 * A thin shim, for the same reason both daemons' mains are: everything worth
 * testing lives behind config_load and the *_command functions, which the suites
 * drive in-process against a fake daemon rather than by running this binary.
 *
 * @param argc Number of command-line arguments.
 * @param argv [-f <rc>] <command> [daemon].
 * @return EXIT_SUCCESS when the command did what it said, EXIT_FAILURE
 * otherwise - including a daemon that failed to start, so a script can tell.
 */
int	main(int argc, char **argv)
{
	t_ctl		ctl;
	const char	*rc;
	int			i;

	rc = NULL;
	i = 1;
	if (argc > 2 && strcmp(argv[1], "-f") == 0)
	{
		rc = argv[2];
		i = 3;
	}
	if (i >= argc)
		return (usage());
	if (config_load(&ctl, rc) != 0)
	{
		fprintf(stderr, "%s: cannot read the roster from %s\n",
			TETRISCTL_COMPONENT_NAME, ctl.rc_path);
		return (EXIT_FAILURE);
	}
	if (dispatch(&ctl, argv[i], i + 1 < argc ? argv[i + 1] : NULL) != 0)
		return (EXIT_FAILURE);
	return (EXIT_SUCCESS);
}

/**
 * @brief Routes one verb to its command.
 *
 * @param ctl Resolved roster.
 * @param verb Command name.
 * @param only One daemon's name, or NULL for all of them.
 * @return 0 on success, -1 on failure or an unknown verb.
 */
static int	dispatch(const t_ctl *ctl, const char *verb, const char *only)
{
	if (strcmp(verb, "start") == 0)
		return (start_command(ctl, only));
	if (strcmp(verb, "status") == 0)
		return (status_command(ctl, only));
	if (strcmp(verb, "stop") == 0)
		return (stop_command(ctl, only));
	if (strcmp(verb, "restart") == 0)
		return (restart_command(ctl, only));
	fprintf(stderr, "%s: unknown command '%s'\n", TETRISCTL_COMPONENT_NAME, verb);
	usage();
	return (-1);
}

/**
 * @brief Prints how to call this program.
 *
 * @return EXIT_FAILURE, so callers can `return (usage());`.
 */
static int	usage(void)
{
	fprintf(stderr,
		"usage: %s [-f <rc>] start|status|stop|restart [daemon]\n"
		"       the daemons and their launch order come from %sDAEMONS\n"
		"       in .tetrishrc; stop and restart reverse that order\n",
		TETRISCTL_COMPONENT_NAME, TETRISCTL_CONFIG_KEY_PREFIX);
	return (EXIT_FAILURE);
}
