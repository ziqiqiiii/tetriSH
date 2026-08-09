/* ************************************************************************** */
/*                                                                            */
/*   recovery.c — boot / recovery (§6)                                        */
/*                                                                            */
/*   Replays the log front->back into the hash map (LWW: latest per username  */
/*   wins), then iterates the map once to seed the skip list. Also derives    */
/*   next_id as the highest player_id seen plus one.                          */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

typedef struct s_replay_ctx
{
	t_hashmap	*hm;		/* map being rebuilt; owns the heap player copies */
	t_player_id	max_id;		/* highest player_id seen so far across the replay */
	int			failed;		/* set on the first alloc failure to abort the boot */
}	t_replay_ctx;

// Static Functions
static void	replay_cb(const t_player *p, void *ctx);
static void	seed_skiplist(t_player *p, void *ctx);

/**
 * @brief Rebuild the in-memory state from the on-disk log (§6).
 *
 * Replays the log front->back into the hash map (latest record per username
 * wins), then iterates the map once to seed the skip list with the surviving
 * players. next_id is the highest player_id seen plus one, so the allocator
 * never reuses a recovered id. A torn tail is handled by log_replay itself.
 *
 * @param log The open log handle to replay.
 * @param hm The (empty) hash map to populate; takes ownership of the copies.
 * @param sl The (empty) skip list to seed from the recovered map.
 * @param out_next_id Receives the next id to allocate (max seen + 1, or 1).
 * @return DB_OK on a clean replay, DB_IO_ERROR on a read/alloc failure.
 */
t_db_result	recovery_run(t_dblog *log, t_hashmap *hm, t_skiplist *sl, t_player_id *out_next_id)
{
	t_replay_ctx	rc;
	t_db_result		r;

	if (!log || !hm || !sl || !out_next_id)
		return (DB_INVALID);
	rc.hm = hm;
	rc.max_id = 0;
	rc.failed = 0;
	r = log_replay(log, replay_cb, &rc);
	if (r != DB_OK)
		return (r);
	if (rc.failed)
		return (DB_IO_ERROR);
	hashmap_foreach(hm, seed_skiplist, sl);
	*out_next_id = rc.max_id + 1;
	return (DB_OK);
}

/**
 * @brief Replay one decoded record into the map, LWW, tracking the max id.
 *
 * log_replay hands a stack-local player, so the record is copied to the heap
 * before being stored — the map owns its entries. A put that displaces an older
 * record frees the loser; an alloc failure flags the context so the caller
 * aborts the boot rather than coming up with a partial dataset.
 *
 * A record whose username the wire cannot carry is dropped rather than
 * recovered. Such a row can only predate db_signup's charset check, and one
 * of them breaks a body that names every player at once — the leaderboard is
 * refused as malformed for everybody, not just for its owner. Its id is still
 * counted towards max_id: the row is gone, but reissuing the number it held
 * would be a second bug on top of the first.
 *
 * @param p The decoded record, owned by the replay loop (must be copied).
 * @param ctx The t_replay_ctx carrying the map, max id, and failure flag.
 */
static void	replay_cb(const t_player *p, void *ctx)
{
	t_replay_ctx	*rc;
	t_player		*copy;
	t_player		*old;

	rc = ctx;
	if (rc->failed)
		return ;
	if (!db_username_valid(p->username))
	{
		if (p->player_id > rc->max_id)
			rc->max_id = p->player_id;
		return ;
	}
	copy = malloc(sizeof(*copy));
	if (!copy)
		return ((void)(rc->failed = 1));
	*copy = *p;
	old = hashmap_put(rc->hm, copy);
	if (old == copy)
		return ((void)(free(copy), rc->failed = 1));
	free(old);
	if (p->player_id > rc->max_id)
		rc->max_id = p->player_id;
}

/**
 * @brief Insert one recovered player into the leaderboard skip list.
 *
 * The hash map already holds the single surviving copy per username, so the
 * skip list shares those pointers by reference (it never frees them). Called
 * once per live player via hashmap_foreach.
 *
 * @param p The player to index, owned by the hash map.
 * @param ctx The t_skiplist to insert into.
 */
static void	seed_skiplist(t_player *p, void *ctx)
{
	skiplist_insert((t_skiplist *)ctx, p);
}
