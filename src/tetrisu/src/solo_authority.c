#include "tetrisu.h"

/*
** Who owns the board.
**
** Solo runs against tetrisd when there is a tetrisd to run against, and
** against the local simulation when there is not. Both are real modes and
** both drive the same renderer, so the difference has to live in exactly one
** place or it lives in every call site - and a game loop that asked "am I
** online?" before each move would be one `if` away from doing both.
**
** So the loop no longer calls solo_game_apply_action or solo_game_update at
** all. It asks the authority, and the authority is either the server or the
** local rules. docs/tetrisu-local-to-tetrisd.md step 8 asks for exactly this:
** the local implementation kept as an explicitly selected offline mode rather
** than deleted, because it is what makes the client testable without a
** server and playable without one.
**
** What stays local either way is presentation - the countdown, the clear
** animation, the personal best, the danger tint. The server has no opinion
** about any of it, and the same document lists them as the client's.
*/

// Static Functions
static bool	online_update(t_solo_authority *authority, t_solo_game *game);
static void	fall_offline(t_solo_authority *authority, t_solo_game *game);

/**
 * @brief Opens Solo against tetrisd when there is a session, else locally.
 *
 * The session is the caller's, already connected and already signed in. This
 * does not sign anybody in: who the player is belongs to the sign-in screen,
 * and a game mode that quietly registered an account would be deciding
 * something it has no business deciding.
 *
 * A missing or unauthenticated session is not an error - it is the offline
 * mode. Either way there is a playable game when this returns, and
 * solo_authority_is_online says which.
 *
 * @param authority Authority to bring up.
 * @param net The app's session, or NULL to play offline.
 * @param game Game to initialise.
 * @param seed Seed for the seven-bag the offline rules deal from.
 */
void	solo_authority_open(t_solo_authority *authority, t_net_client *net,
		t_solo_game *game, uint32_t seed)
{
	memset(authority, 0, sizeof(*authority));
	solo_game_init(game, seed);
	if (net == NULL || net->state < NET_AUTHED)
		return ;
	authority->net = net;
	if (net_solo_start(net, NULL) != 0)
		return ;
	authority->online = true;
}

/**
 * @brief Reports whether the server is the one running this game.
 *
 * @param authority Authority to ask.
 * @return true when the board belongs to tetrisd.
 */
bool	solo_authority_is_online(const t_solo_authority *authority)
{
	return (authority != NULL && authority->online);
}

/**
 * @brief Closes whatever the authority opened.
 *
 * @param authority Authority to close.
 */
void	solo_authority_close(t_solo_authority *authority)
{
	if (authority == NULL || !authority->online)
		return ;
	net_solo_leave(authority->net);
	authority->online = false;
}

/**
 * @brief The descriptor to wait on beside the terminal, if there is one.
 *
 * @param authority Authority to ask.
 * @return The session socket, or -1 when playing offline.
 */
int	solo_authority_fd(const t_solo_authority *authority)
{
	if (authority == NULL || !authority->online)
		return (-1);
	return (net_fd(authority->net));
}

/**
 * @brief Applies one player action through whoever owns the board.
 *
 * Online, the action is a request and the board changes when the snapshot
 * that follows says it did - nothing is applied optimistically, because a
 * client that moved first would have to decide what to do when the server
 * disagreed.
 *
 * @param authority Authority in charge.
 * @param game View model, or the game itself when offline.
 * @param action The action the player asked for.
 * @return true when something the renderer shows may have changed.
 */
bool	solo_authority_action(t_solo_authority *authority, t_solo_game *game,
		t_solo_action action)
{
	t_net_result	result;

	if (!authority->online)
		return (solo_game_apply_action(game, action));
	if (net_solo_action(authority->net, action, &result) != 0)
	{
		fall_offline(authority, game);
		return (true);
	}
	return (false);
}

/**
 * @brief Spends charge on one ability through whoever owns the board.
 *
 * @param authority Authority in charge.
 * @param game View model whose feedback slot is written.
 * @param ability Ability level the player selected.
 * @return true when the HUD has something new to show.
 */
bool	solo_authority_ability(t_solo_authority *authority, t_solo_game *game,
		t_solo_ability ability)
{
	t_net_result	result;

	if (!authority->online)
		return (solo_game_activate_ability(game, ability)
			!= SOLO_ABILITY_RESULT_INVALID);
	if (net_solo_ability(authority->net, ability, &result) != 0)
	{
		fall_offline(authority, game);
		return (true);
	}
	net_solo_ability_feedback(&result, ability, game);
	return (true);
}

/**
 * @brief Toggles pause through whoever owns the board.
 *
 * @param authority Authority in charge.
 * @param game View model, or the game itself when offline.
 * @return true when something the renderer shows may have changed.
 */
bool	solo_authority_pause(t_solo_authority *authority, t_solo_game *game)
{
	t_net_result	result;

	if (!authority->online)
	{
		solo_game_toggle_pause(game);
		return (true);
	}
	if (net_solo_pause(authority->net, !game->paused, &result) != 0)
	{
		fall_offline(authority, game);
		return (true);
	}
	return (false);
}

/**
 * @brief Starts a fresh game through whoever owns the board.
 *
 * The personal best is the client's own record and survives either way; it is
 * kept in a file here, not in the server's store, because it is a fact about
 * this machine's player and not about an account.
 *
 * @param authority Authority in charge.
 * @param game View model, or the game itself when offline.
 * @param seed Seed for the offline seven-bag.
 * @return true when something the renderer shows may have changed.
 */
bool	solo_authority_restart(t_solo_authority *authority, t_solo_game *game,
		uint32_t seed)
{
	t_net_result	result;
	uint64_t		personal_best;

	personal_best = game->personal_best;
	if (!authority->online)
	{
		solo_game_init(game, seed);
		solo_game_set_personal_best(game, personal_best);
		solo_game_start_countdown(game);
		return (true);
	}
	if (net_solo_restart(authority->net, &result) != 0)
	{
		fall_offline(authority, game);
		return (true);
	}
	solo_game_set_personal_best(game, personal_best);
	return (true);
}

/**
 * @brief Advances the game by however much time has passed.
 *
 * Offline this runs the rules. Online it runs nothing: gravity, locking and
 * line clears belong to the server, and all this does is take whatever it has
 * already sent. The animation timers around them are advanced either way,
 * because they are the client's.
 *
 * @param authority Authority in charge.
 * @param game View model, or the game itself when offline.
 * @param elapsed_ms Milliseconds since the last turn of the loop.
 * @return true when the renderer has something new to draw.
 */
bool	solo_authority_update(t_solo_authority *authority, t_solo_game *game,
		int elapsed_ms)
{
	if (!authority->online)
		return (solo_game_update(game, elapsed_ms));
	return (online_update(authority, game));
}

/**
 * @brief Takes whatever the server has sent and puts it on the view model.
 *
 * @param authority Authority in charge.
 * @param game View model to overwrite.
 * @return true when a newer snapshot arrived.
 */
static bool	online_update(t_solo_authority *authority, t_solo_game *game)
{
	int	fresh;

	fresh = net_pump(authority->net);
	if (fresh < 0)
	{
		fall_offline(authority, game);
		return (true);
	}
	if (fresh == 0)
		return (false);
	return (net_solo_apply(authority->net, game));
}

/**
 * @brief Drops back to the local rules when the session is lost mid-game.
 *
 * The alternative is to end the game on a dropped connection, which throws
 * away a session the player was in the middle of for a reason that is not
 * theirs. The board carries on from the last snapshot the server sent, which
 * is the last position both sides agreed on.
 *
 * @param authority Authority losing its session.
 * @param game View model the local rules take over.
 */
static void	fall_offline(t_solo_authority *authority, t_solo_game *game)
{
	(void)game;
	net_disconnect(authority->net);
	authority->online = false;
	authority->lost = true;
}
