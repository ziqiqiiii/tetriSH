/* ************************************************************************** */
/*                                                                            */
/*   bots_smoke.c - one person and three bots in a Battle Royale              */
/*                                                                            */
/*   Not a unit test: it needs a server and it spawns processes, so           */
/*   tests/integration/test_bots.sh starts one and runs this against it.      */
/*                                                                            */
/*   What it proves is the whole of the feature end to end. A player opens a  */
/*   Battle Royale, which needs four and they are one; three bots fill it     */
/*   from the pool without the player naming an account; the room reaches a   */
/*   dealt match; the bots play boards that change; and when the player's     */
/*   process lets them go, they go.                                          */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"
#include "tetrisu_bot.h"

#include <sys/wait.h>

#define BOTS	3

// Static Functions
static int	check_a_room_of_one_fills_up(t_net_client *net, const char *room);
static int	check_the_match_deals(t_net_client *net, const char *room);
static int	check_the_bots_are_playing(t_net_client *net);
static int	check_letting_go_empties_the_room(t_net_client *net,
				const char *room);

static int	sign_up_and_in(t_net_client *net, const char *name);
static int	open_room(t_net_client *net, char *room, size_t cap);
static int	lock_in(t_net_client *net, const char *room);
static int	room_members(t_net_client *net, const char *room);
static int	pump_for(t_net_client *net, int ms);
static int	alive_arena_cards(const t_body_state *snap);
static void	report(const char *name, int ok, int *failures);

int	main(void)
{
	t_net_config	cfg;
	t_net_client	net;
	t_bot_farm		farm;
	char			room[NET_ROOM_MAX];
	char			name[NET_USER_MAX];
	int				failures;

	net_config_load(&cfg);
	memset(&net, 0, sizeof(net));
	bot_farm_init(&farm);
	if (net_connect(&net, &cfg) != 0)
		return (printf("FAIL: no tetrisd at %s:%d\n", cfg.host, cfg.port), 1);
	snprintf(name, sizeof(name), "hum%d", (int)getpid());
	if (!sign_up_and_in(&net, name) || !open_room(&net, room, sizeof(room)))
		return (printf("FAIL: could not open a Battle Royale\n"), 1);
	failures = 0;
	if (bot_farm_add(&farm, room, BOT_NORMAL) != 0
		|| bot_farm_add(&farm, room, BOT_NORMAL) != 0
		|| bot_farm_add(&farm, room, BOT_EASY) != 0)
		return (printf("FAIL: could not spawn bots\n"), 1);
	report("a room of one fills up",
		check_a_room_of_one_fills_up(&net, room), &failures);
	report("the match deals", check_the_match_deals(&net, room), &failures);
	report("the bots are playing", check_the_bots_are_playing(&net), &failures);
	bot_farm_clear(&farm);
	report("letting go empties the room",
		check_letting_go_empties_the_room(&net, room), &failures);
	net_disconnect(&net);
	return (failures != 0);
}

/*
** A Battle Royale needs four and the player is one. Nothing here names an
** account: each bot walks the pool until one takes it, which is the whole
** reason the pool is the server's and not the client's.
*/
static int	check_a_room_of_one_fills_up(t_net_client *net, const char *room)
{
	int	tries;

	tries = 0;
	while (tries < 200)
	{
		if (room_members(net, room) == BOTS + 1)
			return (1);
		pump_for(net, 50);
		tries++;
	}
	return (0);
}

/*
** Only the owner may start a Battle Royale, and the bots have already named
** their fighters - so the select window closes as soon as the player names
** theirs, rather than running its clock out.
*/
static int	check_the_match_deals(t_net_client *net, const char *room)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];
	int				tries;

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "START", path, NULL, &result) != 0
		|| result.status != 200)
		return (0);
	if (!lock_in(net, room))
		return (0);
	tries = 0;
	while (tries < 400)
	{
		pump_for(net, 25);
		if (net->has_state && net->state_snapshot.players == BOTS + 1)
			return (1);
		tries++;
	}
	return (0);
}

/*
** The arena is where a bot's play is visible from here: a card per seat with
** a one-bit mask of that board. A bot that connected and did nothing would
** fill the roster and leave every mask empty, which is exactly the failure
** this is looking for - so what is asserted is that cells appear on boards
** that are not the player's.
*/
static int	check_the_bots_are_playing(t_net_client *net)
{
	int	tries;

	tries = 0;
	while (tries < 800)
	{
		pump_for(net, 25);
		if (net->has_state && net->state_snapshot.arena_present
			&& alive_arena_cards(&net->state_snapshot) >= BOTS)
			return (1);
		tries++;
	}
	return (0);
}

/*
** The bots are this process's children, so letting them go is a signal and
** the seat is released by the disconnect path every dropped connection
** already takes. There is no KICK, no route, and no way to remove a person.
*/
static int	check_letting_go_empties_the_room(t_net_client *net,
			const char *room)
{
	int	tries;

	tries = 0;
	while (tries < 200)
	{
		if (room_members(net, room) == 1)
			return (1);
		pump_for(net, 50);
		tries++;
	}
	return (0);
}

/**
 * @brief Registers a fresh account and logs into it.
 *
 * @param net Connected client.
 * @param name Account name.
 * @return 1 on success, 0 otherwise.
 */
static int	sign_up_and_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	if (net_signup(net, name, "hunter2", &result) != 0 || result.status != 201)
		return (0);
	if (net_login(net, name, "hunter2", &result) != 0 || result.status != 200)
		return (0);
	snprintf(net->username, sizeof(net->username), "%s", name);
	return (1);
}

/**
 * @brief Opens a Battle Royale room and reports its name.
 *
 * @param net Authenticated client.
 * @param room Buffer receiving the room name.
 * @param cap Size of room.
 * @return 1 on success, 0 otherwise.
 */
static int	open_room(t_net_client *net, char *room, size_t cap)
{
	t_net_result	result;

	if (net_request(net, "JOIN", TETRISU_ROUTE_ROOMS,
			"mode br\n", &result) != 0 || result.status != 201)
		return (0);
	if (net_result_field(&result, "room", room, cap) == NULL)
		return (0);
	net->state = NET_IN_ROOM;
	return (net_match_join(net, room) == 0);
}

/**
 * @brief Declares readiness and a fighter, which is what locks a seat in.
 *
 * @param net Authenticated client seated in the room.
 * @param room The room name.
 * @return 1 on success, 0 otherwise.
 */
static int	lock_in(t_net_client *net, const char *room)
{
	t_body_profile	profile;
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			body[64];

	if (net_profile(net, &profile) != 0)
		return (0);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		profile.equipped_character);
	if (net_request(net, "READY", path, body, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

/**
 * @brief How many seats the room currently holds.
 *
 * @param net Authenticated client.
 * @param room The room name.
 * @return The member count, or -1 when the room could not be read.
 */
static int	room_members(t_net_client *net, const char *room)
{
	t_net_result	result;
	t_body_room		view;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "LIST", path, NULL, &result) != 0
		|| result.status != 200)
		return (-1);
	if (body_room_decode(result.body, strlen(result.body), &view) != 0)
		return (-1);
	return ((int)view.member_count);
}

/**
 * @brief Reads the socket for roughly this many milliseconds.
 *
 * @param net The session.
 * @param ms How long to spend.
 * @return 0 on success, -1 when the session failed.
 */
static int	pump_for(t_net_client *net, int ms)
{
	int	slept;

	slept = 0;
	while (slept < ms)
	{
		if (net_pump(net) < 0)
			return (-1);
		usleep(5000);
		slept += 5;
	}
	return (0);
}

/**
 * @brief How many arena cards belong to somebody else and carry a stack.
 *
 * A card with no mask is a board nothing has landed on yet, which is what a
 * bot that connected and never played would leave behind.
 *
 * @param snap The snapshot to read.
 * @return The count of other players' cards with at least one filled cell.
 */
static int	alive_arena_cards(const t_body_state *snap)
{
	size_t	index;
	size_t	byte;
	int		count;

	count = 0;
	index = 0;
	while (index < snap->arena_count)
	{
		byte = 0;
		while (byte < sizeof(snap->arena[index].mask))
		{
			if (((const unsigned char *)snap->arena[index].mask)[byte] != 0)
			{
				count++;
				break ;
			}
			byte++;
		}
		index++;
	}
	return (count);
}

/**
 * @brief Prints one check's verdict and counts the failures.
 *
 * @param name What was checked.
 * @param ok Whether it held.
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
