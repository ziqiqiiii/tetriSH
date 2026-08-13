/* ************************************************************************** */
/*                                                                            */
/*   db_query.c — leaderboard / rank reads + catalogue passthrough (§3)        */
/*                                                                            */
/*   The leaderboard top-N and a player's rank, both read-locked over the skip */
/*   list, plus the two catalogue accessors. The catalogue is immutable after  */
/*   db_open, so its accessors need no lock; the skip-list reads do.           */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Copy the top-ranked players into out, up to cap entries.
 *
 * Read-locked projection of the skip list from the highest score down. The
 * number actually written (0..cap) is reported via out_count.
 *
 * @param db The handle.
 * @param out Destination array of at least cap rank entries.
 * @param cap Maximum entries to write.
 * @param out_count Receives the number of entries written.
 * @return DB_OK on success, DB_INVALID on a NULL argument.
 */
t_db_result	db_leaderboard(t_db *db, t_rank_entry *out, size_t cap, size_t *out_count)
{
	if (!db || !out || !out_count)
		return (DB_INVALID);
	pthread_rwlock_rdlock(&db->lock);
	*out_count = skiplist_topn(db->board, out, cap);
	pthread_rwlock_unlock(&db->lock);
	return (DB_OK);
}

/**
 * @brief Report a player's 1-based leaderboard rank.
 *
 * Read-locked: looks the player up by id, then asks the skip list for its rank.
 * A missing player reports DB_NOT_FOUND and leaves out_rank untouched.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param out_rank Receives the 1-based rank on success.
 * @return DB_OK, DB_INVALID, or DB_NOT_FOUND.
 */
t_db_result	db_rank(t_db *db, t_player_id id, size_t *out_rank)
{
	t_player	*p;
	t_db_result	r;

	if (!db || !out_rank)
		return (DB_INVALID);
	pthread_rwlock_rdlock(&db->lock);
	p = db_find_by_id(db, id);
	if (!p)
		r = DB_NOT_FOUND;
	else
	{
		*out_rank = skiplist_rank(db->board, p);
		r = DB_OK;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Look up a character row in the catalogue by id.
 *
 * No lock: the catalogue is loaded once at db_open and never mutated, so reads
 * are safe to run lock-free against it.
 *
 * @param db The handle (may be NULL).
 * @param cid The character id.
 * @return The catalogue row, or NULL if absent or db is NULL.
 */
const t_character	*db_get_character(t_db *db, t_item_id cid)
{
	if (!db)
		return (NULL);
	return (catalogue_character(db->cat, cid));
}

/**
 * @brief Look up a theme row in the catalogue by id.
 *
 * No lock, for the same reason as db_get_character: the catalogue is immutable
 * after db_open.
 *
 * @param db The handle (may be NULL).
 * @param tid The theme id.
 * @return The catalogue row, or NULL if absent or db is NULL.
 */
const t_theme	*db_get_theme(t_db *db, t_item_id tid)
{
	if (!db)
		return (NULL);
	return (catalogue_theme(db->cat, tid));
}

/**
 * @brief Copy the whole character catalogue into out, up to cap rows.
 *
 * The by-id accessors answer "what is this item", which is the only question a
 * caller who already knows the id can ask. A store front asks the other one —
 * "what is for sale" — and could only reach it by probing ids until one came
 * back NULL, which turns a gap in the numbering into the end of the roster.
 *
 * No lock, for the same reason as db_get_character: the catalogue is immutable
 * after db_open. Rows are copied rather than borrowed so the answer cannot
 * outlive the handle.
 *
 * @param db The handle.
 * @param out Destination array of at least cap character rows.
 * @param cap Maximum rows to write.
 * @param out_count Receives the number of rows written.
 * @return DB_OK, DB_INVALID on a NULL argument, DB_FULL when cap is too small
 *         to hold the whole roster (nothing is written).
 */
t_db_result	db_characters(t_db *db, t_character *out, size_t cap, size_t *out_count)
{
	size_t	i;

	if (!db || !out || !out_count)
		return (DB_INVALID);
	if (db->cat->char_count > cap)
		return (DB_FULL);
	i = 0;
	while (i < db->cat->char_count)
	{
		out[i] = db->cat->characters[i];
		i++;
	}
	*out_count = db->cat->char_count;
	return (DB_OK);
}

/**
 * @brief Copy the whole theme catalogue into out, up to cap rows.
 *
 * The counterpart to db_characters; same reasoning, same lock-free read.
 *
 * @param db The handle.
 * @param out Destination array of at least cap theme rows.
 * @param cap Maximum rows to write.
 * @param out_count Receives the number of rows written.
 * @return DB_OK, DB_INVALID on a NULL argument, DB_FULL when cap is too small
 *         to hold the whole roster (nothing is written).
 */
t_db_result	db_themes(t_db *db, t_theme *out, size_t cap, size_t *out_count)
{
	size_t	i;

	if (!db || !out || !out_count)
		return (DB_INVALID);
	if (db->cat->theme_count > cap)
		return (DB_FULL);
	i = 0;
	while (i < db->cat->theme_count)
	{
		out[i] = db->cat->themes[i];
		i++;
	}
	*out_count = db->cat->theme_count;
	return (DB_OK);
}
