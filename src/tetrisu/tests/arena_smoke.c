/* ************************************************************************** */
/*                                                                            */
/*   arena_smoke.c - four clients, one Battle Royale, against a live tetrisd  */
/*                                                                            */
/*   The Double smoke test proves one rival's whole board crosses. This       */
/*   proves the other channel: ninety-eight rivals cannot ride that section   */
/*   at all, so they ride an arena of thumbnails on a clock of its own, and   */
/*   what has to be true of it is different in kind. The roster is complete   */
/*   or a player is invisible; a card is filed by seat or the whole screen    */
/*   shuffles when somebody is knocked out; the head count comes from the     */
/*   frame or it reads 2/40; and a frame carrying no arena must leave the     */
/*   cards it is not talking about exactly where they were; and a card's      */
/*   mask is a projection of a board somebody is really playing.              */
/*                                                                            */
/*   Everything is asserted through net_match_apply, which is the function    */
/*   the screen actually uses - so what is tested is the client's own         */
/*   decode and not a second reading of the wire written for the test.        */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisu.h"

# define ARENA_SEATS	4
/* TETRISD_BR_ARENA_DEAD_EVERY: how often a dead card carries its mask. */
# define ARENA_DEAD_CYCLE	5

// Static Functions
static int	check_the_arena_carries_every_seat(t_net_client *bots,
				t_mp_match_state *view);
static int	check_cards_are_filed_by_seat(t_net_client *bots,
				t_mp_match_state *view);
static int	check_the_counts_are_the_rooms(t_net_client *bots,
				t_mp_match_state *view);
static int	check_a_frame_without_an_arena_keeps_the_cards(t_net_client *bots,
				t_mp_match_state *view);
static int	check_a_card_is_a_projection_of_a_board(t_net_client *bots,
				t_mp_match_state *view);
static int	check_a_dead_card_keeps_its_board(t_net_client *bots,
				t_mp_match_state *view);
static int	stack_a_board(t_net_client *net, int pieces);
static int	local_cells(const t_mp_match_state *view);
static int	local_slot(const t_mp_match_state *view);
static int	top_out(t_net_client *net);
static int	dead_slot(t_net_client *net, t_mp_match_state *view);
static int	wait_dead_mask(t_net_client *net, t_mp_match_state *view,
				int slot);

static int	seat_everyone(t_net_client *bots, char *room, size_t cap);
static int	sign_up_and_in(t_net_client *net, const char *name);
static int	open_royale(t_net_client *owner, char *room, size_t cap);
static int	join_room(t_net_client *net, const char *room);
static int	start_room(t_net_client *net, const char *room);
static int	lock_in(t_net_client *net, const char *room);
static int	wait_playing(t_net_client *net, int tries);
static int	pump_arena(t_net_client *net, t_mp_match_state *view, int tries);
static int	pump_frame(t_net_client *net, t_mp_match_state *view, int tries);
static int	card_cells(const t_mp_match_state *view, int slot);
static void	report(const char *name, int ok, int *failures);

/**
 * @brief Entry point - four players take a Battle Royale and are drawn.
 *
 * @return 0 when every check passed, 1 otherwise.
 */
int	main(void)
{
	t_net_client		bots[ARENA_SEATS];
	t_mp_match_state	*view;
	char				room[NET_ROOM_MAX];
	int					failures;
	int					index;

	view = calloc(ARENA_SEATS, sizeof(*view));
	if (view == NULL)
		return (printf("FAIL: out of memory\n"), 1);
	if (!seat_everyone(bots, room, sizeof(room)))
		return (free(view), printf("FAIL: no Battle Royale room\n"), 1);
	index = 0;
	while (index < ARENA_SEATS)
	{
		view[index].mode = APP_GAME_MODE_BATTLE_ROYALE;
		index++;
	}
	failures = 0;
	report("the arena carries every seat",
		check_the_arena_carries_every_seat(bots, view), &failures);
	report("cards are filed by seat, not by arrival",
		check_cards_are_filed_by_seat(bots, view), &failures);
	report("the head count is the room's own",
		check_the_counts_are_the_rooms(bots, view), &failures);
	report("a frame without an arena keeps the cards",
		check_a_frame_without_an_arena_keeps_the_cards(bots, view), &failures);
	report("a card is a projection of a real board",
		check_a_card_is_a_projection_of_a_board(bots, view), &failures);
	report("a dead card keeps its board between masks",
		check_a_dead_card_keeps_its_board(bots, view), &failures);
	index = 0;
	while (index < ARENA_SEATS)
		net_disconnect(&bots[index++]);
	free(view);
	return (failures != 0);
}

/*
** The roster is the card list. A client replaces its whole arena from each
** push, so every occupied seat has to be in it - including the recipient's
** own, which is what makes the arena a picture of the room rather than a
** picture of everybody else.
*/
static int	check_the_arena_carries_every_seat(t_net_client *bots,
		t_mp_match_state *view)
{
	int	present;
	int	slot;

	if (!pump_arena(&bots[0], &view[0], 400))
		return (0);
	present = 0;
	slot = 0;
	while (slot < APP_ROOM_MAX_PLAYERS)
	{
		if (view[0].opponents[slot].present)
			present++;
		slot++;
	}
	if (present != ARENA_SEATS)
		return (printf("    arena had %d seats, expected %d\n", present,
				ARENA_SEATS), 0);
	return (1);
}

/*
** Filed by seat, and every client agrees which seat that is. Filed by arrival
** order instead, a rival would slide across the screen the moment anybody
** above them was knocked out - and the slot is what a target, an attacker and
** a knockout line will all name later.
**
** The local card is the one that proves the indexing rather than a count: each
** of the four clients must find its own card in a different seat, and each
** must find exactly one.
*/
static int	check_cards_are_filed_by_seat(t_net_client *bots,
		t_mp_match_state *view)
{
	int	seen[ARENA_SEATS];
	int	index;
	int	slot;

	index = 0;
	while (index < ARENA_SEATS)
	{
		if (!pump_arena(&bots[index], &view[index], 400))
			return (0);
		seen[index] = -1;
		slot = 0;
		while (slot < APP_ROOM_MAX_PLAYERS)
		{
			if (view[index].opponents[slot].local)
			{
				if (seen[index] >= 0)
					return (printf("    two local cards\n"), 0);
				seen[index] = slot;
			}
			slot++;
		}
		if (seen[index] < 0)
			return (printf("    client %d has no card of its own\n",
					index), 0);
		index++;
	}
	index = 1;
	while (index < ARENA_SEATS)
	{
		if (seen[index] == seen[index - 1])
			return (printf("    two clients share seat %d\n", seen[index]), 0);
		index++;
	}
	return (1);
}

/*
** The count is the room's and rides every frame, because it cannot be counted
** from the cards: most frames carry no arena, so a client counting them would
** read ALIVE 0/0 between pushes. Deriving it from the opponents section - what
** the client used to do - reads ALIVE 2/40, because that section carries one.
*/
static int	check_the_counts_are_the_rooms(t_net_client *bots,
		t_mp_match_state *view)
{
	int	index;

	index = 0;
	while (index < ARENA_SEATS)
	{
		if (!pump_frame(&bots[index], &view[index], 400))
			return (0);
		if (view[index].players_total != ARENA_SEATS)
			return (printf("    client %d saw %d players, expected %d\n",
					index, view[index].players_total, ARENA_SEATS), 0);
		if (view[index].players_alive != ARENA_SEATS)
			return (printf("    client %d saw %d alive, expected %d\n",
					index, view[index].players_alive, ARENA_SEATS), 0);
		index++;
	}
	return (1);
}

/*
** `arena absent` means "nothing about the arena this frame", and most frames
** say it. Reading it as an empty arena would clear every card between pushes
** and the whole screen would blink at the board's rate rather than the
** arena's - which is what happened while the opponents section still cleared
** the cards it no longer fills.
**
** So: take an arena, then keep pumping until a frame arrives without one, and
** the cards must still be there.
*/
static int	check_a_frame_without_an_arena_keeps_the_cards(t_net_client *bots,
		t_mp_match_state *view)
{
	int	before;
	int	tries;

	if (!pump_arena(&bots[0], &view[0], 400))
		return (0);
	before = card_cells(&view[0], -1);
	tries = 400;
	while (tries > 0)
	{
		if (net_pump(&bots[0]) < 0)
			return (0);
		if (bots[0].has_state && !bots[0].state_snapshot.arena_present)
		{
			net_match_apply(&bots[0], &view[0]);
			if (card_cells(&view[0], -1) != before)
				return (printf("    an absent arena changed the cards\n"), 0);
			return (1);
		}
		if (bots[0].has_state)
			net_match_apply(&bots[0], &view[0]);
		usleep(3000);
		tries--;
	}
	return (printf("    never saw a frame without an arena\n"), 0);
}

/*
** The mask is the whole point of the arena, and it is the one thing nothing
** above would notice the loss of: a mask that were always empty draws every
** card blank, and a complete roster of blank cards passes every check in this
** file. So one client stacks a board and the arena has to show that stack -
** to its owner, in their own card, and to a rival in the same seat.
**
** A card is counted against the board that arrived in the same frame, which
** is what makes the two comparable at all: the arena rides the recipient's
** own snapshot, so a card and the board beside it are one instant by
** construction. The rival's copy is a second frame and a later instant, and
** it is still an equality because at level 1 a piece takes eighteen seconds
** of gravity to land - nothing settles between the two pumps that was not
** hard dropped.
*/
static int	check_a_card_is_a_projection_of_a_board(t_net_client *bots,
		t_mp_match_state *view)
{
	int	mine;
	int	cells;

	if (!stack_a_board(&bots[0], 6))
		return (0);
	if (!pump_arena(&bots[0], &view[0], 400))
		return (0);
	cells = local_cells(&view[0]);
	if (cells == 0)
		return (printf("    the local board came back empty\n"), 0);
	mine = local_slot(&view[0]);
	if (mine < 0)
		return (printf("    no card of my own\n"), 0);
	if (card_cells(&view[0], mine) != cells)
		return (printf("    my card holds %d cells, my board holds %d\n",
				card_cells(&view[0], mine), cells), 0);
	if (!pump_arena(&bots[1], &view[1], 400))
		return (0);
	if (card_cells(&view[1], mine) != cells)
		return (printf("    a rival draws seat %d with %d cells, not %d\n",
				mine, card_cells(&view[1], mine), cells), 0);
	return (1);
}

/*
** A dead board never changes again, so the server stops paying for it: its
** card carries a mask on every fifth push and none in between, and the client
** keeps the last one it holds. A client that instead reads "no mask" as "no
** board" draws a knocked-out player's thumbnail four pushes in five and blank
** for the other four fifths of the match.
**
** What is promised is bounded staleness and not immediacy: the first push
** after an elimination may well be one of the four that carry no mask, and the
** board the client is holding for that seat is then whatever it last saw. The
** cycle is five pushes long, so the buried board is drawn within five of the
** burial - and from there it must never move again, because a dead board
** never does.
**
** So this waits for the mask, then watches the seat across more pushes than
** the cycle is long. A dead card that flickers and a dead card that moves are
** the same failure.
*/
static int	check_a_dead_card_keeps_its_board(t_net_client *bots,
		t_mp_match_state *view)
{
	int	slot;
	int	cells;
	int	pushes;

	if (!top_out(&bots[ARENA_SEATS - 1]))
		return (printf("    could not top a board out\n"), 0);
	slot = dead_slot(&bots[0], &view[0]);
	if (slot < 0)
		return (printf("    no knocked-out card in the arena\n"), 0);
	cells = wait_dead_mask(&bots[0], &view[0], slot);
	if (cells <= 0)
		return (printf("    seat %d stayed blank for a whole cycle\n",
				slot), 0);
	pushes = 0;
	while (pushes < 8)
	{
		if (!pump_arena(&bots[0], &view[0], 400))
			return (printf("    the arena stopped after %d pushes\n",
					pushes), 0);
		if (card_cells(&view[0], slot) != cells)
			return (printf("    push %d drew seat %d with %d cells, not %d\n",
					pushes, slot, card_cells(&view[0], slot), cells), 0);
		pushes++;
	}
	return (1);
}

/**
 * @brief Pumps until a dead seat's board has arrived, within the elision cycle.
 *
 * @param net The connection to watch the room through.
 * @param view The match model to write.
 * @param slot The seat to wait on.
 * @return How many cells that seat's board holds, or 0 if it never arrived.
 */
static int	wait_dead_mask(t_net_client *net, t_mp_match_state *view, int slot)
{
	int	pushes;

	pushes = 0;
	while (pushes < ARENA_DEAD_CYCLE)
	{
		if (card_cells(view, slot) > 0)
			return (card_cells(view, slot));
		if (!pump_arena(net, view, 400))
			return (0);
		pushes++;
	}
	return (card_cells(view, slot));
}

/**
 * @brief Hard drops until the board tops out.
 *
 * The refusal is the signal: an input into a game that is over is answered
 * 409, which is how a test reads an elimination without a screen to see it on.
 *
 * @param net The connection to bury.
 * @return 1 when the board topped out, 0 when it never did.
 */
static int	top_out(t_net_client *net)
{
	t_net_result	result;
	int				guard;

	guard = 0;
	while (guard < 400)
	{
		if (net_match_action(net, SOLO_HARD_DROP, &result) != 0
			&& result.status != 409)
			return (0);
		if (result.status == 409)
			return (1);
		if (net_pump(net) < 0)
			return (0);
		usleep(20000);
		guard++;
	}
	return (0);
}

/**
 * @brief Pumps until the arena carries a card with its alive bit clear.
 *
 * @param net The connection to watch the room through.
 * @param view The match model to write.
 * @return The seat that card sits in, or -1 when none arrived.
 */
static int	dead_slot(t_net_client *net, t_mp_match_state *view)
{
	int	tries;
	int	slot;

	tries = 40;
	while (tries-- > 0)
	{
		if (!pump_arena(net, view, 400))
			return (-1);
		slot = 0;
		while (slot < APP_ROOM_MAX_PLAYERS)
		{
			if (view->opponents[slot].present && !view->opponents[slot].alive)
				return (slot);
			slot++;
		}
	}
	return (-1);
}

/**
 * @brief Hard drops a number of pieces, letting each one settle.
 *
 * The wait is the settle: a drop is answered before the tick that encodes
 * what it did, so a drop sent on the reply to the last one is a drop the
 * arena has not been told about yet.
 *
 * @param net The connection to play.
 * @param pieces How many pieces to drop.
 * @return 1 when every drop was accepted, 0 otherwise.
 */
static int	stack_a_board(t_net_client *net, int pieces)
{
	t_net_result	result;
	int				tries;

	while (pieces > 0)
	{
		if (net_match_action(net, SOLO_HARD_DROP, &result) != 0
			|| result.status != 200)
			return (printf("    a drop was answered %d\n", result.status), 0);
		tries = 40;
		while (tries-- > 0)
		{
			if (net_pump(net) < 0)
				return (0);
			usleep(3000);
		}
		pieces--;
	}
	return (1);
}

/**
 * @brief Finds the seat this client's own card sits in.
 *
 * @param view The match model to read.
 * @return The slot, or -1 when the arena carries no card of this client's.
 */
static int	local_slot(const t_mp_match_state *view)
{
	int	slot;

	slot = 0;
	while (slot < APP_ROOM_MAX_PLAYERS)
	{
		if (view->opponents[slot].local)
			return (slot);
		slot++;
	}
	return (-1);
}

/**
 * @brief Counts filled cells on the client's own authoritative board.
 *
 * @param view The match model to read.
 * @return How many cells are filled.
 */
static int	local_cells(const t_mp_match_state *view)
{
	int	total;
	int	row;
	int	col;

	total = 0;
	row = 0;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
			total += board_get(&view->local_game.board, col++, row).type
				!= CELL_EMPTY;
		row++;
	}
	return (total);
}

/**
 * @brief Counts filled cells across one card, or across every card.
 *
 * @param view The match model to read.
 * @param slot The seat to count, or -1 for all of them.
 * @return How many cells are filled.
 */
static int	card_cells(const t_mp_match_state *view, int slot)
{
	int	total;
	int	index;
	int	row;
	int	col;

	total = 0;
	index = 0;
	while (index < APP_ROOM_MAX_PLAYERS)
	{
		if (view->opponents[index].present && (slot < 0 || slot == index))
		{
			row = 0;
			while (row < BOARD_HEIGHT)
			{
				col = 0;
				while (col < BOARD_WIDTH)
					total += board_get(&view->opponents[index].board,
							col++, row).type != CELL_EMPTY;
				row++;
			}
		}
		index++;
	}
	return (total);
}

/**
 * @brief Signs four players in, seats them in one room, and starts the match.
 *
 * @param bots Receives the connections; bots[0] creates and owns the room.
 * @param room Receives the room's name.
 * @param cap Size of room.
 * @return 1 when the match is running, 0 otherwise.
 */
static int	seat_everyone(t_net_client *bots, char *room, size_t cap)
{
	t_net_config	cfg;
	char			name[NET_USER_MAX];
	int				index;

	net_config_load(&cfg);
	index = 0;
	while (index < ARENA_SEATS)
	{
		memset(&bots[index], 0, sizeof(bots[index]));
		snprintf(name, sizeof(name), "br%d_%d", index, (int)getpid());
		if (net_connect(&bots[index], &cfg) != 0
			|| !sign_up_and_in(&bots[index], name))
			return (0);
		index++;
	}
	if (!open_royale(&bots[0], room, cap))
		return (0);
	index = 1;
	while (index < ARENA_SEATS)
		if (!join_room(&bots[index++], room))
			return (0);
	if (!start_room(&bots[0], room))
		return (0);
	index = 0;
	while (index < ARENA_SEATS)
		if (!lock_in(&bots[index++], room))
			return (0);
	index = 0;
	while (index < ARENA_SEATS)
		if (!wait_playing(&bots[index++], 1200))
			return (0);
	return (1);
}

static int	sign_up_and_in(t_net_client *net, const char *name)
{
	t_net_result	result;

	if (net_signup(net, name, "hunter2", &result) != 0 || result.status != 201)
		return (0);
	if (net_login(net, name, "hunter2", &result) != 0 || result.status != 200)
		return (0);
	snprintf(net->username, sizeof(net->username), "%s", name);
	return (1);
}

static int	open_royale(t_net_client *owner, char *room, size_t cap)
{
	t_net_result	result;

	if (net_request(owner, "JOIN", TETRISU_ROUTE_ROOMS, "mode br\n",
			&result) != 0 || result.status != 201)
		return (0);
	if (net_result_field(&result, "room", room, cap) == NULL)
		return (0);
	owner->state = NET_IN_ROOM;
	return (net_match_join(owner, room) == 0);
}

static int	join_room(t_net_client *net, const char *room)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "JOIN", path, NULL, &result) != 0
		|| result.status != 200)
		return (0);
	net->state = NET_IN_ROOM;
	return (net_match_join(net, room) == 0);
}

static int	start_room(t_net_client *net, const char *room)
{
	t_net_result	result;
	char			path[NET_PATH_MAX];

	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	if (net_request(net, "START", path, NULL, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

static int	lock_in(t_net_client *net, const char *room)
{
	t_body_profile	profile;
	t_net_result	result;
	char			path[NET_PATH_MAX];
	char			body[64];

	if (net_profile(net, &profile) != 0)
		return (0);
	snprintf(path, sizeof(path), "%s%s", TETRISU_ROUTE_ROOM, room);
	snprintf(body, sizeof(body), "ready 1\ncharacter %u\n",
		profile.equipped_character);
	if (net_request(net, "READY", path, body, &result) != 0
		|| result.status != 200)
		return (0);
	return (1);
}

static int	wait_playing(t_net_client *net, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state
			&& net->state_snapshot.phase != BODY_PHASE_COUNTDOWN)
			return (1);
		usleep(5000);
		tries--;
	}
	return (0);
}

/**
 * @brief Pumps until a frame carrying an arena has been applied.
 *
 * The frame has to be one this client has not applied yet, which is what
 * net_solo_pending answers and what `has_state` does not: a client holds its
 * last snapshot for good, so a caller asking twice for an arena would be
 * handed the same one twice and would be watching nothing.
 *
 * @param net The connection.
 * @param view The match model to write.
 * @param tries How many polls to spend.
 * @return 1 when an arena was applied, 0 otherwise.
 */
static int	pump_arena(t_net_client *net, t_mp_match_state *view, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net_solo_pending(net))
		{
			if (net->state_snapshot.arena_present)
			{
				net_match_apply(net, view);
				return (1);
			}
			net_match_apply(net, view);
		}
		usleep(3000);
		tries--;
	}
	return (0);
}

/**
 * @brief Pumps until any frame has been applied.
 *
 * @param net The connection.
 * @param view The match model to write.
 * @param tries How many polls to spend.
 * @return 1 when a frame was applied, 0 otherwise.
 */
static int	pump_frame(t_net_client *net, t_mp_match_state *view, int tries)
{
	while (tries > 0)
	{
		if (net_pump(net) < 0)
			return (0);
		if (net->has_state)
		{
			net_match_apply(net, view);
			return (1);
		}
		usleep(3000);
		tries--;
	}
	return (0);
}

static void	report(const char *name, int ok, int *failures)
{
	printf("  %-46s %s\n", name, ok ? "PASS" : "FAIL");
	if (!ok)
		(*failures)++;
}
