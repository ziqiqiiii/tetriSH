#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_room(t_room *r, t_game_mode mode, int br_slots);
static bool	probe_down(void *ctx, t_player_id pid);
static void	seat_n(t_room *r, int n);

static void	make_room(t_room *r, t_game_mode mode, int br_slots)
{
	memset(r, 0, sizeof(*r));
	assert(room_init(r, mode, 1, br_slots) == 0);
}

// probe that reports the pid pointed to by ctx as disconnected
static bool	probe_down(void *ctx, t_player_id pid)
{
	return (pid != *(t_player_id *)ctx);
}

static void	seat_n(t_room *r, int n)
{
	char	name[ROOM_NAME_MAX];
	int		i;

	i = 0;
	while (i < n)
	{
		snprintf(name, sizeof(name), "p%d", i + 1);
		assert(room_seat(r, (t_player_id)(100 + i), name, NULL, NULL) > 0);
		i++;
	}
}

void	test_room_init_double_two_slots_min_two(void)
{
	t_room	r;
	int		i;

	make_room(&r, MODE_DOUBLE, 0);
	assert(r.id == 1);
	assert(strcmp(r.name, "D-01") == 0);
	assert(r.slot_count == 2);
	assert(r.min_to_start == 2);
	assert(r.number_of_players == 0);
	assert(r.status == ROOM_WAITING);
	i = 0;
	while (i < r.slot_count)
	{
		assert(r.slots[i].status == SLOT_WAITING);
		assert(r.slots[i].occupied == false);
		i++;
	}
	printf("PASS test_room_init_double_two_slots_min_two\n");
}

void	test_room_init_battle_royale_clamps_slot_count(void)
{
	t_room	r;

	make_room(&r, MODE_BATTLE_ROYALE, 2);
	assert(r.slot_count == 4);
	assert(r.min_to_start == 4);
	make_room(&r, MODE_BATTLE_ROYALE, 50);
	assert(r.slot_count == 50);
	make_room(&r, MODE_BATTLE_ROYALE, 200);
	assert(r.slot_count == 99);
	printf("PASS test_room_init_battle_royale_clamps_slot_count\n");
}

void	test_room_init_single_one_slot_min_one(void)
{
	t_room	r;

	make_room(&r, MODE_SINGLE, 0);
	assert(r.slot_count == 1);
	assert(r.min_to_start == 1);
	printf("PASS test_room_init_single_one_slot_min_one\n");
}

void	test_room_init_derives_name_from_mode_and_id(void)
{
	t_room	r;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_SINGLE, 1, 0) == 0);
	assert(r.id == 1);
	assert(strcmp(r.name, "S-01") == 0);
	assert(room_init(&r, MODE_DOUBLE, 2, 0) == 0);
	assert(strcmp(r.name, "D-02") == 0);
	assert(room_init(&r, MODE_BATTLE_ROYALE, 10, 8) == 0);
	assert(strcmp(r.name, "BR-10") == 0);
	// ids past 99 widen the name instead of truncating
	assert(room_init(&r, MODE_DOUBLE, 123, 0) == 0);
	assert(r.id == 123);
	assert(strcmp(r.name, "D-123") == 0);
	printf("PASS test_room_init_derives_name_from_mode_and_id\n");
}

void	test_room_init_rejects_bad_arguments(void)
{
	t_room	r;

	memset(&r, 0, sizeof(r));
	assert(room_init(NULL, MODE_DOUBLE, 1, 0) == -1);
	assert(room_init(&r, (t_game_mode)99, 1, 0) == -1);
	assert(room_init(&r, MODE_DOUBLE, 0, 0) == -1); // ids are 1-based
	assert(room_init(&r, MODE_DOUBLE, -5, 0) == -1);
	printf("PASS test_room_init_rejects_bad_arguments\n");
}

void	test_can_accept_waiting_with_free_slot_accepted(void)
{
	t_room	r;

	make_room(&r, MODE_DOUBLE, 0);
	seat_n(&r, 1);
	assert(room_can_accept(&r) == JOIN_ACCEPTED);
	assert(r.number_of_players == 1);
	printf("PASS test_can_accept_waiting_with_free_slot_accepted\n");
}

void	test_can_accept_full_room_returns_full_verdict(void)
{
	t_room	r;

	make_room(&r, MODE_DOUBLE, 0);
	seat_n(&r, 2);
	assert(room_can_accept(&r) == JOIN_FULL);
	assert(r.number_of_players == 2);
	assert(r.status == ROOM_READY);
	printf("PASS test_can_accept_full_room_returns_full_verdict\n");
}

void	test_can_accept_in_game_returns_in_game_verdict(void)
{
	t_room	r;

	make_room(&r, MODE_BATTLE_ROYALE, 8);
	seat_n(&r, 4);
	assert(room_start(&r, 100) == START_ACCEPTED);
	// a free slot exists, proving status is checked before occupancy
	assert(r.number_of_players < r.slot_count);
	assert(room_can_accept(&r) == JOIN_IN_GAME);
	assert(r.number_of_players == 4);
	printf("PASS test_can_accept_in_game_returns_in_game_verdict\n");
}

void	test_can_accept_ready_battle_royale_keeps_accepting(void)
{
	t_room	r;

	make_room(&r, MODE_BATTLE_ROYALE, 8);
	seat_n(&r, 5);
	assert(r.status == ROOM_READY);
	assert(room_can_accept(&r) == JOIN_ACCEPTED);
	printf("PASS test_can_accept_ready_battle_royale_keeps_accepting\n");
}

void	test_seat_first_player_creates_owner_in_slot_one(void)
{
	t_room	r;

	make_room(&r, MODE_DOUBLE, 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(r.slots[0].occupied == true);
	assert(r.slots[0].status == SLOT_READY);
	assert(r.slots[0].membership.player_id == 17);
	assert(r.slots[0].membership.role == ROLE_OWNER);
	assert(strcmp(r.slots[0].membership.username, "alice") == 0);
	assert(r.number_of_players == 1);
	assert(r.status == ROOM_WAITING); // 1 < min_to_start
	assert(r.slots[1].status == SLOT_WAITING);
	assert(r.slots[1].occupied == false);
	printf("PASS test_seat_first_player_creates_owner_in_slot_one\n");
}

void	test_seat_second_player_first_free_slot_flips_ready(void)
{
	t_room	r;

	make_room(&r, MODE_DOUBLE, 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(&r, 22, "bob", NULL, NULL) == 2);
	assert(r.slots[1].membership.player_id == 22);
	assert(r.slots[1].membership.role == ROLE_PLAYER);
	assert(r.number_of_players == 2);
	assert(r.status == ROOM_READY); // crossed min_to_start
	assert(r.slots[0].membership.player_id == 17); // owner untouched
	assert(r.slots[0].membership.role == ROLE_OWNER);
	printf("PASS test_seat_second_player_first_free_slot_flips_ready\n");
}

void	test_seat_disconnect_probe_resets_slot_to_waiting(void)
{
	t_room		r;
	t_player_id	down;

	make_room(&r, MODE_DOUBLE, 0);
	down = 17;
	assert(room_seat(&r, 17, "alice", probe_down, &down) == -1);
	assert(r.slots[0].status == SLOT_WAITING);
	assert(r.slots[0].occupied == false);
	assert(r.number_of_players == 0);
	printf("PASS test_seat_disconnect_probe_resets_slot_to_waiting\n");
}

void	test_seat_full_room_fails_without_mutation(void)
{
	t_room	r;
	t_room	before;

	make_room(&r, MODE_DOUBLE, 0);
	seat_n(&r, 2);
	memcpy(&before, &r, sizeof(r));
	assert(room_seat(&r, 33, "cara", NULL, NULL) == -1);
	assert(memcmp(&before, &r, sizeof(r)) == 0);
	printf("PASS test_seat_full_room_fails_without_mutation\n");
}

void	test_seat_same_player_twice_rejected(void)
{
	t_room	r;

	make_room(&r, MODE_DOUBLE, 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == -1);
	assert(r.number_of_players == 1);
	assert(r.slots[1].occupied == false);
	printf("PASS test_seat_same_player_twice_rejected\n");
}

void	test_recompute_only_crossing_min_flips_status(void)
{
	t_room				r;
	t_release_result	res;

	make_room(&r, MODE_BATTLE_ROYALE, 8);
	seat_n(&r, 5);
	assert(r.status == ROOM_READY);
	assert(room_release(&r, 100, NULL, NULL, &res) == 0);
	assert(room_release(&r, 101, NULL, NULL, &res) == 0);
	assert(r.status == ROOM_WAITING); // 3 < min_to_start
	assert(room_seat(&r, 200, "eve", NULL, NULL) > 0);
	assert(r.status == ROOM_READY); // back to 4
	printf("PASS test_recompute_only_crossing_min_flips_status\n");
}

void	test_recompute_never_touches_in_game_or_finished(void)
{
	t_room				r;
	t_release_result	res;

	make_room(&r, MODE_BATTLE_ROYALE, 8);
	seat_n(&r, 4);
	assert(room_start(&r, 100) == START_ACCEPTED);
	assert(room_release(&r, 101, NULL, NULL, &res) == 0);
	assert(r.status == ROOM_IN_GAME); // below min, but mid-game
	room_finish(&r);
	room_recompute_status(&r);
	assert(r.status == ROOM_FINISHED);
	printf("PASS test_recompute_never_touches_in_game_or_finished\n");
}

int	main(void)
{
	test_room_init_double_two_slots_min_two();
	test_room_init_battle_royale_clamps_slot_count();
	test_room_init_single_one_slot_min_one();
	test_room_init_derives_name_from_mode_and_id();
	test_room_init_rejects_bad_arguments();

	test_can_accept_waiting_with_free_slot_accepted();
	test_can_accept_full_room_returns_full_verdict();
	test_can_accept_in_game_returns_in_game_verdict();
	test_can_accept_ready_battle_royale_keeps_accepting();

	test_seat_first_player_creates_owner_in_slot_one();
	test_seat_second_player_first_free_slot_flips_ready();
	test_seat_disconnect_probe_resets_slot_to_waiting();
	test_seat_full_room_fails_without_mutation();
	test_seat_same_player_twice_rejected();

	test_recompute_only_crossing_min_flips_status();
	test_recompute_never_touches_in_game_or_finished();
	return (0);
}
