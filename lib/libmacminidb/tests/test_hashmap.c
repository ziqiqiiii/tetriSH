// Unit tests for hashmap.c / hashmap_ops.c — username -> player_t* index.
// Covers insert + get, LWW replace returning the displaced player, foreach
// visiting every live entry, and a lookup miss. Players are heap-allocated
// because hashmap_destroy owns and frees them.
#include "internal.h"
#include <assert.h>
#include <stdio.h>

// Static Functions
static t_player	*make_player(const char *username, int64_t score);
static void		count_cb(t_player *p, void *ctx);
static void		test_put_get_and_miss(void);
static void		test_lww_replace_returns_previous(void);
static void		test_foreach_visits_all(void);

int	main(void)
{
	test_put_get_and_miss();
	test_lww_replace_returns_previous();
	test_foreach_visits_all();
	return (0);
}

/**
 * @brief Allocate a zeroed player with the given username and score.
 *
 * @param username Username copied into the record (bounded by DB_MAX_USERNAME).
 * @param score Leaderboard score to stamp, so callers can tell records apart.
 * @return The new heap player (ownership passes to the map on put).
 */
static t_player	*make_player(const char *username, int64_t score)
{
	t_player	*p;

	p = calloc(1, sizeof(*p));
	assert(p);
	snprintf(p->username, sizeof(p->username), "%s", username);
	p->leaderboard_score = score;
	return (p);
}

/**
 * @brief foreach callback that bumps the size_t counter pointed to by ctx.
 *
 * @param p The visited player (unused).
 * @param ctx Pointer to a size_t visit counter.
 */
static void	count_cb(t_player *p, void *ctx)
{
	(void)p;
	(*(size_t *)ctx)++;
}

/**
 * @brief A put is retrievable by username and an unknown key misses.
 */
static void	test_put_get_and_miss(void)
{
	t_hashmap	*m;
	t_player	*alice;

	m = hashmap_create(DB_HASH_BUCKETS);
	assert(m);
	alice = make_player("alice", 10);
	assert(hashmap_put(m, alice) == NULL);
	assert(hashmap_get(m, "alice") == alice);
	assert(hashmap_get(m, "nobody") == NULL);
	hashmap_destroy(m);
	printf("PASS test_put_get_and_miss\n");
}

/**
 * @brief Re-putting a username returns the displaced player (LWW).
 */
static void	test_lww_replace_returns_previous(void)
{
	t_hashmap	*m;
	t_player	*first;
	t_player	*second;

	m = hashmap_create(DB_HASH_BUCKETS);
	first = make_player("bob", 1);
	second = make_player("bob", 2);
	assert(hashmap_put(m, first) == NULL);
	assert(hashmap_put(m, second) == first);
	assert(hashmap_get(m, "bob") == second);
	assert(hashmap_get(m, "bob")->leaderboard_score == 2);
	free(first);
	hashmap_destroy(m);
	printf("PASS test_lww_replace_returns_previous\n");
}

/**
 * @brief foreach visits each live entry exactly once, replaces excluded.
 */
static void	test_foreach_visits_all(void)
{
	t_hashmap	*m;
	size_t		seen;
	int			i;
	char		name[DB_MAX_USERNAME];

	m = hashmap_create(DB_HASH_BUCKETS);
	i = 0;
	while (i < 50)
	{
		snprintf(name, sizeof(name), "p%d", i++);
		hashmap_put(m, make_player(name, i));
	}
	seen = 0;
	hashmap_foreach(m, count_cb, &seen);
	assert(seen == 50);
	hashmap_destroy(m);
	printf("PASS test_foreach_visits_all\n");
}
