/* ************************************************************************** */
/*                                                                            */
/*   chat_smoke.c - the room feed, against a live tetrisd                     */
/*                                                                            */
/*   Not a unit test: this needs a server, so tests/integration/              */
/*   test_net_chat.sh starts one and runs this against it.                    */
/*                                                                            */
/*   What it guards is the half of the feed a unit test cannot reach. The     */
/*   ring test proves the ring; this proves that lines actually arrive - that */
/*   the server narrates a join without being asked, that a CHAT comes back   */
/*   to the player who sent it, and above all that a line which crosses a     */
/*   reply is still filed. That last one is the trap: net_request reads the   */
/*   socket too, so a client that only ever looked at net_pump's answer would */
/*   lose most of the feed and every test with a fake socket would pass.      */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

// Static Functions
static int	check_creating_a_room_narrates_it(t_net_client *net);
static int	check_a_sent_line_comes_back(t_net_client *net);
static int	check_a_line_crossing_a_reply_is_kept(t_net_client *net);
static int	check_the_server_refuses_what_it_should(t_net_client *net);
static int	check_the_provider_draws_the_servers_feed(
				t_app_net_session *session);
static int	sign_up_and_in(t_net_client *net, const char *name);
static int	create_room(t_net_client *net, char *room, size_t cap);
static int	settle(t_net_client *net, int tries);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - connect, register, take a room, then drive the feed.
 *
 * @return 0 when every check passed, 1 otherwise.
 */
int	main(void)
{
	t_net_config		cfg;
	t_app_net_session	session;
	char				name[NET_USER_MAX];
	int					failures;

	net_config_load(&cfg);
	memset(&session, 0, sizeof(session));
	if (net_connect(&session.net, &cfg) != 0)
	{
		printf("FAIL: no tetrisd at %s:%d (%s)\n", cfg.host, cfg.port,
			session.net.error);
		return (1);
	}
	session.connected = true;
	snprintf(name, sizeof(name), "chat%d", (int)getpid());
	if (!sign_up_and_in(&session.net, name))
	{
		printf("FAIL: could not register %s\n", name);
		return (1);
	}
	failures = 0;
	report("creating a room narrates it",
		check_creating_a_room_narrates_it(&session.net), &failures);
	report("a sent line comes back to its sender",
		check_a_sent_line_comes_back(&session.net), &failures);
	report("a line crossing a reply is kept",
		check_a_line_crossing_a_reply_is_kept(&session.net), &failures);
	report("the server refuses what it should",
		check_the_server_refuses_what_it_should(&session.net), &failures);
	report("the provider draws the server's feed",
		check_the_provider_draws_the_servers_feed(&session), &failures);
	net_disconnect(&session.net);
	return (failures != 0);
}

/*
** Nobody asked for these two lines: creating a room is what produced them,
** which is the whole difference between narration and chat.
*/
static int	check_creating_a_room_narrates_it(t_net_client *net)
{
	const t_body_chat	*line;
	char				room[NET_ROOM_MAX];

	if (!create_room(net, room, sizeof(room)))
		return (0);
	if (!settle(net, 20))
		return (0);
	if (net_chat_held(net) < 2)
		return (0);
	line = net_chat_at(net, 0);
	if (!line->system || line->sender[0] != '\0' || line->seq != 1
		|| strstr(line->text, "joined the room") == NULL)
		return (0);
	line = net_chat_at(net, 1);
	if (!line->system || line->seq != 2
		|| strstr(line->text, "set as owner") == NULL)
		return (0);
	return (1);
}

static int	check_a_sent_line_comes_back(t_net_client *net)
{
	const t_body_chat	*line;
	size_t				before;

	before = net->chat_received;
	if (net_chat_send(net, net->room, "good luck all", NULL) != 0)
		return (0);
	if (!settle(net, 20))
		return (0);
	if (net->chat_received <= before)
		return (0);
	line = net_chat_at(net, net_chat_held(net) - 1);
	if (line->system || strcmp(line->text, "good luck all") != 0)
		return (0);
	if (strcmp(line->sender, net->username) != 0)
		return (0);
	return (1);
}

/*
** The trap. net_chat_send is itself a net_request, so the echo of the line
** before it arrives while this one is waiting for its own reply - filed by
** net_request, never seen by net_pump. Sending several in a row and counting
** what was kept is what makes that visible.
*/
static int	check_a_line_crossing_a_reply_is_kept(t_net_client *net)
{
	uint64_t	before;
	int			index;

	before = net->chat_received;
	index = 0;
	while (index < 5)
	{
		if (net_chat_send(net, net->room, "rapid fire", NULL) != 0)
			return (0);
		index++;
	}
	settle(net, 20);
	if (net->chat_received - before != 5)
		return (0);
	return (1);
}

static int	check_the_server_refuses_what_it_should(t_net_client *net)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, "S-99");
	if (net_request(net, "CHAT", path, "text nobody here\n", &result) != 0
		|| result.status != 404)
		return (0);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, net->room);
	if (net_request(net, "CHAT", path, "room D-01\n", &result) != 0
		|| result.status != 400)
		return (0);
	return (1);
}

/*
** The seam the waiting room actually calls. What it proves beyond the checks
** above is that the panel draws what the server said and not what the client
** typed: send_chat appends nothing locally, so a line appearing in the room
** model can only have come back down the socket.
*/
static int	check_the_provider_draws_the_servers_feed(
	t_app_net_session *session)
{
	t_app_data_provider		provider;
	t_app_room_view_model	room;
	int						before;

	app_net_provider_init(&provider, session);
	if (provider.send_chat == NULL || provider.refresh_room == NULL)
		return (0);
	memset(&room, 0, sizeof(room));
	if (provider.refresh_room(session, session->net.room, &room)
		!= APP_PROVIDER_OK)
		return (0);
	before = room.chat_count;
	if (before == 0)
		return (0);
	if (provider.send_chat(session, session->net.room, "through the provider",
			&room) != APP_PROVIDER_OK)
		return (0);
	if (room.chat_count < before)
		return (0);
	if (strcmp(room.chat[room.chat_count - 1].text, "through the provider")
		!= 0)
		return (0);
	if (room.chat[room.chat_count - 1].system)
		return (0);
	/* the narration that opened the room is still in the same feed */
	if (!room.chat[0].system)
		return (0);
	if (provider.send_chat(session, session->net.room, "", &room)
		== APP_PROVIDER_OK)
		return (0);
	return (1);
}

static int	sign_up_and_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	if (net_signup(net, name, "hunter2", &result) != 0 || result.status != 201)
		return (0);
	if (net_login(net, name, "hunter2", &result) != 0 || result.status != 200)
		return (0);
	return (1);
}

static int	create_room(t_net_client *net, char *room, size_t cap)
{
	t_net_result	result;

	if (net_request(net, "JOIN", TETRISU_ROUTE_ROOMS, "mode double\n",
			&result) != 0 || result.status != 201)
		return (0);
	if (net_result_field(&result, "room", room, cap) == NULL)
		return (0);
	snprintf(net->room, sizeof(net->room), "%s", room);
	net->state = NET_IN_ROOM;
	return (1);
}

/*
** Pushed messages are not synchronous with the request that caused them, so
** the loop polls rather than assuming one turn is enough. It stops as soon as
** a turn brings nothing, which is what keeps a passing run fast.
*/
static int	settle(t_net_client *net, int tries)
{
	uint64_t	seen;
	int			index;

	index = 0;
	while (index < tries)
	{
		seen = net->chat_received;
		if (net_pump(net) < 0)
			return (0);
		if (net->chat_received == seen && index > 2)
			return (1);
		usleep(20000);
		index++;
	}
	return (1);
}

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
