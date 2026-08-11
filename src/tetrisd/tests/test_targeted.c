/* ************************************************************************** */
/*                                                                            */
/*   test_targeted.c - the eleven abilities that need somebody to aim at      */
/*                                                                            */
/*   They divide on one question, and it is which board *changes* rather      */
/*   than which board is read. Mirror, Pals, Vampire and Copy land on the     */
/*   player who used them and are applied at once; Dark, Bomb, Inversion,     */
/*   Pentaris, Sirtet, Paralysis and Nue land on the Target and wait for      */
/*   that player's next piece lock.                                          */
/*                                                                            */
/*   Waiting is the rule worth testing hardest. A board transform arriving    */
/*   under an active piece can leave the piece inside the stack, and a        */
/*   status effect arriving mid-piece would spend one of the pieces it is     */
/*   counted in before it began - so "not yet" is as much of the behaviour    */
/*   as "eventually".                                                        */
/*                                                                            */
/*   In-process, like test_clearing.c and test_garbage.c: what is under test  */
/*   is when an effect arrives, not how the charge to send it was earned.     */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_a_targeted_effect_waits_for_the_lock(void);
static void	test_pentaris_sends_five_rows(void);
static void	test_sirtet_inverts_at_the_lock(void);
static void	test_bomb_destroys_scattered_cells(void);
static void	test_vampire_moves_the_charge_at_once(void);
static void	test_copy_takes_the_targets_field(void);
static void	test_copy_is_refused_when_the_piece_would_not_fit(void);
static void	test_mirror_sends_the_next_ability_back(void);
static void	test_pals_and_mirror_land_on_their_caster(void);
static void	test_nobody_to_aim_at_is_refused(void);
static void	test_a_full_queue_keeps_the_newest(void);
static void	test_dark_blinds_for_four_pieces(void);
static void	test_pals_turns_ordinary_garbage_into_a_gift(void);
static void	test_fry_sends_its_burned_rows_on(void);

static void	pair(t_game *sender, t_game *target);
static void	arm(t_game *g);
static const t_ability_def	*ability(t_item_id character, int level);
static const t_ability_def	*ability_solo(t_item_id character, int level);
static int	filled_cells(const t_game *g);

int	main(void)
{
	test_a_targeted_effect_waits_for_the_lock();
	test_pentaris_sends_five_rows();
	test_sirtet_inverts_at_the_lock();
	test_bomb_destroys_scattered_cells();
	test_vampire_moves_the_charge_at_once();
	test_copy_takes_the_targets_field();
	test_copy_is_refused_when_the_piece_would_not_fit();
	test_mirror_sends_the_next_ability_back();
	test_pals_and_mirror_land_on_their_caster();
	test_nobody_to_aim_at_is_refused();
	test_a_full_queue_keeps_the_newest();
	test_dark_blinds_for_four_pieces();
	test_pals_turns_ordinary_garbage_into_a_gift();
	test_fry_sends_its_burned_rows_on();
	return (0);
}

/*
** Paralysis is queued and does nothing until the Target locks a piece. Both
** halves matter: an effect counted in pieces that took hold mid-piece would
** be a piece short before it started, and one that never took hold at all
** would be charge spent on nothing.
*/
static void	test_a_targeted_effect_waits_for_the_lock(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(game_ability(&sender, &target, ability(3, 3), 0)
		== ABILITY_ACTIVATED);
	assert(target.pending_count == 1);
	assert(!effect_rotation_blocked(&target.effects));
	assert(game_rotate(&target, 1) || true);
	assert(game_drop(&target, true));
	assert(target.pending_count == 0);
	assert(effect_rotation_blocked(&target.effects));
	printf("PASS test_a_targeted_effect_waits_for_the_lock\n");
}

/*
** Pentaris is garbage with a name, so it rides the queue garbage already
** rides - counted on the wire from the moment it is sent, landing at the
** Target's next lock.
*/
static void	test_pentaris_sends_five_rows(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(game_ability(&sender, &target, ability(2, 3), 0)
		== ABILITY_ACTIVATED);
	/* The ability lane, not the ordinary one: Pals must not absorb it. */
	assert(target.pending_ability_garbage == TETRISD_PENTARIS_ROWS);
	assert(target.pending_garbage == 0);
	assert(filled_cells(&target) == 0);
	assert(game_drop(&target, true));
	assert(target.pending_ability_garbage == 0);
	assert(filled_cells(&target)
		>= TETRISD_PENTARIS_ROWS * (BOARD_WIDTH - 1));
	printf("PASS test_pentaris_sends_five_rows\n");
}

/*
** Sirtet turns the Target's field inside out. It waits, like every other
** board transform aimed at somebody else: inverting a board under a falling
** piece is the exact case that can leave the piece inside the stack.
*/
static void	test_sirtet_inverts_at_the_lock(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	arm(&target);
	before = filled_cells(&target);
	assert(game_ability(&sender, &target, ability(2, 4), 0)
		== ABILITY_ACTIVATED);
	assert(filled_cells(&target) == before);
	assert(game_drop(&target, true));
	assert(filled_cells(&target) != before);
	printf("PASS test_sirtet_inverts_at_the_lock\n");
}

/*
** Bomb scatters. It is asserted as "fewer cells than before, some cells
** still there" rather than against particular coordinates: the scatter walks
** from the game's own counters, and pinning the coordinates would be pinning
** the arithmetic rather than the behaviour.
*/
static void	test_bomb_destroys_scattered_cells(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	arm(&target);
	before = filled_cells(&target);
	assert(before > TETRISD_BOMB_CELLS);
	assert(game_ability(&sender, &target, ability(1, 4), 0)
		== ABILITY_ACTIVATED);
	assert(filled_cells(&target) == before);
	assert(game_drop(&target, true));
	assert(filled_cells(&target) < before);
	printf("PASS test_bomb_destroys_scattered_cells\n");
}

/*
** Vampire lands on its caster, so it does not wait: there is no second board
** to be surprised by it. The charge moves rather than being copied, so the
** total in the room is unchanged - which is what makes it a theft.
*/
static void	test_vampire_moves_the_charge_at_once(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(target.charge.charges == 8);
	assert(game_ability(&sender, &target, ability(1, 3), 0)
		== ABILITY_ACTIVATED);
	assert(target.charge.charges == 0);
	assert(sender.charge.charges > 0);
	assert(target.pending_count == 0);
	printf("PASS test_vampire_moves_the_charge_at_once\n");
}

/*
** Copy reads the Target and writes the caster, so it lands at once - and it
** is the one ability that changes the caster's board out of somebody else's.
*/
static void	test_copy_takes_the_targets_field(void)
{
	t_game	sender;
	t_game	target;
	t_cell	block;

	pair(&sender, &target);
	block.type = CELL_FILLED;
	block.color = 2;
	board_set(&target.board, 3, BOARD_HEIGHT - 1, block);
	assert(filled_cells(&sender) == 0);
	assert(game_ability(&sender, &target, ability(3, 4), 0)
		== ABILITY_ACTIVATED);
	assert(filled_cells(&sender) == 1);
	assert(board_get(&sender.board, 3, BOARD_HEIGHT - 1).type == CELL_FILLED);
	printf("PASS test_copy_takes_the_targets_field\n");
}

/*
** And because it lands at once, it keeps the "try it on a copy" discipline
** the self-affecting abilities use: a Target whose stack reaches the caster's
** falling piece would otherwise arrive underneath it.
*/
static void	test_copy_is_refused_when_the_piece_would_not_fit(void)
{
	t_game	sender;
	t_game	target;
	t_cell	block;
	int		row;
	int		col;

	pair(&sender, &target);
	block.type = CELL_FILLED;
	block.color = 2;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			board_set(&target.board, col, row, block);
			col++;
		}
		row++;
	}
	assert(game_ability(&sender, &target, ability(3, 4), 0) == ABILITY_BLOCKED);
	assert(filled_cells(&sender) == 0);
	assert(sender.charge.charges == 8);
	printf("PASS test_copy_is_refused_when_the_piece_would_not_fit\n");
}

/*
** Mirror steals the next ability aimed at its holder, and the theft happens
** when the ability is aimed rather than when it lands. That ordering is the
** whole hazard: a queued effect can be three effects behind by the time it
** arrives, so checking at the landing would steal the wrong one.
**
** Reflecting is not refusing. The sender paid for it and it still happens -
** to them.
*/
static void	test_mirror_sends_the_next_ability_back(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(game_ability(&target, &sender, ability(3, 2), 0)
		== ABILITY_ACTIVATED);
	assert(target.effects.mirror_armed);
	assert(game_ability(&sender, &target, ability(3, 3), 0)
		== ABILITY_ACTIVATED);
	assert(target.pending_count == 0);
	assert(sender.pending_count == 1);
	assert(!target.effects.mirror_armed);
	/* One Mirror, one theft: the next one through lands where it was aimed. */
	while (sender.charge.charges < 8)
		charge_on_clear(&sender.charge, 4);
	assert(game_ability(&sender, &target, ability(3, 3), 0)
		== ABILITY_ACTIVATED);
	assert(target.pending_count == 1);
	printf("PASS test_mirror_sends_the_next_ability_back\n");
}

/*
** Both are self-affecting abilities that still need somebody in the room:
** there is nothing to steal and no incoming garbage in a room of one, which
** is what needs_target means for them.
*/
static void	test_pals_and_mirror_land_on_their_caster(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(game_ability(&sender, &target, ability(4, 3), 0)
		== ABILITY_ACTIVATED);
	assert(sender.effects.pals);
	assert(!target.effects.pals);
	assert(target.pending_count == 0);
	printf("PASS test_pals_and_mirror_land_on_their_caster\n");
}

/*
** No Target and a Target who has topped out are the same answer, and it costs
** nothing: charge is deducted only once an activation has been accepted
** (UC-14 ext 4a).
*/
static void	test_nobody_to_aim_at_is_refused(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	assert(game_ability(&sender, NULL, ability(3, 3), 0) == ABILITY_NO_TARGET);
	assert(sender.charge.charges == 8);
	assert(!sender.last_ability.accepted);
	target.active = false;
	target.topped_out = true;
	assert(game_ability(&sender, &target, ability(3, 3), 0)
		== ABILITY_NO_TARGET);
	assert(sender.charge.charges == 8);
	printf("PASS test_nobody_to_aim_at_is_refused\n");
}

/*
** A queue this deep means no piece has locked in a very long time. The oldest
** entry is dropped rather than the newest refused, because the newest is the
** one somebody has just paid for.
*/
static void	test_a_full_queue_keeps_the_newest(void)
{
	t_game	target;
	int		sent;

	game_start(&target, 2, 99u);
	sent = 0;
	while (sent < TD_MAX_PENDING + 3)
	{
		game_queue_ability(&target, PENDING_EFFECT, (int)EFFECT_NUE);
		sent++;
	}
	assert(target.pending_count == TD_MAX_PENDING);
	game_queue_ability(&target, PENDING_SIRTET, 0);
	assert(target.pending_count == TD_MAX_PENDING);
	assert(target.pending[TD_MAX_PENDING - 1].kind == PENDING_SIRTET);
	printf("PASS test_a_full_queue_keeps_the_newest\n");
}

/**
 * @brief Starts two games and gives the sender charge for anything.
 *
 * Eight charges is a full meter, which is what a level 4 ability costs twice
 * over - enough that no test here is measuring affordability by accident.
 *
 * @param sender Receives a started game with charge banked.
 * @param target Receives a started game.
 */
static void	pair(t_game *sender, t_game *target)
{
	game_start(sender, 1, 20260811u);
	game_start(target, 2, 20260812u);
	while (sender->charge.charges < 8)
		charge_on_clear(&sender->charge, 4);
	while (target->charge.charges < 8)
		charge_on_clear(&target->charge, 4);
}

/**
 * @brief Fills the bottom half of a board, leaving the falling piece room.
 *
 * Rows rather than a whole board, so a transform can be seen to change
 * something without the piece having nowhere to be.
 *
 * @param g Game whose board is filled.
 */
static void	arm(t_game *g)
{
	t_cell	block;
	int		row;
	int		col;

	block.type = CELL_FILLED;
	block.color = 4;
	row = BOARD_HEIGHT / 2;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH - 1)
		{
			board_set(&g->board, col, row, block);
			col++;
		}
		row++;
	}
}

/**
 * @brief Looks one ability up, asserting it exists and needs a Target.
 *
 * @param character The character offering it.
 * @param level The ability level.
 * @return The definition, never NULL.
 */
static const t_ability_def	*ability(t_item_id character, int level)
{
	const t_ability_def	*def;

	def = ability_lookup(character, level);
	assert(def != NULL && def->needs_target);
	return (def);
}

/**
 * @brief Counts the occupied cells on a board.
 *
 * @param g Game to read.
 * @return How many cells are not empty.
 */
static int	filled_cells(const t_game *g)
{
	int	count;
	int	row;
	int	col;

	count = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			if (board_get(&g->board, col, row).type != CELL_EMPTY)
				count++;
			col++;
		}
		row++;
	}
	return (count);
}

/*
** Dark has no counter in libtetrisbrain - the brain leaves it open-ended and
** says the server decides when it ends. Without that decision "for a limited
** time" is forever, and one Dark would end the game.
**
** The count is armed where it lands rather than where it was sent, so the
** lock that delivers it does not spend one of its pieces.
*/
static void	test_dark_blinds_for_four_pieces(void)
{
	t_game	sender;
	t_game	target;
	int		pieces;

	pair(&sender, &target);
	assert(game_ability(&sender, &target, ability(1, 2), 0)
		== ABILITY_ACTIVATED);
	assert(target.dark_pieces == 0);
	assert(game_drop(&target, true));
	assert(target.dark_pieces == TETRISD_DARK_PIECES);
	assert(target.effects.blackout);
	pieces = 0;
	while (pieces < TETRISD_DARK_PIECES)
	{
		assert(game_drop(&target, true));
		pieces++;
	}
	assert(target.dark_pieces == 0);
	assert(!target.effects.blackout);
	printf("PASS test_dark_blinds_for_four_pieces\n");
}

/*
** "Incoming ordinary garbage lowers the Player's stack instead of raising it;
** garbage created by abilities is excluded." Both halves are the test: Pals
** absorbing Pentaris would make Wolf-man's level 3 a hard counter to
** Mirurun's, which the text refuses.
*/
static void	test_pals_turns_ordinary_garbage_into_a_gift(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	arm(&target);
	assert(game_ability(&target, &sender, ability(4, 3), 0)
		== ABILITY_ACTIVATED);
	assert(target.effects.pals);
	before = filled_cells(&target);
	game_queue_garbage(&target, 2);
	assert(game_drop(&target, true));
	/* Two rows off the floor, not two rows onto it. */
	assert(filled_cells(&target) < before);
	/* Pentaris still lands, Pals or no Pals. */
	before = filled_cells(&target);
	game_queue_ability_garbage(&target, TETRISD_PENTARIS_ROWS);
	assert(game_drop(&target, true));
	assert(filled_cells(&target) > before);
	printf("PASS test_pals_turns_ordinary_garbage_into_a_gift\n");
}

/*
** Fry is two halves and only the first was built: the rows went onto the
** sender's own floor and burned at the next lock, and were then thrown away.
** The text says they are sent to the Target.
**
** They go on whole rather than through garbage_lines_from_clear's N-1,
** because they are not a clear being converted - they are three rows the
** sender deliberately buried themselves under in order to hand over.
*/
static void	test_fry_sends_its_burned_rows_on(void)
{
	t_game	sender;
	t_game	target;
	int		burned;

	pair(&sender, &target);
	assert(game_ability(&sender, &target, ability_solo(1, 1), 0)
		== ABILITY_ACTIVATED);
	assert(effect_fry_rows(&sender.effects) > 0);
	burned = effect_fry_rows(&sender.effects);
	assert(game_drop(&sender, true));
	assert(game_take_fry(&sender) == burned);
	assert(game_take_fry(&sender) == 0);
	printf("PASS test_fry_sends_its_burned_rows_on\n");
}

/**
 * @brief Looks up an ability that does not need a Target.
 *
 * Fry is one: it is playable alone, with the send simply going nowhere, which
 * is why every character has a level 1 a one-player room can serve.
 *
 * @param character The character offering it.
 * @param level The ability level.
 * @return The definition, never NULL.
 */
static const t_ability_def	*ability_solo(t_item_id character, int level)
{
	const t_ability_def	*def;

	def = ability_lookup(character, level);
	assert(def != NULL && !def->needs_target);
	return (def);
}
