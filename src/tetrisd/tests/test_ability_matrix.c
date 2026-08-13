/* ************************************************************************** */
/*                                                                            */
/*   test_ability_matrix.c - every ability in the catalogue, once each        */
/*                                                                            */
/*   test_targeted.c asks how an ability travels: when a queued effect        */
/*   lands, what a Mirror does to the ordering, what happens with nobody to   */
/*   aim at. This file asks a flatter question of all sixteen at once - does  */
/*   this one do anything at all? - because three of them once did not, and   */
/*   nothing failed when they did not. An ability that quietly returns        */
/*   ACTIVATED and changes no board is indistinguishable, from the outside,   */
/*   from an ability that works.                                              */
/*                                                                            */
/*   So every entry asserts a consequence that is specific to it: not "the    */
/*   board changed" but which way it changed, and not "an effect was set"     */
/*   but the predicate that effect exists to answer.                          */
/*                                                                            */
/*   The last test is the one that keeps this file honest. It walks the       */
/*   whole catalogue and fails if any (character, level) pair was not         */
/*   exercised above, so a sixteenth ability cannot be added with a           */
/*   seventeenth left untested.                                               */
/*                                                                            */
/*   In-process, like test_targeted.c: what is under test is the transform,   */
/*   not the request that asked for it - test_actions.c owns the wire.        */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

/*
** One ability's coordinates, ticked off as its test runs. The table is a
** static rather than a local so the completeness check at the end can read
** what the fifteen tests before it did.
*/
typedef struct s_covered
{
	t_item_id	character;
	int			level;
	bool		exercised;
}	t_covered;

// Static Variables
static t_covered	g_covered[16];

// Static Functions
static void	test_fry_buries_its_own_bottom_rows(void);
static void	test_dark_blacks_the_target_out(void);
static void	test_vampire_empties_the_targets_meter(void);
static void	test_bomb_scatters_holes_through_the_target(void);
static void	test_mirurun_lifts_its_own_bottom_away(void);
static void	test_inversion_swaps_the_targets_controls(void);
static void	test_pentaris_is_five_rows_of_garbage(void);
static void	test_sirtet_turns_the_target_upside_down(void);
static void	test_sol_deletes_three_columns_it_aimed_at(void);
static void	test_mirror_arms_rather_than_fires(void);
static void	test_paralysis_takes_the_targets_rotation(void);
static void	test_copy_replaces_its_own_field(void);
static void	test_cut_clears_its_own_top(void);
static void	test_nue_takes_the_targets_fast_drop(void);
static void	test_pals_makes_the_next_garbage_a_gift(void);
static void	test_thwack_arms_on_its_own_board(void);
static void	test_every_ability_in_the_catalogue_was_exercised(void);

static void	pair(t_game *sender, t_game *target);
static void	stack(t_game *g, int from_row);
static void	activate(t_game *sender, t_game *target, t_item_id character,
				int level);
static void	lock_one_piece(t_game *g);
static int	filled_cells(const t_game *g);
static void	cover(t_item_id character, int level);

int	main(void)
{
	test_fry_buries_its_own_bottom_rows();
	test_dark_blacks_the_target_out();
	test_vampire_empties_the_targets_meter();
	test_bomb_scatters_holes_through_the_target();
	test_mirurun_lifts_its_own_bottom_away();
	test_inversion_swaps_the_targets_controls();
	test_pentaris_is_five_rows_of_garbage();
	test_sirtet_turns_the_target_upside_down();
	test_sol_deletes_three_columns_it_aimed_at();
	test_mirror_arms_rather_than_fires();
	test_paralysis_takes_the_targets_rotation();
	test_copy_replaces_its_own_field();
	test_cut_clears_its_own_top();
	test_nue_takes_the_targets_fast_drop();
	test_pals_makes_the_next_garbage_a_gift();
	test_thwack_arms_on_its_own_board();
	test_every_ability_in_the_catalogue_was_exercised();
	return (0);
}

/*
** Halloween L1. Three rows go in now and burn at the next lock, so the
** assertion is both halves: the board grew, and the game knows it owes a fire.
*/
static void	test_fry_buries_its_own_bottom_rows(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	before = filled_cells(&sender);
	activate(&sender, &target, 1, 1);
	assert(filled_cells(&sender) > before);
	assert(effect_fry_rows(&sender.effects) > 0);
	printf("PASS test_fry_buries_its_own_bottom_rows\n");
}

/*
** Halloween L2. The blackout is the Target's and arrives at their lock, so
** before that lock nothing about them has changed.
*/
static void	test_dark_blacks_the_target_out(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 1, 2);
	assert(!target.effects.blackout);
	lock_one_piece(&target);
	assert(target.effects.blackout);
	assert(target.dark_pieces > 0);
	printf("PASS test_dark_blacks_the_target_out\n");
}

/*
** Halloween L3. A theft, not a bonus: the room holds the same total charge
** afterwards, which is what separates charge_transfer from a top-up.
*/
static void	test_vampire_empties_the_targets_meter(void)
{
	t_game	sender;
	t_game	target;
	int		total;

	pair(&sender, &target);
	total = sender.charge.charges + target.charge.charges;
	activate(&sender, &target, 1, 3);
	assert(target.charge.charges == 0);
	/* the level's own cost is spent out of what it just took */
	assert(sender.charge.charges == total - ability_cost(3));
	printf("PASS test_vampire_empties_the_targets_meter\n");
}

/*
** Halloween L4. Bomb takes cells out rather than rows, so a board that lost
** cells without losing a whole line is the shape of the claim.
*/
static void	test_bomb_scatters_holes_through_the_target(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	stack(&target, BOARD_HEIGHT / 2);
	before = filled_cells(&target);
	activate(&sender, &target, 1, 4);
	assert(filled_cells(&target) == before);
	lock_one_piece(&target);
	assert(filled_cells(&target) < before);
	printf("PASS test_bomb_scatters_holes_through_the_target\n");
}

/*
** Mirurun L1. It cuts the caster's own bottom away, which is a repair and
** not an attack - the Target is untouched.
*/
static void	test_mirurun_lifts_its_own_bottom_away(void)
{
	t_game	sender;
	t_game	target;
	int		before;
	int		untouched;

	pair(&sender, &target);
	stack(&sender, BOARD_HEIGHT - 4);
	stack(&target, BOARD_HEIGHT - 4);
	before = filled_cells(&sender);
	untouched = filled_cells(&target);
	activate(&sender, &target, 2, 1);
	assert(filled_cells(&sender) < before);
	assert(filled_cells(&target) == untouched);
	printf("PASS test_mirurun_lifts_its_own_bottom_away\n");
}

/*
** Mirurun L2. Inverted controls are a fact about the Target's next pieces,
** so like every other status effect they begin at their lock.
*/
static void	test_inversion_swaps_the_targets_controls(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 2, 2);
	assert(!effect_controls_inverted(&target.effects));
	lock_one_piece(&target);
	assert(effect_controls_inverted(&target.effects));
	printf("PASS test_inversion_swaps_the_targets_controls\n");
}

/*
** Mirurun L3. Garbage with a name: it is counted on the wire from the moment
** it is sent, which is why the count is checked before the lock as well.
*/
static void	test_pentaris_is_five_rows_of_garbage(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	before = filled_cells(&target);
	activate(&sender, &target, 2, 3);
	assert(target.pending_ability_garbage == TETRISD_PENTARIS_ROWS);
	assert(filled_cells(&target) == before);
	lock_one_piece(&target);
	assert(target.pending_ability_garbage == 0);
	assert(filled_cells(&target) > before);
	printf("PASS test_pentaris_is_five_rows_of_garbage\n");
}

/*
** Mirurun L4. Sirtet turns the field over, so the cells that were at the
** bottom are at the top and the count of them is unchanged.
*/
static void	test_sirtet_turns_the_target_upside_down(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	stack(&target, BOARD_HEIGHT - 3);
	before = filled_cells(&target);
	assert(board_get(&target.board, 0, 0).type == CELL_EMPTY);
	activate(&sender, &target, 2, 4);
	lock_one_piece(&target);
	assert(filled_cells(&target) >= before);
	assert(board_get(&target.board, 0, 0).type != CELL_EMPTY);
	printf("PASS test_sirtet_turns_the_target_upside_down\n");
}

/*
** Princess L1. Sol is aimed, and the three columns it names are the three
** that empty - the rest of the row it cut through is still there.
*/
static void	test_sol_deletes_three_columns_it_aimed_at(void)
{
	t_game	sender;
	t_game	target;
	int		middle;
	int		outside;
	int		row;

	pair(&sender, &target);
	stack(&sender, BOARD_HEIGHT - 2);
	row = BOARD_HEIGHT - 1;
	/* an unaimed shot goes where the falling piece already is */
	middle = sender.piece.col;
	outside = 0;
	if (middle - 1 <= 0)
		outside = BOARD_WIDTH - 2;
	activate(&sender, &target, 3, 1);
	assert(board_get(&sender.board, middle - 1, row).type == CELL_EMPTY);
	assert(board_get(&sender.board, middle, row).type == CELL_EMPTY);
	assert(board_get(&sender.board, middle + 1, row).type == CELL_EMPTY);
	assert(board_get(&sender.board, outside, row).type != CELL_EMPTY);
	printf("PASS test_sol_deletes_three_columns_it_aimed_at\n");
}

/*
** Princess L2. Mirror changes nobody's board: it arms the caster, and the
** next thing aimed at them is what it spends itself on.
*/
static void	test_mirror_arms_rather_than_fires(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 3, 2);
	assert(sender.effects.mirror_armed);
	assert(!target.effects.mirror_armed);
	assert(target.pending_count == 0);
	printf("PASS test_mirror_arms_rather_than_fires\n");
}

/*
** Princess L3. Paralysis is the rotation, and only the rotation - the Target
** can still move and drop, which is what makes it survivable.
*/
static void	test_paralysis_takes_the_targets_rotation(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 3, 3);
	lock_one_piece(&target);
	assert(effect_rotation_blocked(&target.effects));
	assert(!effect_fastdrop_blocked(&target.effects));
	printf("PASS test_paralysis_takes_the_targets_rotation\n");
}

/*
** Princess L4. The one ability that changes the caster's board out of
** somebody else's, so both boards end up holding the same cells.
*/
static void	test_copy_replaces_its_own_field(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	stack(&target, BOARD_HEIGHT - 2);
	assert(filled_cells(&sender) != filled_cells(&target));
	activate(&sender, &target, 3, 4);
	assert(filled_cells(&sender) == filled_cells(&target));
	printf("PASS test_copy_replaces_its_own_field\n");
}

/*
** Wolf-man L1. The top four rows of the stack come off and the rest stays
** exactly where it was - the floor is untouched, and the four rows nearest
** the ceiling are the ones that go. Cutting a stack that is only two rows
** tall takes those two and stops; there is no fifth row to reach into.
*/
static void	test_cut_clears_its_own_top(void)
{
	t_game	sender;
	t_game	target;
	int		before;

	pair(&sender, &target);
	stack(&sender, 8);
	before = filled_cells(&sender);
	assert(board_get(&sender.board, 0, 8).type != CELL_EMPTY);
	activate(&sender, &target, 4, 1);
	assert(board_get(&sender.board, 0, 8).type == CELL_EMPTY);
	assert(board_get(&sender.board, 0, 11).type == CELL_EMPTY);
	assert(board_get(&sender.board, 0, 12).type != CELL_EMPTY);
	assert(board_get(&sender.board, 0, BOARD_HEIGHT - 1).type != CELL_EMPTY);
	assert(filled_cells(&sender) == before - 4 * (BOARD_WIDTH - 1));
	printf("PASS test_cut_clears_its_own_top\n");
}

/*
** Wolf-man L2. Nue takes the fast drop away and leaves the rotation, which is
** the mirror image of Paralysis and the reason both predicates are asked.
*/
static void	test_nue_takes_the_targets_fast_drop(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 4, 2);
	lock_one_piece(&target);
	assert(effect_fastdrop_blocked(&target.effects));
	assert(!effect_rotation_blocked(&target.effects));
	printf("PASS test_nue_takes_the_targets_fast_drop\n");
}

/*
** Wolf-man L3. Pals lands on its caster and changes what arrives next: the
** garbage aimed at them stops being an attack for a few pieces.
*/
static void	test_pals_makes_the_next_garbage_a_gift(void)
{
	t_game	sender;
	t_game	target;

	pair(&sender, &target);
	activate(&sender, &target, 4, 3);
	assert(sender.effects.pals);
	assert(sender.pals_pieces == TETRISD_PALS_PIECES);
	assert(!target.effects.pals);
	printf("PASS test_pals_makes_the_next_garbage_a_gift\n");
}

/*
** Wolf-man L4. Thwack is the one level-4 that needs nobody to aim at, so it
** is also the one a Single player may use.
*/
static void	test_thwack_arms_on_its_own_board(void)
{
	t_game				sender;
	t_game				target;
	const t_ability_def	*def;

	pair(&sender, &target);
	def = ability_lookup(4, 4);
	assert(def != NULL && !def->needs_target);
	assert(ability_is_playable_solo(def));
	activate(&sender, &target, 4, 4);
	assert(effect_thwack_active(&sender.effects));
	assert(!effect_thwack_active(&target.effects));
	printf("PASS test_thwack_arms_on_its_own_board\n");
}

/*
** The check that makes the fifteen above a catalogue rather than a list. Four
** characters offer four levels each, ability_lookup answers for every pair,
** and every pair has to have been ticked off by a test that asserted what it
** does. A new ability arrives here as a failure rather than as silence.
*/
static void	test_every_ability_in_the_catalogue_was_exercised(void)
{
	t_item_id	character;
	int			level;
	int			found;

	found = 0;
	character = 1;
	while (character <= 4)
	{
		level = 1;
		while (level <= 4)
		{
			assert(ability_lookup(character, level) != NULL);
			assert(g_covered[found].exercised);
			assert(g_covered[found].character == character);
			assert(g_covered[found].level == level);
			found++;
			level++;
		}
		character++;
	}
	assert(found == 16);
	assert(ability_lookup(5, 1) == NULL);
	assert(ability_lookup(1, 5) == NULL);
	printf("PASS test_every_ability_in_the_catalogue_was_exercised\n");
}

/**
 * @brief Starts two games with enough charge for anything in the catalogue.
 *
 * @param sender Game that activates.
 * @param target Game aimed at.
 */
static void	pair(t_game *sender, t_game *target)
{
	game_start(sender, 1, 20260812u);
	game_start(target, 2, 20260813u);
	while (sender->charge.charges < 8)
		charge_on_clear(&sender->charge, 4);
	while (target->charge.charges < 8)
		charge_on_clear(&target->charge, 4);
}

/**
 * @brief Fills every row from one downwards, leaving the last column open.
 *
 * The open column is what keeps the rows from clearing themselves, and the
 * falling piece spawns above all of it - so a board can be given a stack
 * without the piece on it having nowhere to be.
 *
 * @param g Game whose board is filled.
 * @param from_row Topmost row to fill.
 */
static void	stack(t_game *g, int from_row)
{
	t_cell	block;
	int		row;
	int		col;

	block.type = CELL_FILLED;
	block.color = 4;
	row = from_row;
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
 * @brief Activates one ability, asserting the server accepted it.
 *
 * Also ticks the pair off the coverage table, so that a test which is written
 * but never called cannot pass the completeness check at the end.
 *
 * @param sender Game activating.
 * @param target Game aimed at.
 * @param character The character offering the ability.
 * @param level The ability level, 1 to 4.
 */
static void	activate(t_game *sender, t_game *target, t_item_id character,
			int level)
{
	const t_ability_def	*def;

	def = ability_lookup(character, level);
	assert(def != NULL);
	assert(game_ability(sender, target, def, -1) == ABILITY_ACTIVATED);
	cover(character, level);
}

/**
 * @brief Drops one piece to its lock, which is where queued effects land.
 *
 * @param g Game to advance by one piece.
 */
static void	lock_one_piece(t_game *g)
{
	assert(game_drop(g, true));
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

/**
 * @brief Records that one (character, level) pair was activated by a test.
 *
 * Idempotent, and it fills the table in catalogue order rather than in the
 * order the tests happen to run, so the completeness check can walk the
 * catalogue and index straight into it.
 *
 * @param character The character offering the ability.
 * @param level The ability level, 1 to 4.
 */
static void	cover(t_item_id character, int level)
{
	int	slot;

	slot = (int)(character - 1) * 4 + level - 1;
	if (slot < 0 || slot >= 16)
		return ;
	g_covered[slot].character = character;
	g_covered[slot].level = level;
	g_covered[slot].exercised = true;
}
