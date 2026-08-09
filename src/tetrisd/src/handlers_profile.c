#include "tetrisd.h"

/*
** UC-20 ProfileView: everything the Settings and Marketplace screens read out
** of an account - the wallet they spend, the score and rank they display, and
** the inventory that decides which shelf tiles are owned and which one is
** equipped.
**
** It is one route rather than three because those facts are one row in the
** store, and answering them separately would let a client draw a wallet from
** one moment against an inventory from another. That is also why BUY and
** EQUIP answer with this same body: acting on the account and re-reading it
** are the same round trip, so the client never has to guess what the wallet
** is now.
*/

// Static Functions
static int	fill_profile(t_server *srv, t_player_id id, t_body_profile *out);
static void	copy_owned(uint32_t *dst, size_t *dst_count, const t_item_id *src,
				size_t count);

/**
 * @brief PROFILE /player/<pid> - the account behind the Settings screen.
 *
 * The subject rides in the path like it does on every input route, and must
 * be the player bound to this connection: a profile carries a wallet and an
 * inventory, so reading somebody else's is not a read a client is entitled
 * to make.
 *
 * @param msg The request (unused; the path is read through the context).
 * @param context The request context.
 * @return 200 with the profile body, 401 unauthenticated, 403 for another
 *         player's profile, 404 for another path, 500 when the store or the
 *         codec fails.
 */
int	profile_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	const char			*path;
	size_t				prefix_len;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	path = ctx->msg->path;
	prefix_len = strlen(TETRISD_ROUTE_PLAYER_PREFIX);
	if (path == NULL
		|| strncmp(path, TETRISD_ROUTE_PLAYER_PREFIX, prefix_len) != 0)
		return (404);
	if (request_player_id(path + prefix_len, NULL) != ctx->cli->player_id)
		return (403);
	return (request_profile_body(ctx));
}

/**
 * @brief Writes this connection's player as a ProfileView body.
 *
 * Shared with BUY and EQUIP, which answer with the account as it stands after
 * the change rather than with a bare status - a client that had to re-ask
 * would be drawing a stale wallet until the answer came back.
 *
 * @param ctx Request context whose body receives the profile.
 * @return 200 on success, 500 when the store or the codec fails.
 */
int	request_profile_body(t_request_context *ctx)
{
	t_body_profile	profile;
	int				len;

	if (fill_profile(ctx->srv, ctx->cli->player_id, &profile) != 0)
		return (500);
	len = body_profile_encode(&profile, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief Reads one player out of the store into the body codec's shape.
 *
 * Rank is a second read, unlike the leaderboard's positional numbering: this
 * player need not be in the top ten at all, so their standing has to be asked
 * for rather than counted off a list they may not appear on.
 *
 * @param srv Server holding the store.
 * @param id Player to read.
 * @param out Receives the profile.
 * @return 0 on success, -1 when the player or their rank cannot be read.
 */
static int	fill_profile(t_server *srv, t_player_id id, t_body_profile *out)
{
	t_player	player;
	size_t		rank;

	memset(out, 0, sizeof(*out));
	if (db_get_player(srv->db, id, &player) != DB_OK)
		return (-1);
	if (db_rank(srv->db, id, &rank) != DB_OK)
		return (-1);
	snprintf(out->username, sizeof(out->username), "%s", player.username);
	out->wallet = 0;
	if (player.wallet_points > 0)
		out->wallet = (uint64_t)player.wallet_points;
	out->score = 0;
	if (player.leaderboard_score > 0)
		out->score = (uint64_t)player.leaderboard_score;
	out->rank = (int)rank;
	out->equipped_character = player.current_equipped_character;
	out->equipped_theme = player.current_equipped_theme;
	copy_owned(out->owned_characters, &out->owned_character_count,
		player.owned_characters, player.owned_characters_count);
	copy_owned(out->owned_themes, &out->owned_theme_count,
		player.owned_themes, player.owned_themes_count);
	return (0);
}

/**
 * @brief Copies one owned-id list, clamped to what the body can carry.
 *
 * The store allows DB_MAX_OWNED entries and the body BODY_OWNED_MAX; they are
 * the same number today, and the clamp is what keeps that from becoming an
 * overrun if either moves.
 *
 * @param dst Destination id array of BODY_OWNED_MAX entries.
 * @param dst_count Receives how many ids were copied.
 * @param src Source id array.
 * @param count How many ids the source holds.
 */
static void	copy_owned(uint32_t *dst, size_t *dst_count, const t_item_id *src,
		size_t count)
{
	size_t	i;

	if (count > BODY_OWNED_MAX)
		count = BODY_OWNED_MAX;
	i = 0;
	while (i < count)
	{
		dst[i] = (uint32_t)src[i];
		i++;
	}
	*dst_count = count;
}
