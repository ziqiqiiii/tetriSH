#include "tetrisroom.h"

/**
 * @brief Projects a room into a lobby row (UC-03.3).
 *
 * Pure read: the room is never mutated by projection.
 *
 * @param r The room to project.
 * @param out Receives id, name, mode, players/slots, status, and owner
 *            name.
 */
void	room_to_summary(const t_room *r, t_room_summary *out)
{
	/* TODO: copy id/name/mode/counts/status; owner name from the
	   ROLE_OWNER membership, empty when the room has no owner yet. */
	(void)r;
	(void)out;
}

/**
 * @brief Projects a room into an admin row with member ids (UC-25/26).
 *
 * @param r The room to project.
 * @param out Receives the summary plus every seated player id, in slot
 *            order.
 */
void	room_to_snapshot(const t_room *r, t_room_snapshot *out)
{
	/* TODO: room_to_summary into out->summary; collect occupied slots'
	   pids into member_pids/member_count. */
	(void)r;
	(void)out;
}
