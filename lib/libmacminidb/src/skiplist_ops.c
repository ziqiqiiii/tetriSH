/* ************************************************************************** */
/*                                                                            */
/*   skiplist_ops.c — ordering predicate, search core, and write ops          */
/*                                                                            */
/*   The shared seek (driven by insert/remove/update) plus the two mutations  */
/*   that change the set of indexed players: insert and remove. The ordering  */
/*   predicate skiplist_cmp lives here too, since every operation is defined   */
/*   relative to it: score descending, id ascending on a tie. update lives in */
/*   skiplist_read.c, the read queries beside it.                             */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static void	splice(t_skiplist *s, t_skipnode *node, t_skipnode **upd, int h);

/**
 * @brief Order two players by (leaderboard_score desc, player_id asc).
 *
 * The leaderboard ranks the highest score first; ties break toward the lower
 * (older) player_id so the order is total and stable. player_id is unique, so
 * a zero return means a and b are the same logical entry.
 *
 * @param a Left player key.
 * @param b Right player key.
 * @return Negative if a ranks before b, positive if after, zero if equal.
 */
int	skiplist_cmp(const t_player *a, const t_player *b)
{
	if (a->leaderboard_score != b->leaderboard_score)
	{
		if (a->leaderboard_score > b->leaderboard_score)
			return (-1);
		return (1);
	}
	if (a->player_id < b->player_id)
		return (-1);
	if (a->player_id > b->player_id)
		return (1);
	return (0);
}

/**
 * @brief Collect the predecessor of p's key at every level into upd.
 *
 * Walks top level down, advancing while the next node still ranks strictly
 * before p, and records the landing node per level in upd. upd[0]->forward[0]
 * is then the node at p's slot (or NULL / a greater node if p is absent), which
 * insert, remove, and rank all key off of.
 *
 * @param s The list to search (assumed non-NULL).
 * @param p The player key to seek the predecessors of.
 * @param upd Array of at least DB_SKIP_MAXLVL slots, filled per level.
 * @return upd, for callers that want to chain off the result.
 */
t_skipnode	**skiplist_seek(t_skiplist *s, const t_player *p, t_skipnode **upd)
{
	t_skipnode	*cur;
	t_skipnode	*nxt;
	int			i;

	cur = s->head;
	i = s->level - 1;
	while (i >= 0)
	{
		nxt = cur->forward[i];
		while (nxt && skiplist_cmp(nxt->player, p) < 0)
		{
			cur = nxt;
			nxt = cur->forward[i];
		}
		upd[i] = cur;
		i--;
	}
	return (upd);
}

/**
 * @brief Index a player at its (score desc, id asc) position.
 *
 * Seeks the per-level predecessors, rejects a duplicate key (same player
 * already present), then draws a tower height and splices the node in. An
 * allocation failure leaves the list unchanged.
 *
 * @param s The list to insert into (may be NULL).
 * @param p The player to index; its current score/id form the key.
 */
void	skiplist_insert(t_skiplist *s, t_player *p)
{
	t_skipnode	*upd[DB_SKIP_MAXLVL];
	t_skipnode	*at;
	t_skipnode	*node;

	if (!s || !p)
		return ;
	at = skiplist_seek(s, p, upd)[0]->forward[0];
	if (at && skiplist_cmp(at->player, p) == 0)
		return ;
	node = node_new(p, random_level(s));
	if (!node)
		return ;
	splice(s, node, upd, node->height);
	s->size++;
}

/**
 * @brief Unlink and free the node holding player p, if present.
 *
 * Seeks the per-level predecessors, then drops p from each level whose forward
 * link points at it. The node tower is freed (the player is owned elsewhere and
 * left intact), the live count is decremented, and the list level is shrunk
 * past any now-empty top levels. A player not in the list is a silent no-op.
 *
 * @param s The list to remove from (may be NULL).
 * @param p The player whose node should be removed.
 */
void	skiplist_remove(t_skiplist *s, t_player *p)
{
	t_skipnode	*upd[DB_SKIP_MAXLVL];
	t_skipnode	*node;
	int			i;

	if (!s || !p)
		return ;
	skiplist_seek(s, p, upd);
	node = upd[0]->forward[0];
	if (!node || skiplist_cmp(node->player, p) != 0)
		return ;
	i = 0;
	while (i < s->level && upd[i]->forward[i] == node)
	{
		upd[i]->forward[i] = node->forward[i];
		i++;
	}
	free(node);
	s->size--;
	while (s->level > 1 && s->head->forward[s->level - 1] == NULL)
		s->level--;
}

/**
 * @brief Link node into the list at h levels, raising the list level if needed.
 *
 * Levels above the current list level have no seeded predecessor, so they
 * default to the head sentinel before the node is chained in at each level.
 *
 * @param s The list being mutated.
 * @param node The freshly allocated node to chain in.
 * @param upd Per-level predecessors from skiplist_seek (levels < s->level).
 * @param h The node's tower height (number of levels to link).
 */
static void	splice(t_skiplist *s, t_skipnode *node, t_skipnode **upd, int h)
{
	int	i;

	i = s->level;
	while (i < h)
		upd[i++] = s->head;
	if (h > s->level)
		s->level = h;
	i = 0;
	while (i < h)
	{
		node->forward[i] = upd[i]->forward[i];
		upd[i]->forward[i] = node;
		i++;
	}
}
