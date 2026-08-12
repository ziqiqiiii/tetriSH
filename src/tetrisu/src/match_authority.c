#include "tetrisu.h"

/*
** Who owns the boards a match is played on.
**
** The Solo sibling of this file is solo_authority.c and it exists for the
** same reason: a match runs against tetrisd when there is a tetrisd to run
** against and against the local fixture when there is not, both are real
** modes, both drive the same renderer, so the difference lives in exactly one
** place or it lives in every call site.
**
** The fixture opponent is kept rather than deleted, on the same terms
** docs/tetrisu-local-to-tetrisd.md keeps Solo's local rules: it is what makes
** the match screen testable without a server and demonstrable without one. It
** is a rehearsal, not a match, and match_authority_is_online is how anything
** that cares tells them apart.
**
** There is no countdown hold here. Solo stops the server's clock for its own
** 3-2-1 by sending PAUSE; a match cannot, because PAUSE is refused in a room
** with anybody else in it - one player stopping their own clock is an
** advantage - and does not need to, because the countdown is the room's and
** arrives in every snapshot.
*/

// Static Functions
static void	fall_offline(t_match_authority *authority,
				t_mp_match_state *state);

/**
 * @brief Opens a match against tetrisd when there is a session, else locally.
 *
 * The session is the caller's, already connected, signed in and seated: this
 * neither joins a room nor starts one. Who is playing and where belongs to the
 * waiting room, and a match screen that quietly took a seat would be deciding
 * something it has no business deciding.
 *
 * @param authority Authority to bring up.
 * @param net The app's session, or NULL to play the local fixture.
 * @param state Match model the authority drives.
 */
void	match_authority_open(t_match_authority *authority, t_net_client *net,
		t_mp_match_state *state)
{
	memset(authority, 0, sizeof(*authority));
	authority->local_slot = -1;
	if (net == NULL || net->state < NET_IN_ROOM
		|| net->play_path[0] == '\0' || state == NULL)
		return ;
	authority->net = net;
	authority->online = true;
	/*
	 * The fixture opponent is a preview and a live one is not. Leaving the
	 * seeded stack up would show the player a rival mid-game until the first
	 * snapshot replaced it, which reads as a match already in progress.
	 */
	solo_game_init(&state->opponent_game, 0);
	memset(state->opponents, 0, sizeof(state->opponents));
	snprintf(state->status, sizeof(state->status), "%s", "");
}

/**
 * @brief Reports whether the server is the one running this match.
 *
 * @param authority Authority to ask.
 * @return true when the boards belong to tetrisd.
 */
bool	match_authority_is_online(const t_match_authority *authority)
{
	return (authority != NULL && authority->online);
}

/**
 * @brief Gives up the room, ending the match the server is running.
 *
 * @param authority Authority to close.
 */
void	match_authority_close(t_match_authority *authority)
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
 * @return The session socket, or -1 when playing the local fixture.
 */
int	match_authority_fd(const t_match_authority *authority)
{
	if (authority == NULL || !authority->online)
		return (-1);
	return (net_fd(authority->net));
}

/**
 * @brief Reports whether a snapshot has already arrived and is unread.
 *
 * A request and the snapshot it causes travel the same socket, so the reply to
 * a move is often read with the next frame already behind it. The loop has to
 * know, or it waits out a poll interval holding the very frame the player is
 * waiting for - and in a match that frame is both boards.
 *
 * @param authority Authority to ask.
 * @return true when the next update has a snapshot to apply.
 */
bool	match_authority_pending(const t_match_authority *authority)
{
	if (authority == NULL || !authority->online)
		return (false);
	return (net_solo_pending(authority->net));
}

/**
 * @brief Applies one player action through whoever owns the boards.
 *
 * Online, the action is a request and the board changes when the snapshot
 * that follows says it did. Nothing is applied optimistically, because a
 * client that moved first would have to decide what to do when the server
 * disagreed - and in a match it would have to decide that while the other
 * player watched.
 *
 * @param authority Authority in charge.
 * @param state Match model, whose local game the fixture drives.
 * @param action The action the player asked for.
 * @return true when something the renderer shows may have changed.
 */
bool	match_authority_action(t_match_authority *authority,
		t_mp_match_state *state, t_solo_action action)
{
	if (authority->lost)
		return (false);
	if (!authority->online)
		return (solo_game_apply_action(&state->local_game, action));
	if (state->local_game.countdown_active)
		return (false);
	if (net_match_send_action(authority->net, action) != 0)
	{
		fall_offline(authority, state);
		return (true);
	}
	return (false);
}

/**
 * @brief Takes the next knockout the room has narrated, if there is one.
 *
 * The feed is the KO announcement's source rather than a second channel of its
 * own, because the server already narrates every knockout there - and it does
 * so on a lane built for exactly this: a room announcing ninety-eight of them
 * must not be able to fill a response FIFO and close a slow connection.
 *
 * One line per call, so a burst of eliminations is announced one card at a
 * time rather than as a stack of overlapping ones.
 *
 * @param authority Authority in charge.
 * @param out Receives the line's text.
 * @param size Size of out.
 * @return true when a knockout was taken, false when the feed has none new.
 */
bool	match_authority_knockout(t_match_authority *authority, char *out,
			size_t size)
{
	const t_body_chat	*line;
	size_t				index;

	if (!authority->online || authority->lost || authority->net == NULL)
		return (false);
	index = 0;
	while (index < net_chat_held(authority->net))
	{
		line = net_chat_at(authority->net, index);
		if (line != NULL && line->seq > authority->feed_seen
			&& line->system && strstr(line->text, "knocked out") != NULL)
		{
			authority->feed_seen = line->seq;
			snprintf(out, size, "%s", line->text);
			return (true);
		}
		if (line != NULL && line->seq > authority->feed_seen)
			authority->feed_seen = line->seq;
		index++;
	}
	return (false);
}

/**
 * @brief Declares a targeting mode to whoever owns the boards.
 *
 * Offline there is nobody to tell and the fixture has no targeting, so the
 * mode is simply the screen's - which is what it has always been, and is now
 * the only place it still is.
 *
 * Online it is send-and-report: the request is made, and a refusal leaves the
 * client's own mode where it was rather than pretending. What confirms the
 * mode took is the next arena, where the rivals it singles out arrive marked.
 *
 * @param authority Authority in charge.
 * @param state Match model holding the mode.
 * @param mode The mode the player selected.
 * @return true when the mode stands, false when the server refused it.
 */
bool	match_authority_target(t_match_authority *authority,
		t_mp_match_state *state, t_target_mode mode)
{
	t_net_result	result;

	if (authority->lost || !authority->online)
		return (true);
	if (net_match_set_target(authority->net, mode, &result) != 0)
	{
		fall_offline(authority, state);
		return (true);
	}
	return (result.status == 200);
}

/**
 * @brief Spends charge on one ability through whoever owns the boards.
 *
 * @param authority Authority in charge.
 * @param state Match model whose feedback slot is written.
 * @param ability Ability level the player selected.
 * @return true when the HUD has something new to show.
 */
bool	match_authority_ability(t_match_authority *authority,
		t_mp_match_state *state, t_solo_ability ability)
{
	t_net_result	result;

	if (authority->lost)
		return (false);
	if (!authority->online)
		return (solo_game_activate_ability(&state->local_game, ability)
			!= SOLO_ABILITY_RESULT_INVALID);
	if (state->local_game.countdown_active)
		return (false);
	if (net_match_ability(authority->net, ability, &result) != 0)
	{
		fall_offline(authority, state);
		return (true);
	}
	net_solo_ability_feedback(&result, ability, &state->local_game);
	return (true);
}

/**
 * @brief Advances the match by however much time has passed.
 *
 * Offline this runs the rules on both boards, which is the fixture. Online it
 * runs neither: gravity, locking, line clears, the countdown and the verdict
 * all belong to the server, and all this does is take whatever it has already
 * sent. The animation timers around them are advanced either way, because
 * they are the client's.
 *
 * @param authority Authority in charge.
 * @param state Match model to advance.
 * @param elapsed_ms Milliseconds since the last turn of the loop.
 * @return true when the renderer has something new to draw.
 */
bool	match_authority_update(t_match_authority *authority,
		t_mp_match_state *state, int elapsed_ms)
{
	bool	changed;
	int		fresh;

	if (authority->lost)
		return (false);
	if (!authority->online)
	{
		changed = solo_game_update(&state->local_game, elapsed_ms);
		return (solo_game_update(&state->opponent_game, elapsed_ms)
			|| changed);
	}
	changed = solo_game_update_presentation(&state->local_game, elapsed_ms);
	fresh = net_pump(authority->net);
	if (fresh < 0)
	{
		fall_offline(authority, state);
		return (true);
	}
	if (!net_solo_pending(authority->net))
		return (changed);
	return (net_match_apply(authority->net, state) || changed);
}

/**
 * @brief Reports a lost session and stops pretending there is a match.
 *
 * Solo drops back to the local rules when its session goes away, because the
 * game it was playing is still a game without a server. A match is not: the
 * opponent was the point of it, and carrying on against a fixture that had
 * quietly taken their place would be a worse answer than saying so. The
 * boards are left exactly as the last snapshot had them and the screen says
 * what happened.
 *
 * `lost` is what makes that true rather than merely intended. Clearing
 * `online` alone put every entry point back on its offline branch - gravity
 * resumed under the local rules, the keys started driving the board again,
 * and the fixture rival this screen had blanked on the way in stood up and
 * played on. So the flag is read by all three: a lost match does nothing at
 * all until the player leaves it.
 *
 * @param authority Authority losing its session.
 * @param state Match model to leave standing.
 */
static void	fall_offline(t_match_authority *authority,
		t_mp_match_state *state)
{
	net_disconnect(authority->net);
	authority->online = false;
	authority->lost = true;
	snprintf(state->status, sizeof(state->status),
		"CONNECTION LOST - THE MATCH IS OVER");
}
