#include "tetrisctl.h"

// Static Functions
static int	dispatch(const t_ctl *ctl, int argc, char **argv);
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
		daemon_report_error(TETRISCTL_COMPONENT_NAME, ctl.rc_path, "cannot read the roster");
		return (EXIT_FAILURE);
	}
	if (dispatch(&ctl, argc - i, argv + i) != 0)
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
static int	dispatch(const t_ctl *ctl, int argc, char **argv)
{
	const char	*verb;
	const char	*only;

	verb = argv[0];
	only = argc > 1 ? argv[1] : NULL;
	if (strcmp(verb, "start") == 0)
		return (start_command(ctl, only));
	if (strcmp(verb, "status") == 0)
		return (status_command(ctl, only));
	if (strcmp(verb, "stop") == 0)
		return (stop_command(ctl, only));
	if (strcmp(verb, "restart") == 0)
		return (restart_command(ctl, only));
	if (strcmp(verb, "rooms") == 0)
		return (rooms_command(ctl, only));
	if (strcmp(verb, "players") == 0)
		return (players_command(ctl, only));
	if (strcmp(verb, "dropped-logs") == 0)
		return (dropped_command(ctl, only));
	if (strcmp(verb, "kick") == 0)
		return (kick_command(ctl, only, argc > 2 ? argv[2] : NULL));
	daemon_report_error(TETRISCTL_COMPONENT_NAME, verb, "unknown command");
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
		"       %s rooms|players|dropped-logs [daemon]\n"
		"       %s kick <player-id> [daemon]\n"
		"       the daemons and their launch order come from %sDAEMONS\n"
		"       in .tetrishrc; stop and restart reverse that order\n"
		"       rooms, players and dropped-logs ask the running daemon over\n"
		"       its control channel, so they need it up\n",
		TETRISCTL_COMPONENT_NAME, TETRISCTL_COMPONENT_NAME,
		TETRISCTL_COMPONENT_NAME, TETRISCTL_CONFIG_KEY_PREFIX);
	return (EXIT_FAILURE);
}