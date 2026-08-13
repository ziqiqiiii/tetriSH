#include "tetrisctl.h"

// Static Functions
static const t_managed	*channel_daemon(const t_ctl *ctl, const char *only);
static const t_managed	*find_channel(const t_ctl *ctl, const char *only);
static int				ask(const t_ctl *ctl, const char *only, const char *method, char *body, size_t cap, size_t *len);
static void				print_rooms(const char *body, size_t len);
static void				print_players(const char *body, size_t len);
static const char		*mode_name(t_body_mode mode);
static const char		*status_name(t_body_room_status status);

/*
** The four read-only admin verbs, each one question asked of one daemon.
**
** They share a shape: find the daemon that has a Control channel, ask it,
** decode the body with the same libstatusbody codec tetrisu would use, and
** print rows. Nothing here interprets the answer - a listing cap, for
** instance, is the server's decision and arrives as a header it already
** applied.
*/

/**
 * @brief ROOMS - the live room directory (UC-25).
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL to pick the one with a channel.
 * @return 0 when the daemon answered, -1 otherwise.
 */
int	rooms_command(const t_ctl *ctl, const char *only)
{
	static char	body[TETRISCTL_CONTROL_BODY_MAX];
	size_t		len;

	if (ask(ctl, only, "ROOMS", body, sizeof(body), &len) != 0)
		return (-1);
	daemon_report_break();
	print_rooms(body, len);
	daemon_report_break();
	return (0);
}

/**
 * @brief PLAYERS - every connection, logged in or not (UC-26).
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL to pick the one with a channel.
 * @return 0 when the daemon answered, -1 otherwise.
 */
int	players_command(const t_ctl *ctl, const char *only)
{
	static char	body[TETRISCTL_CONTROL_BODY_MAX];
	size_t		len;

	if (ask(ctl, only, "PLAYERS", body, sizeof(body), &len) != 0)
		return (-1);
	daemon_report_break();
	print_players(body, len);
	daemon_report_break();
	return (0);
}

/**
 * @brief DROPPED - the producer-side Dropped counter (UC-27).
 *
 * The verb is `dropped-logs` and the method is `DROPPED`. Dropped already
 * counts Log records, so the wire name does not repeat the unit; the operator's
 * verb keeps it because at a terminal it says what it fetches.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL to pick the one with a channel.
 * @return 0 when the daemon answered, -1 otherwise.
 */
int	dropped_command(const t_ctl *ctl, const char *only)
{
	static char		body[TETRISCTL_CONTROL_BODY_MAX];
	t_body_dropped	dropped;
	size_t			len;

	if (ask(ctl, only, "DROPPED", body, sizeof(body), &len) != 0)
		return (-1);
	if (body_dropped_decode(body, len, &dropped) != 0)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, "DROPPED",
			"the daemon's answer could not be read");
		return (-1);
	}
	daemon_report_break();
	printf("  dropped log records   %llu\n",
		(unsigned long long)dropped.dropped);
	daemon_report_break();
	return (0);
}

/**
 * @brief Prints the running daemon's Health beneath the pidfile report.
 *
 * Called by status_command after the pidfile lines, which are the half that
 * needs no running server. An unreachable channel is reported and returns -1,
 * but the lines already printed stand: a stopped daemon is a successful
 * report, and that is what status can honestly say about it.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL to pick the one with a channel.
 * @return 0 when Health was printed, -1 when the channel could not answer.
 */
int	health_report(const t_ctl *ctl, const char *only)
{
	static char		body[TETRISCTL_CONTROL_BODY_MAX];
	t_body_health	health;
	size_t			len;

	/*
	** Nothing selected has a channel - tetrislogd alone, say - so there is no
	** Health to print and that is not a failure. status is a report, and a
	** daemon that implements no channel has nothing to answer rather than
	** having failed to answer.
	*/
	if (find_channel(ctl, only) == NULL)
		return (0);
	if (ask(ctl, only, "STATUS", body, sizeof(body), &len) != 0)
		return (-1);
	if (body_health_decode(body, len, &health) != 0)
	{
		daemon_report_error(TETRISCTL_COMPONENT_NAME, "STATUS",
			"the daemon's answer could not be read");
		return (-1);
	}
	printf("  uptime       %llus\n",
		(unsigned long long)(health.uptime_ms / 1000));
	printf("  connections  %d\n", health.connections);
	printf("  rooms        %d\n", health.rooms);
	printf("  tick         %dms (configured)\n", health.tick_ms);
	printf("  sink         %s\n",
		health.sink_reaching ? "reaching" : "unreachable");
	return (0);
}

/**
 * @brief Picks the daemon an admin question goes to.
 *
 * With no name given there is exactly one candidate, because which daemons
 * expose a channel is compiled in rather than configured. A name that was
 * given is honoured, so an operator is never silently redirected to a
 * different daemon than the one they asked about.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for the one with a channel.
 * @return The daemon to ask, or NULL after reporting why there is none.
 */
static const t_managed	*channel_daemon(const t_ctl *ctl, const char *only)
{
	const t_managed	*d;
	int				i;

	if (only != NULL)
	{
		d = ctl_find_daemon(ctl, only);
		if (d == NULL)
			return (daemon_report_error(TETRISCTL_COMPONENT_NAME, only,
					"is not in " TETRISCTL_CONFIG_KEY_PREFIX "DAEMONS"), NULL);
		if (d->control_path[0] == '\0')
			return (daemon_report_error(TETRISCTL_COMPONENT_NAME, only,
					"has no control channel"), NULL);
		return (d);
	}
	i = 0;
	while (i < ctl->count)
	{
		if (ctl->daemons[i].control_path[0] != '\0')
			return (&ctl->daemons[i]);
		i++;
	}
	daemon_report_error(TETRISCTL_COMPONENT_NAME, "TETRISD_CONTROL_PATH",
		"is not set, so no daemon has a control channel");
	return (NULL);
}

/**
 * @brief Finds the daemon a channel question would go to, saying nothing.
 *
 * The silent twin of channel_daemon. A verb the operator typed deserves to be
 * told why it cannot be answered; status asks on their behalf and must not
 * turn "this daemon has no channel" into an error they did not ask about.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for the one with a channel.
 * @return The daemon to ask, or NULL when nothing selected has a channel.
 */
static const t_managed	*find_channel(const t_ctl *ctl, const char *only)
{
	const t_managed	*d;
	int				i;

	if (only != NULL)
	{
		d = ctl_find_daemon(ctl, only);
		if (d == NULL || d->control_path[0] == '\0')
			return (NULL);
		return (d);
	}
	i = 0;
	while (i < ctl->count)
	{
		if (ctl->daemons[i].control_path[0] != '\0')
			return (&ctl->daemons[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Asks one admin question and reports any failure in operator terms.
 *
 * @param ctl Resolved roster.
 * @param only One daemon's name, or NULL for the one with a channel.
 * @param method Method to send.
 * @param body Buffer receiving the response body.
 * @param cap Size of body.
 * @param len Receives the body length.
 * @return 0 on a 200, -1 otherwise.
 */
static int	ask(const t_ctl *ctl, const char *only, const char *method, char *body, size_t cap, size_t *len)
{
	const t_managed	*d;
	char			reason[256];
	int				status;

	d = channel_daemon(ctl, only);
	if (d == NULL)
		return (-1);
	status = control_ask(d, method, body, cap, len);
	if (status < 0)
	{
		snprintf(reason, sizeof(reason),
			"daemon not running, or the control channel cannot be reached");
		daemon_report_error(TETRISCTL_COMPONENT_NAME, d->control_path, reason);
		return (-1);
	}
	if (status != 200)
	{
		snprintf(reason, sizeof(reason), "the daemon answered %d", status);
		daemon_report_error(TETRISCTL_COMPONENT_NAME, method, reason);
		return (-1);
	}
	return (0);
}

/**
 * @brief Prints a decoded room directory, one line per room.
 *
 * @param body The response body.
 * @param len Length of body.
 */
static void	print_rooms(const char *body, size_t len)
{
	static t_body_room_row	rows[TETRISCTL_MAX_ROOM_ROWS];
	size_t					count;
	size_t					i;

	if (body_rooms_decode(body, len, rows, TETRISCTL_MAX_ROOM_ROWS,
			&count) != 0)
		return ((void)daemon_report_error(TETRISCTL_COMPONENT_NAME, "ROOMS",
				"the daemon's answer could not be read"));
	if (count == 0)
		return ((void)printf("  no open rooms\n"));
	i = 0;
	while (i < count)
	{
		printf("  %-8s %-14s %3d/%-3d %-10s %s\n", rows[i].name,
			mode_name(rows[i].mode), rows[i].players, rows[i].slot_count,
			status_name(rows[i].status), rows[i].owner);
		i++;
	}
}

/**
 * @brief Prints a decoded connection listing, one line per connection.
 *
 * @param body The response body.
 * @param len Length of body.
 */
static void	print_players(const char *body, size_t len)
{
	static t_body_player_row	rows[BODY_PLAYERS_MAX];
	size_t						count;
	size_t						i;

	if (body_players_decode(body, len, rows, BODY_PLAYERS_MAX, &count) != 0)
		return ((void)daemon_report_error(TETRISCTL_COMPONENT_NAME, "PLAYERS",
				"the daemon's answer could not be read"));
	if (count == 0)
		return ((void)printf("  nobody connected\n"));
	i = 0;
	while (i < count)
	{
		printf("  %-6llu %-20s %s\n",
			(unsigned long long)rows[i].connection,
			rows[i].authenticated ? rows[i].username : BODY_ANONYMOUS,
			rows[i].room[0] != '\0' ? rows[i].room : "-");
		i++;
	}
}

/**
 * @brief Names a room mode for the listing.
 *
 * @param mode Mode to name.
 * @return A short upper-case word, never NULL.
 */
static const char	*mode_name(t_body_mode mode)
{
	if (mode == BODY_MODE_SINGLE)
		return ("SINGLE");
	if (mode == BODY_MODE_DOUBLE)
		return ("DOUBLE");
	return ("BATTLE_ROYALE");
}

/**
 * @brief Names a room status for the listing.
 *
 * @param status Status to name.
 * @return A short upper-case word, never NULL.
 */
static const char	*status_name(t_body_room_status status)
{
	if (status == BODY_ROOM_WAITING)
		return ("WAITING");
	if (status == BODY_ROOM_READY)
		return ("READY");
	if (status == BODY_ROOM_SELECTING)
		return ("SELECTING");
	if (status == BODY_ROOM_IN_GAME)
		return ("IN_GAME");
	return ("FINISHED");
}
