#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_double_with_two(t_room *r);
static bool	probe_down(void *ctx, t_player_id pid);

static void	make_double_with_two(t_room *r)
{
	memset(r, 0, sizeof(*r));
	assert(room_init(r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(r, 22, "bob", NULL, NULL) == 2);
}

// probe that reports the pid pointed to by ctx as disconnected
static bool	probe_down(void *ctx, t_player_id pid)
{
	return (pid != *(t_player_id *)ctx);
}

void	test_release_non_owner_clears_slot_only(void)
{
	t_room				r;
	t_release_result	res;

	make_double_with_two(&r);
	assert(room_release(&r, 22, NULL, NULL, &res) == 0);
	assert(res.released == true);
	assert(res.owner_changed == false);
	assert(r.slots[1].occupied == false);
	assert(r.slots[1].status == SLOT_WAITING);
	assert(r.number_of_players == 1);
	assert(r.slots[0].membership.player_id == 17); // owner untouched
	assert(r.slots[0].membership.role == ROLE_OWNER);
	printf("PASS test_release_non_owner_clears_slot_only\n");
}

void	test_release_owner_promotes_and_moves_successor(void)
{
	t_room				r;
	t_release_result	res;

	make_double_with_two(&r);
	assert(room_release(&r, 17, NULL, NULL, &res) == 0);
	// successor promoted and moved into the vacated owner slot (slot 1)
	assert(r.slots[0].occupied == true);
	assert(r.slots[0].membership.player_id == 22);
	assert(r.slots[0].membership.role == ROLE_OWNER);
	// the successor's old slot is cleared back to WAITING
	assert(r.slots[1].occupied == false);
	assert(r.slots[1].status == SLOT_WAITING);
	assert(r.number_of_players == 1);
	assert(r.status == ROOM_WAITING); // recompute ran (1 < min)
	printf("PASS test_release_owner_promotes_and_moves_successor\n");
}

void	test_release_result_reports_new_owner_after_promotion(void)
{
	t_room				r;
	t_release_result	res;

	make_double_with_two(&r);
	assert(room_release(&r, 17, NULL, NULL, &res) == 0);
	assert(res.released == true);
	assert(res.owner_changed == true);
	assert(res.new_owner == 22);
	assert(strcmp(res.new_owner_name, "bob") == 0);
	// role is already OWNER when the caller reads the result, so a
	// broadcast built from it can never show an ownerless room
	assert(room_find_member(&r, 22) != NULL);
	assert(room_find_member(&r, 22)->role == ROLE_OWNER);
	printf("PASS test_release_result_reports_new_owner_after_promotion\n");
}

void	test_select_successor_skips_disconnected_in_slot_order(void)
{
	t_room			r;
	t_player_id		down;
	t_membership	*m;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_BATTLE_ROYALE, 1, 8) == 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(&r, 22, "bob", NULL, NULL) == 2);
	assert(room_seat(&r, 33, "cara", NULL, NULL) == 3);
	assert(room_seat(&r, 44, "dan", NULL, NULL) == 4);
	down = 22; // first candidate in slot order is disconnected
	m = room_select_successor(&r, probe_down, &down);
	assert(m != NULL);
	assert(m->player_id == 33);
	printf("PASS test_select_successor_skips_disconnected_in_slot_order\n");
}

void	test_select_successor_none_connected_returns_null(void)
{
	t_room		r;
	t_player_id	owner_only;

	make_double_with_two(&r);
	owner_only = 22; // the single candidate is disconnected
	assert(room_select_successor(&r, probe_down, &owner_only) == NULL);
	printf("PASS test_select_successor_none_connected_returns_null\n");
}

void	test_select_successor_mutates_nothing(void)
{
	t_room	r;
	t_room	before;

	make_double_with_two(&r);
	memcpy(&before, &r, sizeof(r));
	(void)room_select_successor(&r, NULL, NULL);
	assert(memcmp(&before, &r, sizeof(r)) == 0);
	printf("PASS test_select_successor_mutates_nothing\n");
}

void	test_release_last_member_reports_room_empty(void)
{
	t_room				r;
	t_release_result	res;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_release(&r, 17, NULL, NULL, &res) == 0);
	assert(res.released == true);
	assert(res.owner_changed == false); // nobody left to promote
	assert(res.room_empty == true);
	assert(r.number_of_players == 0);
	printf("PASS test_release_last_member_reports_room_empty\n");
}

void	test_release_unknown_player_fails_without_mutation(void)
{
	t_room				r;
	t_room				before;
	t_release_result	res;

	make_double_with_two(&r);
	memcpy(&before, &r, sizeof(r));
	assert(room_release(&r, 99, NULL, NULL, &res) == -1);
	assert(res.released == false);
	assert(memcmp(&before, &r, sizeof(r)) == 0);
	printf("PASS test_release_unknown_player_fails_without_mutation\n");
}

void	test_release_below_min_flips_ready_to_waiting(void)
{
	t_room				r;
	t_release_result	res;

	make_double_with_two(&r);
	assert(r.status == ROOM_READY);
	assert(room_release(&r, 22, NULL, NULL, &res) == 0);
	assert(r.status == ROOM_WAITING);
	printf("PASS test_release_below_min_flips_ready_to_waiting\n");
}

int	main(void)
{
	test_release_non_owner_clears_slot_only();
	test_release_owner_promotes_and_moves_successor();
	test_release_result_reports_new_owner_after_promotion();

	test_select_successor_skips_disconnected_in_slot_order();
	test_select_successor_none_connected_returns_null();
	test_select_successor_mutates_nothing();

	test_release_last_member_reports_room_empty();
	test_release_unknown_player_fails_without_mutation();
	test_release_below_min_flips_ready_to_waiting();
	return (0);
}
