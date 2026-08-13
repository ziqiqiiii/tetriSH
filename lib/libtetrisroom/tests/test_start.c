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

void	test_abort_start_returns_the_room_to_its_seated_status(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_start(&r, 17) == START_ACCEPTED);
	assert(r.status == ROOM_IN_GAME);
	room_abort_start(&r);
	assert(r.status == ROOM_READY);
	assert(r.number_of_players == 2);
	assert(room_find_member(&r, 17) != NULL);
	assert(room_can_start(&r, 17) == START_ACCEPTED);
	printf("PASS test_abort_start_returns_the_room_to_its_seated_status\n");
}

void	test_abort_start_only_undoes_a_start(void)
{
	t_room	r;

	make_ready_double(&r);
	room_abort_start(&r);
	assert(r.status == ROOM_READY);
	room_finish(&r);
	room_abort_start(&r);
	assert(r.status == ROOM_FINISHED);
	printf("PASS test_abort_start_only_undoes_a_start\n");
}

void	test_rematch_keeps_the_seats_and_withdraws_readiness(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(room_set_ready(&r, 17, true) == 0);
	assert(room_set_ready(&r, 22, true) == 0);
	assert(room_start(&r, 17) == START_ACCEPTED);
	room_rematch(&r);
	assert(r.status == ROOM_READY);
	assert(r.number_of_players == 2);
	assert(r.slots[0].occupied == true);
	assert(r.slots[1].occupied == true);
	assert(r.slots[0].status == SLOT_WAITING);
	assert(r.slots[1].status == SLOT_WAITING);
	assert(membership_is_owner(room_find_member(&r, 17)) == true);
	assert(room_can_start(&r, 17) == START_ACCEPTED);
	printf("PASS test_rematch_keeps_the_seats_and_withdraws_readiness\n");
}

void	test_rematch_only_ends_a_match(void)
{
	t_room	r;

	make_ready_double(&r);
	room_rematch(&r);
	assert(r.status == ROOM_READY);
	room_finish(&r);
	room_rematch(&r);
	assert(r.status == ROOM_WAITING); // finished and emptied stays emptied
	assert(r.number_of_players == 0);
	printf("PASS test_rematch_only_ends_a_match\n");
}

void	test_selection_holds_a_ready_room_open_without_starting_it(void)
{
	t_room	r;

	make_ready_double(&r);
	assert(r.status == ROOM_READY);
	assert(room_begin_selection(&r) == 0);
	assert(r.status == ROOM_SELECTING);
	/* committed but not playing: still startable, closed to newcomers */
	assert(room_can_start(&r, 17) == START_ACCEPTED);
	assert(room_can_accept(&r) == JOIN_IN_GAME);
	assert(room_set_ready(&r, 22, true) == 0);
	assert(strcmp(room_state_message(&r), "CHOOSING FIGHTERS") == 0);
	assert(room_begin_selection(&r) == -1); // not twice
	assert(room_start(&r, 17) == START_ACCEPTED);
	assert(r.status == ROOM_IN_GAME);
	printf("PASS test_selection_holds_a_ready_room_open_without_starting_it\n");
}

void	test_aborting_selection_returns_the_room_to_its_seated_status(void)
{
	t_room				r;
	t_release_result	res;

	make_ready_double(&r);
	assert(room_begin_selection(&r) == 0);
	memset(&res, 0, sizeof(res));
	room_release(&r, 22, NULL, NULL, &res);
	room_abort_selection(&r);
	assert(r.status == ROOM_WAITING); // one player left, below the minimum
	assert(room_can_accept(&r) == JOIN_ACCEPTED);
	room_abort_selection(&r);
	assert(r.status == ROOM_WAITING); // idempotent, and only undoes SELECTING
	printf("PASS test_aborting_selection_returns_the_room_to_its_seated_status\n");
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
	test_abort_start_returns_the_room_to_its_seated_status();
	test_abort_start_only_undoes_a_start();
	test_rematch_keeps_the_seats_and_withdraws_readiness();
	test_rematch_only_ends_a_match();
	test_selection_holds_a_ready_room_open_without_starting_it();
	test_aborting_selection_returns_the_room_to_its_seated_status();
	return (0);
}
