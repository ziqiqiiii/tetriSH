#include "tetrisd.h"

/*
** The Gaiden ability catalogue and what tetrisd is allowed to do with it.
** libtetrisbrain's charge.c names this file: it owns the charge arithmetic
** and the pure board transforms, and says ownership, targeting and the HTTTP
** mapping live here.
**
** Two rules decide every request, in this order:
**
**   1. The ability is (character, level), not level. Level 1 is Fry for
**      Halloween and Cut for Wolf-man, so the level a client sends means
**      nothing until it is read against the character that client has
**      equipped - a fact this file takes from the store, never from the
**      request.
**   2. Single mode has no Target (docs/CONTEXT.md), so an ability that lands
**      on somebody else has nobody to land on. It is refused, not redirected
**      at the player who paid for it.
**
** Charge is deducted only once an activation has actually been accepted, so
** a refusal costs nothing (UC-14 ext 4a). The client's own charge counter is
** never read - it arrives in STATE and goes nowhere else.
*/

// Static Variables

/*
** Character ids are libmacminidb's: 1 Halloween, 2 Mirurun, 3 Princess,
** 4 Wolf-man (lib/libmacminidb/config/characters.cfg). Ability text is
** docs/themes.md, which is the source of truth for it.
**
** Fry is the one entry where reading the text carefully changes the answer.
** It fills the player's *own* bottom three rows and burns them on the next
** lock; sending those burned lines on is the part that wants a Target, and
** libtetrisbrain's effects.c says as much - "forwarding Fry's burned rows"
** is listed as tetrisd's business, separate from the effect itself. So it is
** playable alone, with the send simply going nowhere. Every character
** therefore has a level 1 a one-player room can serve, which is the least a
** default account should be able to do with the meter it is filling.
*/
static const t_ability_def	g_abilities[] = {
	{1, 1, "Fry", false},
	{1, 2, "Dark", true},
	{1, 3, "Vampire", true},
	{1, 4, "Bomb", true},
	{2, 1, "Mirurun", false},
	{2, 2, "Inversion", true},
	{2, 3, "Pentaris", true},
	{2, 4, "Sirtet", true},
	{3, 1, "Sol", false},
	{3, 2, "Mirror", true},
	{3, 3, "Paralysis", true},
	{3, 4, "Copy", true},
	{4, 1, "Cut", false},
	{4, 2, "Nue", true},
	{4, 3, "Pals", true},
	{4, 4, "Thwack", false}
};

// Static Functions
static t_ability_verdict	apply_self(t_game *g, const t_ability_def *def,
								int argument);
static bool					apply_fry(t_game *g);
static bool					apply_mirurun(t_game *g);
static bool					apply_cut(t_game *g);
static bool					apply_sol(t_game *g, int argument);
static void					remember(t_game *g, int level, bool accepted);

/**
 * @brief Finds one ability by the character offering it and its level.
 *
 * @param character_id Character the player has equipped.
 * @param level Ability level, 1 to 4.
 * @return The definition, or NULL when no character offers that level.
 */
const t_ability_def	*ability_lookup(t_item_id character_id, int level)
{
	size_t	i;

	i = 0;
	while (i < sizeof(g_abilities) / sizeof(g_abilities[0]))
	{
		if (g_abilities[i].character_id == character_id
			&& g_abilities[i].level == level)
			return (&g_abilities[i]);
		i++;
	}
	return (NULL);
}

/**
 * @brief Reports whether an ability can be used in a room with no Target.
 *
 * @param def Ability being considered.
 * @return true when the ability only ever touches its own player's board.
 */
bool	ability_is_playable_solo(const t_ability_def *def)
{
	return (def != NULL && !def->needs_target);
}

/**
 * @brief Activates one ability against its own player's game.
 *
 * The transform is tried on a copy first and only kept when the falling piece
 * survives it, so an ability that would leave the piece inside the stack is
 * refused whole rather than half-applied. Charge is spent after that check,
 * never before it.
 *
 * @param g Game activating the ability.
 * @param def Ability being activated.
 * @param argument Ability-specific parameter; Sol's aiming column, else 0.
 * @return What the client should be told happened.
 */
t_ability_verdict	game_ability(t_game *g, const t_ability_def *def,
					int argument)
{
	t_ability_verdict	verdict;

	if (g == NULL || def == NULL)
		return (ABILITY_INVALID);
	if (!g->active || g->paused)
	{
		remember(g, def->level, false);
		return (ABILITY_UNAVAILABLE);
	}
	if (!charge_can_afford(&g->charge, def->level))
	{
		remember(g, def->level, false);
		return (ABILITY_NO_CHARGE);
	}
	verdict = apply_self(g, def, argument);
	if (verdict != ABILITY_ACTIVATED)
	{
		remember(g, def->level, false);
		return (verdict);
	}
	charge_deduct(&g->charge, def->level);
	remember(g, def->level, true);
	g->seq++;
	return (ABILITY_ACTIVATED);
}

/**
 * @brief Names a verdict for the refusal body, so a client can act on it.
 *
 * @param verdict Verdict to name.
 * @return Short machine-readable reason, never NULL.
 */
const char	*ability_verdict_reason(t_ability_verdict verdict)
{
	if (verdict == ABILITY_NO_CHARGE)
		return ("no-charge");
	if (verdict == ABILITY_NO_TARGET)
		return ("no-target");
	if (verdict == ABILITY_BLOCKED)
		return ("ability-blocked");
	if (verdict == ABILITY_UNAVAILABLE)
		return ("ability-unavailable");
	return ("ability-invalid");
}

/**
 * @brief Applies whichever self-affecting transform this ability is.
 *
 * Only the four abilities a one-player room can serve reach here; the
 * targeted twelve are turned away before any charge is looked at.
 *
 * @param g Game being transformed.
 * @param def Ability being applied.
 * @param argument Sol's aiming column, ignored by the others.
 * @return ACTIVATED when the board took it, BLOCKED when it could not.
 */
static t_ability_verdict	apply_self(t_game *g, const t_ability_def *def,
							int argument)
{
	bool	ok;

	if (def->needs_target)
		return (ABILITY_NO_TARGET);
	if (def->character_id == 1 && def->level == 1)
		ok = apply_fry(g);
	else if (def->character_id == 2 && def->level == 1)
		ok = apply_mirurun(g);
	else if (def->character_id == 3 && def->level == 1)
		ok = apply_sol(g, argument);
	else if (def->character_id == 4 && def->level == 1)
		ok = apply_cut(g);
	else if (def->character_id == 4 && def->level == 4)
	{
		effect_apply(&g->effects, EFFECT_THWACK);
		ok = true;
	}
	else
		return (ABILITY_INVALID);
	if (!ok)
		return (ABILITY_BLOCKED);
	return (ABILITY_ACTIVATED);
}

/**
 * @brief Halloween L1 (Fry): fills the own bottom three rows, to burn later.
 *
 * The rows go in now and come out at the next lock, which is what game.c's
 * fry counter is for. The hole rides on the sequence number rather than on a
 * random source: libtetrisbrain owns no RNG on purpose, and a hole that moves
 * predictably with the game's own clock is fair without inventing one.
 *
 * @param g Game to transform.
 * @return true when the transform was kept.
 */
static bool	apply_fry(t_game *g)
{
	t_board	next;

	board_copy(&next, &g->board);
	board_fill_rows(&next, 3, (int)(g->seq % BOARD_WIDTH));
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	effect_apply(&g->effects, EFFECT_FRY);
	return (true);
}

/**
 * @brief Mirurun L1: removes the player's own bottom four rows.
 *
 * @param g Game to transform.
 * @return true when the transform was kept.
 */
static bool	apply_mirurun(t_game *g)
{
	t_board	next;

	board_copy(&next, &g->board);
	board_cut_bottom(&next, 4);
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	return (true);
}

/**
 * @brief Wolf-man L1 (Cut): clears the player's own top four rows.
 *
 * @param g Game to transform.
 * @return true when the transform was kept.
 */
static bool	apply_cut(t_game *g)
{
	t_board	next;

	board_copy(&next, &g->board);
	board_cut_top(&next, 4);
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	return (true);
}

/**
 * @brief Princess L1 (Sol): clears three adjacent columns off the own field.
 *
 * Sol is aimable, so the request carries the middle column; the three-second
 * auto-fire the original describes is the client's aiming affordance, and by
 * the time a request arrives the aim has already been taken. An out-of-range
 * or absent column falls back to the column the falling piece is over, which
 * is where an unaimed shot would go.
 *
 * @param g Game to transform.
 * @param argument Middle column to aim at, or -1 for the piece's own column.
 * @return true when the transform was kept.
 */
static bool	apply_sol(t_game *g, int argument)
{
	t_board	next;
	int		middle;

	middle = argument;
	if (middle < 0 || middle >= BOARD_WIDTH)
		middle = g->piece.col;
	if (middle < 1)
		middle = 1;
	if (middle > BOARD_WIDTH - 2)
		middle = BOARD_WIDTH - 2;
	board_copy(&next, &g->board);
	board_delete_columns(&next, middle - 1, middle + 1);
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	return (true);
}

/**
 * @brief Records the last activation so the next STATE carries its verdict.
 *
 * The client shows a short acknowledgement for an ability it asked for, and
 * it has to come from the server: the response says the request was received,
 * the snapshot says what the board did about it.
 *
 * @param g Game whose feedback slot is written.
 * @param level Ability level that was asked for.
 * @param accepted true when the activation stood.
 */
static void	remember(t_game *g, int level, bool accepted)
{
	g->last_ability.level = level;
	g->last_ability.accepted = accepted;
}
