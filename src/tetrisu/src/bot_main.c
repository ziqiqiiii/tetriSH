/* ************************************************************************** */
/*                                                                            */
/*   bot_main.c — a bot's own process: sign in, join a room, and play          */
/*                                                                            */
/*   A second binary, because a bot has no screen. It links no notcurses and   */
/*   no SDL; everything it does is over the same socket a person's client      */
/*   uses, with the same handshake and the same requests. tetrisd never        */
/*   learns that it is not a person.                                          */
/*                                                                            */
/*       bin/tetrisu-bot --room r-1234 [--level normal] [--deadman FD]        */
/*                                                                            */
/*   Nothing here is printed for a human. The parent redirects this process's */
/*   stdout and stderr to a log before exec, because the terminal it would    */
/*   otherwise inherit is the one notcurses is drawing the board on - and the */
/*   frozen common.c prints a certificate report on every handshake.          */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu_net.h"
#include "tetrisu_bot.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
** One running bot. It lives here rather than in tetrisu_bot.h because the
** brain is pure and must not be made to depend on the session types to be
** unit-tested; the parent's side of a bot is a different shape again and
** lives in bot_proc.c.
**
** `deadman` is the read end of a pipe whose write end the parent holds. A
** parent that exits by any means - cleanly, killed, crashed - closes it, and
** the read returns EOF. It is watched on the same poll the session already
** runs, so an orphaned bot costs one fd and no timer.
*/
typedef struct s_bot_run
{
	t_net_client	net;
	t_bot			brain;
	int				deadman;
	/*
	** The write end of the pipe the parent reads the claimed account off.
	**
	** It is written once, right after the login that succeeds, and closed.
	** The parent forks long before there is an account to report and cannot
	** work out which one this bot got - the pool is walked for the first free
	** name and who else is connected decides where that lands - so the only
	** party that knows is this process.
	*/
	int				report;
	char			room[NET_ROOM_MAX];
	/*
	** What this bot knows about who it is fighting, kept here because the
	** snapshot cannot carry it between frames: the arena rides a slower clock
	** than the board does, so most frames say nothing about the rivals at all
	** and reading `attacked` off the frame in hand would have it flicker off
	** between arena pushes.
	**
	** `declared_target` is the mode the server has been told, or -1 for none,
	** and it is what stops this being a request per piece - a mode is sent
	** when the answer changes and not otherwise. It is reset between matches,
	** because a participant starts every match on RANDOM (room.c) and a bot
	** that remembered its last declaration would never re-send it.
	**
	** `has_arena` is how a Battle Royale is told from a Double without asking
	** the room: only that mode ever pushes an arena, and TARGET means nothing
	** in a game with one opponent in it.
	*/
	bool			attacked;
	bool			has_arena;
	int				declared_target;
}	t_bot_run;

// Static Functions
static int	parse_args(int argc, char **argv, t_bot_run *run,
				t_bot_level *level, t_net_config *cfg);
static int	claim_account(t_net_client *net);
static void	report_account(t_bot_run *run);
static int	join_and_ready(t_bot_run *run);
static int	declare_fighter(t_bot_run *run, const char *path);
static int	run_match(t_bot_run *run);
static int	answer_select_window(t_bot_run *run);
static bool	seat_is_locked(const t_body_room *view, uint64_t player_id);
static bool	orphaned(const t_bot_run *run);
static int	pump_once(t_bot_run *run);
static int	place_piece(t_bot_run *run);
static int	slide_to(t_bot_run *run, int target);
static int	await_move(t_bot_run *run, int from);
static void	note_attackers(t_bot_run *run);
static int	steer_target(t_bot_run *run);
static int	act(t_bot_run *run, t_solo_action action);
static int	settle_after(t_bot_run *run, uint64_t seq, int tries);
static bool	playable(const t_net_client *net);
static long	monotonic_ms(void);
static int	pace_until(t_bot_run *run, long deadline);

/**
 * @brief Entry point - claim an account, join the named room, and play it.
 *
 * @param argc Argument count.
 * @param argv Arguments; --room is required.
 * @return 0 when the bot left on purpose, 1 when it could not start.
 */
int	main(int argc, char **argv)
{
	t_bot_run		run;
	t_net_config	cfg;
	t_bot_level		level;

	memset(&run, 0, sizeof(run));
	run.deadman = -1;
	run.report = -1;
	/*
	** -1 rather than the zero memset left, because zero is TARGET_RANDOM and a
	** bot that thought it had already declared that would never declare
	** anything.
	*/
	run.declared_target = -1;
	level = BOT_NORMAL;
	/*
	** Loaded before the arguments are read, not after: the environment is the
	** default and the command line is the override. The parent knows which
	** server it is actually talking to and this process cannot - a typed
	** SERVER ID lives in the client's memory and was never in anybody's
	** environment - so what it passes has to win.
	*/
	net_config_load(&cfg);
	if (parse_args(argc, argv, &run, &level, &cfg) != 0)
		return (fprintf(stderr, "usage: %s --room NAME [--level easy|normal|"
				"ultra] [--deadman FD] [--report FD] [--host H] [--port P] "
				"[--ca PATH]\n",
				argv[0]), 1);
	bot_init(&run.brain, level, (uint32_t)getpid());
	if (net_connect(&run.net, &cfg) != 0)
		return (fprintf(stderr, "bot: no tetrisd at %s:%d\n", cfg.host,
				cfg.port), 1);
	if (claim_account(&run.net) != 0)
		return (fprintf(stderr, "bot: the account pool is full\n"),
			net_disconnect(&run.net), 1);
	report_account(&run);
	if (join_and_ready(&run) != 0)
		return (fprintf(stderr, "bot: could not join %s\n", run.room),
			net_disconnect(&run.net), 1);
	run_match(&run);
	net_disconnect(&run.net);
	return (0);
}

/**
 * @brief Read the command line.
 *
 * @param argc Argument count.
 * @param argv Arguments.
 * @param run Receives the room and the deadman descriptor.
 * @param level Receives the difficulty.
 * @param cfg Receives whichever server coordinates were passed.
 * @return 0 when a room was named, -1 otherwise.
 */
static int	parse_args(int argc, char **argv, t_bot_run *run,
			t_bot_level *level, t_net_config *cfg)
{
	int	index;

	index = 1;
	while (index + 1 < argc)
	{
		if (strcmp(argv[index], "--room") == 0)
			snprintf(run->room, sizeof(run->room), "%s", argv[index + 1]);
		else if (strcmp(argv[index], "--level") == 0)
			bot_level_parse(argv[index + 1], level);
		else if (strcmp(argv[index], "--deadman") == 0)
			run->deadman = atoi(argv[index + 1]);
		else if (strcmp(argv[index], "--report") == 0)
			run->report = atoi(argv[index + 1]);
		else if (strcmp(argv[index], "--host") == 0)
			snprintf(cfg->host, sizeof(cfg->host), "%s", argv[index + 1]);
		else if (strcmp(argv[index], "--port") == 0)
			cfg->port = atoi(argv[index + 1]);
		else if (strcmp(argv[index], "--ca") == 0)
			snprintf(cfg->ca_path, sizeof(cfg->ca_path), "%s",
				argv[index + 1]);
		index += 2;
	}
	if (run->room[0] == '\0')
		return (-1);
	return (0);
}

/**
 * @brief Log in as the first pool account that is not already connected.
 *
 * The walk is the whole of the claim. There is no reservation to take and
 * none to give back: a bot account already in use answers 409 rather than
 * displacing its holder, so trying the next name is both how a free one is
 * found and how a taken one is respected. A name past the end of the pool was
 * never created and answers 401, which ends the walk.
 *
 * @param net A connected, unauthenticated client.
 * @return 0 once logged in, -1 when the pool had nothing free.
 */
static int	claim_account(t_net_client *net)
{
	t_net_result	result;
	char			name[NET_USER_MAX];
	char			password[NET_USER_MAX * 2];
	int				index;

	index = 0;
	while (index < BOT_POOL_MAX)
	{
		snprintf(name, sizeof(name), "%s%02d", BOT_ACCOUNT_PREFIX, index + 1);
		snprintf(password, sizeof(password), "%s%s", BOT_ACCOUNT_SECRET, name);
		if (net_login(net, name, password, &result) != 0)
			return (-1);
		if (result.status == 200)
		{
			snprintf(net->username, sizeof(net->username), "%s", name);
			return (0);
		}
		if (result.status != 409)
			return (-1);
		index++;
	}
	return (-1);
}

/**
 * @brief Tells the parent which account this bot ended up with.
 *
 * One short write and then the pipe is closed, so the parent sees the name
 * followed by EOF and never has to guess whether more is coming. A parent that
 * has already gone makes the write fail, which is not worth acting on here -
 * the deadman is what ends this process in that case.
 *
 * Written after the login rather than before, because before it there is
 * nothing true to say: the pool is walked for the first free name, and which
 * one that is depends on who else is connected. That is the whole reason this
 * pipe exists - the parent forks the child and cannot work out its account
 * from anything it holds, so kicking could only ever mean "the one added last".
 *
 * @param run The bot, for its report descriptor and its account.
 */
static void	report_account(t_bot_run *run)
{
	char	line[BOT_NAME_MAX];
	int		length;

	if (run->report < 0)
		return ;
	length = snprintf(line, sizeof(line), "%s\n", run->net.username);
	if (length > 0 && write(run->report, line, (size_t)length) < 0)
		fprintf(stderr, "bot: could not report %s\n", run->net.username);
	close(run->report);
	run->report = -1;
}

/**
 * @brief Take a seat in the named room and declare a fighter for it.
 *
 * Readiness and the character are one request, because a seat is locked in
 * exactly when it names a fighter - so a bot that readied without one would
 * hold the room's select window open until it timed out.
 *
 * @param run The bot.
 * @return 0 on success, -1 when any step was refused.
 */
static int	join_and_ready(t_bot_run *run)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, run->room);
	if (net_request(&run->net, "JOIN", path, NULL, &result) != 0
		|| result.status != 200)
		return (-1);
	run->net.state = NET_IN_ROOM;
	if (net_match_join(&run->net, run->room) != 0)
		return (-1);
	return (declare_fighter(run, path));
}

/**
 * @brief Declare readiness and a fighter, which is what locks a seat in.
 *
 * The two are one request because a seat is locked exactly when it names a
 * character - so a bot that readied without one would hold a room's select
 * window open until it timed out.
 *
 * The fighter is whatever this pool account has equipped, which is the
 * starter every account is created with. Nothing buys anything for a bot.
 *
 * @param run The bot.
 * @param path The room's path.
 * @return 0 on success, -1 when the request failed.
 */
static int	declare_fighter(t_bot_run *run, const char *path)
{
	t_body_profile	profile;
	t_net_result	result;
	char			body[64];

	if (net_profile(&run->net, &profile) != 0)
		return (-1);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		profile.equipped_character);
	if (net_request(&run->net, "READY", path, body, &result) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Play until the parent goes away or the session does.
 *
 * The loop places a piece whenever there is one to place and otherwise waits
 * on the socket, so a bot in a waiting room, a countdown or a finished match
 * costs nothing. It does not stop at a result: a room can rematch, and a bot
 * that exited at game over would leave the seat it was added to fill.
 *
 * A piece gets a budget rather than the whole of the socket's speed, and the
 * budget is taken here rather than inside place_piece because it has to start
 * when the bot *notices* the piece: a bot that waited its tempo on top of the
 * time it spent placing would play a different tempo on every link. What is
 * left of the budget is spent pumping, so a paced bot is still reading its
 * board and still watching for a parent that has gone.
 *
 * @param run The bot.
 * @return 0 when it stopped on purpose, -1 when the session failed.
 */
static int	run_match(t_bot_run *run)
{
	long	deadline;
	int		idle;

	idle = 0;
	while (!orphaned(run))
	{
		if (pump_once(run) < 0)
			return (-1);
		if (playable(&run->net))
		{
			idle = 0;
			deadline = monotonic_ms() + bot_piece_pace_ms(&run->brain,
					run->net.state_snapshot.level);
			note_attackers(run);
			if (steer_target(run) < 0 || place_piece(run) < 0)
				return (-1);
			if (pace_until(run, deadline) < 0)
				return (-1);
			continue ;
		}
		if (++idle < BOT_SELECT_POLL_ROUNDS)
			continue ;
		idle = 0;
		if (answer_select_window(run) < 0)
			return (-1);
	}
	return (0);
}

/**
 * @brief Name a fighter again whenever the room opens a select window.
 *
 * A seat is locked in exactly when it names a character, and opening a select
 * window *clears* every seat's - deliberately, so that "locked in" is a fact
 * about this match rather than a leftover from the last one. A bot that named
 * its fighter when it joined and never again is therefore un-locked the moment
 * the owner starts, and the room has to wait TETRISD_MATCH_SELECT_MS for a
 * choice that is never coming. Every match with a bot in it began fifteen
 * seconds late, and nothing about it looked like an error.
 *
 * Asked rather than assumed, and only while there is no board to play: the
 * room's status is the one thing that says a window is open, and a bot that
 * re-readied on a timer would be sending a request per second all match.
 *
 * @param run The bot.
 * @return 0 on success, -1 when the session failed.
 */
static int	answer_select_window(t_bot_run *run)
{
	t_net_result	result;
	t_body_room		view;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, run->room);
	if (net_request(&run->net, "LIST", path, NULL, &result) != 0)
		return (-1);
	if (result.status != 200)
		return (0);
	if (body_room_decode(result.body, strlen(result.body), &view) != 0)
		return (0);
	if (view.select_ms <= 0 || seat_is_locked(&view, run->net.player_id))
		return (0);
	/*
	** A select window is the start of a match, and a match starts every
	** participant on TARGET_RANDOM (room.c). Forgetting the declaration here is
	** what makes the next one get sent; remembering it across a rematch would
	** leave the bot believing it had aimed when the server had reset it.
	*/
	run->declared_target = -1;
	run->attacked = false;
	return (declare_fighter(run, path));
}

/**
 * @brief Has this player's seat already named a fighter for this match?
 *
 * @param view The room as the server describes it.
 * @param player_id The seat to look for.
 * @return true when the seat is present and has a character.
 */
static bool	seat_is_locked(const t_body_room *view, uint64_t player_id)
{
	size_t	index;

	index = 0;
	while (index < view->member_count)
	{
		if (view->members[index].player_id == player_id)
			return (view->members[index].character != 0);
		index++;
	}
	return (false);
}

/**
 * @brief Has the parent gone away?
 *
 * A read of zero on the pipe is the parent's end being closed, which happens
 * however it died. Anything actually readable is treated the same way: the
 * pipe carries no messages, so a byte on it can only be a parent asking to be
 * rid of us.
 *
 * @param run The bot.
 * @return true when the bot should stop.
 */
static bool	orphaned(const t_bot_run *run)
{
	struct pollfd	pfd;
	char			scratch;

	if (run->deadman < 0)
		return (false);
	pfd.fd = run->deadman;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, 0) <= 0)
		return (false);
	if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))
		return (true);
	return (read(run->deadman, &scratch, 1) <= 0);
}

/**
 * @brief Wait briefly for the socket and read whatever came.
 *
 * @param run The bot.
 * @return 0 on success, -1 when the session failed.
 */
static int	pump_once(t_bot_run *run)
{
	struct pollfd	pfd;

	pfd.fd = net_fd(&run->net);
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (pfd.fd < 0)
		return (-1);
	poll(&pfd, 1, BOT_POLL_MS);
	if (net_pump(&run->net) < 0)
		return (-1);
	return (0);
}

/**
 * @brief Is there a piece of ours to place right now?
 *
 * @param net The session.
 * @return true when the snapshot shows an active piece to steer.
 */
static bool	playable(const t_net_client *net)
{
	return (net->has_state && net->state_snapshot.phase == BODY_PHASE_ACTIVE);
}

/**
 * @brief Rotate, slide and drop the falling piece where it does least harm.
 *
 * The column is chosen twice: once to pick the rotation, and again from the
 * snapshot that came back after rotating. A rotation near a wall kicks the
 * piece sideways, so a column planned before it is not the column the piece is
 * in afterwards.
 *
 * The wait after the drop is not politeness, it is the whole loop. Without it
 * the next piece is planned from the snapshot before this one landed - same
 * board, same piece - so every piece after the first is placed by a plan for
 * the one before it, and the stack goes straight to the ceiling without ever
 * completing a row.
 *
 * @param run The bot.
 * @return 0 on success, -1 when the session failed.
 */
static int	place_piece(t_bot_run *run)
{
	uint64_t	seq;
	int			rotation;
	int			target;
	int			steps;

	bot_begin_piece(&run->brain);
	seq = run->net.state_snapshot.seq;
	if (bot_plan(&run->brain, &run->net.state_snapshot, true, &rotation,
			&target))
	{
		steps = (rotation - run->net.state_snapshot.piece.rotation + 4) % 4;
		while (steps-- > 0)
			if (act(run, SOLO_ROTATE_CW) < 0)
				return (-1);
		settle_after(run, seq, BOT_PLAN_SETTLE_TRIES);
	}
	if (bot_plan(&run->brain, &run->net.state_snapshot, false, &rotation,
			&target)
		&& slide_to(run, target) < 0)
		return (-1);
	seq = run->net.state_snapshot.seq;
	if (act(run, SOLO_HARD_DROP) < 0)
		return (-1);
	settle_after(run, seq, BOT_SETTLE_TRIES);
	return (0);
}

/**
 * @brief Slide the piece to the column the plan named, verifying every step.
 *
 * One move at a time, each confirmed against the board that comes back. That
 * confirmation is the whole point, because a move is fire-and-forget on the
 * wire: net_send writes it and reads nothing, so the only evidence a move
 * landed is the snapshot afterwards.
 *
 * This was open loop until now. The distance was measured once and that many
 * moves were fired without looking, and `act` reports whether the *request*
 * went out and never whether the piece went anywhere - so a move the server
 * refused was counted as taken, and the rest of the slide was aimed from a
 * column the piece had never reached. Whatever that left was then hard-dropped:
 * a placement nothing chose, in the one situation where the choice matters
 * most, because refusals need something in the way and something in the way
 * means a tall stack.
 *
 * A refusal ends the slide instead of retrying it. Nothing about the board
 * changes while the bot holds the piece still, so a second attempt is the first
 * attempt; the piece is dropped from where it actually is, which is the honest
 * answer and the best one left. It also stops the bot spending its input rate
 * limit pushing against a wall.
 *
 * **How often this fires, measured: never.** Three bots over ~100 pieces each
 * in a real Battle Royale refused not one move. The reason is the geometry
 * rather than luck - a slide happens the moment a piece spawns, at the top of
 * the board, and the stack it could collide with is at the bottom. A refusal
 * needs the stack to have reached the spawn row, and a bot in that position is
 * dying regardless of which column this picks. So this is insurance and not a
 * repair: it is kept because it is correct and costs one server tick per
 * column out of a budget already spent idling, not because it was losing
 * placements. It was ranked above the attack weights before it was measured,
 * and that was the wrong way round.
 *
 * @param run The bot.
 * @param target The column the plan named.
 * @return 0 when the slide finished or was refused, -1 when the session failed.
 */
static int	slide_to(t_bot_run *run, int target)
{
	int	col;
	int	guard;
	int	moved;

	guard = 0;
	col = run->net.state_snapshot.piece.col;
	while (col != target && guard++ < BOT_SLIDE_MAX)
	{
		if (act(run, col < target ? SOLO_MOVE_RIGHT : SOLO_MOVE_LEFT) < 0)
			return (-1);
		moved = await_move(run, col);
		if (moved < 0)
			return (-1);
		if (moved > 0)
			return (0);
		col = run->net.state_snapshot.piece.col;
	}
	return (0);
}

/**
 * @brief Wait for the board that took a sideways move, and say whether it did.
 *
 * The column alone is watched, and not whether a frame arrived. Gravity pushes
 * a board of its own during the wait - a new row, the same column - so
 * "something came back" would read a falling piece as a move that landed.
 *
 * Silence is a real answer here rather than a timeout to be endured: an input
 * the server accepts marks the game dirty and the next tick pushes it, and one
 * it refuses marks nothing at all, so a column that has not moved by the end of
 * this is a column the piece cannot be in.
 *
 * @param run The bot.
 * @param from The column the piece was in when the move was sent.
 * @return 0 when the piece moved, 1 when nothing took it, -1 on a lost session.
 */
static int	await_move(t_bot_run *run, int from)
{
	int	tries;

	tries = 0;
	while (tries++ < BOT_MOVE_SETTLE_TRIES)
	{
		if (orphaned(run) || pump_once(run) < 0)
			return (-1);
		if (run->net.state_snapshot.piece.col != from)
			return (0);
	}
	return (1);
}

/**
 * @brief Remember whether anybody is landing rows on this bot.
 *
 * Read off the arena rather than off the opponents, because the arena is the
 * only section that says who is attacking *whom* - and only from a frame that
 * carries one. Most frames do not: the arena rides a slower clock than the
 * board, so `attacked` is left standing between pushes rather than recomputed
 * from a frame that says nothing, which would have it flicker off every board
 * tick and re-declare a mode twice a second.
 *
 * Seeing an arena at all is also what tells a Battle Royale from a Double,
 * which is why has_arena is latched here rather than asked of the room.
 *
 * @param run The bot; its attacked and has_arena flags are updated.
 */
static void	note_attackers(t_bot_run *run)
{
	const t_body_state	*snap;
	size_t				index;

	snap = &run->net.state_snapshot;
	if (!run->net.has_state || !snap->arena_present)
		return ;
	run->has_arena = true;
	run->attacked = false;
	index = 0;
	while (index < snap->arena_count)
	{
		if (snap->arena[index].player_id != run->net.player_id
			&& (snap->arena[index].flags & BODY_ARENA_ATTACKING_YOU) != 0)
			run->attacked = true;
		index++;
	}
}

/**
 * @brief Declare a targeting mode, but only when it is not the one already sent.
 *
 * Sent on the change and not on the piece. A bot that re-declared every piece
 * would put a request per second per bot onto a server that has ninety-eight
 * other seats to serve, for an answer that is the same one it gave last time.
 *
 * A refusal is remembered exactly like an acceptance, and that is deliberate:
 * what must not happen is a mode the server will not take being re-sent for
 * the rest of the match. The mode can still change later, which is the only
 * retry worth having.
 *
 * @param run The bot.
 * @return 0 on success or when there was nothing to send, -1 on a transport
 *         failure.
 */
static int	steer_target(t_bot_run *run)
{
	t_net_result	result;
	t_target_mode	mode;

	if (!run->has_arena)
		return (0);
	mode = bot_target_mode(&run->brain, run->attacked);
	if ((int)mode == run->declared_target)
		return (0);
	if (net_match_set_target(&run->net, mode, &result) != 0)
		return (-1);
	run->declared_target = (int)mode;
	return (0);
}

/**
 * @brief Send one action, respecting the rate limit every player is under.
 *
 * A throttled client waits rather than dropping the input, because the plan
 * it is halfway through executing is only correct if every step of it lands:
 * a rotation that went and a slide that did not is a piece in a column
 * nothing chose.
 *
 * @param run The bot.
 * @param action The action to send.
 * @return 0 on success, -1 when the session failed.
 */
static int	act(t_bot_run *run, t_solo_action action)
{
	int	waited;

	waited = 0;
	while (net_throttled(&run->net) && waited < BOT_SETTLE_TRIES)
	{
		if (orphaned(run) || pump_once(run) < 0)
			return (-1);
		waited++;
	}
	if (net_match_send_action(&run->net, action) != 0)
		return (-1);
	return (pump_once(run));
}

/**
 * @brief Wait for a snapshot newer than the one an action was planned from.
 *
 * @param run The bot.
 * @param seq The sequence number to get past.
 * @param tries How many poll rounds to spend waiting.
 * @return 0 once a newer snapshot arrived, -1 on failure or a lost parent.
 */
static int	settle_after(t_bot_run *run, uint64_t seq, int tries)
{
	while (tries-- > 0)
	{
		if (orphaned(run))
			return (-1);
		if (pump_once(run) < 0)
			return (-1);
		if (run->net.has_state && run->net.state_snapshot.seq > seq)
			return (0);
	}
	return (-1);
}

/**
 * @brief Monotonic milliseconds, for measuring a piece's budget.
 *
 * Monotonic and not wall-clock: a tempo measured against a clock somebody can
 * set backwards is a bot that stops for an hour.
 *
 * @return Milliseconds since an arbitrary fixed point.
 */
static long	monotonic_ms(void)
{
	struct timespec	now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return (0);
	return ((long)now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

/**
 * @brief Spend what is left of a piece's budget reading the socket.
 *
 * Pumping rather than sleeping, for two reasons that are both about not going
 * deaf for most of a second: the STATE frames that arrive during the wait are
 * the board the *next* piece is planned from, and the deadman pipe is how a
 * bot learns its player has closed the game.
 *
 * @param run The bot.
 * @param deadline The monotonic millisecond to wait until.
 * @return 0 once the budget is spent, -1 on a lost parent or session.
 */
static int	pace_until(t_bot_run *run, long deadline)
{
	while (monotonic_ms() < deadline)
	{
		if (orphaned(run))
			return (-1);
		if (pump_once(run) < 0)
			return (-1);
	}
	return (0);
}
