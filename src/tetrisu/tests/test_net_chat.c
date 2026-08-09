#include "tetrisu.h"

/*
** The client's half of the room feed, fed lines by hand.
**
** No socket and no server: net_chat_take is where a decoded line lands, so a
** t_net_client and a t_body_chat are the whole of what the ring needs. The
** suite that drives a real tetrisd is tests/integration/test_net_chat.sh.
**
** What this guards is the bargain the ring makes. tetrisd keeps no history,
** so this is the only backlog anybody has - and it is allowed to lose its
** oldest line, but never to refuse the newest one and never to hand back a
** line in the wrong order once it has wrapped.
*/

// Static Functions
static void	test_lines_come_back_oldest_first(void);
static void	test_a_full_ring_drops_its_oldest_not_its_newest(void);
static void	test_received_counts_every_line_not_the_ones_held(void);
static void	test_an_empty_feed_reads_as_empty(void);
static void	test_narration_and_chat_share_the_ring(void);
static void	test_send_refuses_what_the_wire_would_refuse(void);

static void	take_line(t_net_client *net, const char *sender, const char *text);

int	main(void)
{
	test_lines_come_back_oldest_first();
	test_a_full_ring_drops_its_oldest_not_its_newest();
	test_received_counts_every_line_not_the_ones_held();
	test_an_empty_feed_reads_as_empty();
	test_narration_and_chat_share_the_ring();
	test_send_refuses_what_the_wire_would_refuse();
	return (0);
}

static void	test_lines_come_back_oldest_first(void)
{
	t_net_client	net;

	memset(&net, 0, sizeof(net));
	take_line(&net, "amber", "first");
	take_line(&net, "blake", "second");
	assert(net_chat_held(&net) == 2);
	assert(strcmp(net_chat_at(&net, 0)->text, "first") == 0);
	assert(strcmp(net_chat_at(&net, 1)->text, "second") == 0);
	assert(net_chat_at(&net, 2) == NULL);
	printf("PASS test_lines_come_back_oldest_first\n");
}

/*
** The wrap is where an off-by-one would show as lines in the wrong order
** rather than as a crash, so every held line is checked, not just the ends.
*/
static void	test_a_full_ring_drops_its_oldest_not_its_newest(void)
{
	t_net_client	net;
	char			text[32];
	size_t			index;

	memset(&net, 0, sizeof(net));
	index = 0;
	while (index < NET_CHAT_HISTORY + 5)
	{
		snprintf(text, sizeof(text), "line %zu", index);
		take_line(&net, "amber", text);
		index++;
	}
	assert(net_chat_held(&net) == NET_CHAT_HISTORY);
	index = 0;
	while (index < NET_CHAT_HISTORY)
	{
		snprintf(text, sizeof(text), "line %zu", index + 5);
		assert(strcmp(net_chat_at(&net, index)->text, text) == 0);
		index++;
	}
	printf("PASS test_a_full_ring_drops_its_oldest_not_its_newest\n");
}

/*
** A screen notices a new line by comparing this against what it last drew.
** Were it the held count it would stop moving the moment the ring filled,
** and every line after that would go undrawn.
*/
static void	test_received_counts_every_line_not_the_ones_held(void)
{
	t_net_client	net;
	size_t			index;

	memset(&net, 0, sizeof(net));
	index = 0;
	while (index < NET_CHAT_HISTORY * 2)
	{
		take_line(&net, "amber", "chatter");
		index++;
	}
	assert(net.chat_received == NET_CHAT_HISTORY * 2);
	assert(net_chat_held(&net) == NET_CHAT_HISTORY);
	printf("PASS test_received_counts_every_line_not_the_ones_held\n");
}

static void	test_an_empty_feed_reads_as_empty(void)
{
	t_net_client	net;

	memset(&net, 0, sizeof(net));
	assert(net_chat_held(&net) == 0);
	assert(net_chat_at(&net, 0) == NULL);
	assert(net_chat_held(NULL) == 0);
	assert(net_chat_at(NULL, 0) == NULL);
	net_chat_take(NULL, NULL);
	printf("PASS test_an_empty_feed_reads_as_empty\n");
}

static void	test_narration_and_chat_share_the_ring(void)
{
	t_net_client	net;
	t_body_chat		line;

	memset(&net, 0, sizeof(net));
	take_line(&net, "amber", "good luck");
	memset(&line, 0, sizeof(line));
	line.seq = 2;
	line.system = true;
	snprintf(line.text, sizeof(line.text), "PLAYER blake joined the room D-01");
	net_chat_take(&net, &line);
	assert(net_chat_held(&net) == 2);
	assert(!net_chat_at(&net, 0)->system);
	assert(strcmp(net_chat_at(&net, 0)->sender, "amber") == 0);
	assert(net_chat_at(&net, 1)->system);
	assert(net_chat_at(&net, 1)->sender[0] == '\0');
	printf("PASS test_narration_and_chat_share_the_ring\n");
}

/*
** The composer refuses locally what the server would refuse anyway, so a
** player finds out while they can still edit the line rather than through a
** 400 they have to interpret. Nothing is filed on the way out either way -
** the server echoes the sender's own message back, and appending it here too
** would draw it twice.
*/
static void	test_send_refuses_what_the_wire_would_refuse(void)
{
	t_net_client	net;
	char			toolong[BODY_CHAT_TEXT_MAX + 8];

	memset(&net, 0, sizeof(net));
	net.state = NET_IN_ROOM;
	assert(net_chat_send(&net, "D-01", "", NULL) == -1);
	assert(net_chat_send(&net, "D-01", "two\nlines", NULL) == -1);
	assert(net_chat_send(&net, "D-01", "\033[2Jgotcha", NULL) == -1);
	assert(net_chat_send(&net, "", "hello", NULL) == -1);
	assert(net_chat_send(&net, "D-01", NULL, NULL) == -1);
	assert(net_chat_send(NULL, "D-01", "hello", NULL) == -1);
	memset(toolong, 'x', sizeof(toolong) - 1);
	toolong[sizeof(toolong) - 1] = '\0';
	assert(net_chat_send(&net, "D-01", toolong, NULL) == -1);
	net.state = NET_AUTHED;
	assert(net_chat_send(&net, "D-01", "not seated yet", NULL) == -1);
	assert(net_chat_held(&net) == 0);
	printf("PASS test_send_refuses_what_the_wire_would_refuse\n");
}

static void	take_line(t_net_client *net, const char *sender, const char *text)
{
	t_body_chat	line;

	memset(&line, 0, sizeof(line));
	line.seq = net->chat_received + 1;
	line.system = false;
	snprintf(line.sender, sizeof(line.sender), "%s", sender);
	snprintf(line.text, sizeof(line.text), "%s", text);
	net_chat_take(net, &line);
}
