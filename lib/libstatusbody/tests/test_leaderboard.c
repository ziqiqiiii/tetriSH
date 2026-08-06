#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_rows(t_sb_lb_row *rows, size_t count);

static void	make_rows(t_sb_lb_row *rows, size_t count)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		memset(&rows[i], 0, sizeof(rows[i]));
		rows[i].rank = (int)(i + 1);
		snprintf(rows[i].username, BODY_USER_MAX, "player%zu", i + 1);
		rows[i].score = 1000 - (i * 50);
		i++;
	}
}

void	test_leaderboard_encode_one_line_per_rank(void)
{
	t_sb_lb_row	rows[3];
	char		out[512];
	int			n;

	make_rows(rows, 3);
	strcpy(rows[0].username, "bob");
	rows[0].score = 1500;
	strcpy(rows[1].username, "alice");
	rows[1].score = 1200;
	strcpy(rows[2].username, "cara");
	rows[2].score = 900;
	n = body_leaderboard_encode(rows, 3, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out,
			"1 bob 1500\n"
			"2 alice 1200\n"
			"3 cara 900\n") == 0);
	printf("PASS test_leaderboard_encode_one_line_per_rank\n");
}

void	test_leaderboard_round_trip_and_empty(void)
{
	t_sb_lb_row	in[10];
	t_sb_lb_row	back[10];
	char		out[1024];
	size_t		count;
	int			n;

	make_rows(in, 10);
	n = body_leaderboard_encode(in, 10, out, sizeof(out));
	assert(n > 0);
	memset(back, 0, sizeof(back));
	assert(body_leaderboard_decode(out, (size_t)n, back, 10, &count) == 0);
	assert(count == 10);
	assert(memcmp(in, back, sizeof(in)) == 0);
	assert(body_leaderboard_encode(NULL, 0, out, sizeof(out)) == 0);
	count = 99;
	assert(body_leaderboard_decode(out, 0, back, 10, &count) == 0);
	assert(count == 0); // UC-21 ext 2a: empty board, not an error
	printf("PASS test_leaderboard_round_trip_and_empty\n");
}

void	test_leaderboard_decode_rejects_bad_row(void)
{
	t_sb_lb_row	back[4];
	t_sb_lb_row	in[3];
	char		out[512];
	size_t		count;
	const char	*bad_rank;
	const char	*bad_score;
	int			n;

	bad_rank = "first bob 100\n";
	assert(body_leaderboard_decode(bad_rank, strlen(bad_rank), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	bad_score = "1 bob many\n";
	assert(body_leaderboard_decode(bad_score, strlen(bad_score), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	make_rows(in, 3);
	n = body_leaderboard_encode(in, 3, out, sizeof(out));
	assert(n > 0);
	assert(body_leaderboard_decode(out, (size_t)n, back, 2, &count) == -1);
	assert(errno == ERANGE); // more rows than the caller's cap
	printf("PASS test_leaderboard_decode_rejects_bad_row\n");
}

int	main(void)
{
	test_leaderboard_encode_one_line_per_rank();
	test_leaderboard_round_trip_and_empty();
	test_leaderboard_decode_rejects_bad_row();
	return (0);
}
