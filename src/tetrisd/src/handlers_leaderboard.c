#include "tetrisd.h"

/*
** Reading back what db_record_game wrote.
**
** Every finished or forfeited game is already recorded (ADR-0002), so the
** store has had a ranking since M1 - there was simply no way to ask for it,
** and tetrisu's Leaderboard screen answered "not served" against a live
** session while showing fixtures against a preview one. This is the route
** that closes it.
**
** It is a top-N read, not a page. The store indexes players in a skip list
** keyed by (score, id) precisely so the first N come out in order without a
** scan, and a client asking for rank 400 is asking a different question than
** the screen this serves.
*/

// Static Functions
static size_t	top_rows(t_server *srv, t_body_leaderboard_row *rows);

/**
 * @brief LEADERBOARD /leaderboard - the top players by recorded score.
 *
 * Authenticated like every other read: the ranking is not a secret, but a
 * connection that has not said who it is has no business asking tetrisd for
 * anything, and answering it would be the one route that disagreed.
 *
 * @param msg The request (unused; the path is read through the context).
 * @param context The request context.
 * @return 200 with the leaderboard body, 401 unauthenticated, 404 for another
 *         path, 500 when the body will not encode.
 */
int	leaderboard_handler(const t_htttp_message *msg, void *context)
{
	t_body_leaderboard_row	rows[TETRISD_LEADERBOARD_ROWS];
	t_request_context		*ctx;
	size_t					count;
	int						len;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (ctx->msg->path == NULL
		|| strcmp(ctx->msg->path, TETRISD_ROUTE_LEADERBOARD) != 0)
		return (404);
	count = top_rows(ctx->srv, rows);
	len = body_leaderboard_encode(rows, count, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief Reads the store's top players into the body codec's row shape.
 *
 * The rank is the position in the answer rather than a second query per row:
 * db_leaderboard returns them already ordered, so asking db_rank for each
 * would be the same walk done N more times and could disagree with the list
 * it was numbering if a game finished between the two reads.
 *
 * @param srv Server holding the store.
 * @param rows Buffer of TETRISD_LEADERBOARD_ROWS rows.
 * @return How many rows were filled.
 */
static size_t	top_rows(t_server *srv, t_body_leaderboard_row *rows)
{
	t_rank_entry	entries[TETRISD_LEADERBOARD_ROWS];
	size_t			count;
	size_t			i;

	count = 0;
	if (db_leaderboard(srv->db, entries, TETRISD_LEADERBOARD_ROWS,
			&count) != DB_OK)
		return (0);
	i = 0;
	while (i < count)
	{
		rows[i].rank = (int)i + 1;
		snprintf(rows[i].username, sizeof(rows[i].username), "%s",
			entries[i].username);
		rows[i].score = 0;
		if (entries[i].leaderboard_score > 0)
			rows[i].score = (uint64_t)entries[i].leaderboard_score;
		i++;
	}
	return (count);
}
