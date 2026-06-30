/* ************************************************************************** */
/*                                                                            */
/*   db_record.c — post-game write (§3 record_game, write lock)               */
/*                                                                            */
/*   The only write that touches the leaderboard skip list. Applies the score  */
/*   and wallet deltas, bumps the games/wins counters, re-sorts the player on   */
/*   the board via skiplist_update, then persists. skiplist_update must run     */
/*   before any manual score write — it removes the node at the old key, stamps */
/*   the new score, and re-inserts, so the search key still matches on unlink.  */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Apply a finished game's results to a player and the leaderboard.
 *
 * Adds score_delta to the leaderboard score (re-sorting the board through
 * skiplist_update), adds points_delta to the wallet, increments games_played,
 * and increments games_won when won is true. The updated player is persisted.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param score_delta Signed change to the leaderboard score.
 * @param points_delta Signed change to the wallet points.
 * @param won Whether this game counts as a win.
 * @return DB_OK, DB_INVALID, DB_NOT_FOUND, or DB_IO_ERROR.
 */
t_db_result	db_record_game(t_db *db, t_player_id id, int64_t score_delta, int64_t points_delta, bool won)
{
	t_player	*p;
	t_db_result	r;

	if (!db)
		return (DB_INVALID);
	pthread_rwlock_wrlock(&db->lock);
	p = db_find_by_id(db, id);
	if (!p)
		r = DB_NOT_FOUND;
	else
	{
		skiplist_update(db->board, p, p->leaderboard_score + score_delta);
		p->wallet_points += points_delta;
		p->games_played++;
		p->games_won += (won ? 1 : 0);
		r = db_persist(db, p);
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}
