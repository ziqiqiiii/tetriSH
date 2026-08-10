/* ************************************************************************** */
/*                                                                            */
/*   test_net_state_routing.c - STATE belongs to one active game             */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

#include <assert.h>

static void	make_state(t_htttp_message *msg, const char *path, uint64_t seq);

int	main(void)
{
	t_net_client		net;
	t_htttp_message	msg;

	memset(&net, 0, sizeof(net));
	snprintf(net.room, sizeof(net.room), "S-01");
	net.player_id = 17;
	snprintf(net.play_path, sizeof(net.play_path),
		"/room/S-01/player/17");
	make_state(&msg, "/room/S-02/player/17", 900);
	assert(!net_state_take(&net, &msg));
	assert(!net.has_state && net.last_seq == 0);
	htttp_message_free(&msg);
	make_state(&msg, "/room/S-01/player/18", 901);
	assert(!net_state_take(&net, &msg));
	assert(!net.has_state && net.last_seq == 0);
	htttp_message_free(&msg);
	make_state(&msg, "/room/S-01/player/17", 3);
	assert(net_state_take(&net, &msg));
	assert(net.has_state && net.last_seq == 3);
	htttp_message_free(&msg);
	make_state(&msg, "/room/S-02/player/17", 1000);
	assert(!net_state_take(&net, &msg));
	assert(net.last_seq == 3);
	htttp_message_free(&msg);
	net.play_path[0] = '\0';
	make_state(&msg, "/room/S-01/player/17", 1001);
	assert(!net_state_take(&net, &msg));
	assert(net.last_seq == 3);
	htttp_message_free(&msg);
	printf("PASS test_state_is_routed_to_the_current_game_only\n");
	return (0);
}

static void	make_state(t_htttp_message *msg, const char *path, uint64_t seq)
{
	t_body_state	snap;
	char			body[NET_BODY_MAX];
	int			len;

	memset(&snap, 0, sizeof(snap));
	snap.seq = seq;
	len = body_state_encode(&snap, body, sizeof(body));
	assert(len > 0);
	htttp_message_init(msg);
	assert(htttp_message_make_request(msg, "STATE", path) == HTTTP_OK);
	assert(htttp_message_set_body(msg, body, (size_t)len) == HTTTP_OK);
}
