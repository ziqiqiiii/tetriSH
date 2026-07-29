#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

void	test_slot_init_waiting_and_unoccupied(void)
{
	t_slot	s;

	memset(&s, 0xAA, sizeof(s));
	slot_init(&s, 3);
	assert(s.status == SLOT_WAITING);
	assert(s.occupied == false);
	assert(s.index == 3);
	printf("PASS test_slot_init_waiting_and_unoccupied\n");
}

void	test_slot_occupy_from_joining_sets_ready(void)
{
	t_slot			s;
	t_membership	m;

	slot_init(&s, 1);
	s.status = SLOT_JOINING;
	m = membership_make(17, "alice", ROLE_OWNER);
	assert(slot_occupy(&s, m) == 0);
	assert(s.occupied == true);
	assert(s.membership.player_id == 17);
	assert(s.status == SLOT_READY);
	assert(s.index == 1);
	printf("PASS test_slot_occupy_from_joining_sets_ready\n");
}

void	test_slot_occupy_rejected_when_not_joining(void)
{
	t_slot			s;
	t_membership	m;

	m = membership_make(17, "alice", ROLE_OWNER);
	slot_init(&s, 1);
	assert(slot_occupy(&s, m) == -1);
	assert(s.status == SLOT_WAITING);
	assert(s.occupied == false);
	slot_init(&s, 1);
	s.status = SLOT_READY;
	assert(slot_occupy(&s, m) == -1);
	assert(s.status == SLOT_READY);
	assert(s.occupied == false);
	printf("PASS test_slot_occupy_rejected_when_not_joining\n");
}

void	test_slot_clear_removes_occupant_and_sets_waiting(void)
{
	t_slot			s;
	t_membership	m;

	slot_init(&s, 2);
	s.status = SLOT_JOINING;
	m = membership_make(22, "bob", ROLE_PLAYER);
	assert(slot_occupy(&s, m) == 0);
	s.status = SLOT_LEAVING;
	slot_clear(&s);
	assert(s.occupied == false);
	assert(s.status == SLOT_WAITING);
	assert(s.index == 2);
	printf("PASS test_slot_clear_removes_occupant_and_sets_waiting\n");
}

int	main(void)
{
	test_slot_init_waiting_and_unoccupied();
	test_slot_occupy_from_joining_sets_ready();
	test_slot_occupy_rejected_when_not_joining();
	test_slot_clear_removes_occupant_and_sets_waiting();
	return (0);
}
