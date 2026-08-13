#include "statusbody.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

// Static Functions
static void	make_health(t_body_health *h);

static void	make_health(t_body_health *h)
{
	memset(h, 0, sizeof(*h));
	h->pid = 4123;
	h->uptime_ms = 8412000;
	h->connections = 7;
	h->rooms = 3;
	h->tick_ms = 12;
	h->sink_reaching = true;
}

void	test_health_encode_matches_the_use_case(void)
{
	t_body_health	h;
	char			out[512];
	int				n;

	make_health(&h);
	n = body_health_encode(&h, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out,
			"pid 4123\n"
			"uptime_ms 8412000\n"
			"connections 7\n"
			"rooms 3\n"
			"tick_ms 12 configured\n"
			"sink reaching\n") == 0);
	assert((size_t)n == strlen(out));
	printf("PASS test_health_encode_matches_the_use_case\n");
}

void	test_health_sink_says_which_way_records_are_going(void)
{
	t_body_health	h;
	t_body_health	back;
	char			out[512];
	int				n;

	make_health(&h);
	h.sink_reaching = false;
	n = body_health_encode(&h, out, sizeof(out));
	assert(n > 0);
	assert(strstr(out, "sink unreachable\n") != NULL);
	assert(strstr(out, "reaching\n") == NULL);
	assert(body_health_decode(out, (size_t)n, &back) == 0);
	assert(back.sink_reaching == false);
	printf("PASS test_health_sink_says_which_way_records_are_going\n");
}

void	test_health_round_trip(void)
{
	t_body_health	h;
	t_body_health	back;
	char			out[512];
	int				n;

	make_health(&h);
	n = body_health_encode(&h, out, sizeof(out));
	assert(n > 0);
	memset(&back, 0xff, sizeof(back));
	assert(body_health_decode(out, (size_t)n, &back) == 0);
	assert(back.pid == 4123);
	assert(back.uptime_ms == 8412000);
	assert(back.connections == 7);
	assert(back.rooms == 3);
	assert(back.tick_ms == 12);
	assert(back.sink_reaching == true);
	printf("PASS test_health_round_trip\n");
}

void	test_health_rejects_bad_input(void)
{
	t_body_health	h;
	t_body_health	back;
	char			out[512];
	const char		*no_label;
	const char		*bad_sink;
	const char		*truncated;

	make_health(&h);
	assert(body_health_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_health_encode(&h, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	h.pid = -1;
	assert(body_health_encode(&h, out, sizeof(out)) == -1);
	assert(errno == EINVAL); // a pid is never negative
	make_health(&h);
	assert(body_health_encode(&h, out, 8) == -1);
	assert(errno == ERANGE);
	/* the `configured` label is part of the line, not decoration */
	no_label = "pid 1\nuptime_ms 2\nconnections 3\nrooms 4\n"
		"tick_ms 12\nsink reaching\n";
	assert(body_health_decode(no_label, strlen(no_label), &back) == -1);
	assert(errno == EBADMSG);
	bad_sink = "pid 1\nuptime_ms 2\nconnections 3\nrooms 4\n"
		"tick_ms 12 configured\nsink maybe\n";
	assert(body_health_decode(bad_sink, strlen(bad_sink), &back) == -1);
	assert(errno == EBADMSG);
	truncated = "pid 1\nuptime_ms 2\n";
	assert(body_health_decode(truncated, strlen(truncated), &back) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_health_rejects_bad_input\n");
}

int	main(void)
{
	test_health_encode_matches_the_use_case();
	test_health_sink_says_which_way_records_are_going();
	test_health_round_trip();
	test_health_rejects_bad_input();
	return (0);
}
