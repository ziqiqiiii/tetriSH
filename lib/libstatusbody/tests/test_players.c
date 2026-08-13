#include "statusbody.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

// Static Functions
static void	set_row(t_body_player_row *row, uint64_t conn, const char *user, const char *room);
static void	set_row(t_body_player_row *row, uint64_t conn, const char *user, const char *room)
{
	memset(row, 0, sizeof(*row));
	row->connection = conn;
	row->authenticated = (user != NULL);
	if (user != NULL)
		snprintf(row->username, BODY_USER_MAX, "%s", user);
	if (room != NULL)
		snprintf(row->room, BODY_NAME_MAX, "%s", room);
}

void	test_players_encode_matches_the_use_case(void)
{
	t_body_player_row	rows[3];
	char				out[512];
	int					n;

	set_row(&rows[0], 17, "alice", "main");
	set_row(&rows[1], 18, "bob", "main");
	set_row(&rows[2], 19, NULL, NULL);
	n = body_players_encode(rows, 3, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out,
			"17 alice main\n"
			"18 bob main\n"
			"19 (anonymous) -\n") == 0);
	printf("PASS test_players_encode_matches_the_use_case\n");
}

void	test_players_empty_listing_is_a_valid_body(void)
{
	t_body_player_row	back[4];
	char				out[64];
	size_t				count;
	int					n;

	n = body_players_encode(NULL, 0, out, sizeof(out));
	assert(n == 0); // UC-26 ext 2a: nobody connected is an answer, not an error
	assert(body_players_decode("", 0, back, 4, &count) == 0);
	assert(count == 0);
	printf("PASS test_players_empty_listing_is_a_valid_body\n");
}

void	test_players_round_trip_keeps_anonymous_apart(void)
{
	t_body_player_row	rows[2];
	t_body_player_row	back[2];
	char				out[512];
	size_t				count;
	int					n;

	set_row(&rows[0], 17, "alice", "BR-10");
	set_row(&rows[1], 19, NULL, NULL);
	n = body_players_encode(rows, 2, out, sizeof(out));
	assert(n > 0);
	assert(body_players_decode(out, (size_t)n, back, 2, &count) == 0);
	assert(count == 2);
	assert(back[0].connection == 17);
	assert(back[0].authenticated == true);
	assert(strcmp(back[0].username, "alice") == 0);
	assert(strcmp(back[0].room, "BR-10") == 0);
	assert(back[1].connection == 19);
	assert(back[1].authenticated == false);
	assert(back[1].username[0] == '\0'); // the placeholder is not a name
	assert(back[1].room[0] == '\0');
	printf("PASS test_players_round_trip_keeps_anonymous_apart\n");
}

void	test_players_rejects_bad_input(void)
{
	t_body_player_row	rows[2];
	t_body_player_row	back[4];
	char				out[512];
	size_t				count;
	const char			*short_row;
	const char			*bad_conn;
	int					n;

	set_row(&rows[0], 1, "alice", "main");
	assert(body_players_encode(rows, 1, NULL, 16) == -1);
	assert(errno == EINVAL);
	assert(body_players_encode(NULL, 1, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	rows[1] = rows[0];
	rows[1].username[0] = '\0'; // authenticated but nameless is a caller bug
	assert(body_players_encode(&rows[1], 1, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_players_encode(rows, 1, out, 4) == -1);
	assert(errno == ERANGE);
	short_row = "17 alice\n";
	assert(body_players_decode(short_row, strlen(short_row), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	bad_conn = "seventeen alice main\n";
	assert(body_players_decode(bad_conn, strlen(bad_conn), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	set_row(&rows[0], 1, "alice", "main");
	set_row(&rows[1], 2, "bob", "main");
	n = body_players_encode(rows, 2, out, sizeof(out));
	assert(body_players_decode(out, (size_t)n, back, 1, &count) == -1);
	assert(errno == ERANGE); // more rows than the caller's cap
	printf("PASS test_players_rejects_bad_input\n");
}

int	main(void)
{
	test_players_encode_matches_the_use_case();
	test_players_empty_listing_is_a_valid_body();
	test_players_round_trip_keeps_anonymous_apart();
	test_players_rejects_bad_input();
	return (0);
}
