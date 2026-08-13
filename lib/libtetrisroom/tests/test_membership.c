#include "tetrisroom.h"

#include <assert.h>
#include <stdio.h>

void	test_membership_make_owner_preserves_identity_and_role(void)
{
	t_membership	m;

	m = membership_make(17, "alice", ROLE_OWNER);
	assert(m.player_id == 17);
	assert(strcmp(m.username, "alice") == 0);
	assert(m.role == ROLE_OWNER);
	assert(membership_is_owner(&m));
	assert(m.muted == false);
	printf("PASS test_membership_make_owner_preserves_identity_and_role\n");
}

void	test_membership_make_player_is_not_owner(void)
{
	t_membership	m;

	m = membership_make(22, "bob", ROLE_PLAYER);
	assert(m.player_id == 22);
	assert(strcmp(m.username, "bob") == 0);
	assert(m.role == ROLE_PLAYER);
	assert(!membership_is_owner(&m));
	printf("PASS test_membership_make_player_is_not_owner\n");
}

void	test_membership_set_role_promotes_to_owner(void)
{
	t_membership	m;

	m = membership_make(22, "bob", ROLE_PLAYER);
	membership_set_role(&m, ROLE_OWNER);
	assert(m.role == ROLE_OWNER);
	assert(membership_is_owner(&m));
	assert(m.player_id == 22);
	assert(strcmp(m.username, "bob") == 0);
	printf("PASS test_membership_set_role_promotes_to_owner\n");
}

int	main(void)
{
	test_membership_make_owner_preserves_identity_and_role();
	test_membership_make_player_is_not_owner();
	test_membership_set_role_promotes_to_owner();
	return (0);
}
