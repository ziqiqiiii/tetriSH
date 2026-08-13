/* ************************************************************************** */
/*                                                                            */
/*   stress_client.c - many players at once, against one live tetrisd         */
/*                                                                            */
/*   Not a test: nothing here asserts what the server should have said. It    */
/*   asks how much the server can say at all, and reports what it measured -  */
/*   how long a handshake took when forty of them arrived together, what a    */
/*   move's round trip costs while twenty rooms are ticking, and whether any  */
/*   session was lost on the way. scripts/stress.sh starts the server.        */
/*                                                                            */
/*   One process per worker, not one thread. tetrisu's network layer blocks   */
/*   on its socket by design - a client has one player and one board to wait  */
/*   for - so concurrency here has to come from outside it. Processes also    */
/*   make a bot that dies a bot that dies alone.                              */
/*                                                                            */
/*   The load is deliberately the real protocol from the real client: the     */
/*   same handshake, the same HTTTP messages, the same bodies. A generator    */
/*   that spoke a cheaper dialect would measure a server nobody plays on.     */
/*                                                                            */
/* ************************************************************************** */

#include "stress.h"

// Static Functions
static int		parse_plan(int argc, char **argv, t_stress_plan *plan);
static void		usage(const char *program);
static int		run_workers(const t_stress_plan *plan, t_stress_report *total);
static int		collect(int fd, int workers, t_stress_report *total);
static void		worker(const t_stress_plan *plan, int index, int out_fd);
static int		bots_sign_in(t_net_client *bots, int count, int base,
					t_stress_report *report);
static int		bots_seat(const t_stress_plan *plan, t_net_client *bots,
					int count, t_stress_report *report);
static int		deal_match(const t_stress_plan *plan, t_net_client *bots,
					int count, t_stress_report *report);
static int		deal_single(t_net_client *net, t_stress_report *report);
static int		deal_double(t_net_client *bots, int count,
					t_stress_report *report);
static int		lock_in(t_net_client *net);
static int		wait_playing(t_net_client *net, int tries);
static void		play_until(const t_stress_plan *plan, t_net_client *bots,
					int count, t_stress_report *report, uint64_t deadline);
static void		one_action(t_net_client *net, t_stress_report *report,
					uint32_t *seed);
static bool		match_is_over(const t_net_client *bots, int count,
					t_stress_mode mode);
static void		record(t_stress_report *report, uint64_t started_us);
static void		fail(t_stress_report *report, const char *what);
static void		merge(t_stress_report *total, const t_stress_report *one);
static void		print_report(const t_stress_plan *plan,
					const t_stress_report *total, uint64_t wall_ms);
static uint32_t	percentile_us(const t_stress_report *report, int per_mille);
static uint64_t	now_us(void);
static void		nap_us(uint64_t microseconds);

// Static Variables
/*
** What a bot does with its turn. Hard drop is one face in eight rather than
** one in seven so a board still fills and tops out inside a run - a fleet
** that never finishes a game never exercises the end of one.
*/
static const t_solo_action	g_actions[STRESS_ACTION_COUNT] = {
	SOLO_MOVE_LEFT, SOLO_MOVE_RIGHT, SOLO_ROTATE_CW, SOLO_ROTATE_CCW,
	SOLO_SOFT_DROP, SOLO_MOVE_LEFT, SOLO_MOVE_RIGHT, SOLO_HARD_DROP
};

/**
 * @brief Entry point - forks the fleet, plays for a while, prints what it cost.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments.
 * @return 0 when every bot connected and nothing errored, 1 otherwise, 2 on a
 *         bad command line.
 */
int	main(int argc, char **argv)
{
	t_stress_plan	plan;
	t_stress_report	total;
	uint64_t		started;
	int				failed;

	if (parse_plan(argc, argv, &plan) != 0)
	{
		usage(argv[0]);
		return (2);
	}
	memset(&total, 0, sizeof(total));
	started = now_us();
	failed = run_workers(&plan, &total);
	print_report(&plan, &total, (now_us() - started) / 1000);
	if (failed != 0 || total.errors != 0 || total.connected != total.bots)
	{
		printf("FAIL: %u of %u bots reached the server, %u errors\n",
			total.connected, total.bots, total.errors);
		return (1);
	}
	printf("PASS: %u players played %u matches with no lost session\n",
		total.bots, total.matches);
	return (0);
}

/**
 * @brief Reads the command line into a plan, filling the defaults first.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments.
 * @param plan Receives the run to perform.
 * @return 0 on success, -1 when an argument was not understood.
 */
/**
 * @brief How many bots one worker process holds.
 *
 * Double is a room of two and Single is a room of one, so a fleet of those is
 * many small rooms and one process each. A Battle Royale is the opposite
 * shape: its cost is that every seat is sent every other seat, so spreading
 * the players over rooms would measure a load the mode never puts on anything.
 * The whole fleet goes in one room, which makes it one worker.
 *
 * @param plan The run being performed.
 * @return Seats per worker.
 */
static int	plan_seats(const t_stress_plan *plan)
{
	if (plan->mode == STRESS_MODE_BATTLE_ROYALE)
	{
		if (plan->players > STRESS_MAX_SEATS)
			return (STRESS_MAX_SEATS);
		return (plan->players);
	}
	return (1 + (plan->mode == STRESS_MODE_DOUBLE));
}

/**
 * @brief Names a mode for the report.
 *
 * @param mode The mode to name.
 * @return Its name.
 */
static const char	*plan_mode_name(t_stress_mode mode)
{
	if (mode == STRESS_MODE_SINGLE)
		return ("single");
	if (mode == STRESS_MODE_BATTLE_ROYALE)
		return ("battle royale");
	return ("double");
}

static int	parse_plan(int argc, char **argv, t_stress_plan *plan)
{
	int	index;

	plan->mode = STRESS_MODE_DOUBLE;
	plan->players = STRESS_DEFAULT_PLAYERS;
	plan->seconds = STRESS_DEFAULT_SECONDS;
	plan->actions_per_second = STRESS_DEFAULT_RATE;
	plan->ramp_ms = 0;
	index = 1;
	while (index < argc)
	{
		if (strcmp(argv[index], "--mode") == 0 && index + 1 < argc)
		{
			index++;
			if (strcmp(argv[index], "single") == 0)
				plan->mode = STRESS_MODE_SINGLE;
			else if (strcmp(argv[index], "br") == 0)
				plan->mode = STRESS_MODE_BATTLE_ROYALE;
			else
				plan->mode = STRESS_MODE_DOUBLE;
		}
		else if (strcmp(argv[index], "--players") == 0 && index + 1 < argc)
			plan->players = atoi(argv[++index]);
		else if (strcmp(argv[index], "--seconds") == 0 && index + 1 < argc)
			plan->seconds = atoi(argv[++index]);
		else if (strcmp(argv[index], "--rate") == 0 && index + 1 < argc)
			plan->actions_per_second = atoi(argv[++index]);
		else if (strcmp(argv[index], "--ramp-ms") == 0 && index + 1 < argc)
			plan->ramp_ms = atoi(argv[++index]);
		else
			return (-1);
		index++;
	}
	if (plan->players < 1 || plan->seconds < 1
		|| plan->actions_per_second < 1 || plan->ramp_ms < 0)
		return (-1);
	return (0);
}

/**
 * @brief Prints how to drive this binary.
 *
 * @param program How it was invoked.
 */
static void	usage(const char *program)
{
	printf("usage: %s [--mode double|single|br] [--players N] "
		"[--seconds S]\n", program);
	printf("       [--rate ACTIONS_PER_SECOND] [--ramp-ms MS]\n\n");
	printf("  double  two bots per process, one Double room each\n");
	printf("  single  one bot per process - N simultaneous handshakes\n");
	printf("  br      every bot in one Battle Royale room, which is the "
		"only\n          shape that costs what the arena costs\n");
	printf("  the server's address comes from TETRISU_HOST / TETRISU_PORT\n");
}

/**
 * @brief Forks one worker per room and merges the accounts they send home.
 *
 * The workers all write into one pipe. Each report is smaller than PIPE_BUF
 * and goes out in a single write, so the kernel keeps them from interleaving -
 * which is why no lock, no file per worker, and no ordering is needed here.
 *
 * @param plan The run to perform.
 * @param total Receives the merged account.
 * @return 0 when every worker reported, 1 otherwise.
 */
static int	run_workers(const t_stress_plan *plan, t_stress_report *total)
{
	int		fds[2];
	int		seats;
	int		workers;
	int		index;
	pid_t	pid;

	seats = plan_seats(plan);
	workers = (plan->players + seats - 1) / seats;
	if (pipe(fds) != 0)
		return (perror("pipe"), 1);
	index = 0;
	while (index < workers)
	{
		pid = fork();
		if (pid < 0)
			return (perror("fork"), 1);
		if (pid == 0)
		{
			close(fds[0]);
			worker(plan, index, fds[1]);
			_exit(0);
		}
		index++;
	}
	close(fds[1]);
	index = collect(fds[0], workers, total);
	close(fds[0]);
	while (wait(NULL) > 0)
		;
	return (index);
}

/**
 * @brief Reads one report per worker off the pipe and merges each in.
 *
 * @param fd The pipe's read end.
 * @param workers How many reports to expect.
 * @param total Receives the merged account.
 * @return 0 when every report arrived whole, 1 otherwise.
 */
static int	collect(int fd, int workers, t_stress_report *total)
{
	t_stress_report	one;
	char			*into;
	size_t			held;
	ssize_t			taken;
	int				index;

	index = 0;
	while (index < workers)
	{
		into = (char *)&one;
		held = 0;
		while (held < sizeof(one))
		{
			taken = read(fd, into + held, sizeof(one) - held);
			if (taken <= 0)
				return (1);
			held += (size_t)taken;
		}
		merge(total, &one);
		index++;
	}
	return (0);
}

/**
 * @brief One worker's whole life: sign in, take a room, play, report.
 *
 * A worker that cannot get as far as a dealt match still reports. Its note
 * says which step refused it, and that note is the answer to the question
 * this program exists to ask - a fleet that half-arrives has found the limit.
 *
 * @param plan The run to perform.
 * @param index This worker's position in the fleet.
 * @param out_fd The pipe to report down.
 */
static void	worker(const t_stress_plan *plan, int index, int out_fd)
{
	t_net_client	bots[STRESS_MAX_SEATS];
	t_stress_report	report;
	uint64_t		deadline;
	int				seats;
	int				seat;

	seats = plan_seats(plan);
	memset(bots, 0, sizeof(bots));
	memset(&report, 0, sizeof(report));
	report.bots = (uint32_t)seats;
	if (plan->ramp_ms > 0)
		nap_us((uint64_t)plan->ramp_ms * 1000ull * (uint64_t)index);
	deadline = now_us() + (uint64_t)plan->seconds * 1000000ull;
	if (bots_sign_in(bots, seats, index * seats, &report) == 0
		&& bots_seat(plan, bots, seats, &report) == 0
		&& deal_match(plan, bots, seats, &report) == 0)
		play_until(plan, bots, seats, &report, deadline);
	seat = 0;
	while (seat < seats)
		net_disconnect(&bots[seat++]);
	if (write(out_fd, &report, sizeof(report)) != (ssize_t)sizeof(report))
		perror("report");
	close(out_fd);
}

/**
 * @brief Connects and registers this worker's bots, timing the handshake.
 *
 * The account is created rather than assumed, so a rerun against a server
 * that is still up finds its own names taken - 409 is accepted for exactly
 * that, and the password is the same one it signed up with.
 *
 * @param bots The clients to bring up.
 * @param count How many of them.
 * @param base This worker's first bot number, so names are unique fleet-wide.
 * @param report Receives the timings and the failure note.
 * @return 0 when every bot is signed in, -1 otherwise.
 */
static int	bots_sign_in(t_net_client *bots, int count, int base,
			t_stress_report *report)
{
	t_net_config	config;
	t_net_result	result;
	char			name[NET_USER_MAX];
	uint64_t		spent;
	int				index;

	net_config_load(&config);
	index = 0;
	while (index < count)
	{
		spent = now_us();
		if (net_connect(&bots[index], &config) != 0)
			return (fail(report, "connect"), -1);
		spent = (now_us() - spent) / 1000;
		report->connect_ms_total += spent;
		if ((uint32_t)spent > report->connect_ms_max)
			report->connect_ms_max = (uint32_t)spent;
		report->connected++;
		snprintf(name, sizeof(name), "st%d_%d", (int)getppid(), base + index);
		if (net_signup(&bots[index], name, STRESS_PASSWORD, &result) != 0
			|| (result.status != 201 && result.status != 409))
			return (fail(report, "signup"), -1);
		if (net_login(&bots[index], name, STRESS_PASSWORD, &result) != 0
			|| result.status != 200)
			return (fail(report, "login"), -1);
		snprintf(bots[index].username, NET_USER_MAX, "%s", name);
		report->authed++;
		index++;
	}
	return (0);
}

/**
 * @brief Puts this worker's bots in a room of the requested mode.
 *
 * Single is seated by deal_match rather than here: net_solo_start creates the
 * room and starts it in one call, so doing it here would deal a match that
 * deal_match immediately restarts - and a restarted match costs another
 * countdown, three seconds in which every input a bot sends is refused.
 * Double seats both bots and stops, because its room is dealt by deal_match
 * too, which has to run again after every match.
 *
 * @param plan The run to perform.
 * @param bots This worker's clients.
 * @param count How many of them.
 * @param report Receives the failure note.
 * @return 0 when every bot is seated, -1 otherwise.
 */
static int	bots_seat(const t_stress_plan *plan, t_net_client *bots,
			int count, t_stress_report *report)
{
	t_net_result	result;
	char			room[NET_ROOM_MAX];
	char			path[NET_PATH_MAX];
	int				index;

	if (plan->mode == STRESS_MODE_SINGLE)
	{
		report->seated++;
		return (0);
	}
	if (net_request(&bots[0], "JOIN", TETRISU_ROUTE_ROOMS,
			plan->mode == STRESS_MODE_BATTLE_ROYALE
			? "mode br\n" : "mode double\n",
			&result) != 0 || result.status != 201
		|| net_result_field(&result, "room", room, sizeof(room)) == NULL)
		return (fail(report, "open room"), -1);
	index = 0;
	while (index < count)
	{
		snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
		if (index > 0 && (net_request(&bots[index], "JOIN", path, NULL,
					&result) != 0 || result.status != 200))
			return (fail(report, "join room"), -1);
		bots[index].state = NET_IN_ROOM;
		if (net_match_join(&bots[index], room) != 0)
			return (fail(report, "bind room"), -1);
		report->seated++;
		index++;
	}
	return (0);
}

/**
 * @brief Deals this worker a match, first time and after every one that ends.
 *
 * @param plan The run to perform.
 * @param bots This worker's clients.
 * @param count How many of them.
 * @param report Receives the failure note.
 * @return 0 when a match is running, -1 otherwise.
 */
static int	deal_match(const t_stress_plan *plan, t_net_client *bots,
			int count, t_stress_report *report)
{
	if (plan->mode == STRESS_MODE_SINGLE)
		return (deal_single(&bots[0], report));
	return (deal_double(bots, count, report));
}

/**
 * @brief Puts a fresh Single game under one bot, whichever way still works.
 *
 * RESTART is only accepted mid-game: a board that has topped out is no longer
 * active, and server_room_input refuses every input to an inactive game. So
 * the room that just ended is left and a new one taken - which is exactly what
 * solo_authority.c does behind the R the Solo screen offers, for the same
 * reason. A bot that assumed RESTART worked spent the rest of its run being
 * refused, which read as a server buckling under load and was nothing of the
 * kind.
 *
 * @param net The bot's client.
 * @param report Receives the failure note.
 * @return 0 when a game is running, -1 otherwise.
 */
static int	deal_single(t_net_client *net, t_stress_report *report)
{
	t_net_result	result;

	net->has_state = false;
	memset(&result, 0, sizeof(result));
	if (net->state == NET_IN_GAME
		&& net_solo_restart(net, &result) == 0 && result.status == 200)
		return (0);
	if (net->state >= NET_IN_ROOM)
		net_solo_leave(net);
	if (net_solo_start(net, &result) != 0)
		return (fail(report, "solo start"), -1);
	return (0);
}

/**
 * @brief Opens the select window, locks both fighters in, waits for the deal.
 *
 * Both bots re-JOIN the room they are already sitting in. That is not a
 * repair: it is the request the client makes coming back from a results
 * screen, and it is what makes this one path serve the first match and every
 * rematch after it.
 *
 * @param bots This worker's clients.
 * @param count How many of them.
 * @param report Receives the failure note.
 * @return 0 when both boards are dealt, -1 otherwise.
 */
static int	deal_double(t_net_client *bots, int count, t_stress_report *report)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			note[STRESS_NOTE_MAX];
	int				index;

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, bots[0].room);
	index = 0;
	while (index < count)
	{
		bots[index].state = NET_IN_ROOM;
		bots[index].has_state = false;
		if (net_request(&bots[index], "JOIN", path, NULL, &result) != 0
			|| result.status != 200)
			return (fail(report, "rejoin"), -1);
		index++;
	}
	/*
	 * Retried, because the verdict reaches the client before the room is
	 * ready to be started again. advance_and_push sends the frame carrying
	 * the result and record_and_reset returns the seats on the same tick but
	 * after it - so a bot that starts the moment it sees the verdict is
	 * asking a room that is still IN_GAME, and is told 409 already-started.
	 * Waiting for a tick to pass is the bot's problem and not the server's.
	 */
	index = 0;
	while (index < STRESS_START_TRIES)
	{
		if (net_request(&bots[0], "START", path, NULL, &result) == 0
			&& result.status == 200)
			break ;
		nap_us(60000);
		index++;
	}
	if (index == STRESS_START_TRIES)
	{
		snprintf(note, sizeof(note), "start %d %.24s", result.status,
			result.reason);
		return (fail(report, note), -1);
	}
	index = 0;
	while (index < count)
		if (lock_in(&bots[index++]) != 0)
			return (fail(report, "lock in"), -1);
	index = 0;
	while (index < count)
		if (wait_playing(&bots[index++], 2400) != 0)
			return (fail(report, "deal"), -1);
	return (0);
}

/**
 * @brief Declares ready and names the fighter this bot already owns.
 *
 * @param net Client seated in the room.
 * @return 0 once the declaration was accepted, -1 otherwise.
 */
static int	lock_in(t_net_client *net)
{
	t_body_profile	profile;
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			body[64];

	if (net_profile(net, &profile) != 0)
		return (-1);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, net->room);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		profile.equipped_character);
	if (net_request(net, "READY", path, body, &result) != 0
		|| result.status != 200)
		return (-1);
	return (0);
}

/**
 * @brief Pumps until the room's countdown has run out and the board is live.
 *
 * @param net Client whose match was dealt.
 * @param tries How many 5 ms attempts to make.
 * @return 0 once play has begun, -1 otherwise.
 */
static int	wait_playing(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (-1);
		if (net->has_state && net->state_snapshot.countdown_ms == 0
			&& net->state_snapshot.phase != BODY_PHASE_COUNTDOWN)
		{
			net->state = NET_IN_GAME;
			return (0);
		}
		usleep(5000);
		tries--;
	}
	return (-1);
}

/**
 * @brief Plays every seat at the requested rate until the clock runs out.
 *
 * Frames are counted off the sequence number rather than off net_pump's
 * return, because a snapshot that crosses a reply is filed by net_request and
 * a loop that asked net_pump "did anything arrive?" would miss most of them.
 *
 * @param plan The run to perform.
 * @param bots This worker's clients.
 * @param count How many of them.
 * @param report Receives the counters and timings.
 * @param deadline When to stop, in the monotonic microseconds now_us returns.
 */
static void	play_until(const t_stress_plan *plan, t_net_client *bots,
			int count, t_stress_report *report, uint64_t deadline)
{
	uint64_t	seen[STRESS_MAX_SEATS];
	uint32_t	seed;
	int			index;

	memset(seen, 0, sizeof(seen));
	seed = (uint32_t)getpid() * 2654435761u;
	while (now_us() < deadline)
	{
		index = 0;
		while (index < count)
		{
			one_action(&bots[index], report, &seed);
			if (net_pump(&bots[index]) < 0)
			{
				fail(report, "session lost");
				return ;
			}
			if (bots[index].last_seq > seen[index])
			{
				report->frames += (uint32_t)(bots[index].last_seq
						- seen[index]);
				if (bots[index].state_snapshot.arena_present)
					report->arenas++;
			}
			seen[index] = bots[index].last_seq;
			index++;
		}
		if (match_is_over(bots, count, plan->mode))
		{
			report->matches++;
			memset(seen, 0, sizeof(seen));
			if (deal_match(plan, bots, count, report) != 0)
				return ;
		}
		nap_us(1000000ull / (uint64_t)plan->actions_per_second);
	}
}

/**
 * @brief Sends one bot's next input, or spends its charge, and times it.
 *
 * @param net Client with a match running.
 * @param report Receives the counters and the round trip.
 * @param seed The worker's pseudo-random state, advanced here.
 */
static void	one_action(t_net_client *net, t_stress_report *report,
			uint32_t *seed)
{
	t_net_result	result;
	uint64_t		started;
	int				sent;

	if (net->state != NET_IN_GAME)
		return ;
	*seed = *seed * 1103515245u + 12345u;
	started = now_us();
	if ((*seed >> 8) % STRESS_ABILITY_EVERY == 0)
	{
		sent = net_match_ability(net, (t_solo_ability)(SOLO_ABILITY_MIRURUN
					+ (*seed >> 16) % SOLO_ABILITY_COUNT), &result);
		report->abilities++;
	}
	else
		sent = net_match_action(net,
				g_actions[(*seed >> 16) % STRESS_ACTION_COUNT], &result);
	if (sent != 0)
	{
		fail(report, "input");
		return ;
	}
	record(report, started);
	report->actions++;
	if (result.status == 200)
		report->accepted++;
	else if (result.status == 429)
		report->throttled++;
	else
	{
		report->refused++;
		if (report->refusal[0] == '\0')
			snprintf(report->refusal, STRESS_NOTE_MAX, "%d %.48s",
				result.status, result.reason);
	}
}

/**
 * @brief Reports whether any seat has been told the match is decided.
 *
 * @param bots This worker's clients.
 * @param count How many of them.
 * @return true when a verdict or a top-out has arrived, false otherwise.
 */
static bool	match_is_over(const t_net_client *bots, int count,
			t_stress_mode mode)
{
	int	index;

	index = 0;
	while (index < count)
	{
		if (bots[index].has_state
			&& bots[index].state_snapshot.result != BODY_RESULT_NONE)
			return (true);
		/*
		 * A topped-out board is the end of a Double match and is not the end
		 * of a Battle Royale - the player watches the rest of it, which is the
		 * whole of D10. Reading it as the end had the fleet ask for a new
		 * match the moment the first of twenty-four bots died, and be told 409
		 * already-started for as long as the real match lasted.
		 *
		 * The verdict above is the only answer that means the match is over,
		 * in every mode: it is a fact about who else is left and nothing on
		 * the board says it.
		 */
		if (mode != STRESS_MODE_BATTLE_ROYALE && bots[index].has_state
			&& bots[index].state_snapshot.phase == BODY_PHASE_TOP_OUT)
			return (true);
		index++;
	}
	return (false);
}

/**
 * @brief Files one round trip in the histogram and against the maximum.
 *
 * @param report Receives the measurement.
 * @param started_us When the request went out.
 */
static void	record(t_stress_report *report, uint64_t started_us)
{
	uint64_t	spent;
	size_t		bucket;

	spent = now_us() - started_us;
	report->latency_us_total += spent;
	if (spent > report->latency_us_max)
		report->latency_us_max = (uint32_t)spent;
	bucket = (size_t)(spent / STRESS_BUCKET_US);
	if (bucket >= STRESS_BUCKETS)
		bucket = STRESS_BUCKETS - 1;
	report->latency[bucket]++;
}

/**
 * @brief Counts one fault and keeps the first thing that went wrong.
 *
 * The first is kept rather than the last because everything after a lost
 * session is a consequence of it, and a note saying "input" when the
 * handshake never finished names the symptom instead of the cause.
 *
 * @param report Receives the fault.
 * @param what Which step refused.
 */
static void	fail(t_stress_report *report, const char *what)
{
	report->errors++;
	if (report->note[0] == '\0')
		snprintf(report->note, STRESS_NOTE_MAX, "%s", what);
}

/**
 * @brief Adds one worker's account into the fleet's.
 *
 * @param total The running total.
 * @param one The worker's report.
 */
static void	merge(t_stress_report *total, const t_stress_report *one)
{
	size_t	bucket;

	total->bots += one->bots;
	total->connected += one->connected;
	total->authed += one->authed;
	total->seated += one->seated;
	total->matches += one->matches;
	total->actions += one->actions;
	total->abilities += one->abilities;
	total->accepted += one->accepted;
	total->throttled += one->throttled;
	total->refused += one->refused;
	total->errors += one->errors;
	total->frames += one->frames;
	total->arenas += one->arenas;
	total->connect_ms_total += one->connect_ms_total;
	total->latency_us_total += one->latency_us_total;
	if (one->connect_ms_max > total->connect_ms_max)
		total->connect_ms_max = one->connect_ms_max;
	if (one->latency_us_max > total->latency_us_max)
		total->latency_us_max = one->latency_us_max;
	if (one->note[0] != '\0' && total->note[0] == '\0')
		snprintf(total->note, STRESS_NOTE_MAX, "%s", one->note);
	if (one->refusal[0] != '\0' && total->refusal[0] == '\0')
		snprintf(total->refusal, STRESS_NOTE_MAX, "%s", one->refusal);
	bucket = 0;
	while (bucket < STRESS_BUCKETS)
	{
		total->latency[bucket] += one->latency[bucket];
		bucket++;
	}
}

/**
 * @brief Prints what the fleet cost the server.
 *
 * @param plan The run that was performed.
 * @param total The merged account.
 * @param wall_ms How long the whole run took.
 */
static void	print_report(const t_stress_plan *plan,
			const t_stress_report *total, uint64_t wall_ms)
{
	double	seconds;

	seconds = (double)wall_ms / 1000.0;
	printf("\n%d players, %s, %d s at %d actions/s each\n\n", plan->players,
		plan_mode_name(plan->mode),
		plan->seconds, plan->actions_per_second);
	printf("  connected     %u/%u   handshake avg %.0f ms   max %u ms\n",
		total->connected, total->bots, total->connected == 0 ? 0.0
		: (double)total->connect_ms_total / total->connected,
		total->connect_ms_max);
	printf("  signed in     %u      seated %u      matches %u\n",
		total->authed, total->seated, total->matches);
	printf("  requests      %u sent (%u abilities)   %.0f/s\n",
		total->actions, total->abilities,
		seconds > 0.0 ? total->actions / seconds : 0.0);
	printf("                %u accepted   %u throttled   %u refused%s%s\n",
		total->accepted, total->throttled, total->refused,
		total->refusal[0] != '\0' ? " - first: " : "", total->refusal);
	printf("  round trip    p50 %.2f ms   p95 %.2f ms   p99 %.2f ms"
		"   max %.2f ms\n", percentile_us(total, 500) / 1000.0,
		percentile_us(total, 950) / 1000.0,
		percentile_us(total, 990) / 1000.0, total->latency_us_max / 1000.0);
	printf("  state frames  %u   %.0f/s\n", total->frames,
		seconds > 0.0 ? total->frames / seconds : 0.0);
	if (plan->mode == STRESS_MODE_BATTLE_ROYALE)
		printf("  arena pushes  %u   %.1f/s per client   %u cards each\n",
			total->arenas, seconds > 0.0 && total->bots > 0
			? total->arenas / seconds / total->bots : 0.0, total->bots);
	printf("  errors        %u%s%s\n\n", total->errors,
		total->note[0] != '\0' ? "   first: " : "", total->note);
}

/**
 * @brief Reads one percentile out of the round-trip histogram.
 *
 * The bucket's lower edge is returned, so the figure understates by at most
 * one bucket - and the last bucket is everything that overflowed it, which is
 * why the maximum is printed from its own counter and not from here.
 *
 * @param report The merged account.
 * @param per_mille Which percentile, in thousandths.
 * @return The round trip at that percentile, in microseconds.
 */
static uint32_t	percentile_us(const t_stress_report *report, int per_mille)
{
	uint64_t	samples;
	uint64_t	wanted;
	uint64_t	seen;
	size_t		bucket;

	samples = 0;
	bucket = 0;
	while (bucket < STRESS_BUCKETS)
		samples += report->latency[bucket++];
	if (samples == 0)
		return (0);
	wanted = samples * (uint64_t)per_mille / 1000;
	seen = 0;
	bucket = 0;
	while (bucket < STRESS_BUCKETS)
	{
		seen += report->latency[bucket];
		if (seen >= wanted)
			return ((uint32_t)bucket * STRESS_BUCKET_US);
		bucket++;
	}
	return ((uint32_t)(STRESS_BUCKETS - 1) * STRESS_BUCKET_US);
}

/**
 * @brief The monotonic clock, in microseconds.
 *
 * @return Microseconds since an unspecified fixed point.
 */
static uint64_t	now_us(void)
{
	struct timespec	now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return ((uint64_t)now.tv_sec * 1000000ull + (uint64_t)now.tv_nsec / 1000);
}

/**
 * @brief Sleeps for any number of microseconds, however large.
 *
 * usleep is undefined past a second and refuses it on some libcs, which a
 * ramp of a hundred workers reaches on its own. nanosleep has no such ceiling
 * and is the one both the ramp and the pacing use.
 *
 * @param microseconds How long to sleep.
 */
static void	nap_us(uint64_t microseconds)
{
	struct timespec	span;

	span.tv_sec = (time_t)(microseconds / 1000000ull);
	span.tv_nsec = (long)(microseconds % 1000000ull) * 1000L;
	nanosleep(&span, NULL);
}
