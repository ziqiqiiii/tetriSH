// Unit tests for skiplist.c / skiplist_ops.c — leaderboard ordered by
// (leaderboard_score desc, player_id asc). Covers insert ordering with the id
// tiebreak, update re-sorting a player into a new slot, topN honouring the cap,
// 1-based rank (and 0 when absent), and remove. The skip list borrows players
// (the hash map owns them), so this file allocates and frees them itself.
#include "internal.h"
#include <assert.h>
#include <stdio.h>

// Static Functions
static t_player	*make_player(t_player_id id, int64_t score);
static void		free_all(t_player **ps, size_t n);
static void		test_insert_orders_by_score_then_id(void);
static void		test_update_resorts(void);
static void		test_topn_rank_and_remove(void);
static void		test_many_inserts_stay_sorted(void);

int	main(void)
{
	test_insert_orders_by_score_then_id();
	test_update_resorts();
	test_topn_rank_and_remove();
	test_many_inserts_stay_sorted();
	return (0);
}

/**
 * @brief Allocate a zeroed player with the given id and leaderboard score.
 *
 * @param id The player_id, also used to build a distinguishable username.
 * @param score Leaderboard score the skip list orders by.
 * @return The new heap player (the test owns and frees it).
 */
static t_player	*make_player(t_player_id id, int64_t score)
{
	t_player	*p;

	p = calloc(1, sizeof(*p));
	assert(p);
	p->player_id = id;
	p->leaderboard_score = score;
	snprintf(p->username, sizeof(p->username), "p%llu", (unsigned long long)id);
	return (p);
}

/**
 * @brief Free an array of n heap players.
 *
 * @param ps Array of player pointers.
 * @param n Number of entries to free.
 */
static void	free_all(t_player **ps, size_t n)
{
	size_t	i;

	i = 0;
	while (i < n)
		free(ps[i++]);
}

/**
 * @brief topN comes out by score descending, with the lower id winning a tie.
 */
static void	test_insert_orders_by_score_then_id(void)
{
	t_skiplist		*s;
	t_player		*ps[3];
	t_rank_entry	out[3];

	s = skiplist_create();
	assert(s);
	ps[0] = make_player(1, 50);
	ps[1] = make_player(2, 50);
	ps[2] = make_player(3, 90);
	skiplist_insert(s, ps[0]);
	skiplist_insert(s, ps[1]);
	skiplist_insert(s, ps[2]);
	assert(skiplist_topn(s, out, 3) == 3);
	assert(out[0].player_id == 3);
	assert(out[1].player_id == 1 && out[2].player_id == 2);
	skiplist_destroy(s);
	free_all(ps, 3);
	printf("PASS test_insert_orders_by_score_then_id\n");
}

/**
 * @brief Raising a score moves the player up; rank reflects the new order.
 */
static void	test_update_resorts(void)
{
	t_skiplist		*s;
	t_player		*ps[3];

	s = skiplist_create();
	ps[0] = make_player(1, 10);
	ps[1] = make_player(2, 20);
	ps[2] = make_player(3, 30);
	skiplist_insert(s, ps[0]);
	skiplist_insert(s, ps[1]);
	skiplist_insert(s, ps[2]);
	assert(skiplist_rank(s, ps[0]) == 3);
	skiplist_update(s, ps[0], 100);
	assert(ps[0]->leaderboard_score == 100);
	assert(skiplist_rank(s, ps[0]) == 1);
	assert(skiplist_rank(s, ps[2]) == 2);
	skiplist_destroy(s);
	free_all(ps, 3);
	printf("PASS test_update_resorts\n");
}

/**
 * @brief topN clamps to cap, rank is 1-based / 0 when absent, remove unlinks.
 */
static void	test_topn_rank_and_remove(void)
{
	t_skiplist		*s;
	t_player		*ps[5];
	t_rank_entry	out[2];
	int				i;

	s = skiplist_create();
	i = 0;
	while (i < 5)
	{
		ps[i] = make_player(i + 1, (i + 1) * 10);
		skiplist_insert(s, ps[i++]);
	}
	assert(skiplist_topn(s, out, 2) == 2);
	assert(out[0].player_id == 5 && out[1].player_id == 4);
	skiplist_remove(s, ps[4]);
	assert(skiplist_rank(s, ps[4]) == 0);
	assert(skiplist_rank(s, ps[3]) == 1);
	assert(skiplist_topn(s, out, 2) == 2 && out[0].player_id == 4);
	skiplist_destroy(s);
	free_all(ps, 5);
	printf("PASS test_topn_rank_and_remove\n");
}

/**
 * @brief Across many random-height towers, topN stays fully sorted after removes.
 *
 * Drives enough inserts to grow the towers past one level, then deletes every
 * other player, exercising the per-level unlink and level shrink. The surviving
 * order must still be score-descending and rank must stay contiguous from 1.
 */
static void	test_many_inserts_stay_sorted(void)
{
	t_skiplist		*s;
	t_player		*ps[200];
	t_rank_entry	out[200];
	size_t			n;
	int				i;

	s = skiplist_create();
	i = 0;
	while (i < 200)
	{
		ps[i] = make_player(i + 1, (int64_t)((i * 2654435761u) % 1000));
		skiplist_insert(s, ps[i++]);
	}
	i = 0;
	while (i < 200)
	{
		skiplist_remove(s, ps[i]);
		i += 2;
	}
	n = skiplist_topn(s, out, 200);
	assert(n == 100);
	i = 1;
	while ((size_t)i < n)
	{
		assert(out[i - 1].leaderboard_score >= out[i].leaderboard_score);
		i++;
	}
	assert(skiplist_rank(s, ps[1]) >= 1 && skiplist_rank(s, ps[0]) == 0);
	skiplist_destroy(s);
	free_all(ps, 200);
	printf("PASS test_many_inserts_stay_sorted\n");
}
