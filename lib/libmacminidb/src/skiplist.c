/* ************************************************************************** */
/*                                                                            */
/*   skiplist.c — leaderboard index: lifecycle + tower construction           */
/*                                                                            */
/*   §7.4: chosen over B+/AVL for simplest implementation and no rotations to */
/*   coordinate under the rwlock. Only db_record_game mutates it. Nodes point */
/*   at the same player_t objects owned by the hash map. This file owns the   */
/*   list lifecycle and the two primitives the mutators build on — allocating */
/*   a node tower and drawing a random height. The ordering predicate, search */
/*   core, and write ops live in skiplist_ops.c; the read queries in          */
/*   skiplist_read.c.                                                          */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Allocate an empty skip list with a single full-height head sentinel.
 *
 * The head tower carries DB_SKIP_MAXLVL forward links, all NULL, so the list
 * can grow to its maximum height without ever reallocating the sentinel. The
 * rng is seeded off the head address to decorrelate the towers between runs.
 *
 * @return The new list, or NULL on allocation failure.
 */
t_skiplist	*skiplist_create(void)
{
	t_skiplist	*s;

	s = malloc(sizeof(*s));
	if (!s)
		return (NULL);
	s->head = calloc(1, sizeof(*s->head)
			+ DB_SKIP_MAXLVL * sizeof(t_skipnode *));
	if (!s->head)
	{
		free(s);
		return (NULL);
	}
	s->head->player = NULL;
	s->head->height = DB_SKIP_MAXLVL;
	s->level = 1;
	s->size = 0;
	s->rng = (uint64_t)(uintptr_t)s->head | 1ULL;
	return (s);
}

/**
 * @brief Free every node and the list itself, leaving the players untouched.
 *
 * The player_t objects are owned by the hash map, not the skip list, so only
 * the node towers and the sentinel are freed here. Safe to call with NULL.
 *
 * @param s The list to destroy (may be NULL).
 */
void	skiplist_destroy(t_skiplist *s)
{
	t_skipnode	*cur;
	t_skipnode	*next;

	if (!s)
		return ;
	cur = s->head;
	while (cur)
	{
		next = cur->forward[0];
		free(cur);
		cur = next;
	}
	free(s);
}

/**
 * @brief Allocate a node of the given tower height holding player p.
 *
 * The forward links are zeroed so an un-spliced node reads as a clean tail at
 * every level. Height is recorded on the node so remove can unlink exactly the
 * links it owns.
 *
 * @param p The player the node indexes (borrowed, not owned).
 * @param height Number of forward links in the tower (>= 1).
 * @return The new node, or NULL on allocation failure.
 */
t_skipnode	*node_new(t_player *p, int height)
{
	t_skipnode	*node;

	node = calloc(1, sizeof(*node) + (size_t)height * sizeof(t_skipnode *));
	if (!node)
		return (NULL);
	node->player = p;
	node->height = height;
	return (node);
}

/**
 * @brief Draw a random tower height in [1, DB_SKIP_MAXLVL].
 *
 * Each extra level is kept with probability 1/DB_SKIP_P, giving the classic
 * geometric height distribution and ~log_P(n) expected search cost. A small
 * xorshift64 keeps the draw self-contained — no libc rng, no global state.
 *
 * @param s The list whose rng state is advanced.
 * @return A tower height between 1 and DB_SKIP_MAXLVL inclusive.
 */
int	random_level(t_skiplist *s)
{
	int	lvl;

	lvl = 1;
	s->rng ^= s->rng << 13;
	s->rng ^= s->rng >> 7;
	s->rng ^= s->rng << 17;
	while (lvl < DB_SKIP_MAXLVL && (s->rng % DB_SKIP_P) == 0)
	{
		lvl++;
		s->rng ^= s->rng << 13;
		s->rng ^= s->rng >> 7;
		s->rng ^= s->rng << 17;
	}
	return (lvl);
}
