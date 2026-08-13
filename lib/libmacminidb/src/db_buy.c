/* ************************************************************************** */
/*                                                                            */
/*   db_buy.c — purchase a character or theme (§3 write, write lock)           */
/*                                                                            */
/*   Characters and themes both cost wallet_points (from the catalogue). Both   */
/*   reject an unknown item, an already-owned item, an unaffordable price, or   */
/*   a full owned list, then deduct the cost, append the id to the owned list,  */
/*   and persist. Run under the write lock with a page-cache log append.        */
/*                                                                            */
/*   The charge and the grant land in one record: both are applied to a copy   */
/*   of the player, that copy is what is persisted, and only a persisted copy  */
/*   is committed back over the live row. Deducting after the log append wrote */
/*   a record that owned the item and had not paid for it — replayed after a   */
/*   crash, that is a free purchase.                                           */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// Static Functions
static t_db_result	grant(t_db *db, t_player *p, t_item_id *ids, size_t *count, t_item_id id);

/**
 * @brief Buy a character, charging its catalogue cost to the wallet.
 *
 * Fails if the player or character id is unknown (DB_NOT_FOUND), the character
 * is already owned (DB_EXISTS), or the wallet cannot cover the cost
 * (DB_INSUFFICIENT). The charge and the grant are applied to a copy, which is
 * persisted before it is committed, so a record never says "owned" without
 * also saying "paid".
 *
 * @param db The handle.
 * @param id The buying player's id.
 * @param cid The character id to purchase.
 * @return DB_OK or one of DB_INVALID/DB_NOT_FOUND/DB_EXISTS/DB_INSUFFICIENT/DB_FULL.
 */
t_db_result	db_buy_character(t_db *db, t_player_id id, t_item_id cid)
{
	t_player			*p;
	t_player			tmp;
	const t_character	*c;
	t_db_result			r;

	if (!db)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	p = db_find_by_id(db, id);
	c = catalogue_character(db->cat, cid);
	if (!p || !c)
		r = DB_NOT_FOUND;
	else if (owned_has(p->owned_characters, p->owned_characters_count, cid))
		r = DB_EXISTS;
	else if (p->wallet_points < c->cost_points)
		r = DB_INSUFFICIENT;
	else if (p->owned_characters_count >= DB_MAX_OWNED)
		r = DB_FULL;
	else
	{
		tmp = *p;
		tmp.wallet_points -= c->cost_points;
		r = grant(db, &tmp, tmp.owned_characters, &tmp.owned_characters_count, cid);
		if (r == DB_OK)
			*p = tmp;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Buy a theme, charging its catalogue cost to the wallet.
 *
 * Fails if the player or theme id is unknown (DB_NOT_FOUND), the theme is
 * already owned (DB_EXISTS), or the wallet cannot cover the cost
 * (DB_INSUFFICIENT). The charge and the grant are applied to a copy, which is
 * persisted before it is committed, so a record never says "owned" without
 * also saying "paid".
 *
 * @param db The handle.
 * @param id The buying player's id.
 * @param tid The theme id to purchase.
 * @return DB_OK or one of DB_INVALID/DB_NOT_FOUND/DB_EXISTS/DB_INSUFFICIENT/DB_FULL.
 */
t_db_result	db_buy_theme(t_db *db, t_player_id id, t_item_id tid)
{
	t_player		*p;
	t_player		tmp;
	const t_theme	*t;
	t_db_result		r;

	if (!db)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	p = db_find_by_id(db, id);
	t = catalogue_theme(db->cat, tid);
	if (!p || !t)
		r = DB_NOT_FOUND;
	else if (owned_has(p->owned_themes, p->owned_themes_count, tid))
		r = DB_EXISTS;
	else if (p->wallet_points < t->cost_points)
		r = DB_INSUFFICIENT;
	else if (p->owned_themes_count >= DB_MAX_OWNED)
		r = DB_FULL;
	else
	{
		tmp = *p;
		tmp.wallet_points -= t->cost_points;
		r = grant(db, &tmp, tmp.owned_themes, &tmp.owned_themes_count, tid);
		if (r == DB_OK)
			*p = tmp;
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}

/**
 * @brief Append id to the owned list, then persist the player.
 *
 * Adds the id (guarded by owned_add's capacity check) and logs the updated
 * player so the purchase is durable. The caller holds the write lock and has
 * already deducted the cost — from p, which is the caller's uncommitted copy,
 * never the live row, so a failure here leaves nothing behind.
 *
 * @param db The handle (for the log append).
 * @param p The player being modified and persisted.
 * @param ids The owned-id array to grow.
 * @param count In/out live-entry count.
 * @param id The item id to append.
 * @return DB_OK on success, DB_FULL if the list is full, DB_IO_ERROR on a log fail.
 */
static t_db_result	grant(t_db *db, t_player *p, t_item_id *ids, size_t *count, t_item_id id)
{
	t_db_result	r;

	r = owned_add(ids, count, id);
	if (r != DB_OK)
		return (r);
	return (db_persist(db, p));
}
