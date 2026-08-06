#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

// Static Variables
static char	g_big[70000];
static char	g_out[70000];

void	test_all_decoders_reject_empty_buffer(void)
{
	t_body_state		st;
	t_body_profile	pr;
	t_body_room_row	rooms[2];
	t_body_leaderboard_row		lb[2];
	size_t			count;

	assert(body_state_decode("", 0, &st) == -1);
	assert(errno == EBADMSG);
	assert(body_profile_decode("", 0, &pr) == -1);
	assert(errno == EBADMSG);
	assert(body_rooms_decode("", 0, rooms, 2, &count) == 0);
	assert(count == 0);
	assert(body_leaderboard_decode("", 0, lb, 2, &count) == 0);
	assert(count == 0);
	printf("PASS test_all_decoders_reject_empty_buffer\n");
}

void	test_all_codecs_reject_null_arguments(void)
{
	t_body_state		st;
	t_body_profile	pr;
	t_body_room_row	rooms[2];
	t_body_leaderboard_row		lb[2];
	size_t			count;
	char			out[256];

	memset(&st, 0, sizeof(st));
	memset(&pr, 0, sizeof(pr));
	assert(body_state_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_state_encode(&st, NULL, 64) == -1);
	assert(errno == EINVAL);
	assert(body_state_decode(NULL, 4, &st) == -1);
	assert(errno == EINVAL);
	assert(body_state_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_rooms_encode(rooms, 2, NULL, 64) == -1);
	assert(errno == EINVAL);
	assert(body_rooms_decode("x", 1, NULL, 2, &count) == -1);
	assert(errno == EINVAL);
	assert(body_rooms_decode("x", 1, rooms, 2, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_profile_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_profile_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_leaderboard_encode(lb, 2, NULL, 64) == -1);
	assert(errno == EINVAL);
	assert(body_leaderboard_decode("x", 1, NULL, 2, &count) == -1);
	assert(errno == EINVAL);
	printf("PASS test_all_codecs_reject_null_arguments\n");
}

void	test_worst_case_encodes_fit_frame_cap(void)
{
	static t_body_state		st;
	static t_body_profile		pr;
	static t_body_room_row	rooms[99];
	static t_body_leaderboard_row		lb[10];
	int						row;
	int						col;
	int						i;
	int						n;

	memset(&st, 0, sizeof(st));
	st.seq = UINT64_MAX;
	st.score = UINT64_MAX;
	st.charge = BODY_CHARGE_MAX;
	st.clearing_count = BODY_CLEARING_MAX;
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			st.cells[row][col].type = 2;
			st.cells[row][col].color = BODY_COLOR_MAX;
			col++;
		}
		row++;
	}
	n = body_state_encode(&st, g_out, sizeof(g_out));
	assert(n > 0 && n < 65536);
	i = 0;
	while (i < 99)
	{
		memset(&rooms[i], 0, sizeof(rooms[i]));
		snprintf(rooms[i].name, BODY_NAME_MAX, "BR-%02d", i + 1);
		rooms[i].mode = BODY_MODE_BATTLE_ROYALE;
		rooms[i].players = 99;
		rooms[i].slot_count = 99;
		rooms[i].status = BODY_ROOM_WAITING;
		memset(rooms[i].owner, 'o', BODY_USER_MAX - 1);
		i++;
	}
	n = body_rooms_encode(rooms, 99, g_out, sizeof(g_out));
	assert(n > 0 && n < 65536);
	memset(&pr, 0, sizeof(pr));
	memset(pr.username, 'u', BODY_USER_MAX - 1);
	pr.wallet = UINT64_MAX;
	pr.score = UINT64_MAX;
	pr.owned_character_count = BODY_OWNED_MAX;
	pr.owned_theme_count = BODY_OWNED_MAX;
	i = 0;
	while (i < BODY_OWNED_MAX)
	{
		pr.owned_characters[i] = UINT32_MAX;
		pr.owned_themes[i] = UINT32_MAX;
		i++;
	}
	n = body_profile_encode(&pr, g_out, sizeof(g_out));
	assert(n > 0 && n < 65536);
	i = 0;
	while (i < 10)
	{
		memset(&lb[i], 0, sizeof(lb[i]));
		lb[i].rank = i + 1;
		memset(lb[i].username, 'p', BODY_USER_MAX - 1);
		lb[i].score = UINT64_MAX;
		i++;
	}
	n = body_leaderboard_encode(lb, 10, g_out, sizeof(g_out));
	assert(n > 0 && n < 65536);
	printf("PASS test_worst_case_encodes_fit_frame_cap\n");
}

void	test_decode_is_bounded_on_hostile_input(void)
{
	t_body_state		st;
	t_body_profile	pr;
	t_body_room_row	rooms[4];
	t_body_leaderboard_row		lb[4];
	size_t			count;

	memset(g_big, 'A', sizeof(g_big)); // 64 KiB+, no newline, no NUL
	assert(body_state_decode(g_big, 65536, &st) == -1);
	assert(body_profile_decode(g_big, 65536, &pr) == -1);
	assert(body_rooms_decode(g_big, 65536, rooms, 4, &count) == -1);
	assert(body_leaderboard_decode(g_big, 65536, lb, 4, &count) == -1);
	memset(g_big, 0, 64); // a line of NULs
	assert(body_state_decode(g_big, 64, &st) == -1);
	assert(body_profile_decode(g_big, 64, &pr) == -1);
	strcpy(g_big, "seq 42"); // valid-ish start, no trailing newline
	assert(body_state_decode(g_big, 6, &st) == -1);
	printf("PASS test_decode_is_bounded_on_hostile_input\n");
}

int	main(void)
{
	test_all_decoders_reject_empty_buffer();
	test_all_codecs_reject_null_arguments();
	test_worst_case_encodes_fit_frame_cap();
	test_decode_is_bounded_on_hostile_input();
	return (0);
}
