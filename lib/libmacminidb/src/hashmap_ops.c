/* ************************************************************************** */
/*                                                                            */
/*   hashmap_ops.c — primary index lookup & mutation (get / put + helpers)    */
/*                                                                            */
/*   The hot path for the username index whose lifecycle lives in hashmap.c.  */
/*   hashmap_put is Last-Writer-Wins: putting a player under a username that  */
/*   already exists swaps in the new pointer and hands the old one back to    */
/*   the caller, who then owns (and frees) the displaced player.              */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Look up the live player stored under username.
 *
 * Hashes to the bucket and walks its chain. A return of NULL means no such
 * player (a recovery miss or an unknown login), not an error.
 *
 * @param m The map to search (may be NULL, treated as empty).
 * @param username NUL-terminated username key.
 * @return The owned player pointer, or NULL if absent.
 */
t_player	*hashmap_get(t_hashmap *m, const char *username)
{
	t_hm_entry	*e;

	if (!m || !username)
		return (NULL);
	e = hashmap_find(m, username, NULL);
	if (!e)
		return (NULL);
	return (e->player);
}

/**
 * @brief Insert or LWW-replace the player keyed by its own username.
 *
 * If the username is already present, p replaces the stored pointer and the
 * displaced player is returned for the caller to free. Otherwise a fresh entry
 * is chained at the bucket head and NULL is returned. On allocation failure
 * the map is left unchanged and p is handed straight back (nothing was stored).
 *
 * @param m The map to mutate.
 * @param p The player to store; p->username is the key.
 * @return The displaced player on replace, p on alloc failure, else NULL.
 */
t_player	*hashmap_put(t_hashmap *m, t_player *p)
{
	t_hm_entry	*e;
	t_player	*old;
	size_t		idx;

	if (!m || !p)
		return (p);
	e = hashmap_find(m, p->username, &idx);
	if (e)
	{
		old = e->player;
		e->player = p;
		return (old);
	}
	e = malloc(sizeof(*e));
	if (!e)
		return (p);
	e->player = p;
	e->next = m->buckets[idx];
	m->buckets[idx] = e;
	m->size++;
	return (NULL);
}

/**
 * @brief Hash a username to a bucket index (FNV-1a, 64-bit, folded by modulo).
 *
 * FNV-1a is small, fast, and spreads the short ASCII usernames well across the
 * fixed bucket count. The username is bounded by DB_MAX_USERNAME, so the walk
 * stops at the NUL or that width, whichever comes first.
 *
 * @param username NUL-terminated username key.
 * @param bucket_count Number of buckets (the modulo divisor, must be non-zero).
 * @return The bucket index in [0, bucket_count).
 */
size_t	hashmap_hash(const char *username, size_t bucket_count)
{
	uint64_t	h;
	size_t		i;

	h = 1469598103934665603ULL;
	i = 0;
	while (i < DB_MAX_USERNAME && username[i])
	{
		h ^= (uint8_t)username[i++];
		h *= 1099511628211ULL;
	}
	return ((size_t)(h % bucket_count));
}

/**
 * @brief Find the chain entry for username and optionally report its bucket.
 *
 * Shared by get (which ignores the index) and put (which needs it to chain a
 * new entry). out_idx is written even on a miss, so put can insert without
 * re-hashing.
 *
 * @param m The map to search (assumed non-NULL; callers guard).
 * @param username NUL-terminated username key.
 * @param out_idx If non-NULL, receives the hashed bucket index.
 * @return The matching entry, or NULL if the username is absent.
 */
t_hm_entry	*hashmap_find(t_hashmap *m, const char *username, size_t *out_idx)
{
	t_hm_entry	*e;
	size_t		idx;

	idx = hashmap_hash(username, m->bucket_count);
	if (out_idx)
		*out_idx = idx;
	e = m->buckets[idx];
	while (e)
	{
		if (strncmp(e->player->username, username, DB_MAX_USERNAME) == 0)
			return (e);
		e = e->next;
	}
	return (NULL);
}
