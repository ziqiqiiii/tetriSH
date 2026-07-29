#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_row(t_sb_room_row *row, const char *name, t_sb_mode mode,
				t_sb_room_status status, const char *owner);

static void	make_row(t_sb_room_row *row, const char *name, t_sb_mode mode,
				t_sb_room_status status, const char *owner)
{
	memset(row, 0, sizeof(*row));
	strcpy(row->name, name);
	row->mode = mode;
	row->players = 1;
	row->slot_count = 2;
	row->status = status;
	strcpy(row->owner, owner);
}

void	test_rooms_encode_one_line_per_row(void)
{
	t_sb_room_row	rows[3];
	char			out[1024];
	int				n;

	// the name's prefix always matches the mode field (S / D / BR)
	make_row(&rows[0], "D-01", SB_MODE_DOUBLE, SB_ROOM_WAITING, "alice");
	make_row(&rows[1], "BR-02", SB_MODE_BATTLE_ROYALE, SB_ROOM_READY, "bob");
	rows[1].players = 5;
	rows[1].slot_count = 8;
	make_row(&rows[2], "S-03", SB_MODE_SINGLE, SB_ROOM_WAITING, "cara");
	rows[2].slot_count = 1;
	n = sb_rooms_encode(rows, 3, out, sizeof(out));
	assert(n > 0);
	assert(strcmp(out,
			"D-01 DOUBLE 1/2 WAITING alice\n"
			"BR-02 BATTLE_ROYALE 5/8 READY bob\n"
			"S-03 SINGLE 1/1 WAITING cara\n") == 0);
	printf("PASS test_rooms_encode_one_line_per_row\n");
}

void	test_rooms_round_trip(void)
{
	t_sb_room_row	in[4];
	t_sb_room_row	back[4];
	char			out[1024];
	size_t			count;
	int				n;

	// ids repeat across modes; only the prefixed name is unique
	make_row(&in[0], "S-01", SB_MODE_SINGLE, SB_ROOM_WAITING, "alice");
	make_row(&in[1], "D-01", SB_MODE_DOUBLE, SB_ROOM_READY, "bob");
	make_row(&in[2], "BR-01", SB_MODE_BATTLE_ROYALE, SB_ROOM_IN_GAME, "cara");
	make_row(&in[3], "D-02", SB_MODE_DOUBLE, SB_ROOM_FINISHED, "dan");
	n = sb_rooms_encode(in, 4, out, sizeof(out));
	assert(n > 0);
	memset(back, 0, sizeof(back));
	assert(sb_rooms_decode(out, (size_t)n, back, 4, &count) == 0);
	assert(count == 4);
	assert(memcmp(in, back, sizeof(in)) == 0);
	printf("PASS test_rooms_round_trip\n");
}

void	test_rooms_empty_list_round_trips(void)
{
	t_sb_room_row	back[4];
	char			out[64];
	size_t			count;

	assert(sb_rooms_encode(NULL, 0, out, sizeof(out)) == 0);
	assert(out[0] == '\0');
	count = 99;
	assert(sb_rooms_decode(out, 0, back, 4, &count) == 0);
	assert(count == 0);
	printf("PASS test_rooms_empty_list_round_trips\n");
}

void	test_rooms_decode_rejects_bad_mode_or_status_token(void)
{
	t_sb_room_row	back[4];
	size_t			count;
	const char		*bad_mode;
	const char		*bad_status;

	bad_mode = "D-01 TRIPLE 1/2 WAITING alice\n";
	assert(sb_rooms_decode(bad_mode, strlen(bad_mode), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	bad_status = "D-01 DOUBLE 1/2 LOBBY alice\n";
	assert(sb_rooms_decode(bad_status, strlen(bad_status), back, 4,
			&count) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_rooms_decode_rejects_bad_mode_or_status_token\n");
}

void	test_rooms_decode_more_rows_than_cap_fails(void)
{
	t_sb_room_row	in[3];
	t_sb_room_row	back[2];
	char			out[1024];
	size_t			count;
	int				n;

	make_row(&in[0], "D-01", SB_MODE_DOUBLE, SB_ROOM_WAITING, "alice");
	make_row(&in[1], "D-02", SB_MODE_DOUBLE, SB_ROOM_WAITING, "bob");
	make_row(&in[2], "D-03", SB_MODE_DOUBLE, SB_ROOM_WAITING, "cara");
	n = sb_rooms_encode(in, 3, out, sizeof(out));
	assert(n > 0);
	assert(sb_rooms_decode(out, (size_t)n, back, 2, &count) == -1);
	assert(errno == ERANGE);
	printf("PASS test_rooms_decode_more_rows_than_cap_fails\n");
}

void	test_rooms_decode_rejects_overlong_name_or_owner(void)
{
	t_sb_room_row	back[2];
	size_t			count;
	const char		*long_name;
	const char		*long_owner;

	// 16+ chars, so it cannot fit SB_NAME_MAX with its NUL
	long_name = "D-0123456789ABCDEF DOUBLE 1/2 WAITING alice\n";
	assert(sb_rooms_decode(long_name, strlen(long_name), back, 2,
			&count) == -1);
	assert(errno == EBADMSG);
	long_owner = "D-01 DOUBLE 1/2 WAITING "
		"a-thirty-two-plus-character-owner-name\n";
	assert(sb_rooms_decode(long_owner, strlen(long_owner), back, 2,
			&count) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_rooms_decode_rejects_overlong_name_or_owner\n");
}

int	main(void)
{
	test_rooms_encode_one_line_per_row();
	test_rooms_round_trip();
	test_rooms_empty_list_round_trips();
	test_rooms_decode_rejects_bad_mode_or_status_token();
	test_rooms_decode_more_rows_than_cap_fails();
	test_rooms_decode_rejects_overlong_name_or_owner();
	return (0);
}
