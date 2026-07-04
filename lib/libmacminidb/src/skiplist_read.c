/* ************************************************************************** */
/*                                                                            */
/*   skiplist_read.c — leaderboard read queries (rank, top-n) + re-sort       */
/*                                                                            */
/*   The read side of the index: a player's 1-based rank and the top N rows,  */
/*   both linear bottom-level scans (trivial at ~300 users, and they keep the */
/*   node small). skiplist_update sits here too: it is a remove + re-insert    */
/*   built on skiplist_ops.c, so it touches no node internals of its own.     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Return p's 1-based position in the leaderboard, or 0 if absent.
 *
 * Walks the bottom level counting nodes until it reaches the one for p (matched
 * by key), so rank 1 is the top of the board. Cheaper structures exist, but at
 * ~300 users a linear bottom-level scan is trivial and keeps the node small.
 *
 * @param s The list to query (may be NULL).
 * @param p The player to locate.
 * @return The 1-based rank, or 0 if p is not in the list.
 */
size_t	skiplist_rank(t_skiplist *s, t_player *p)
{
	t_skipnode	*cur;
	size_t		rank;

	if (!s || !p)
		return (0);
	cur = s->head->forward[0];
	rank = 1;
	while (cur)
	{
		if (skiplist_cmp(cur->player, p) == 0)
			return (rank);
		cur = cur->forward[0];
		rank++;
	}
	return (0);
}

/**
 * @brief Copy the top-ranked players into out, up to cap entries.
 *
 * Walks the bottom level from the highest score down, projecting each player to
 * a t_rank_entry, and stops at cap or the end of the list. The leaderboard read
 * path (db_leaderboard) uses this to answer "top N".
 *
 * @param s The list to read (may be NULL, yields zero).
 * @param out Destination array of at least cap entries.
 * @param cap Maximum number of entries to write.
 * @return The number of entries actually written (0..cap).
 */
size_t	skiplist_topn(t_skiplist *s, t_rank_entry *out, size_t cap)
{
	t_skipnode	*cur;
	size_t		n;

	if (!s || !out)
		return (0);
	cur = s->head->forward[0];
	n = 0;
	while (cur && n < cap)
	{
		out[n].player_id = cur->player->player_id;
		out[n].leaderboard_score = cur->player->leaderboard_score;
		snprintf(out[n].username, DB_MAX_USERNAME, "%s", cur->player->username);
		cur = cur->forward[0];
		n++;
	}
	return (n);
}

/**
 * @brief Move a player to its new position after its score changes.
 *
 * Removes the node under p's current key, stamps the new score, then re-inserts
 * so the tower lands at the new (score, id) slot. Removing first is required:
 * the search key must still match the old position when we unlink. A player not
 * yet present is simply inserted at the new score.
 *
 * @param s The list to re-sort.
 * @param p The player whose score is changing.
 * @param score The new leaderboard score to stamp and re-sort by.
 */
void	skiplist_update(t_skiplist *s, t_player *p, int64_t score)
{
	if (!s || !p)
		return ;
	skiplist_remove(s, p);
	p->leaderboard_score = score;
	skiplist_insert(s, p);
}
