#include "tetrisroom.h"

/**
 * @brief Builds a membership value for a seated player.
 *
 * @param pid The player's id.
 * @param username Display name, truncated to ROOM_NAME_MAX - 1.
 * @param role ROLE_OWNER or ROLE_PLAYER.
 * @return The membership, with muted defaulting to false.
 */
t_membership	membership_make(t_player_id pid, const char *username, t_room_role role)
{
	t_membership	m;

	/* TODO: zero m; copy pid/username (NUL-terminated)/role; muted false. */
	(void)pid;
	(void)username;
	(void)role;
	memset(&m, 0, sizeof(m));
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
	/* TODO: m->role = role; identity fields untouched. */
	(void)m;
	(void)role;
}

/**
 * @brief Reports whether a membership holds the owner role.
 *
 * @param m The membership to query.
 * @return true when role == ROLE_OWNER, false otherwise.
 */
bool	membership_is_owner(const t_membership *m)
{
	/* TODO: return m->role == ROLE_OWNER. */
	(void)m;
	return (false);
}
