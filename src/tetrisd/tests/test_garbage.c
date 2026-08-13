/* ************************************************************************** */
/*                                                                            */
/*   test_garbage.c - a clear on one board becomes rows on another            */
/*                                                                            */
/*   The rule under test is a game rule and not a scheduling convenience:     */
/*   garbage lands at the Target's next piece lock, never on arrival.         */
/*   Injecting rows raises the stack under whatever is falling, which can     */
/*   produce a board piece_is_valid would reject - and there is no correct    */
/*   thing to do with a piece already in the air on a board that is no        */
/*   longer legal. CLAUDE.md states it; this is what it has to mean.          */
/*                                                                            */
/*   These drive t_game in-process, for the reason test_clearing.c does:      */
/*   clearing ten columns through MOVE and DROP would take dozens of pieces   */
/*   and depend on what the bag deals, and the question here is about when    */
/*   rows arrive rather than about how they were earned.                      */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_queued_garbage_does_not_touch_the_board(void);
static void	test_garbage_lands_at_the_next_lock(void);
static void	test_garbage_waits_out_a_held_clear(void);
static void	test_the_hole_walks_between_rows(void);
static void	test_the_holes_are_not_a_diagonal(void);
static void	test_a_finished_game_takes_nothing(void);
static void	test_the_pending_count_reaches_the_wire(void);
static void	test_cleared_lines_are_taken_once(void);
static void	test_the_target_is_the_other_live_seat(void);
static void	test_an_attacker_is_reported_at_the_lock_that_lands(void);
static void	test_randoms_and_badges_name_one_rival(void);
static void	test_attackers_names_everyone_attacking(void);
static void	test_kos_names_every_stack_that_is_level(void);
static void	test_a_mode_that_matched_nobody_draws_one(void);
static void	test_a_clear_reaches_every_target_whole(void);

static void	seat_a_pair(t_server_room *server_room, t_room *room);
static void	seat_a_crowd(t_server_room *server_room, t_room *room,
				int count);
static void	declare(t_server_room *server_room, int slot, t_target_mode mode);
static void	attacked_by(t_server_room *server_room, int victim, int attacker);
static void	raise_stack(t_game *g, int rows);
static bool	set_has(const int *set, int count, int slot);
static void	fill_bottom_row(t_game *g);
static int	row_filled_cells(const t_game *g, int row);
static int	row_hole(const t_game *g, int row);

int	main(void)
{
	test_queued_garbage_does_not_touch_the_board();
	test_garbage_lands_at_the_next_lock();
	test_garbage_waits_out_a_held_clear();
	test_the_hole_walks_between_rows();
	test_the_holes_are_not_a_diagonal();
	test_a_finished_game_takes_nothing();
	test_the_pending_count_reaches_the_wire();
	test_cleared_lines_are_taken_once();
	test_the_target_is_the_other_live_seat();
	test_an_attacker_is_reported_at_the_lock_that_lands();
	test_randoms_and_badges_name_one_rival();
	test_attackers_names_everyone_attacking();
	test_kos_names_every_stack_that_is_level();
	test_a_mode_that_matched_nobody_draws_one();
	test_a_clear_reaches_every_target_whole();
	return (0);
}

/*
** Two of the four modes name a person, so they answer with one rival however
** many matched. Randoms matches everybody by construction and Badges matches
** everybody holding a knockout, which late in a Battle Royale is most of the
** room - spraying either would mean one clear hitting fifty boards.
**
** The draw is asserted as "exactly one, and it is a live opponent" rather
** than as a particular seat: which one comes out of the room's own RNG, and
** pinning it here would be a test of xorshift.
*/
static void	test_randoms_and_badges_name_one_rival(void)
{
	t_server_room	server_room;
	t_room			room;
	int				targets[TD_MAX_GAMES];

	seat_a_crowd(&server_room, &room, 5);
	declare(&server_room, 0, TARGET_RANDOM);
	assert(server_room_targets_of(&server_room, 0, targets) == 1);
	assert(targets[0] > 0 && targets[0] < 5);
	server_room.participants[1].ko = 1;
	server_room.participants[2].ko = 2;
	server_room.participants[3].ko = 1;
	declare(&server_room, 0, TARGET_BADGES);
	assert(server_room_targets_of(&server_room, 0, targets) == 1);
	assert(targets[0] >= 1 && targets[0] <= 3);
	printf("PASS test_randoms_and_badges_name_one_rival\n");
}

/*
** Attackers is the mode that answers being hit by several people at once, so
** it hits all of them. Hitting back at one of three and leaving the other two
** to carry on is the thing it exists to stop.
**
** The fourth seat is attacking somebody else, which is what makes this an
** assertion about the ring and not about "everybody who is alive".
*/
static void	test_attackers_names_everyone_attacking(void)
{
	t_server_room	server_room;
	t_room			room;
	int				targets[TD_MAX_GAMES];

	seat_a_crowd(&server_room, &room, 5);
	declare(&server_room, 0, TARGET_ATTACKERS);
	attacked_by(&server_room, 0, 1);
	attacked_by(&server_room, 0, 3);
	attacked_by(&server_room, 2, 4);
	assert(server_room_targets_of(&server_room, 0, targets) == 2);
	assert(set_has(targets, 2, 1) && set_has(targets, 2, 3));
	assert(!set_has(targets, 2, 4));
	printf("PASS test_attackers_names_everyone_attacking\n");
}

/*
** KOs names the tallest stack in the room - one player, normally. Two players
** equally close to topping out are equally the answer, and the tie is the
** only way this mode reaches more than one board.
*/
static void	test_kos_names_every_stack_that_is_level(void)
{
	t_server_room	server_room;
	t_room			room;
	int				targets[TD_MAX_GAMES];

	seat_a_crowd(&server_room, &room, 5);
	declare(&server_room, 0, TARGET_KO);
	raise_stack(&server_room.games[1], 4);
	raise_stack(&server_room.games[2], 9);
	raise_stack(&server_room.games[3], 9);
	assert(server_room_targets_of(&server_room, 0, targets) == 2);
	assert(set_has(targets, 2, 2) && set_has(targets, 2, 3));
	raise_stack(&server_room.games[3], 12);
	assert(server_room_targets_of(&server_room, 0, targets) == 1);
	assert(targets[0] == 3);
	printf("PASS test_kos_names_every_stack_that_is_level\n");
}

/*
** The distinction the fan-out turns on. A mode that matched nobody falls back
** to every live opponent, and that fallback is drawn from rather than sprayed
** - it means "no preference applies", which is Randoms, and Randoms hits one
** person. Without it, declaring Attackers before anybody had attacked would
** hit the entire room, which is the opposite of what choosing it asks for.
*/
static void	test_a_mode_that_matched_nobody_draws_one(void)
{
	t_server_room	server_room;
	t_room			room;
	int				targets[TD_MAX_GAMES];

	seat_a_crowd(&server_room, &room, 5);
	declare(&server_room, 0, TARGET_ATTACKERS);
	assert(server_room_targets_of(&server_room, 0, targets) == 1);
	declare(&server_room, 0, TARGET_KO);
	assert(server_room_targets_of(&server_room, 0, targets) == 1);
	printf("PASS test_a_mode_that_matched_nobody_draws_one\n");
}

/*
** What the fan-out is worth on the receiving end: every Target is queued the
** whole amount, not a share of it. Dividing the rows between the victims
** would make choosing a crowded mode a way of hitting softer, which would
** make Attackers a punishment for being ganged up on.
*/
static void	test_a_clear_reaches_every_target_whole(void)
{
	t_server_room	server_room;
	t_room			room;
	t_body_state	snap;

	int	targets[TD_MAX_GAMES];
	int	count;

	seat_a_crowd(&server_room, &room, 5);
	declare(&server_room, 0, TARGET_ATTACKERS);
	attacked_by(&server_room, 0, 1);
	attacked_by(&server_room, 0, 2);
	fill_bottom_row(&server_room.games[0]);
	assert(game_drop(&server_room.games[0], true));
	assert(game_gravity(&server_room.games[0],
			clear_duration_ms(server_room.games[0].level)));
	count = server_room_targets_of(&server_room, 0, targets);
	assert(count == 2);
	server_room_charge_targets(&server_room, 0, targets, count);
	game_snapshot(&server_room.games[1], &snap);
	assert(snap.pending == garbage_lines_from_clear(1));
	game_snapshot(&server_room.games[2], &snap);
	assert(snap.pending == garbage_lines_from_clear(1));
	game_snapshot(&server_room.games[3], &snap);
	assert(snap.pending == 0);
	assert(game_take_cleared(&server_room.games[0]) == 0);
	printf("PASS test_a_clear_reaches_every_target_whole\n");
}

/*
** Who a clear is aimed at. Three answers and they are deliberately the same
** shape: Double names the other seat, Single names nobody, and an opponent
** who has already topped out names nobody either - so no caller has to tell
** "no opponent" apart from "no opponent left", because nothing crosses in
** either case.
**
** This is where Battle Royale's four targeting modes will land, which is why
** it is a function and not an expression.
*/
static void	test_the_target_is_the_other_live_seat(void)
{
	t_server_room	server_room;
	t_room			room;

	seat_a_pair(&server_room, &room);
	assert(server_room_target_of(&server_room, 0) == 1);
	assert(server_room_target_of(&server_room, 1) == 0);
	server_room.games[1].active = false;
	assert(server_room_target_of(&server_room, 0) == -1);
	seat_a_pair(&server_room, &room);
	assert(room_init(&room, MODE_SINGLE, 1, 0) == 0);
	assert(server_room_target_of(&server_room, 0) == -1);
	printf("PASS test_the_target_is_the_other_live_seat\n");
}

/*
** The whole of the rule's first half: queueing changes the count and nothing
** else. A player who was mid-piece keeps the board they were playing on.
*/
static void	test_queued_garbage_does_not_touch_the_board(void)
{
	t_game	g;
	t_piece	held;

	game_start(&g, 1, 20260811u);
	held = g.piece;
	game_queue_garbage(&g, 2, 7);
	assert(g.pending_garbage == 2);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 1) == 0);
	assert(g.piece.row == held.row && g.piece.col == held.col);
	assert(game_move(&g, -1) || game_move(&g, 1));
	printf("PASS test_queued_garbage_does_not_touch_the_board\n");
}

/*
** And its second half. The rows appear at the lock, all of them, and the
** queue is empty afterwards - a row left owed would land twice.
*/
static void	test_garbage_lands_at_the_next_lock(void)
{
	t_game	g;

	game_start(&g, 1, 20260811u);
	game_queue_garbage(&g, 3, 7);
	assert(game_drop(&g, true));
	assert(g.pending_garbage == 0);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 1) == BOARD_WIDTH - 1);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 2) == BOARD_WIDTH - 1);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 3) == BOARD_WIDTH - 1);
	printf("PASS test_garbage_lands_at_the_next_lock\n");
}

/*
** A lock that completes a row does not deal with garbage at the lock, it
** deals with it when the clear finishes. Otherwise a player would take rows
** in the middle of watching their own go, and worse: rows injected before
** board_clear_lines ran would shift the very rows it was about to take.
*/
static void	test_garbage_waits_out_a_held_clear(void)
{
	t_game	g;

	game_start(&g, 1, 20260809u);
	fill_bottom_row(&g);
	game_queue_garbage(&g, 1, 7);
	assert(game_drop(&g, true));
	assert(g.clearing_count > 0);
	assert(g.pending_garbage == 1);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 1) == BOARD_WIDTH);
	assert(game_gravity(&g, clear_duration_ms(g.level)));
	assert(g.clearing_count == 0);
	assert(g.lines == 1);
	assert(g.pending_garbage == 0);
	assert(row_filled_cells(&g, BOARD_HEIGHT - 1) == BOARD_WIDTH - 1);
	printf("PASS test_garbage_waits_out_a_held_clear\n");
}

/*
** Successive rows leave the hole in different columns. A run of rows sharing
** one hole is a wall rather than a handicap: nothing but an I piece on end
** could ever answer it, and the receiver would be dead on arrival.
**
** The column is drawn from the game's own seeded state rather than the C
** library, because libtetrisbrain is pure by contract and a board has to
** replay the same way twice.
*/
static void	test_the_hole_walks_between_rows(void)
{
	t_game	g;

	game_start(&g, 1, 20260811u);
	game_queue_garbage(&g, 2, 7);
	assert(game_drop(&g, true));
	assert(row_hole(&g, BOARD_HEIGHT - 1) >= 0);
	assert(row_hole(&g, BOARD_HEIGHT - 2) >= 0);
	assert(row_hole(&g, BOARD_HEIGHT - 1) != row_hole(&g, BOARD_HEIGHT - 2));
	printf("PASS test_the_hole_walks_between_rows\n");
}

/*
** The holes do not march. "Adjacent rows differ" is what the test above
** asserts and it is not enough: the column used to be a counter taken modulo
** BOARD_WIDTH, so every row differed from the one below it by exactly one and
** a player taking a Battle Royale's worth of garbage got a clean diagonal
** across the whole board. Every neighbouring pair passed the old assertion
** while the shape was as legible as a shape gets.
**
** So this asks the stronger question the eye was actually asking: over a full
** board of rows, the step from one hole to the next must not be the same
** number every time. Any draw passes; only a march fails.
**
** The step is taken modulo BOARD_WIDTH, and that is not a detail. A plain
** subtraction reads the march's wrap from column 9 back to column 0 as a step
** of -9 and calls that variety, which is how the first version of this test
** passed against the very code it was written to catch. Around the cylinder
** the columns actually live on, a march is one step repeated and nothing else.
**
** Sixteen rows arrive as one batch at one lock, and only those sixteen are
** read. The piece that locked is stamped before the rows are injected, so it
** is pushed up out of the way and the bottom sixteen rows are nothing but the
** sixteen draws - which is what makes each row's single empty cell the hole
** rather than something the piece happened to leave.
*/
static void	test_the_holes_are_not_a_diagonal(void)
{
	t_game	g;
	int		holes[16];
	int		index;
	int		step;
	bool	varies;

	game_start(&g, 1, 20260813u);
	game_queue_garbage(&g, 16, 7);
	assert(game_drop(&g, true));
	index = 0;
	while (index < 16)
	{
		holes[index] = row_hole(&g, BOARD_HEIGHT - 16 + index);
		assert(holes[index] >= 0);
		index++;
	}
	step = (holes[1] - holes[0] + BOARD_WIDTH) % BOARD_WIDTH;
	varies = false;
	index = 2;
	while (index < 16)
	{
		if ((holes[index] - holes[index - 1] + BOARD_WIDTH) % BOARD_WIDTH
			!= step)
			varies = true;
		index++;
	}
	assert(varies);
	printf("PASS test_the_holes_are_not_a_diagonal\n");
}

/*
** Burying a board nobody is playing on would change a result that is already
** settled - and in a match the sender's last clear resolves on the same tick
** the receiver tops out.
*/
static void	test_a_finished_game_takes_nothing(void)
{
	t_game	g;

	game_start(&g, 1, 20260811u);
	g.topped_out = true;
	g.active = false;
	game_queue_garbage(&g, 4, 7);
	assert(g.pending_garbage == 0);
	printf("PASS test_a_finished_game_takes_nothing\n");
}

/*
** The count is the receiver's warning, so it has to survive the codec. A
** client told nothing between the queueing and the landing would see four
** rows appear from nowhere.
*/
static void	test_the_pending_count_reaches_the_wire(void)
{
	t_game			g;
	t_body_state	sent;
	t_body_state	received;
	char			body[TETRISD_BODY_MAX_BYTES];
	int				len;

	game_start(&g, 1, 20260811u);
	game_queue_garbage(&g, 4, 7);
	game_snapshot(&g, &sent);
	assert(sent.pending == 4);
	len = body_state_encode(&sent, body, sizeof(body));
	assert(len > 0);
	memset(&received, 0, sizeof(received));
	assert(body_state_decode(body, (size_t)len, &received) == 0);
	assert(received.pending == 4);
	printf("PASS test_the_pending_count_reaches_the_wire\n");
}

/*
** room.c charges a clear to the Target by taking it, so taking it twice would
** send the rows twice. The count accumulates until somebody asks, because a
** tick that ran two clear completions owes both.
*/
static void	test_cleared_lines_are_taken_once(void)
{
	t_game	g;

	game_start(&g, 1, 20260809u);
	fill_bottom_row(&g);
	assert(game_drop(&g, true));
	assert(game_gravity(&g, clear_duration_ms(g.level)));
	assert(g.lines == 1);
	assert(game_take_cleared(&g) == 1);
	assert(game_take_cleared(&g) == 0);
	printf("PASS test_cleared_lines_are_taken_once\n");
}

/**
 * @brief Builds a two-seat Double room with a live game in each seat.
 *
 * Assembled by hand rather than driven through a running server, because the
 * reactor owns every game it holds and reaching into one from a test thread
 * would be exactly the shared mutable state tetrisd does not have.
 *
 * @param server_room Receives the room runtime.
 * @param room Receives the domain room it points at.
 */
/*
** A Battle Royale's worth of seats, driven in-process. room_init is asked for
** MODE_BATTLE_ROYALE rather than for a wide Double, because is_solo and the
** slot count both read the mode and a Double that claims five seats is not a
** room this server would ever hold.
**
** Every seat starts alive with a distinct player id and its own seed, so a
** target set can be asserted by seat without two boards being the same board.
*/
static void	seat_a_crowd(t_server_room *server_room, t_room *room, int count)
{
	int	slot;

	memset(server_room, 0, sizeof(*server_room));
	assert(room_init(room, MODE_BATTLE_ROYALE, 1, count) == 0);
	server_room->room = room;
	server_room->rng = 0x9e3779b9u;
	server_room->alive = count;
	slot = 0;
	while (slot < count)
	{
		game_start(&server_room->games[slot], (t_player_id)(11 + slot),
			(uint32_t)(slot + 1));
		server_room->participants[slot].player_id = (t_player_id)(11 + slot);
		server_room->participants[slot].alive = true;
		server_room->participants[slot].target_mode = TARGET_RANDOM;
		slot++;
	}
}

/*
** Writes a seat's declared mode straight onto its participant record.
** server_room_set_target wants a t_client, and a connection is the one thing
** an in-process test of the resolution does not need.
*/
static void	declare(t_server_room *server_room, int slot, t_target_mode mode)
{
	server_room->participants[slot].target_mode = mode;
}

/*
** Puts one seat in another's attacker ring, stamped now. match_ms stays 0, so
** every entry is inside TETRISD_BR_ATTACKER_MS and none has aged out.
*/
static void	attacked_by(t_server_room *server_room, int victim, int attacker)
{
	t_participant	*p;

	p = &server_room->participants[victim];
	p->attackers[p->attacker_next].player_id
		= server_room->games[attacker].player_id;
	p->attackers[p->attacker_next].when_ms = server_room->match_ms;
	p->attacker_next = (p->attacker_next + 1) % TETRISD_BR_ATTACKER_RING;
}

/*
** Raises a board's stack to a known height, so "the tallest" is a fact the
** test set rather than one it has to go and measure.
*/
static void	raise_stack(t_game *g, int rows)
{
	int	row;
	int	col;

	row = BOARD_HEIGHT - rows;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			board_set(&g->board, col, row, (t_cell){CELL_FILLED, 1});
			col++;
		}
		row++;
	}
}

/*
** Whether a target set names a seat. The sets are gathered in seat order but
** asserting that would pin an implementation detail rather than the rule.
*/
static bool	set_has(const int *set, int count, int slot)
{
	int	index;

	index = 0;
	while (index < count)
	{
		if (set[index] == slot)
			return (true);
		index++;
	}
	return (false);
}

static void	seat_a_pair(t_server_room *server_room, t_room *room)
{
	memset(server_room, 0, sizeof(*server_room));
	assert(room_init(room, MODE_DOUBLE, 1, 0) == 0);
	server_room->room = room;
	game_start(&server_room->games[0], 11, 1u);
	game_start(&server_room->games[1], 22, 2u);
}

/**
 * @brief Fills the bottom row so the next lock completes it.
 *
 * @param g Game whose board is being set up.
 */
static void	fill_bottom_row(t_game *g)
{
	t_cell	cell;
	int		col;

	cell.type = CELL_FILLED;
	cell.color = 1;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		board_set(&g->board, col, BOARD_HEIGHT - 1, cell);
		col++;
	}
}

/**
 * @brief Counts the occupied cells in one row.
 *
 * @param g Game to read.
 * @param row The row to count.
 * @return How many cells are not empty.
 */
static int	row_filled_cells(const t_game *g, int row)
{
	int	filled;
	int	col;

	filled = 0;
	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (board_get(&g->board, col, row).type != CELL_EMPTY)
			filled++;
		col++;
	}
	return (filled);
}

/**
 * @brief Finds the one empty column in a garbage row.
 *
 * @param g Game to read.
 * @param row The row to search.
 * @return The empty column, or -1 when the row is not a garbage row.
 */
static int	row_hole(const t_game *g, int row)
{
	int	col;

	if (row_filled_cells(g, row) != BOARD_WIDTH - 1)
		return (-1);
	col = 0;
	while (col < BOARD_WIDTH)
	{
		if (board_get(&g->board, col, row).type == CELL_EMPTY)
			return (col);
		col++;
	}
	return (-1);
}

/*
** Who sent the rows travels with them, and is reported at the landing rather
** than at the queueing. That difference is the whole of a knockout being fair:
** rows queued against a player who clears them away first buried nobody, and
** rows that arrive after their sender has left the room still did.
**
** It is a one-shot report. A second reader would file the same attack twice,
** and the count it feeds is a count of knockouts.
*/
static void	test_an_attacker_is_reported_at_the_lock_that_lands(void)
{
	t_game	g;

	game_start(&g, 1, 20260811u);
	/* nothing has landed, so there is nobody to name */
	assert(game_take_attacker(&g) == 0);
	game_queue_garbage(&g, 2, 77);
	/* still nobody: the rows are owed and the board has not taken them */
	assert(game_take_attacker(&g) == 0);
	assert(game_drop(&g, true));
	assert(g.pending_garbage == 0);
	assert(game_take_attacker(&g) == 77);
	assert(game_take_attacker(&g) == 0);
	/* the last sender is the one credited when two of them queue */
	game_queue_garbage(&g, 1, 77);
	game_queue_ability_garbage(&g, 1, 91);
	assert(game_drop(&g, true));
	assert(game_take_attacker(&g) == 91);
	printf("PASS test_an_attacker_is_reported_at_the_lock_that_lands\n");
}
