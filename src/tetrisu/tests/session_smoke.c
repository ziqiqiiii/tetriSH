/* ************************************************************************** */
/*                                                                            */
/*   session_smoke.c - what a session leaves behind, against a live tetrisd   */
/*                                                                            */
/*   Not a unit test: every check here is about a resource or a claim that    */
/*   only exists once there is a real server on the other end, so             */
/*   tests/integration/test_net_session.sh starts one and runs this.          */
/*                                                                            */
/*   These are regressions, not features. Each one shipped once: a socket     */
/*   that was never closed, an identity the client awarded itself, and a room */
/*   nobody could get back out of. None of them is visible to a test with a   */
/*   fake socket - the first needs a descriptor to leak, the second needs a   */
/*   server that answers SIGNUP without binding anybody, and the third needs  */
/*   a lobby that remembers the seat.                                         */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

#include <dirent.h>
#include <signal.h>

// Static Functions
static int	check_reconnecting_leaks_no_descriptor(const t_net_config *cfg);
static int	check_signup_alone_claims_no_identity(const t_net_config *cfg,
				const char *name);
static int	check_a_refused_solo_start_leaves_the_session_intact(
				const t_net_config *cfg, const char *name);
static int	check_a_reply_timeout_closes_the_session(
				const t_net_config *cfg, const char *name);
static int	check_a_timed_out_solo_leave_stays_offline(
				const t_net_config *cfg, const char *name);
static pid_t	fixture_pid(void);
static int	wait_until_stopped(pid_t pid);
static int	open_descriptors(void);
static int	sign_up_and_in(t_net_client *net, const char *name);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - each check owns its own connections start to finish.
 *
 * @return 0 when every check passed, 1 otherwise.
 */
int	main(void)
{
	t_net_config	cfg;
	t_net_client	probe;
	char			name[NET_USER_MAX];
	int				failures;

	net_config_load(&cfg);
	memset(&probe, 0, sizeof(probe));
	if (net_connect(&probe, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			probe.error);
		return (1);
	}
	net_disconnect(&probe);
	snprintf(name, sizeof(name), "sess%d", (int)getpid());
	failures = 0;
	report("reconnecting leaks no descriptor",
		check_reconnecting_leaks_no_descriptor(&cfg), &failures);
	report("signing up alone claims no identity",
		check_signup_alone_claims_no_identity(&cfg, name), &failures);
	report("a refused solo start leaves the session intact",
		check_a_refused_solo_start_leaves_the_session_intact(&cfg, name),
		&failures);
	report("a reply timeout closes the session",
		check_a_reply_timeout_closes_the_session(&cfg, name), &failures);
	report("a timed-out solo leave stays offline",
		check_a_timed_out_solo_leave_stays_offline(&cfg, name), &failures);
	return (failures != 0);
}

/**
 * @brief Every connection gives its descriptor back when it is closed.
 *
 * session_close wipes the keys and forgets the descriptor - it does not close
 * it, because the descriptor belongs to the caller. net_disconnect did not
 * either, so each reconnect leaked one; worse, no FIN was ever sent, so
 * tetrisd went on believing in a connection nobody was reading and held that
 * player's seat with it.
 *
 * Counting /proc/self/fd is the only honest check: the fault is a descriptor
 * that stays open, and nothing above the socket can see one.
 *
 * @param cfg Where the server is.
 * @return 1 when the count is unchanged after several full cycles.
 */
static int	check_reconnecting_leaks_no_descriptor(const t_net_config *cfg)
{
	t_net_client	net;
	int				before;
	int				round;

	memset(&net, 0, sizeof(net));
	if (net_connect(&net, cfg) != 0)
		return (0);
	net_disconnect(&net);
	/* counted after one full cycle, so nothing lazily opened is mistaken
	   for a leak */
	before = open_descriptors();
	round = 0;
	while (round < 8)
	{
		if (net_connect(&net, cfg) != 0)
			return (0);
		net_disconnect(&net);
		round++;
	}
	return (open_descriptors() == before);
}

/**
 * @brief SIGNUP creates an account; it does not sign anybody in.
 *
 * Identity belongs to the connection and only LOGIN claims it, so
 * a signup comes back with no Player-Id at all. Reading its 201 as "signed
 * in" left the client certain it was authenticated on a socket tetrisd
 * considered anonymous, and every route it then called answered 401.
 *
 * The existing provider smoke cannot catch this: it signs up and logs in and
 * then asserts the session is authenticated, which is true either way. What
 * has to be asserted is the state *between* the two.
 *
 * @param cfg Where the server is.
 * @param name Account to register.
 * @return 1 when signup leaves the connection anonymous and login binds it.
 */
static int	check_signup_alone_claims_no_identity(const t_net_config *cfg,
			const char *name)
{
	t_net_client	net;
	t_net_result	result;
	int				ok;

	memset(&net, 0, sizeof(net));
	if (net_connect(&net, cfg) != 0)
		return (0);
	memset(&result, 0, sizeof(result));
	if (net_signup(&net, name, "hunter2", &result) != 0 || result.status != 201)
		return (net_disconnect(&net), 0);
	ok = net.state < NET_AUTHED && net.player_id == 0;
	/* and the same socket still signs in, rather than needing a fresh one */
	memset(&result, 0, sizeof(result));
	if (net_login(&net, name, "hunter2", &result) != 0 || result.status != 200)
		return (net_disconnect(&net), 0);
	ok = ok && net.state >= NET_AUTHED && net.player_id != 0;
	net_disconnect(&net);
	return (ok);
}

/**
 * @brief A refused solo start changes nothing about the session.
 *
 * net_solo_start is two requests, and it used to keep whatever the first one
 * won when the second failed: the player was left in a room they were not
 * playing in and could not see, because the authority reports itself offline
 * and its close does nothing. Every later JOIN was then answered 409
 * already-in-room, and Solo silently ran the local rules for the rest of the
 * session.
 *
 * Read what this does and does not guard. The rollback itself cannot be
 * reached against a real tetrisd: a single room has one slot and
 * min_to_start of one, so the owner asking to start it is never refused, and
 * START has no rate limit to exhaust either. The rollback stays because the
 * branch above it - a reply that times out on a live socket - does fire, and
 * there it hands the seat back.
 *
 * So this guards the invariant rather than the fix: the only refusal a real
 * server will produce here is of the *first* request, which is what a second
 * start on a client already in a room gets, and it must change nothing. It
 * was confirmed to pass with the rollback removed, which is exactly why it is
 * named for the invariant and not for the branch.
 *
 * @param cfg Where the server is.
 * @param name Account to play as; already registered by the check above.
 * @return 1 when a refused start leaves the session intact and usable.
 */
static int	check_a_refused_solo_start_leaves_the_session_intact(
			const t_net_config *cfg, const char *name)
{
	t_net_client	net;
	char			held[NET_ROOM_MAX];
	int				ok;

	memset(&net, 0, sizeof(net));
	if (net_connect(&net, cfg) != 0)
		return (0);
	if (!sign_up_and_in(&net, name))
		return (net_disconnect(&net), 0);
	if (net_solo_start(&net, NULL) != 0)
		return (net_disconnect(&net), 0);
	snprintf(held, sizeof(held), "%s", net.room);
	/* a second start cannot be served, and must cost nothing */
	ok = net_solo_start(&net, NULL) != 0;
	ok = ok && net.state == NET_IN_GAME;
	ok = ok && strcmp(net.room, held) == 0;
	/* the room it was already in is still its own, not orphaned */
	net_solo_leave(&net);
	ok = ok && net.state == NET_AUTHED && net.room[0] == '\0';
	/* and having left, the next game is grantable - the seat really went back */
	ok = ok && net_solo_start(&net, NULL) == 0;
	net_solo_leave(&net);
	net_disconnect(&net);
	return (ok);
}

/**
 * @brief A timed-out request makes the stream unusable for later requests.
 *
 * HTTTP responses carry no request id. If the connection stayed open after a
 * timeout, the delayed response could be consumed as the answer to the next
 * request. The fixture daemon is stopped after login so the request reaches
 * the socket but no answer can arrive before the client's deadline.
 *
 * @param cfg Where the server is.
 * @param name Existing account used to authenticate the connection.
 * @return 1 when timeout closes the descriptor and marks the client offline.
 */
static int	check_a_reply_timeout_closes_the_session(
			const t_net_config *cfg, const char *name)
{
	t_net_client	net;
	t_net_result	result;
	pid_t		pid;
	int			rc;
	int			ok;

	pid = fixture_pid();
	if (pid <= 0)
	{
		printf("  timeout diagnostic: fixture pid unavailable\n");
		return (0);
	}
	memset(&net, 0, sizeof(net));
	if (net_connect(&net, cfg) != 0)
	{
		printf("  timeout diagnostic: connect failed\n");
		return (0);
	}
	if (!sign_up_and_in(&net, name))
	{
		printf("  timeout diagnostic: authentication failed\n");
		return (net_disconnect(&net), 0);
	}
	if (kill(pid, SIGSTOP) != 0)
	{
		printf("  timeout diagnostic: SIGSTOP failed (%d)\n", errno);
		return (net_disconnect(&net), 0);
	}
	if (!wait_until_stopped(pid))
	{
		(void)kill(pid, SIGCONT);
		printf("  timeout diagnostic: daemon did not stop\n");
		return (net_disconnect(&net), 0);
	}
	memset(&result, 0, sizeof(result));
	rc = net_request(&net, "LIST", TETRISU_ROUTE_ROOMS, NULL, &result);
	(void)kill(pid, SIGCONT);
	ok = rc == -1 && net.state == NET_OFFLINE && net.fd == -1;
	if (!ok)
		printf("  timeout diagnostic: rc=%d state=%d fd=%d status=%d\n",
			rc, (int)net.state, net.fd, result.status);
	net_disconnect(&net);
	return (ok);
}

/**
 * @brief A solo leave whose answer never comes must not fake a live session.
 *
 * net_solo_leave used to stamp NET_AUTHED unconditionally once its LEAVE
 * returned - including the timeout path, where the request had already closed
 * the connection. The client then believed it was signed in on a dead socket,
 * and the next game opened online only to fail its first request. Offline is
 * the honest state there: the server is told nothing more, and it reaps the
 * seat itself when the FIN arrives.
 *
 * @param cfg Where the server is.
 * @param name Existing account used to authenticate the connection.
 * @return 1 when the leave leaves the client offline with no descriptor.
 */
static int	check_a_timed_out_solo_leave_stays_offline(
			const t_net_config *cfg, const char *name)
{
	t_net_client	net;
	pid_t			pid;
	int				ok;

	pid = fixture_pid();
	if (pid <= 0)
	{
		printf("  leave diagnostic: fixture pid unavailable\n");
		return (0);
	}
	memset(&net, 0, sizeof(net));
	if (net_connect(&net, cfg) != 0)
	{
		printf("  leave diagnostic: connect failed\n");
		return (0);
	}
	if (!sign_up_and_in(&net, name) || net_solo_start(&net, NULL) != 0)
	{
		printf("  leave diagnostic: no game to leave\n");
		return (net_disconnect(&net), 0);
	}
	if (kill(pid, SIGSTOP) != 0 || !wait_until_stopped(pid))
	{
		(void)kill(pid, SIGCONT);
		printf("  leave diagnostic: daemon did not stop\n");
		return (net_disconnect(&net), 0);
	}
	net_solo_leave(&net);
	(void)kill(pid, SIGCONT);
	ok = net.state == NET_OFFLINE && net.fd == -1 && net.room[0] == '\0';
	if (!ok)
		printf("  leave diagnostic: state=%d fd=%d room=%s\n",
			(int)net.state, net.fd, net.room);
	net_disconnect(&net);
	return (ok);
}

/**
 * @brief Reads the daemon pid exported by the integration fixture.
 *
 * @return A positive pid, or -1 when the fixture did not provide one.
 */
static pid_t	fixture_pid(void)
{
	const char	*text;
	char		*end;
	long		value;

	text = getenv("TETRISD_TEST_PID");
	if (text == NULL || text[0] == '\0')
		return (-1);
	errno = 0;
	value = strtol(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0' || value <= 0
		|| value > INT_MAX)
		return (-1);
	return ((pid_t)value);
}

/**
 * @brief Waits until Linux reports that SIGSTOP has actually taken effect.
 *
 * kill() confirms delivery, not scheduling. Sending the request immediately
 * after it leaves a race in which the daemon can answer before the stop is
 * observed, so the test waits on the process state instead of sleeping for an
 * arbitrary interval.
 *
 * @param pid Daemon process to inspect.
 * @return 1 once stopped, 0 if it did not stop within one second.
 */
static int	wait_until_stopped(pid_t pid)
{
	char	path[64];
	char	line[128];
	FILE	*status;
	int		attempt;

	snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);
	attempt = 0;
	while (attempt < 100)
	{
		status = fopen(path, "r");
		if (status == NULL)
			return (0);
		while (fgets(line, sizeof(line), status) != NULL)
		{
			if (strncmp(line, "State:", strlen("State:")) == 0
				&& strchr(line, 'T') != NULL)
			{
				fclose(status);
				return (1);
			}
		}
		fclose(status);
		(void)poll(NULL, 0, 10);
		attempt++;
	}
	return (0);
}

/**
 * @brief Counts this process's open descriptors.
 *
 * @return The number of entries in /proc/self/fd, or -1 when it cannot be read.
 */
static int	open_descriptors(void)
{
	DIR				*dir;
	struct dirent	*entry;
	int				count;

	dir = opendir("/proc/self/fd");
	if (dir == NULL)
		return (-1);
	count = 0;
	entry = readdir(dir);
	while (entry != NULL)
	{
		if (entry->d_name[0] != '.')
			count++;
		entry = readdir(dir);
	}
	closedir(dir);
	return (count);
}

/**
 * @brief Registers an account if it is new, then signs in on this connection.
 *
 * @param net Connected client.
 * @param name Account name.
 * @return 1 when the connection is authenticated.
 */
static int	sign_up_and_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	memset(&result, 0, sizeof(result));
	net_signup(net, name, "hunter2", &result);
	if (result.status != 201 && result.status != 409)
		return (0);
	memset(&result, 0, sizeof(result));
	if (net_login(net, name, "hunter2", &result) != 0 || result.status != 200)
		return (0);
	return (net->state >= NET_AUTHED);
}

/**
 * @brief Prints one check's verdict and counts a failure.
 *
 * @param name What was checked.
 * @param ok Whether it passed.
 * @param failures Running failure count.
 */
static void	report(const char *name, int ok, int *failures)
{
	if (ok)
		printf("PASS: %s\n", name);
	else
	{
		printf("FAIL: %s\n", name);
		(*failures)++;
	}
}
