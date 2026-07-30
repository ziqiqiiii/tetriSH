#include "tetrisroom.h"

// Static Functions
static t_slot	*slot_of(t_room *r, t_player_id pid);
static void		promote_into(t_room *r, t_slot *vacated,
					bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx);
static void		report_owner(t_release_result *out, const t_membership *m);

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
	t_slot	*s;

	if (out)
		memset(out, 0, sizeof(*out));
	s = slot_of(r, pid);
	if (!s)
		return (-1);
	s->status = SLOT_LEAVING;
	if (membership_is_owner(&s->membership))
		promote_into(r, s, probe, probe_ctx);
	else
		slot_clear(s);
	r->number_of_players--;
	room_recompute_status(r);
	if (out)
	{
		out->released = true;
		out->room_empty = (r->number_of_players == 0);
		if (s->occupied)
			report_owner(out, &s->membership);
	}
	return (0);
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
	t_slot	*slots;
	int		i;

	if (!r)
		return (NULL);
	slots = (t_slot *)r->slots;
	i = 0;
	while (i < r->slot_count)
	{
		if (slots[i].occupied && !membership_is_owner(&slots[i].membership)
			&& (!probe || probe(probe_ctx, slots[i].membership.player_id)))
			return (&slots[i].membership);
		i++;
	}
	return (NULL);
}

/**
 * @brief Finds the slot a player occupies.
 *
 * @param r The room to search.
 * @param pid The player to find.
 * @return The occupied slot, or NULL when the player is not seated.
 */
static t_slot	*slot_of(t_room *r, t_player_id pid)
{
	int	i;

	if (!r)
		return (NULL);
	i = 0;
	while (i < r->slot_count)
	{
		if (r->slots[i].occupied && r->slots[i].membership.player_id == pid)
			return (&r->slots[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Moves the successor into the departing owner's slot as OWNER.
 *
 * With no successor the room is simply emptied of the owner, leaving the
 * caller a room that reports empty rather than one owned by nobody.
 *
 * @param r The room whose owner is leaving.
 * @param vacated The owner's slot, already marked LEAVING.
 * @param probe Connection probe; NULL means always connected.
 * @param probe_ctx Opaque context passed to probe.
 */
static void	promote_into(t_room *r, t_slot *vacated,
		bool (*probe)(void *ctx, t_player_id pid), void *probe_ctx)
{
	t_membership	m;
	t_membership	*succ;
	t_slot			*old;

	succ = room_select_successor(r, probe, probe_ctx);
	if (!succ)
	{
		slot_clear(vacated);
		return ;
	}
	m = *succ;
	membership_set_role(&m, ROLE_OWNER);
	old = slot_of(r, m.player_id);
	if (old)
		slot_clear(old);
	slot_clear(vacated);
	vacated->status = SLOT_JOINING;
	slot_occupy(vacated, m);
}

/**
 * @brief Records a promoted owner in the release result.
 *
 * @param out The result to fill.
 * @param m The membership now holding ROLE_OWNER.
 */
static void	report_owner(t_release_result *out, const t_membership *m)
{
	out->owner_changed = true;
	out->new_owner = m->player_id;
	strncpy(out->new_owner_name, m->username, ROOM_USER_MAX - 1);
}
