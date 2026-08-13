/* ************************************************************************** */
/*                                                                            */
/*   db_owned.c — owned-list membership helpers (§3 buy / equip / owns)        */
/*                                                                            */
/*   Pure helpers over a player's fixed-width owned_characters / owned_themes  */
/*   arrays: test membership and append with a capacity guard. No locking and  */
/*   no I/O — the callers in db_buy.c / db_equip.c / db_read.c hold the rwlock  */
/*   and persist as needed.                                                    */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Test whether id is present in an owned list.
 *
 * Linear scan over the count live entries; the lists are bounded by
 * DB_MAX_OWNED, so this is cheap. Shared by the buy (reject re-purchase) and
 * equip (require ownership) paths.
 *
 * @param ids The owned-id array.
 * @param count Number of live entries in ids.
 * @param id The item id to look for.
 * @return true if id is owned, false otherwise.
 */
bool	owned_has(const t_item_id *ids, size_t count, t_item_id id)
{
	size_t	i;

	i = 0;
	while (i < count)
	{
		if (ids[i] == id)
			return (true);
		i++;
	}
	return (false);
}

/**
 * @brief Append id to an owned list, growing the count.
 *
 * Assumes the caller has already ruled out a duplicate; rejects the append only
 * when the list is at DB_MAX_OWNED capacity. On success the new id is stored and
 * *count is incremented.
 *
 * @param ids The owned-id array (at least DB_MAX_OWNED wide).
 * @param count In/out live-entry count, incremented on success.
 * @param id The item id to append.
 * @return DB_OK on append, DB_FULL if the list is at capacity.
 */
t_db_result	owned_add(t_item_id *ids, size_t *count, t_item_id id)
{
	if (*count >= DB_MAX_OWNED)
		return (DB_FULL);
	ids[*count] = id;
	(*count)++;
	return (DB_OK);
}
