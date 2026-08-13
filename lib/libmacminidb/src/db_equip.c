/* ************************************************************************** */
/*                                                                            */
/*   db_equip.c — set the equipped character or theme (§3 write, write lock)   */
/*                                                                            */
/*   Equipping only sets a pointer-by-id into an already-owned item, so the    */
/*   sole failure beyond a bad id is "not owned". The change is persisted so   */
/*   the loadout survives a restart. Run under the write lock with a           */
/*   page-cache log append.                                                    */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Equip a character the player already owns.
 *
 * Fails if the player id is unknown (DB_NOT_FOUND) or the character is not in
 * the player's owned list (DB_NOT_OWNED). On success the equipped character is
 * set and the player is persisted.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param cid The character id to equip.
 * @return DB_OK, DB_INVALID, DB_NOT_FOUND, DB_NOT_OWNED, or DB_IO_ERROR.
 */
t_db_result	db_equip_character(t_db *db, t_player_id id, t_item_id cid)
{
	t_player	*p;
	t_player	tmp;
	t_db_result	r;

	if (!db)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	p = db_find_by_id(db, id);
	if (!p)
		r = DB_NOT_FOUND;
	else if (!owned_has(p->owned_characters, p->owned_characters_count, cid))
		r = DB_NOT_OWNED;
	else
	{
		tmp = *p;
		tmp.current_equipped_character = cid;
		r = db_persist(db, &tmp);
		if (r == DB_OK)
			*p = tmp;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Equip a theme the player already owns.
 *
 * Fails if the player id is unknown (DB_NOT_FOUND) or the theme is not in the
 * player's owned list (DB_NOT_OWNED). On success the equipped theme is set and
 * the player is persisted.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param tid The theme id to equip.
 * @return DB_OK, DB_INVALID, DB_NOT_FOUND, DB_NOT_OWNED, or DB_IO_ERROR.
 */
t_db_result	db_equip_theme(t_db *db, t_player_id id, t_item_id tid)
{
	t_player	*p;
	t_player	tmp;
	t_db_result	r;

	if (!db)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	p = db_find_by_id(db, id);
	if (!p)
		r = DB_NOT_FOUND;
	else if (!owned_has(p->owned_themes, p->owned_themes_count, tid))
		r = DB_NOT_OWNED;
	else
	{
		tmp = *p;
		tmp.current_equipped_theme = tid;
		r = db_persist(db, &tmp);
		if (r == DB_OK)
			*p = tmp;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}
