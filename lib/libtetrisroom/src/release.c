#include "tetrisroom.h"

/**
 * @brief Releases a seated player, promoting a successor if the owner left.
 *
 * Owner leave (UC-07a): select the successor, promote them to OWNER, move
 * them into the vacated slot, clear their old slot. The result is filled
 * after the role change, so the caller's broadcast can never show an
 * ownerless room. Decrements the count and recomputes status.
 *
 * @param r The room to release from.
 * @param pid The leaving player's id.
 * @param probe Connection probe for successor selection; NULL = connected.
 * @param probe_ctx Opaque context passed to probe.
 * @param out Release outcome for the caller's broadcast/narration.
 * @return 0 on success, -1 when pid is not seated (room unchanged,
 *         out->released false).
 */
int	room_release(t_room *r, t_player_id pid, bool (*probe)(void *ctx, t_player_id pid), \
				void *probe_ctx,
				t_release_result *out)
{
	/* TODO: find slot; LEAVING; if owner: room_select_successor ->
	   membership_set_role(OWNER) -> move to vacated slot -> clear old
	   slot; else slot_clear; count--; recompute; fill out. */
	(void)r;
	(void)pid;
	(void)probe;
	(void)probe_ctx;
	(void)out;
	return (-1);
}

/**
 * @brief Picks the next owner: first occupied slot in order, skipping
 * disconnected members (UC-07a.1b).
 *
 * Selection alone mutates nothing - promotion happens in room_release.
 *
 * @param r The room to search.
 * @param probe Connection probe; NULL means always connected.
 * @param probe_ctx Opaque context passed to probe.
 * @return The successor's membership, or NULL when no connected non-owner
 *         candidate remains.
 */
t_membership	*room_select_successor(const t_room *r, bool (*probe)(void *ctx, t_player_id pid), \
										void *probe_ctx)
{
	/* TODO: scan slots in order; skip the owner and disconnected pids;
	   return first match. */
	(void)r;
	(void)probe;
	(void)probe_ctx;
	return (NULL);
}
