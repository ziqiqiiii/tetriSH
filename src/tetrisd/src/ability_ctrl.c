#include "tetrisd.h"

/*
** The Gaiden ability catalogue: ownership, targeting and the HTTTP mapping.
** libtetrisbrain's charge.c owns the charge arithmetic and the pure board
** transforms, and names this file for the rest.
**
** Two rules decide every request, in this order:
**
**   1. The ability is (character, level), not level - level 1 is Fry for
**      Halloween and Cut for Wolf-man - so the level a client sends is read
**      against the character the store says it has equipped, never against
**      the request.
**   2. Single mode has no Target (docs/CONTEXT.md), so an ability landing on
**      somebody else is refused, not redirected at the player who paid.
**
** Charge is deducted only once an activation is accepted, so a refusal costs
** nothing (UC-14 ext 4a). The client's own charge counter is never read.
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
static t_ability_verdict	apply_targeted(t_game *g, t_game *target,
								const t_ability_def *def);
static t_game				*mirror_redirect(t_game *g, t_game *target);
static bool					apply_vampire(t_game *g, t_game *target);
static bool					apply_copy(t_game *g, const t_game *target);
static bool					apply_fry(t_game *g);
static bool					apply_mirurun(t_game *g);
static bool					apply_cut(t_game *g);
static int					highest_occupied_row(const t_board *b);
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
 * @brief Activates one ability, spending its charge only once it is accepted.
 *
 * @param g Game activating the ability.
 * @param target The other player's game, or NULL when there is none.
 * @param def Ability being activated.
 * @param argument Ability-specific parameter; Sol's aiming column, else 0.
 * @return What the client should be told happened.
 */
t_ability_verdict	game_ability(t_game *g, t_game *target,
					const t_ability_def *def, int argument)
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
	if (def->needs_target)
		verdict = apply_targeted(g, target, def);
	else
		verdict = apply_self(g, def, argument);
	if (verdict != ABILITY_ACTIVATED)
	{
		remember(g, def->level, false);
		return (verdict);
	}
	charge_deduct(&g->charge, def->level);
	remember(g, def->level, true);
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
 * @param g Game being transformed.
 * @param def Ability being applied.
 * @param argument Sol's aiming column, ignored by the others.
 * @return ACTIVATED when the board took it, BLOCKED when it could not.
 */
static t_ability_verdict	apply_self(t_game *g, const t_ability_def *def,
							int argument)
{
	bool	ok;

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
 * @brief Applies one ability that needed somebody to aim at.
 *
 * Abilities changing the caster's board apply at once; abilities changing the
 * Target's queue until its next piece lock.
 *
 * @param g Game activating the ability.
 * @param target The other player's game, or NULL when there is none.
 * @param def Ability being activated.
 * @return ACTIVATED when it stood, NO_TARGET when nobody was there, BLOCKED
 *         when the caster's own board would not take it.
 */
static t_ability_verdict	apply_targeted(t_game *g, t_game *target,
								const t_ability_def *def)
{
	if (target == NULL || !target->active)
		return (ABILITY_NO_TARGET);
	if (def->character_id == 3 && def->level == 2)
	{
		effect_apply(&g->effects, EFFECT_MIRROR);
		return (ABILITY_ACTIVATED);
	}
	if (def->character_id == 4 && def->level == 3)
	{
		effect_apply(&g->effects, EFFECT_PALS);
		g->pals_pieces = TETRISD_PALS_PIECES;
		return (ABILITY_ACTIVATED);
	}
	if (def->character_id == 1 && def->level == 3)
		return (apply_vampire(g, target) ? ABILITY_ACTIVATED : ABILITY_BLOCKED);
	if (def->character_id == 3 && def->level == 4)
		return (apply_copy(g, target) ? ABILITY_ACTIVATED : ABILITY_BLOCKED);
	target = mirror_redirect(g, target);
	if (def->character_id == 1 && def->level == 2)
		game_queue_ability(target, PENDING_EFFECT, (int)EFFECT_DARK);
	else if (def->character_id == 1 && def->level == 4)
		game_queue_ability(target, PENDING_BOMB, 0);
	else if (def->character_id == 2 && def->level == 2)
		game_queue_ability(target, PENDING_EFFECT, (int)EFFECT_INVERSION);
	else if (def->character_id == 2 && def->level == 3)
		game_queue_ability_garbage(target, TETRISD_PENTARIS_ROWS);
	else if (def->character_id == 2 && def->level == 4)
		game_queue_ability(target, PENDING_SIRTET, 0);
	else if (def->character_id == 3 && def->level == 3)
		game_queue_ability(target, PENDING_EFFECT, (int)EFFECT_PARALYSIS);
	else if (def->character_id == 4 && def->level == 2)
		game_queue_ability(target, PENDING_EFFECT, (int)EFFECT_NUE);
	else
		return (ABILITY_INVALID);
	return (ABILITY_ACTIVATED);
}

/**
 * @brief Sends an ability back at its sender when the Target holds a Mirror.
 *
 * Consumed on use, so one Mirror steals one ability.
 *
 * @param g The player who aimed it.
 * @param target The player it was aimed at.
 * @return Whichever of the two the effect should now be queued against.
 */
static t_game	*mirror_redirect(t_game *g, t_game *target)
{
	if (!target->effects.mirror_armed)
		return (target);
	effect_clear(&target->effects, EFFECT_MIRROR);
	return (g);
}

/**
 * @brief Halloween L3 (Vampire): takes the Target's stored charge.
 *
 * @param g Game receiving the charge.
 * @param target Game losing it.
 * @return true always; there is no board to refuse it.
 */
static bool	apply_vampire(t_game *g, t_game *target)
{
	charge_transfer(&target->charge, &g->charge);
	return (true);
}

/**
 * @brief Princess L4 (Copy): replaces the caster's field with the Target's.
 *
 * @param g Game whose board is replaced.
 * @param target Game whose board is copied.
 * @return true when the caster's piece survived the swap.
 */
static bool	apply_copy(t_game *g, const t_game *target)
{
	t_board	next;

	board_copy(&next, &target->board);
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	return (true);
}

/**
 * @brief Halloween L1 (Fry): fills the own bottom three rows, to burn later.
 *
 * The rows go in now and come out at the next lock, which is what game.c's
 * fry counter is for.
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
 * The top four rows *of the stack*, not of the board - which is why this
 * clears cells in place rather than calling board_cut_top.
 *
 * @param g Game to transform.
 * @return true when the transform was kept.
 */
static bool	apply_cut(t_game *g)
{
	t_board	next;
	t_cell	empty;
	int		row;
	int		stop;
	int		col;

	row = highest_occupied_row(&g->board);
	if (row >= BOARD_HEIGHT)
		return (false);
	stop = row + 4;
	if (stop > BOARD_HEIGHT)
		stop = BOARD_HEIGHT;
	board_copy(&next, &g->board);
	empty.type = CELL_EMPTY;
	empty.color = 0;
	while (row < stop)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			board_set(&next, col, row, empty);
			col++;
		}
		row++;
	}
	if (!piece_is_valid(&next, &g->piece))
		return (false);
	board_copy(&g->board, &next);
	return (true);
}

/**
 * @brief Finds the topmost row holding anything at all.
 *
 * @param b Board to scan.
 * @return That row, or BOARD_HEIGHT when the board is empty.
 */
static int	highest_occupied_row(const t_board *b)
{
	int	row;
	int	col;

	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (board_get(b, col, row).type != CELL_EMPTY)
				return (row);
			col++;
		}
		row++;
	}
	return (BOARD_HEIGHT);
}

/**
 * @brief Princess L1 (Sol): clears three adjacent columns off the own field.
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
 * @param g Game whose feedback slot is written.
 * @param level Ability level that was asked for.
 * @param accepted true when the activation stood.
 */
static void	remember(t_game *g, int level, bool accepted)
{
	g->last_ability.level = level;
	g->last_ability.accepted = accepted;
}
