#include "tetrisroom.h"

/**
 * @brief Builds a membership value for a seated player.
 *
 * @param pid The player's id.
 * @param username Display name, truncated to ROOM_USER_MAX - 1.
 * @param role ROLE_OWNER or ROLE_PLAYER.
 * @return The membership, with muted defaulting to false.
 */
t_membership	membership_make(t_player_id pid, const char *username, t_room_role role)
{
	t_membership	m;

	memset(&m, 0, sizeof(m));
	m.player_id = pid;
	if (username)
		strncpy(m.username, username, ROOM_USER_MAX - 1);
	m.role = role;
	m.muted = false;
	return (m);
}

/**
 * @brief Changes a membership's role (owner succession, UC-07a).
 *
 * @param m The membership to update.
 * @param role The new role.
 */
void	membership_set_role(t_membership *m, t_room_role role)
{
	if (!m)
		return ;
	m->role = role;
}

/**
 * @brief Reports whether a membership holds the owner role.
 *
 * @param m The membership to query.
 * @return true when role == ROLE_OWNER, false otherwise.
 */
bool	membership_is_owner(const t_membership *m)
{
	if (!m)
		return (false);
	return (m->role == ROLE_OWNER);
}
