#include "tetrisd.h"

/*
** The three requests that act on the game session rather than on the falling
** piece: stopping its clock, throwing it away for a fresh one, and spending
** banked charge on a Gaiden ability.
**
** PAUSE and RESTART are Single-mode only, and for the same reason: in a room
** with anybody else in it, one player stopping or rewinding their own game
** while everyone else's runs on is not a pause or a restart, it is an
** advantage. ABILITY asks the mode a different question - a room with one
** player in it has no Target (docs/CONTEXT.md), so the twelve abilities that
** land on somebody else have nobody to land on.
*/

// Static Functions
static int	solo_only(t_request_context *ctx, t_server_room *server_room);
static int	equipped_character(t_request_context *ctx, t_item_id *out);
static int	ability_body(t_request_context *ctx, int *level, int *column);
static int	activate(t_request_context *ctx, t_server_room *server_room,
				const t_ability_def *def, int column);

/**
 * @brief PAUSE /room/<name>/player/<pid> - stop or restart this player's clock.
 *
 * Time spent paused is not owed back to the piece: resuming must not drop it
 * four rows to make up for the pause.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when the state changed, 409 when it was already there or the
 *         room is not Single, 400 on a body that is neither word.
 */
int	pause_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	char				token[16];
	int					status;
	int					paused;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	status = solo_only(ctx, server_room);
	if (status != 0)
		return (status);
	if (request_body_token(ctx, token, sizeof(token)) != 0)
		return (400);
	if (strcmp(token, "PAUSE") == 0)
		paused = 1;
	else if (strcmp(token, "RESUME") == 0)
		paused = 0;
	else
		return (400);
	if (!server_room_input(server_room, ctx->cli, INPUT_PAUSE, paused))
		return (request_refuse(ctx, "input-blocked"));
	return (200);
}

/**
 * @brief RESTART /room/<name>/player/<pid> - deal this player a fresh game.
 *
 * The abandoned game is not recorded. Restarting is the player deciding it
 * did not happen; what a room records is what a player finishes or forfeits,
 * and a discarded game is neither.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when a new game was dealt, 409 when the room is not Single or
 *         this connection holds no game.
 */
int	restart_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	int					status;

	(void)msg;
	ctx = context;
	status = request_input_target(ctx, &server_room);
	if (status != 0)
		return (status);
	status = solo_only(ctx, server_room);
	if (status != 0)
		return (status);
	if (!server_room_input(server_room, ctx->cli, INPUT_RESTART, 0))
		return (request_refuse(ctx, "input-blocked"));
	return (200);
}

/**
 * @brief ABILITY /room/<name>/player/<pid> - spend charge on a Gaiden power.
 *
 * The body names a level, never an ability. Which four abilities a level
 * selects from is decided by the character this player has equipped, and that
 * is read out of the store rather than taken from the request - it is a fact
 * about the account, not something a client may assert. The client's own
 * charge counter is never read either: it arrives in STATE and goes nowhere.
 *
 * @param msg The request (unused).
 * @param context The request context.
 * @return 200 when the ability was activated, 409 with the domain's reason
 *         when it was refused, 400 on a bad body.
 */
int	ability_handler(const t_htttp_message *msg, void *context)
{
	t_request_context	*ctx;
	t_server_room		*server_room;
	const t_ability_def	*def;
	t_item_id			character;
	int					ask[2];

	(void)msg;
	ctx = context;
	ask[0] = request_input_target(ctx, &server_room);
	if (ask[0] != 0)
		return (ask[0]);
	if (equipped_character(ctx, &character) != 0)
		return (500);
	if (ability_body(ctx, &ask[0], &ask[1]) != 0)
		return (400);
	def = ability_lookup(character, ask[0]);
	if (def == NULL)
		return (400);
	if (server_room_is_solo(server_room) && !ability_is_playable_solo(def))
		return (request_refuse(ctx, "no-target"));
	return (activate(ctx, server_room, def, ask[1]));
}

/**
 * @brief Refuses a request that only a one-player room can serve.
 *
 * @param ctx Request context, whose body receives the reason.
 * @param server_room Room the request addressed.
 * @return 0 when the room is Single, otherwise the status to answer.
 */
static int	solo_only(t_request_context *ctx, t_server_room *server_room)
{
	if (server_room_is_solo(server_room))
		return (0);
	return (request_refuse(ctx, "not-single"));
}

/**
 * @brief Reads the character this connection's player has equipped.
 *
 * @param ctx Request context naming the player.
 * @param out Receives the equipped character id.
 * @return 0 on success, -1 when the store has no such player.
 */
static int	equipped_character(t_request_context *ctx, t_item_id *out)
{
	t_player	player;

	if (db_get_player(ctx->srv->db, ctx->cli->player_id, &player) != DB_OK)
		return (-1);
	*out = player.current_equipped_character;
	return (0);
}

/**
 * @brief Reads an ABILITY body: a required level and an optional column.
 *
 * The column is the aim for the one aimable ability (Princess's Sol). An
 * absent one is -1, which the domain reads as "wherever the piece is".
 *
 * @param ctx Request context holding the body.
 * @param level Receives the requested ability level.
 * @param column Receives the aiming column, or -1 when the body omits it.
 * @return 0 on success, -1 when the level is missing or not a number.
 */
static int	ability_body(t_request_context *ctx, int *level, int *column)
{
	char	value[16];

	*column = -1;
	if (request_body_field(ctx, "level", value, sizeof(value)) == NULL)
		return (-1);
	*level = atoi(value);
	if (*level < 1 || *level > 4)
		return (-1);
	if (request_body_field(ctx, "column", value, sizeof(value)) != NULL)
		*column = atoi(value);
	return (0);
}

/**
 * @brief Applies one ability and turns the domain's verdict into an answer.
 *
 * The snapshot the activation owes is marked, not pushed: the tick owns the
 * outgoing STATE stream, here as everywhere else.
 *
 * @param ctx Request context.
 * @param server_room Room holding the game.
 * @param def Ability being activated.
 * @param column Aiming column, or -1 for the falling piece's own.
 * @return 200 when it was activated, 409 carrying the refusal's reason.
 */
static int	activate(t_request_context *ctx, t_server_room *server_room,
			const t_ability_def *def, int column)
{
	t_ability_verdict	verdict;

	verdict = game_ability(server_room_game_of(server_room, ctx->cli), def,
			column);
	server_room_mark_dirty(server_room, ctx->cli);
	if (verdict != ABILITY_ACTIVATED)
		return (request_refuse(ctx, ability_verdict_reason(verdict)));
	return (200);
}
