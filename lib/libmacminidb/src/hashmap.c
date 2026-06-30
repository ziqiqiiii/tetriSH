/* ************************************************************************** */
/*                                                                            */
/*   hashmap.c — primary index: username -> player_t*  (lifecycle + iterate)  */
/*                                                                            */
/*   Holds one pointer per live player; the player_t objects are owned here   */
/*   and shared by reference with the skip list. Collisions are resolved by   */
/*   separate chaining. The lookup/mutation path (get / put / hash / find)    */
/*   lives in hashmap_ops.c; hashmap_put is LWW (the latest record wins).     */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Allocate an empty hash map with the given number of buckets.
 *
 * The bucket array is zero-initialised so every chain starts empty. A request
 * for zero buckets is rejected, since the modulo in hashmap_hash needs a
 * positive divisor.
 *
 * @param buckets Number of buckets to allocate (must be non-zero).
 * @return The new map, or NULL on bad argument or allocation failure.
 */
t_hashmap	*hashmap_create(size_t buckets)
{
	t_hashmap	*m;

	if (buckets == 0)
		return (NULL);
	m = malloc(sizeof(*m));
	if (!m)
		return (NULL);
	m->buckets = calloc(buckets, sizeof(*m->buckets));
	if (!m->buckets)
	{
		free(m);
		return (NULL);
	}
	m->bucket_count = buckets;
	m->size = 0;
	return (m);
}

/**
 * @brief Free the map, every chained entry, and every owned player.
 *
 * The player_t objects are owned by the map, so they are freed here; callers
 * holding the same pointers via the skip list must drop them first. Safe to
 * call with a NULL map.
 *
 * @param m The map to destroy (may be NULL).
 */
void	hashmap_destroy(t_hashmap *m)
{
	t_hm_entry	*e;
	t_hm_entry	*next;
	size_t		i;

	if (!m)
		return ;
	i = 0;
	while (i < m->bucket_count)
	{
		e = m->buckets[i++];
		while (e)
		{
			next = e->next;
			free(e->player);
			free(e);
			e = next;
		}
	}
	free(m->buckets);
	free(m);
}

/**
 * @brief Invoke fn(player, ctx) once for every live player, in no fixed order.
 *
 * Used at boot to seed the skip list from the recovered map (§6). fn must not
 * insert into or remove from the map, as the iteration holds no snapshot.
 *
 * @param m The map to iterate (may be NULL, treated as empty).
 * @param fn Callback receiving each player and the opaque ctx.
 * @param ctx Opaque pointer passed through to every fn call.
 */
void	hashmap_foreach(t_hashmap *m, void (*fn)(t_player *, void *), void *ctx)
{
	t_hm_entry	*e;
	size_t		i;

	if (!m || !fn)
		return ;
	i = 0;
	while (i < m->bucket_count)
	{
		e = m->buckets[i++];
		while (e)
		{
			fn(e->player, ctx);
			e = e->next;
		}
	}
}
