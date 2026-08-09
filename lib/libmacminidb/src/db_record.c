/* ************************************************************************** */
/*                                                                            */
/*   db_record.c — post-game write (§3 record_game, write lock)               */
/*                                                                            */
/*   The only write that touches the leaderboard skip list. Ranks the player   */
/*   on their best single game, adds the game to their lifetime total and the  */
/*   caller's credit to their wallet, bumps the games/wins counters, then      */
/*   persists.                                                                 */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

/**
 * @brief Apply a finished game's results to a player and the leaderboard.
 *
 * The leaderboard score is this player's *best* single game, so game_score
 * replaces it only when it beats it — playing more games never raises a rank
 * on its own, and a bad game cannot lower one. The running total of everything
 * ever scored is kept separately in lifetime_points, because the wallet's
 * exchange rate is charged against it; a caller that wants the credit to carry
 * its remainder reads that field, works out the difference, and passes it as
 * points_delta.
 *
 * Wallet points are a delta and not a score, because they are spent as well as
 * earned: this call is told what to add, never what the balance should be.
 *
 * skiplist_update must run before any manual score write — it removes the node
 * at the old key, stamps the new score, and re-inserts, so the search key still
 * matches on unlink.
 *
 * @param db The handle.
 * @param id The player's id.
 * @param game_score This game's final score; the best of them is ranked.
 * @param points_delta Signed change to the wallet points.
 * @param won Whether this game counts as a win.
 * @return DB_OK, DB_INVALID, DB_NOT_FOUND, or DB_IO_ERROR.
 */
t_db_result	db_record_game(t_db *db, t_player_id id, int64_t game_score, int64_t points_delta, bool won)
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
		if (game_score > p->leaderboard_score)
			skiplist_update(db->board, p, game_score);
		if (game_score > 0)
			p->lifetime_points += game_score;
		p->wallet_points += points_delta;
		p->games_played++;
		p->games_won += (won ? 1 : 0);
		r = db_persist(db, p);
	}
	pthread_rwlock_unlock(&db->lock);
	return (r);
}
