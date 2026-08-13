#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tetrisu_bot.h"

// Static Functions
static void	test_a_row_one_cell_short_is_completed(void);
static void	test_normal_walks_away_from_a_single(void);
static void	test_easy_takes_the_single_it_is_offered(void);
static void	test_a_single_is_taken_when_the_stack_is_high(void);
static void	test_normal_prices_a_tetris_by_what_it_sends(void);
static void	test_easy_throws_pieces_away_and_normal_does_not(void);
static void	test_a_plan_with_no_rotation_keeps_the_one_it_has(void);
static void	test_the_piece_known_to_be_next_changes_the_answer(void);
static void	test_a_seed_replays_exactly(void);
static void	test_a_full_board_plans_nothing(void);
static void	test_a_tier_is_a_tempo_as_well_as_a_price(void);

static void	snapshot_init(t_body_state *snap, int type);
static void	snapshot_board(t_body_state *snap, const t_board *board);
static void	row_fill(t_board *board, int row, int hole);
static void	rows_fill(t_board *board, int from, int hole);
static int	play(t_bot_level level, uint32_t seed, int pieces, uint32_t *sig);
static int	place(t_bot *bot, t_board *board, int type, int next, int *col);
static int	peek(const t_piece_bag *bag);
static int	lookahead_moved(const t_board *board, int type, int next);

/**
 * @brief Runs the bot brain suite.
 *
 * The tiers are the point. Easy and normal are the same search over two
 *   different prices for a clear, so most of what is asserted here is what a
 *   price makes the bot do rather than whether the search finds a placement.
 *
 * @return 0 after every regression passes.
 */
int	main(void)
{
	test_a_row_one_cell_short_is_completed();
	test_normal_walks_away_from_a_single();
	test_easy_takes_the_single_it_is_offered();
	test_a_single_is_taken_when_the_stack_is_high();
	test_normal_prices_a_tetris_by_what_it_sends();
	test_easy_throws_pieces_away_and_normal_does_not();
	test_a_plan_with_no_rotation_keeps_the_one_it_has();
	test_the_piece_known_to_be_next_changes_the_answer();
	test_a_seed_replays_exactly();
	test_a_full_board_plans_nothing();
	test_a_tier_is_a_tempo_as_well_as_a_price();
	printf("\nAll bot brain tests passed.\n");
	return (0);
}

/**
 * @brief Two rows missing the same two cells are completed, not buried.
 *
 * The gap is two wide and two deep so that one O fills it exactly, which
 *   makes the assertion about the plan rather than about which cells a
 *   rotation happens to occupy. Both rows send a row when they go, so this is
 *   not the clear either tier is asked to walk away from.
 */
static void	test_a_row_one_cell_short_is_completed(void)
{
	t_board	board;
	t_bot	bot;
	int		col;

	board_init(&board);
	row_fill(&board, BOARD_HEIGHT - 1, 4);
	row_fill(&board, BOARD_HEIGHT - 2, 4);
	board_set(&board, 5, BOARD_HEIGHT - 1, (t_cell){CELL_EMPTY, 0});
	board_set(&board, 5, BOARD_HEIGHT - 2, (t_cell){CELL_EMPTY, 0});
	bot_init(&bot, BOT_NORMAL, 7u);
	bot_begin_piece(&bot);
	assert(place(&bot, &board, PIECE_O, -1, &col) == 2);
	printf("PASS test_a_row_one_cell_short_is_completed\n");
}

/**
 * @brief With room to spare, normal refuses a clear that sends nothing.
 *
 * garbage_lines_from_clear is {0, 0, 1, 2, 3}, so a single sends nothing at
 *   all and a tier that plays to attack has no reason to take one. The whole
 *   reason BOT_W_WASTED_CLEAR has to be as large as it is is that clearing a
 *   row drops every column by one, and the surface terms pay for that windfall
 *   whether the clear sent anything or not.
 */
static void	test_normal_walks_away_from_a_single(void)
{
	t_board	clearing;
	t_board	holding;

	board_init(&clearing);
	board_init(&holding);
	row_fill(&clearing, BOARD_HEIGHT - 1, -1);
	row_fill(&holding, BOARD_HEIGHT - 1, 3);
	assert(bot_placement_score(BOT_NORMAL, &clearing)
		< bot_placement_score(BOT_NORMAL, &holding));
	printf("PASS test_normal_walks_away_from_a_single\n");
}

/**
 * @brief Easy takes the same single normal walked away from.
 *
 * Same two boards, same search, one different price. This pair is what makes
 *   the tiers a difficulty setting rather than two programs.
 */
static void	test_easy_takes_the_single_it_is_offered(void)
{
	t_board	clearing;
	t_board	holding;

	board_init(&clearing);
	board_init(&holding);
	row_fill(&clearing, BOARD_HEIGHT - 1, -1);
	row_fill(&holding, BOARD_HEIGHT - 1, 3);
	assert(bot_placement_score(BOT_EASY, &clearing)
		> bot_placement_score(BOT_EASY, &holding));
	printf("PASS test_easy_takes_the_single_it_is_offered\n");
}

/**
 * @brief Above the danger line, normal takes the single after all.
 *
 * The penalty is what makes normal build; the ceiling on it is what stops
 *   normal building all the way to the top. Without this the bot refuses a
 *   single at row 19 exactly as readily as at row 2, and tops out holding the
 *   row that would have saved it.
 *
 * Asked of the search rather than of bot_placement_score, because the answer
 *   is no longer in a settled board: the evaluator's height sensitivity is the
 *   landing-height term, which is a fact about a *placement*. Two stacks of
 *   the same shape and different heights score identically once settled, and
 *   correctly so.
 */
static void	test_a_single_is_taken_when_the_stack_is_high(void)
{
	t_board	board;
	t_bot	bot;
	int		top;
	int		col;

	top = BOARD_HEIGHT - (BOT_DANGER_HEIGHT + 3);
	board_init(&board);
	rows_fill(&board, top, 3);
	board_set(&board, 3, BOARD_HEIGHT - 1, (t_cell){CELL_FILLED, 0});
	board_set(&board, 3, BOARD_HEIGHT - 2, (t_cell){CELL_FILLED, 0});
	bot_init(&bot, BOT_NORMAL, 7u);
	bot_begin_piece(&bot);
	assert(place(&bot, &board, PIECE_I, -1, &col) > 0);
	printf("PASS test_a_single_is_taken_when_the_stack_is_high\n");
}

/**
 * @brief Normal prices four rows by the three they send, easy by the four.
 *
 * Both boards clear to nothing, so what is left on each side above an empty
 *   board's own score is exactly the price of the clear. An empty board does
 *   not score zero under this evaluator - twenty empty rows are two row
 *   transitions each - but that offset is on every candidate equally and so
 *   never decides between two of them. Easy pays per row
 *   cleared; normal pays per row *sent*, and since a single sends nothing the
 *   ratio is not four to one but three to less than nothing.
 *
 * Each board is built again before it is scored, because scoring one clears
 *   it: the score of a placement is the score of the board the *next* piece
 *   falls on, so the rows have to go before anything is measured. A second
 *   call on the same board would be scoring an empty one.
 */
static void	test_normal_prices_a_tetris_by_what_it_sends(void)
{
	t_board	board;
	int		base;

	board_init(&board);
	base = bot_placement_score(BOT_NORMAL, &board);
	board_init(&board);
	rows_fill(&board, BOARD_HEIGHT - 4, -1);
	assert(bot_placement_score(BOT_NORMAL, &board) - base
		== 3 * BOT_W_GARBAGE);
	board_init(&board);
	row_fill(&board, BOARD_HEIGHT - 1, -1);
	assert(bot_placement_score(BOT_NORMAL, &board) - base
		== -BOT_W_WASTED_CLEAR);
	/* easy is not playing the attack game, so both boards price the same */
	board_init(&board);
	rows_fill(&board, BOARD_HEIGHT - 4, -1);
	assert(bot_placement_score(BOT_EASY, &board) == base);
	board_init(&board);
	row_fill(&board, BOARD_HEIGHT - 1, -1);
	assert(bot_placement_score(BOT_EASY, &board) == base);
	printf("PASS test_normal_prices_a_tetris_by_what_it_sends\n");
}

/**
 * @brief Easy dies on a clean board and normal does not.
 *
 * The weights alone are a near-perfect survival set - left to itself the bot
 *   does not die, it merely never attacks, which in a Battle Royale is not an
 *   easy opponent but a stalemate that outlives most of the room doing
 *   nothing. The thrown pieces are what make easy losable, so this is the
 *   test that the tier does its job.
 */
static void	test_easy_throws_pieces_away_and_normal_does_not(void)
{
	uint32_t	sig;

	assert(play(BOT_EASY, 20260813u, 400, &sig) < 400);
	assert(play(BOT_NORMAL, 20260813u, 400, &sig) == 400);
	printf("PASS test_easy_throws_pieces_away_and_normal_does_not\n");
}

/**
 * @brief The second plan of a piece never proposes a different rotation.
 *
 * A piece is planned twice, and the second call is made after the caller has
 *   already turned to the rotation the first one chose. Answering with a
 *   different rotation there would have the caller slide to a column measured
 *   from an anchor the piece no longer has.
 */
static void	test_a_plan_with_no_rotation_keeps_the_one_it_has(void)
{
	t_body_state	snap;
	t_board			board;
	t_bot			bot;
	int				rotation;
	int				col;

	board_init(&board);
	row_fill(&board, BOARD_HEIGHT - 1, 9);
	snapshot_init(&snap, PIECE_L);
	snapshot_board(&snap, &board);
	snap.piece.rotation = 2;
	bot_init(&bot, BOT_NORMAL, 11u);
	bot_begin_piece(&bot);
	assert(bot_plan(&bot, &snap, false, &rotation, &col));
	assert(rotation == 2);
	printf("PASS test_a_plan_with_no_rotation_keeps_the_one_it_has\n");
}

/**
 * @brief Naming the next piece changes where the current one goes.
 *
 * Which board and which pair of pieces disagree is not something to assert -
 *   it depends on weights that are meant to be tuned. That there exists a
 *   disagreement at all is: if the lookahead were never consulted, no pair
 *   could differ, and this is the assertion that catches a scan built with
 *   `next` left at -1.
 */
static void	test_the_piece_known_to_be_next_changes_the_answer(void)
{
	t_board	board;
	int		differ;
	int		type;
	int		next;

	board_init(&board);
	rows_fill(&board, BOARD_HEIGHT - 3, 9);
	row_fill(&board, BOARD_HEIGHT - 4, 6);
	row_fill(&board, BOARD_HEIGHT - 5, 2);
	differ = 0;
	type = 0;
	while (type < BRAIN_BAG_SIZE)
	{
		next = 0;
		while (next < BRAIN_BAG_SIZE)
			differ += lookahead_moved(&board, type, next++);
		type++;
	}
	assert(differ > 0);
	printf("PASS test_the_piece_known_to_be_next_changes_the_answer\n");
}

/**
 * @brief Does naming this next piece move where this current one goes?
 *
 * @param board The board to plan on.
 * @param type The piece to place.
 * @param next The piece to claim is behind it.
 * @return 1 when the two plans disagree, 0 when they do not.
 */
static int	lookahead_moved(const t_board *board, int type, int next)
{
	t_body_state	snap;
	t_bot			bot;
	int				blind[2];
	int				seeing[2];

	bot_init(&bot, BOT_NORMAL, 13u);
	snapshot_init(&snap, type);
	snapshot_board(&snap, board);
	if (!bot_plan(&bot, &snap, true, &blind[0], &blind[1]))
		return (0);
	snap.next[0] = next;
	if (!bot_plan(&bot, &snap, true, &seeing[0], &seeing[1]))
		return (0);
	return (blind[0] != seeing[0] || blind[1] != seeing[1]);
}

/**
 * @brief Two bots on one seed play the same game, and two seeds do not.
 *
 * The generator is the bot's own rather than a global one, which is what lets
 *   a failure be replayed and what keeps two bots in one process from drawing
 *   out of each other's stream. The signature folds every placement in, so
 *   two games that ended at the same piece for different reasons are still
 *   two different games here.
 */
static void	test_a_seed_replays_exactly(void)
{
	uint32_t	first;
	uint32_t	again;
	uint32_t	other;

	play(BOT_EASY, 4242u, 500, &first);
	play(BOT_EASY, 4242u, 500, &again);
	play(BOT_EASY, 99u, 500, &other);
	assert(first == again);
	assert(first != other);
	printf("PASS test_a_seed_replays_exactly\n");
}

/**
 * @brief A board with nowhere to put the piece plans nothing, and says so.
 *
 * The caller has to be able to tell "here" from "nowhere", because the
 *   rotation and column it is handed mean nothing unless a placement was
 *   found - and a bot sliding to column 0 on a full board looks exactly like
 *   a bot that has crashed.
 */
static void	test_a_full_board_plans_nothing(void)
{
	t_body_state	snap;
	t_board			board;
	t_bot			bot;
	int				rotation;
	int				col;

	board_init(&board);
	rows_fill(&board, 0, -1);
	snapshot_init(&snap, PIECE_O);
	snapshot_board(&snap, &board);
	bot_init(&bot, BOT_NORMAL, 5u);
	bot_begin_piece(&bot);
	assert(!bot_plan(&bot, &snap, true, &rotation, &col));
	assert(bot_level_parse("ultra", &bot.level) && bot.level == BOT_ULTRA);
	assert(!bot_level_parse("insane", &bot.level));
	assert(strcmp(bot_level_word(BOT_EASY), "easy") == 0);
	printf("PASS test_a_full_board_plans_nothing\n");
}

/**
 * @brief Every tier plays at a human tempo, and no two bots in lockstep.
 *
 * The regression is the whole of why this exists. Nothing paced a bot: it
 * planned, moved and hard dropped as fast as the socket and the input rate
 * limit allowed, which measured at six to thirteen pieces a second against a
 * person's one or two. Three of them buried a Battle Royale player who did
 * nothing under seventeen rows in twenty-two seconds.
 *
 * So what is asserted is a bound in pieces per second rather than the constant
 * itself: a tier is allowed to be retuned, and is not allowed to leave the
 * range a person plays in. The jitter is asserted as a spread actually
 * observed, because a generator that returned the base every time would pass
 * every bound above and still put three bots' garbage into one pulse.
 */
static void	test_a_tier_is_a_tempo_as_well_as_a_price(void)
{
	t_bot	bot;
	int		ms;
	int		low;
	int		high;
	int		round;

	assert(bot_piece_pace_ms(NULL, 1) == 0);
	bot_init(&bot, BOT_EASY, 20260813u);
	low = 100000;
	high = 0;
	round = 0;
	while (round < 500)
	{
		ms = bot_piece_pace_ms(&bot, 1);
		/* the two ends of what a person does: no slower than a piece every
		** two and a half seconds, no faster than four a second */
		assert(ms >= 250 && ms <= 2500);
		low = (ms < low) * ms + (ms >= low) * low;
		high = (ms > high) * ms + (ms <= high) * high;
		round++;
	}
	assert(low < high);
	assert(high - low > BOT_PACE_EASY_MS / 4);
	bot_init(&bot, BOT_NORMAL, 20260813u);
	assert(bot_piece_pace_ms(&bot, 1) < BOT_PACE_EASY_MS);
	bot_init(&bot, BOT_ULTRA, 20260813u);
	assert(bot_piece_pace_ms(&bot, 1) < BOT_PACE_NORMAL_MS);
	/* the level shortens the tempo, and the floor stops it running away */
	bot_init(&bot, BOT_NORMAL, 20260813u);
	low = bot_piece_pace_ms(&bot, 1);
	bot_init(&bot, BOT_NORMAL, 20260813u);
	high = bot_piece_pace_ms(&bot, 12);
	assert(high < low);
	bot_init(&bot, BOT_NORMAL, 20260813u);
	assert(bot_piece_pace_ms(&bot, 60) >= BOT_PACE_NORMAL_MS
		* BOT_PACE_FLOOR_PCT / 100 * (100 - BOT_PACE_JITTER_PCT) / 100);
	bot_init(&bot, BOT_NORMAL, 20260813u);
	assert(bot_piece_pace_ms(&bot, 0) == low);
	printf("PASS test_a_tier_is_a_tempo_as_well_as_a_price\n");
}

/**
 * @brief An empty snapshot holding one piece at its spawn position.
 *
 * @param snap Receives the snapshot.
 * @param type The piece type it is holding.
 */
static void	snapshot_init(t_body_state *snap, int type)
{
	t_piece	spawn;

	memset(snap, 0, sizeof(*snap));
	spawn = piece_spawn((t_piece_type)type);
	snap->piece.type = type;
	snap->piece.rotation = spawn.rotation;
	snap->piece.col = spawn.col;
	snap->piece.row = spawn.row;
	snap->next[0] = -1;
	snap->next[1] = -1;
	snap->next[2] = -1;
	snap->hold = BODY_HOLD_EMPTY;
}

/**
 * @brief Copy a board into a snapshot, the way a STATE frame would carry it.
 *
 * @param snap The snapshot to write into.
 * @param board The board to copy.
 */
static void	snapshot_board(t_body_state *snap, const t_board *board)
{
	int	row;
	int	col;

	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			snap->cells[row][col].type
				= (int)board_get(board, col, row).type;
			col++;
		}
		row++;
	}
}

/**
 * @brief Fill one row, leaving one column empty.
 *
 * @param board The board to write into.
 * @param row The row to fill.
 * @param hole The column to leave empty; -1 fills the row completely.
 */
static void	row_fill(t_board *board, int row, int hole)
{
	int	col;

	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (col != hole)
			board_set(board, col, row, (t_cell){CELL_FILLED, 0});
		col++;
	}
}

/**
 * @brief Fill every row from `from` down, leaving one column empty in each.
 *
 * @param board The board to write into.
 * @param from The topmost row to fill.
 * @param hole The column to leave empty; -1 fills the rows completely.
 */
static void	rows_fill(t_board *board, int from, int hole)
{
	int	row;

	row = from;
	while (row < BOARD_HEIGHT)
	{
		row_fill(board, row, hole);
		row++;
	}
}

/**
 * @brief Play a bot against a local board and report how far it got.
 *
 * A whole game with no server in it: the 7-bag deals, the bot plans, the
 *   piece is stamped where it planned, and rows clear. It is what makes the
 *   tiers assertable at all, because a tier is a claim about a game rather
 *   than about a placement.
 *
 * @param level The difficulty to play at.
 * @param seed The seed for both the bag and the bot.
 * @param pieces How many pieces to play before calling it survived.
 * @param sig Receives a fold of every placement, for comparing two games.
 * @return How many pieces were placed before topping out, or `pieces`.
 */
static int	play(t_bot_level level, uint32_t seed, int pieces, uint32_t *sig)
{
	t_board		board;
	t_piece_bag	bag;
	t_bot		bot;
	int			placed;
	int			col;
	int			type;

	board_init(&board);
	piece_bag_init(&bag, seed);
	bot_init(&bot, level, seed);
	*sig = 2166136261u;
	placed = 0;
	while (placed < pieces)
	{
		bot_begin_piece(&bot);
		type = (int)piece_bag_next(&bag);
		if (place(&bot, &board, type, peek(&bag), &col) < 0)
			return (placed);
		*sig = (*sig ^ (uint32_t)(col + BOT_SCAN_MARGIN)) * 16777619u;
		placed++;
	}
	return (placed);
}

/**
 * @brief Look at the piece behind the one just drawn, without taking it.
 *
 * The bag is copied, because a 7-bag that has been drawn from has changed -
 *   and a peek that consumed a draw would deal the bot a different game from
 *   the one it was shown.
 *
 * @param bag The bag as it stands.
 * @return The piece it would deal next.
 */
static int	peek(const t_piece_bag *bag)
{
	t_piece_bag	copy;

	copy = *bag;
	return ((int)piece_bag_next(&copy));
}


/**
 * @brief Plan one piece onto a local board and stamp it where the plan said.
 *
 * The two-call shape is the caller's contract and is reproduced rather than
 *   shortened, because a helper that planned once would not exercise the rule
 *   the second call exists for.
 *
 * The plan is resolved through bot_entry rather than by checking it at the
 *   piece's own row. Checking it there rejects every vertical I, because an I
 *   spawns at row -1 and its vertical rotations reach a row above their
 *   anchor - so a harness that got this wrong would declare game over on the
 *   first I the bot tried to stand up, which is how it was first got wrong.
 *
 * @param bot The bot planning.
 * @param board The board, written in place.
 * @param type The piece to place.
 * @param next The piece behind it, for the tiers that look ahead.
 * @param col Receives the column it was placed at.
 * @return Rows cleared, or -1 when nothing fit and the game is over.
 */
static int	place(t_bot *bot, t_board *board, int type, int next, int *col)
{
	t_body_state	snap;
	t_piece			piece;
	int				rotation;

	snapshot_init(&snap, type);
	snapshot_board(&snap, board);
	snap.next[0] = next;
	if (!bot_plan(bot, &snap, true, &rotation, col))
		return (-1);
	snap.piece.rotation = rotation;
	if (!bot_plan(bot, &snap, false, &rotation, col))
		return (-1);
	if (!bot_entry(&snap, rotation, *col, &piece))
		return (-1);
	piece_hard_drop(board, &piece);
	piece_stamp(board, &piece);
	return (board_clear_lines(board));
}
