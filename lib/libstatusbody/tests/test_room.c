/* Detailed waiting-room snapshot codec tests. */
#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

static void	seed_room(t_body_room *room);

void	test_room_round_trip_preserves_ordered_seats(void)
{
	t_body_room	in;
	t_body_room	out;
	char		body[4096];
	int			len;

	seed_room(&in);
	len = body_room_encode(&in, body, sizeof(body));
	assert(len > 0);
	assert(body_room_decode(body, (size_t)len, &out) == 0);
	assert(strcmp(out.name, "BR-12") == 0);
	assert(out.mode == BODY_MODE_BATTLE_ROYALE);
	assert(out.status == BODY_ROOM_READY);
	assert(out.min_to_start == 4 && out.slot_count == 8);
	assert(out.member_count == 2);
	assert(out.members[0].slot == 1 && out.members[0].owner);
	assert(strcmp(out.members[0].username, "amber") == 0);
	assert(out.members[1].slot == 4 && !out.members[1].owner);
	assert(out.members[1].ready);
	printf("PASS test_room_round_trip_preserves_ordered_seats\n");
}

void	test_room_empty_roster_round_trips(void)
{
	t_body_room	in;
	t_body_room	out;
	char		body[512];
	int			len;

	seed_room(&in);
	in.member_count = 0;
	len = body_room_encode(&in, body, sizeof(body));
	assert(len > 0);
	assert(body_room_decode(body, (size_t)len, &out) == 0);
	assert(out.member_count == 0);
	printf("PASS test_room_empty_roster_round_trips\n");
}

void	test_room_codec_rejects_invalid_snapshots(void)
{
	t_body_room	room;
	char		body[512];

	seed_room(&room);
	room.members[1].slot = 1;
	assert(body_room_encode(&room, body, sizeof(body)) == -1);
	seed_room(&room);
	room.min_to_start = room.slot_count + 1;
	assert(body_room_encode(&room, body, sizeof(body)) == -1);
	assert(body_room_encode(NULL, body, sizeof(body)) == -1);
	printf("PASS test_room_codec_rejects_invalid_snapshots\n");
}

void	test_room_decode_rejects_malformed_rows(void)
{
	static const char	bad_order[] =
		"room D-01\nmode double\nstatus ready\nrequired 2\ncapacity 2\n"
		"members 2\nslot 2 1 owner ready amber\n"
		"slot 1 2 player ready blake\n";
	static const char	trailing[] =
		"room D-01\nmode double\nstatus ready\nrequired 2\ncapacity 2\n"
		"members 1\nslot 1 1 owner ready amber junk\n";
	t_body_room			room;

	assert(body_room_decode(bad_order, sizeof(bad_order) - 1, &room) == -1);
	assert(body_room_decode(trailing, sizeof(trailing) - 1, &room) == -1);
	assert(body_room_decode(NULL, 0, &room) == -1);
	printf("PASS test_room_decode_rejects_malformed_rows\n");
}

int	main(void)
{
	test_room_round_trip_preserves_ordered_seats();
	test_room_empty_roster_round_trips();
	test_room_codec_rejects_invalid_snapshots();
	test_room_decode_rejects_malformed_rows();
	return (0);
}

static void	seed_room(t_body_room *room)
{
	memset(room, 0, sizeof(*room));
	snprintf(room->name, sizeof(room->name), "BR-12");
	room->mode = BODY_MODE_BATTLE_ROYALE;
	room->status = BODY_ROOM_READY;
	room->min_to_start = 4;
	room->slot_count = 8;
	room->member_count = 2;
	room->members[0].slot = 1;
	room->members[0].player_id = 17;
	room->members[0].owner = true;
	room->members[0].ready = true;
	snprintf(room->members[0].username,
		sizeof(room->members[0].username), "amber");
	room->members[1].slot = 4;
	room->members[1].player_id = 22;
	room->members[1].ready = true;
	snprintf(room->members[1].username,
		sizeof(room->members[1].username), "blake");
}
