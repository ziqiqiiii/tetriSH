#include "tetrisd.h"

/*
** The Marketplace half of the account: what is for sale, buying it, and
** choosing which of what you own to play as.
**
** Every one of these decisions is the store's, not the client's. tetrisd does
** not take a price from the request - it reads the catalogue - and it does
** not take ownership from the request either, because a client that could
** assert what it owns could equip a character it never bought and play with
** its abilities (the equipped character is what ability_ctrl.c reads to
** decide what an ABILITY level means).
**
** LIST /store is the catalogue alone. Who owns what rides in the profile, so
** the store front is the same answer for every player and the account is
** asked for separately.
*/

// Static Functions
static int	store_target(t_request_context *ctx, const char *path,
				t_item_kind *kind, t_item_id *id);
static int	item_exists(t_server *srv, t_item_kind kind, t_item_id id);
static int	buy_status(t_request_context *ctx, t_db_result res);
static int	equip_status(t_request_context *ctx, t_db_result res);
static size_t	character_rows(t_server *srv, t_body_catalogue *out);
static size_t	theme_rows(t_server *srv, t_body_catalogue *out);

/**
 * @brief BUY /store/character/<cid> or /store/theme/<tid> - UC-15 / UC-16.
 *
 * Affordability and ownership are not checked here: db_buy_* enforces both
 * atomically under its own write lock and reports which one refused, so a
 * check on this side could only disagree with the decision that counts.
 *
 * A purchase answers with the account as it stands afterwards, so the wallet
 * the screen redraws is the one the store actually holds.
 *
 * @param msg The request (unused; the path is read through the context).
 * @param context The request context.
 * @return 200 with the updated profile, 401, 403 insufficient, 404 no such
 *         item, 409 inventory full, 429 too fast, 500 on a store failure.
 */
int	buy_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_item_kind			kind;
	t_item_id			id;
	t_db_result			res;
	int					status;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (!rate_limit_take_token(ctx->cli))
		return (429);
	status = store_target(ctx, TETRISD_ROUTE_STORE_PREFIX, &kind, &id);
	if (status != 0)
		return (status);
	if (kind == ITEM_CHARACTER)
		res = db_buy_character(ctx->srv->db, ctx->cli->player_id, id);
	else
		res = db_buy_theme(ctx->srv->db, ctx->cli->player_id, id);
	if (res == DB_OK)
		logger_emit(&ctx->srv->log, COREIPC_LOG_INFO,
			"player %llu bought %s %" PRIu32,
			(unsigned long long)ctx->cli->player_id,
			kind == ITEM_CHARACTER ? "character" : "theme", id);
	return (buy_status(ctx, res));
}

/**
 * @brief EQUIP /player/<pid>/character/<cid> or /theme/<tid> - UC-18 / UC-19.
 *
 * The subject rides in the path like it does on the input routes, so the
 * request says whose loadout it is changing and the server checks that it is
 * this connection's.
 *
 * Equipping is refused for anything the player does not own, which is the one
 * rule that stops a client granting itself a character's abilities by naming
 * it. db_equip_* holds that rule; this handler only reports it.
 *
 * @param msg The request (unused; the path is read through the context).
 * @param context The request context.
 * @return 200 with the updated profile, 401, 403 not owned or another
 *         player, 404 no such item, 429 too fast, 500 on a store failure.
 */
int	equip_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_item_kind			kind;
	t_item_id			id;
	t_db_result			res;
	int					status;

	(void)msg;
	ctx = context;
	if (!request_is_authorised(ctx))
		return (401);
	if (!rate_limit_take_token(ctx->cli))
		return (429);
	status = store_target(ctx, TETRISD_ROUTE_PLAYER_PREFIX, &kind, &id);
	if (status != 0)
		return (status);
	if (kind == ITEM_CHARACTER)
		res = db_equip_character(ctx->srv->db, ctx->cli->player_id, id);
	else
		res = db_equip_theme(ctx->srv->db, ctx->cli->player_id, id);
	return (equip_status(ctx, res));
}

/**
 * @brief LIST /store - the catalogue, prices included.
 *
 * The client cannot hold its own copy of this: the prices and the roster live
 * in the store's config files, and a client that guessed them would offer a
 * player a purchase the server then refuses at a different price.
 *
 * Reached through list_handler rather than a method of its own, because "list
 * the collection at this path" is what LIST already means for /rooms.
 *
 * @param ctx Request context, already authorised by list_handler.
 * @return 200 with the catalogue body, 500 when the store or codec fails.
 */
int	store_list_catalogue(t_request_context *ctx)
{
	t_body_catalogue	store;
	int					len;

	memset(&store, 0, sizeof(store));
	store.character_count = character_rows(ctx->srv, &store);
	store.theme_count = theme_rows(ctx->srv, &store);
	len = body_catalogue_encode(&store, ctx->body, sizeof(ctx->body));
	if (len < 0)
		return (500);
	ctx->body_len = (size_t)len;
	return (200);
}

/**
 * @brief Parses which catalogue item a store path addresses.
 *
 * BUY addresses the collection (`/store/character/<cid>`) and EQUIP addresses
 * the player (`/player/<pid>/character/<cid>`), so the two differ only in
 * what stands before the kind - which is what the prefix argument names. The
 * player id is checked here rather than by the caller, so a path that named
 * somebody else can never reach a store call.
 *
 * @param ctx Request context.
 * @param prefix The route prefix this method addresses.
 * @param kind Receives which catalogue the path names.
 * @param id Receives the item id.
 * @return 0 when the request may proceed, otherwise the status to answer.
 */
static int	store_target(t_request_context *ctx, const char *prefix,
		t_item_kind *kind, t_item_id *id)
{
	const char	*path;
	const char	*rest;
	uint64_t	value;

	path = ctx->msg->path;
	if (path == NULL || strncmp(path, prefix, strlen(prefix)) != 0)
		return (404);
	path += strlen(prefix);
	if (strcmp(prefix, TETRISD_ROUTE_PLAYER_PREFIX) == 0)
	{
		if (request_player_id(path, &rest) != ctx->cli->player_id)
			return (403);
		if (*rest != '/')
			return (404);
		path = rest + 1;
	}
	if (strncmp(path, TETRISD_SEGMENT_CHARACTER,
			strlen(TETRISD_SEGMENT_CHARACTER)) == 0)
		*kind = ITEM_CHARACTER;
	else if (strncmp(path, TETRISD_SEGMENT_THEME,
			strlen(TETRISD_SEGMENT_THEME)) == 0)
		*kind = ITEM_THEME;
	else
		return (404);
	if (*kind == ITEM_CHARACTER)
		path += strlen(TETRISD_SEGMENT_CHARACTER);
	else
		path += strlen(TETRISD_SEGMENT_THEME);
	value = (uint64_t)request_player_id(path, &rest);
	if (value == 0 || *rest != '\0' || value > UINT32_MAX)
		return (404);
	*id = (t_item_id)value;
	if (!item_exists(ctx->srv, *kind, *id))
		return (404);
	return (0);
}

/**
 * @brief Reports whether the catalogue holds one item.
 *
 * Checked before the store call so an unknown id is always 404. db_equip_*
 * would otherwise answer DB_NOT_OWNED for an item that does not exist, which
 * reads to a client as "buy it first" for something it never can.
 *
 * @param srv Server holding the store.
 * @param kind Which catalogue to look in.
 * @param id The item id.
 * @return 1 when the item exists, 0 otherwise.
 */
static int	item_exists(t_server *srv, t_item_kind kind, t_item_id id)
{
	if (kind == ITEM_CHARACTER)
		return (db_get_character(srv->db, id) != NULL);
	return (db_get_theme(srv->db, id) != NULL);
}

/**
 * @brief Maps a purchase outcome onto its status and reason.
 *
 * Already owning it is not a refusal (UC-15): the player asked for a state
 * the account is already in, so it answers 200 with the profile like a
 * successful purchase, and the owned list in that body is what tells the
 * screen the tile is theirs.
 *
 * @param ctx Request context, whose body receives the profile or the reason.
 * @param res Result from db_buy_character / db_buy_theme.
 * @return The status to answer with.
 */
static int	buy_status(t_request_context *ctx, t_db_result res)
{
	if (res == DB_OK || res == DB_EXISTS)
		return (request_profile_body(ctx));
	if (res == DB_INSUFFICIENT)
	{
		request_body_printf(ctx, "reason insufficient-funds\n");
		return (403);
	}
	if (res == DB_FULL)
		return (request_refuse(ctx, "inventory-full"));
	if (res == DB_NOT_FOUND)
		return (404);
	return (500);
}

/**
 * @brief Maps an equip outcome onto its status and reason.
 *
 * @param ctx Request context, whose body receives the profile or the reason.
 * @param res Result from db_equip_character / db_equip_theme.
 * @return The status to answer with.
 */
static int	equip_status(t_request_context *ctx, t_db_result res)
{
	if (res == DB_OK)
		return (request_profile_body(ctx));
	if (res == DB_NOT_OWNED)
	{
		request_body_printf(ctx, "reason not-owned\n");
		return (403);
	}
	if (res == DB_NOT_FOUND)
		return (404);
	return (500);
}

/**
 * @brief Reads the character catalogue into the body codec's row shape.
 *
 * @param srv Server holding the store.
 * @param out Catalogue body whose character rows are filled.
 * @return How many rows were filled.
 */
static size_t	character_rows(t_server *srv, t_body_catalogue *out)
{
	t_character	rows[BODY_CATALOGUE_MAX];
	size_t		count;
	size_t		i;

	count = 0;
	if (db_characters(srv->db, rows, BODY_CATALOGUE_MAX, &count) != DB_OK)
		return (0);
	i = 0;
	while (i < count)
	{
		out->characters[i].id = (uint32_t)rows[i].character_id;
		out->characters[i].price = 0;
		if (rows[i].cost_points > 0)
			out->characters[i].price = (uint64_t)rows[i].cost_points;
		snprintf(out->characters[i].name, BODY_ITEM_NAME_MAX, "%s",
			rows[i].name);
		i++;
	}
	return (count);
}

/**
 * @brief Reads the theme catalogue into the body codec's row shape.
 *
 * @param srv Server holding the store.
 * @param out Catalogue body whose theme rows are filled.
 * @return How many rows were filled.
 */
static size_t	theme_rows(t_server *srv, t_body_catalogue *out)
{
	t_theme	rows[BODY_CATALOGUE_MAX];
	size_t	count;
	size_t	i;

	count = 0;
	if (db_themes(srv->db, rows, BODY_CATALOGUE_MAX, &count) != DB_OK)
		return (0);
	i = 0;
	while (i < count)
	{
		out->themes[i].id = (uint32_t)rows[i].theme_id;
		out->themes[i].price = 0;
		if (rows[i].cost_points > 0)
			out->themes[i].price = (uint64_t)rows[i].cost_points;
		snprintf(out->themes[i].name, BODY_ITEM_NAME_MAX, "%s", rows[i].name);
		i++;
	}
	return (count);
}
