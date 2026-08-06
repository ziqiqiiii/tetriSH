#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

void	test_to_summary_projects_current_room_state(void)
{
	t_room			r;
	t_room			before;
	t_room_summary	s;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	memcpy(&before, &r, sizeof(r));
	room_to_summary(&r, &s);
	assert(s.id == 1);
	assert(strcmp(s.name, "D-01") == 0);
	assert(s.mode == MODE_DOUBLE);
	assert(s.players == 1);
	assert(s.slot_count == 2);
	assert(s.status == ROOM_WAITING);
	assert(strcmp(s.owner_name, "alice") == 0);
	assert(memcmp(&before, &r, sizeof(r)) == 0); // projection never mutates
	printf("PASS test_to_summary_projects_current_room_state\n");
}

void	test_to_snapshot_lists_member_player_ids(void)
{
	t_room			r;
	t_room_snapshot	snap;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(&r, 22, "bob", NULL, NULL) == 2);
	room_to_snapshot(&r, &snap);
	assert(snap.member_count == 2);
	assert(snap.member_pids[0] == 17);
	assert(snap.member_pids[1] == 22);
	assert(strcmp(snap.summary.name, "D-01") == 0);
	assert(snap.summary.status == ROOM_READY);
	assert(snap.summary.players == 2);
	printf("PASS test_to_snapshot_lists_member_player_ids\n");
}

int	main(void)
{
	test_to_summary_projects_current_room_state();
	test_to_snapshot_lists_member_player_ids();
	return (0);
}
