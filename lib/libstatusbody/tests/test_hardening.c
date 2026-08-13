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
	t_body_room		room;
	t_body_chat		chat;
	t_body_room_row	rooms[2];
	t_body_leaderboard_row		lb[2];
	t_body_player_row	players[2];
	t_body_health		health;
	t_body_dropped		dropped;
	size_t			count;

	assert(body_state_decode("", 0, &st) == -1);
	assert(errno == EBADMSG);
	assert(body_profile_decode("", 0, &pr) == -1);
	assert(errno == EBADMSG);
	assert(body_room_decode("", 0, &room) == -1);
	assert(errno == EBADMSG);
	assert(body_chat_decode("", 0, &chat) == -1);
	assert(errno == EBADMSG);
	assert(body_rooms_decode("", 0, rooms, 2, &count) == 0);
	assert(count == 0);
	assert(body_leaderboard_decode("", 0, lb, 2, &count) == 0);
	assert(count == 0);
	assert(body_health_decode("", 0, &health) == -1);
	assert(errno == EBADMSG);
	assert(body_dropped_decode("", 0, &dropped) == -1);
	assert(errno == EBADMSG);
	/* a listing, so like rooms and the leaderboard an empty body is no rows */
	assert(body_players_decode("", 0, players, 2, &count) == 0);
	assert(count == 0);
	printf("PASS test_all_decoders_reject_empty_buffer\n");
}

void	test_all_codecs_reject_null_arguments(void)
{
	t_body_state		st;
	t_body_profile	pr;
	t_body_room		room;
	t_body_chat		chat;
	t_body_room_row	rooms[2];
	t_body_leaderboard_row		lb[2];
	t_body_player_row	players[2];
	t_body_health		health;
	t_body_dropped		dropped;
	size_t			count;
	char			out[256];

	memset(&st, 0, sizeof(st));
	memset(&pr, 0, sizeof(pr));
	memset(&room, 0, sizeof(room));
	memset(&chat, 0, sizeof(chat));
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
	assert(body_room_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_room_encode(&room, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_room_decode(NULL, 1, &room) == -1);
	assert(errno == EINVAL);
	assert(body_room_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_chat_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_chat_encode(&chat, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_chat_decode(NULL, 1, &chat) == -1);
	assert(errno == EINVAL);
	assert(body_chat_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_leaderboard_encode(lb, 2, NULL, 64) == -1);
	assert(errno == EINVAL);
	assert(body_leaderboard_decode("x", 1, NULL, 2, &count) == -1);
	assert(errno == EINVAL);
	assert(body_health_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_health_encode(&health, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_health_decode(NULL, 1, &health) == -1);
	assert(errno == EINVAL);
	assert(body_health_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_players_encode(players, 2, NULL, 64) == -1);
	assert(errno == EINVAL);
	assert(body_players_decode("x", 1, NULL, 2, &count) == -1);
	assert(errno == EINVAL);
	assert(body_players_decode("x", 1, players, 2, NULL) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_encode(NULL, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_encode(&dropped, NULL, sizeof(out)) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_decode(NULL, 1, &dropped) == -1);
	assert(errno == EINVAL);
	assert(body_dropped_decode("x", 1, NULL) == -1);
	assert(errno == EINVAL);
	printf("PASS test_all_codecs_reject_null_arguments\n");
}

void	test_worst_case_encodes_fit_frame_cap(void)
{
	static t_body_state		st;
	static t_body_profile		pr;
	static t_body_room		room;
	static t_body_room_row	rooms[99];
	static t_body_leaderboard_row		lb[10];
	static t_body_player_row	players[BODY_PLAYERS_MAX];
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
	memset(&room, 0, sizeof(room));
	snprintf(room.name, sizeof(room.name), "BR-99");
	room.mode = BODY_MODE_BATTLE_ROYALE;
	room.status = BODY_ROOM_READY;
	room.min_to_start = 4;
	room.slot_count = BODY_ROOM_MEMBERS_MAX;
	room.member_count = BODY_ROOM_MEMBERS_MAX;
	i = 0;
	while (i < BODY_ROOM_MEMBERS_MAX)
	{
		room.members[i].slot = i + 1;
		room.members[i].player_id = UINT64_MAX - (uint64_t)i;
		room.members[i].owner = i == 0;
		room.members[i].ready = true;
		memset(room.members[i].username, 'u', BODY_USER_MAX - 1);
		i++;
	}
	n = body_room_encode(&room, g_out, sizeof(g_out));
	assert(n > 0 && n < 8192);
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
	i = 0;
	while (i < BODY_PLAYERS_MAX)
	{
		memset(&players[i], 0, sizeof(players[i]));
		players[i].connection = UINT64_MAX;
		players[i].authenticated = true;
		memset(players[i].username, 'u', BODY_USER_MAX - 1);
		memset(players[i].room, 'r', BODY_NAME_MAX - 1);
		i++;
	}
	n = body_players_encode(players, BODY_PLAYERS_MAX, g_out, sizeof(g_out));
	/*
	** 32768 is TETRISD_CONTROL_BODY_MAX, which this library must not name.
	** The bound is what makes BODY_PLAYERS_MAX's stated reasoning checkable
	** rather than asserted: a full listing at the widest possible row still
	** fits one control body.
	*/
	assert(n > 0 && n < 32768);
	printf("PASS test_worst_case_encodes_fit_frame_cap\n");
}

void	test_decode_is_bounded_on_hostile_input(void)
{
	t_body_state		st;
	t_body_profile	pr;
	t_body_room		room;
	t_body_chat		chat;
	t_body_room_row	rooms[4];
	t_body_leaderboard_row		lb[4];
	t_body_player_row	players[4];
	t_body_health		health;
	t_body_dropped		dropped;
	size_t			count;

	memset(g_big, 'A', sizeof(g_big)); // 64 KiB+, no newline, no NUL
	assert(body_state_decode(g_big, 65536, &st) == -1);
	assert(body_profile_decode(g_big, 65536, &pr) == -1);
	assert(body_room_decode(g_big, 65536, &room) == -1);
	assert(body_chat_decode(g_big, 65536, &chat) == -1);
	assert(body_rooms_decode(g_big, 65536, rooms, 4, &count) == -1);
	assert(body_leaderboard_decode(g_big, 65536, lb, 4, &count) == -1);
	assert(body_health_decode(g_big, 65536, &health) == -1);
	assert(body_dropped_decode(g_big, 65536, &dropped) == -1);
	assert(body_players_decode(g_big, 65536, players, 4, &count) == -1);
	memset(g_big, 0, 64); // a line of NULs
	assert(body_state_decode(g_big, 64, &st) == -1);
	assert(body_profile_decode(g_big, 64, &pr) == -1);
	assert(body_chat_decode(g_big, 64, &chat) == -1);
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
