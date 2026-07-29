#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

// Static Functions
static void	make_ready_double(t_room *r);

static void	make_ready_double(t_room *r)
{
	memset(r, 0, sizeof(*r));
	assert(room_init(r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(r, 17, "alice", NULL, NULL) == 1);
	assert(room_seat(r, 22, "bob", NULL, NULL) == 2);
}

void	test_can_start_owner_with_min_players_accepted(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_can_start(&r, 17) == START_ACCEPTED);
	printf("PASS test_can_start_owner_with_min_players_accepted\n");
}

void	test_can_start_non_owner_rejected(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_can_start(&r, 22) == START_NOT_OWNER);
	printf("PASS test_can_start_non_owner_rejected\n");
}

void	test_can_start_too_few_players_rejected(void)
{
	t_room	r;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_DOUBLE, 1, 0) == 0);
	assert(room_seat(&r, 17, "alice", NULL, NULL) == 1);
	assert(room_can_start(&r, 17) == START_TOO_FEW_PLAYERS);
	printf("PASS test_can_start_too_few_players_rejected\n");
}

void	test_can_start_already_started_rejected(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_start(&r, 17) == START_ACCEPTED);
	assert(room_can_start(&r, 17) == START_ALREADY_STARTED);
	printf("PASS test_can_start_already_started_rejected\n");
}

void	test_room_start_flips_to_in_game_only_on_accept(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_start(&r, 22) == START_NOT_OWNER);
	assert(r.status == ROOM_READY); // rejection leaves status alone
	assert(room_start(&r, 17) == START_ACCEPTED);
	assert(r.status == ROOM_IN_GAME);
	printf("PASS test_room_start_flips_to_in_game_only_on_accept\n");
}

void	test_room_finish_clears_slots_and_sets_finished(void)
{
	t_room	r;
	int		i;

	make_ready_double(&r);
	assert(room_start(&r, 17) == START_ACCEPTED);
	room_finish(&r);
	assert(r.status == ROOM_FINISHED);
	assert(r.number_of_players == 0);
	i = 0;
	while (i < r.slot_count)
	{
		assert(r.slots[i].occupied == false);
		assert(r.slots[i].status == SLOT_WAITING);
		i++;
	}
	printf("PASS test_room_finish_clears_slots_and_sets_finished\n");
}

void	test_state_message_four_fixed_strings(void)
{
	t_room		r;
	const char	*msg;

	memset(&r, 0, sizeof(r));
	assert(room_init(&r, MODE_DOUBLE, 1, 0) == 0);
	msg = room_state_message(&r);
	assert(msg != NULL);
	assert(strcmp(msg, "WAITING FOR OPPONENT") == 0);
	r.status = ROOM_READY;
	msg = room_state_message(&r);
	assert(msg != NULL);
	assert(strcmp(msg, "READY TO START, OWNER CAN START ANYTIME") == 0);
	r.status = ROOM_IN_GAME;
	msg = room_state_message(&r);
	assert(msg != NULL);
	assert(strcmp(msg, "GAME IN PROGRESS") == 0);
	r.status = ROOM_FINISHED;
	msg = room_state_message(&r);
	assert(msg != NULL);
	assert(strcmp(msg, "GAME OVER, RECORDING THE RESULTS") == 0);
	printf("PASS test_state_message_four_fixed_strings\n");
}

void	test_find_member_returns_seated_identity_or_null(void)
{
	t_room			r;
	t_membership	*m;

	make_ready_double(&r);
	m = room_find_member(&r, 17);
	assert(m != NULL);
	assert(m == &r.slots[0].membership); // pointer identity, not a copy
	assert(m->player_id == 17);
	assert(room_find_member(&r, 99) == NULL);
	printf("PASS test_find_member_returns_seated_identity_or_null\n");
}

int	main(void)
{
	test_can_start_owner_with_min_players_accepted();
	test_can_start_non_owner_rejected();
	test_can_start_too_few_players_rejected();
	test_can_start_already_started_rejected();
	test_room_start_flips_to_in_game_only_on_accept();
	test_room_finish_clears_slots_and_sets_finished();
	test_state_message_four_fixed_strings();
	test_find_member_returns_seated_identity_or_null();
	return (0);
}
