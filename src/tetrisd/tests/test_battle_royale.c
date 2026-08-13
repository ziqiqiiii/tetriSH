/* ************************************************************************** */
/*                                                                            */
/*   test_battle_royale.c - many players, one room, one owner                  */
/*                                                                            */
/*   What separates a Battle Royale from a Double room is that it starts       */
/*   below capacity: everybody present being ready says nothing about          */
/*   whether the match should begin, so the owner decides, and one player      */
/*   leaving is an ordinary event rather than the end of the match. Both       */
/*   rules are asserted here from a client's point of view only.               */
/*                                                                            */
/*   The other half of the mode is what happens when a player stops: a        */
/*   placing taken at the elimination rather than at the end, shared by        */
/*   everybody who went out on one tick, and a knockout credited to whoever    */
/*   last landed rows on them.                                                 */
/*                                                                            */
/*   One case is deliberately not here. A knockout credited to a real          */
/*   attacker needs one player's rows to land on another, which needs a line   */
/*   clear, which needs a client that can place pieces rather than drop them   */
/*   where they fall. What a top-out attributes to is asserted where it is     */
/*   decided instead - test_garbage.c drives the queue, the lock and the       */
/*   report - and what is asserted here is the half that has no attacker:      */
/*   a player who buried themselves is credited to nobody.                     */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

# define BR_SEATS	5

// Static Functions
static void	test_readiness_alone_does_not_start_a_battle_royale(void);
static void	test_the_owner_starts_a_battle_royale(void);
static void	test_a_non_owner_cannot_start_a_battle_royale(void);
static void	test_a_departure_does_not_cancel_a_crowded_window(void);
static void	test_a_departure_below_the_minimum_closes_the_window(void);
static void	test_the_last_undecided_player_leaving_deals_the_match(void);
static void	test_a_match_sends_an_arena_of_every_seat(void);
static void	test_the_arena_is_slower_than_the_board(void);
static void	test_counts_ride_every_frame(void);
static void	test_a_placing_is_taken_when_a_player_goes_out(void);
static void	test_players_out_together_share_a_placing(void);
static void	test_a_top_out_nobody_caused_credits_nobody(void);
static void	test_a_quitter_keeps_the_placing_they_quit_at(void);
static void	test_a_targeting_mode_is_declared_and_kept(void);
static void	test_a_target_outside_a_battle_royale_is_refused(void);
static void	test_a_mode_marks_the_rivals_it_singles_out(void);
static void	test_a_card_tells_garbage_from_a_piece(void);

static int	start_match(t_fixture *fx, t_harness *players, int count,
				char *room, size_t cap, char *path, size_t path_cap);
static int	wait_arena(t_harness *hc, t_body_state *out, int timeout_ms);
static int	wait_phase(t_harness *hc, t_body_state *out, t_body_phase phase,
				int window_ms);
static int	seat_many(t_fixture *fx, t_harness *players, int count,
				char *room, size_t cap);
static int	simple(t_harness *hc, const char *method, const char *path,
				const char *body);
static int	list_room(t_harness *hc, const char *path, t_body_room *out);
static void	ready_all(t_harness *players, int count, const char *path);
static void	close_all(t_harness *players, int count);
static void	play_path(char *out, size_t cap, const char *room,
				t_player_id pid);
static int	bury(t_harness *hc, const char *room);
static int	drop_pieces(t_harness *hc, const char *room, int pieces);
static int	stack_high(t_harness *hc, const char *room);
static int	top_filled_row(const t_body_state *snap);
static int	wait_rank(t_harness *hc, t_body_state *out, int timeout_ms);
static int	wait_chat_line(t_harness *hc, const char *needle, int timeout_ms);
static int	card_of(const t_body_state *snap, t_player_id pid);

int	main(void)
{
	test_readiness_alone_does_not_start_a_battle_royale();
	test_the_owner_starts_a_battle_royale();
	test_a_non_owner_cannot_start_a_battle_royale();
	test_a_departure_does_not_cancel_a_crowded_window();
	test_a_departure_below_the_minimum_closes_the_window();
	test_the_last_undecided_player_leaving_deals_the_match();
	test_a_match_sends_an_arena_of_every_seat();
	test_the_arena_is_slower_than_the_board();
	test_counts_ride_every_frame();
	test_a_placing_is_taken_when_a_player_goes_out();
	test_players_out_together_share_a_placing();
	test_a_top_out_nobody_caused_credits_nobody();
	test_a_quitter_keeps_the_placing_they_quit_at();
	test_a_targeting_mode_is_declared_and_kept();
	test_a_target_outside_a_battle_royale_is_refused();
	test_a_mode_marks_the_rivals_it_singles_out();
	test_a_card_tells_garbage_from_a_piece();
	return (0);
}

/*
** Readiness commits a Double room because two players agreeing is the whole of
** that room's decision. A Battle Royale starts below capacity by design, so
** "everybody here is ready" is true of five people in a ninety-nine seat room
** and is not an answer to whether the match should begin.
**
** It used to be treated as one: the room opened its character-select window
** the moment the last person present declared, so a room filling up started
** itself under whoever happened to be joining.
*/
static void	test_readiness_alone_does_not_start_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(list_room(&players[0], path, &snapshot) == 200);
	/* committed as far as it goes, and waiting for somebody to say so */
	assert(snapshot.status == BODY_ROOM_READY);
	assert(snapshot.select_ms == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_readiness_alone_does_not_start_a_battle_royale\n");
}

/*
** The owner's START is what opens the window, which is the same route Double
** reaches a match by and the same verdict - what changes is only that nothing
** else opens it first.
*/
static void	test_the_owner_starts_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	assert(list_room(&players[0], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.select_ms > 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_the_owner_starts_a_battle_royale\n");
}

/*
** Whose call it is matters more here than in Double, because in a room of
** ninety-nine anybody could otherwise start a match the other ninety-eight
** were not finished assembling for.
*/
static void	test_a_non_owner_cannot_start_a_battle_royale(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[3], "START", path, NULL) == 403);
	assert(list_room(&players[0], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_READY);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_non_owner_cannot_start_a_battle_royale\n");
}

/*
** One player leaving used to cancel the window for everybody, unconditionally.
** In Double that is right - there is no match without the second player - but
** in a Battle Royale it handed any one of thirty people a cancel button, and a
** disconnect two seconds into character select ended the match for the rest.
**
** Above min_to_start the room is still startable, so the window keeps its own
** clock. The owner leaving is the sharper version of the same case, because
** succession moves somebody into their seat while the window is open.
*/
static void	test_a_departure_does_not_cancel_a_crowded_window(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* the owner walks out of the room they just started */
	assert(simple(&players[0], "LEAVE", path, NULL) == 200);
	assert(list_room(&players[1], path, &snapshot) == 200);
	assert(snapshot.status == BODY_ROOM_SELECTING);
	assert(snapshot.select_ms > 0);
	assert(snapshot.member_count == BR_SEATS - 1);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_departure_does_not_cancel_a_crowded_window\n");
}

/*
** The other half of the same rule, and the reason it is one rule rather than a
** Battle Royale exception: below min_to_start the room cannot deal the match
** it was setting up, so the window closes and whoever is left goes back to
** waiting rather than sitting on a roster screen behind a clock with nothing
** on the other side of it.
*/
static void	test_a_departure_below_the_minimum_closes_the_window(void)
{
	t_body_room	snapshot;
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];
	int			i;

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* down to three, one under a Battle Royale's floor of four */
	i = 0;
	while (i < BR_SEATS - 3)
	{
		assert(simple(&players[i], "LEAVE", path, NULL) == 200);
		i++;
	}
	assert(list_room(&players[BR_SEATS - 1], path, &snapshot) == 200);
	assert(snapshot.status != BODY_ROOM_SELECTING);
	assert(snapshot.select_ms == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_departure_below_the_minimum_closes_the_window\n");
}

/*
** A window ends early when every seat has chosen. If the one seat still
** deciding leaves, the room is waiting for nobody - so the departure has to be
** re-asked, or the room sits out the full select clock before dealing a match
** everybody remaining had already locked in for.
*/
static void	test_the_last_undecided_player_leaving_deals_the_match(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	int				i;

	assert(fx_start(&fx) == 0);
	assert(seat_many(&fx, players, BR_SEATS, room, sizeof(room)) == 0);
	snprintf(path, sizeof(path), "/room/%s", room);
	ready_all(players, BR_SEATS, path);
	assert(simple(&players[0], "START", path, NULL) == 200);
	/* everybody but the last seat locks in a fighter */
	i = 0;
	while (i < BR_SEATS - 1)
	{
		assert(hc_lock_in(&players[i], &fx, path) == 200);
		i++;
	}
	assert(simple(&players[BR_SEATS - 1], "LEAVE", path, NULL) == 200);
	/* dealt at once, without waiting out TETRISD_MATCH_SELECT_MS */
	assert(hc_wait_state(&players[0], &state, HC_TIMEOUT_MS) == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_the_last_undecided_player_leaving_deals_the_match\n");
}

/**
 * @brief Signs up `count` players and seats them all in one Battle Royale.
 *
 * @param fx Running fixture.
 * @param players Receives the connections; players[0] creates and owns.
 * @param count How many to seat.
 * @param room Receives the room's name.
 * @param cap Size of room.
 * @return 0 when every player is seated, -1 otherwise.
 */
static int	seat_many(t_fixture *fx, t_harness *players, int count,
		char *room, size_t cap)
{
	char	name[32];
	char	path[64];
	int		i;

	i = 0;
	while (i < count)
	{
		snprintf(name, sizeof(name), "player%d", i);
		if (hc_connect(&players[i], fx) != 0
			|| hc_signup(&players[i], name, "hunter2") != 201
			|| hc_login(&players[i], name, "hunter2") != 200)
			return (-1);
		i++;
	}
	if (hc_join_new(&players[0], "br", room, cap) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	i = 1;
	while (i < count)
	{
		if (simple(&players[i], "JOIN", path, NULL) != 200)
			return (-1);
		i++;
	}
	return (0);
}

/**
 * @brief Declares every seated player ready, without naming a fighter.
 *
 * @param players The connections.
 * @param count How many there are.
 * @param path The room's path.
 */
static void	ready_all(t_harness *players, int count, const char *path)
{
	int	i;

	i = 0;
	while (i < count)
	{
		assert(simple(&players[i], "READY", path, "ready 1\n") == 200);
		i++;
	}
}

/**
 * @brief Closes every connection.
 *
 * @param players The connections.
 * @param count How many there are.
 */
static void	close_all(t_harness *players, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		hc_close(&players[i]);
		i++;
	}
}

/**
 * @brief Sends one request and reports only its status.
 *
 * @param hc The connection.
 * @param method HTTTP method.
 * @param path Request path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 on a transport failure.
 */
static int	simple(t_harness *hc, const char *method, const char *path,
		const char *body)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, method, path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Reads a room snapshot.
 *
 * @param hc The connection.
 * @param path The room's path.
 * @param out Receives the decoded snapshot.
 * @return The status code, or -1 on a transport failure.
 */
static int	list_room(t_harness *hc, const char *path, t_body_room *out)
{
	t_htttp_message	resp;
	int				status;

	if (hc_request(hc, "LIST", path, NULL, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	if (status == 200 && resp.body != NULL)
		body_room_decode((const char *)resp.body, resp.body_len, out);
	htttp_message_free(&resp);
	return (status);
}

/*
** The arena is the mode. Ninety-eight rivals cannot ride the opponents section
** - one full board is 496 bytes and BODY_OPPONENTS_MAX is 1 on purpose - so
** they ride a second detail level, one card per occupied seat, each carrying
** the silhouette of a stack rather than a board.
**
** Every seat appears, including the local player's own: the card list is the
** roster, so a client replaces its whole arena from a push and a seat that
** does not appear is a seat nobody is in.
*/
static void	test_a_match_sends_an_arena_of_every_seat(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	size_t			i;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	assert(state.arena_count == (size_t)BR_SEATS);
	/* the opponents section stays Double's, and stays empty here */
	assert(state.opponent_count == 0);
	i = 0;
	while (i < state.arena_count)
	{
		assert(state.arena[i].player_id != 0);
		assert(state.arena[i].flags & BODY_ARENA_ALIVE);
		/* a live card always carries its board */
		assert(state.arena[i].flags & BODY_ARENA_MASK_PRESENT);
		assert(state.arena[i].cells_valid);
		assert(state.arena[i].rank == 0);
		i++;
	}
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_match_sends_an_arena_of_every_seat\n");
}

/*
** A card says what each cell *is*, not merely that something is there.
**
** It was one bit per cell until a player reported that every block in the mode
** drew grey, and it could not have drawn anything else: a silhouette cannot
** tell a piece somebody placed from a row somebody else sent them. So the
** codes are asserted rather than the occupancy - 1 for garbage, 2 upward for a
** piece in the seven types' own order - because "not zero" is exactly the
** assertion that passed all the way through the bug.
*/
static void	test_a_card_tells_garbage_from_a_piece(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	char			play[128];
	int				garbage;
	int				piece;
	int				row;
	int				col;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	play_path(play, sizeof(play), room, players[0].player_id);
	assert(hc_send(&players[0], "DROP", play, "HARD\n") == 0);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	assert(state.arena[0].cells_valid);
	garbage = 0;
	piece = 0;
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			garbage += (state.arena[0].cells[row][col] == 1);
			piece += (state.arena[0].cells[row][col] >= 2);
			col++;
		}
		row++;
	}
	/* one tetromino landed and nobody has sent anybody anything, so the card
	** carries exactly four piece cells and no garbage at all */
	assert(piece == 4);
	assert(garbage == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_card_tells_garbage_from_a_piece\n");
}

/*
** The arena has a clock of its own, and that is the whole reason the mode is
** affordable: every client is sent every card, so the cost grows with the
** square of the room, and the cadence is the one lever that divides all of it.
**
** So frames outnumber arenas. A player's own board is pushed the instant it
** changes - that is the thing they are steering - while the thumbnails they
** glance at arrive at TETRISD_BR_ARENA_MS. A frame carrying no arena says
** `absent` and the client keeps what it has, which is not the same as an
** arena of zero cards and must not be read as one.
*/
static void	test_the_arena_is_slower_than_the_board(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	int				frames;
	int				arenas;
	int				waited;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	frames = 0;
	arenas = 0;
	waited = 0;
	while (waited < 4000 && frames < 60)
	{
		if (hc_wait_state(&players[0], &state, 250) != 0)
		{
			waited += 250;
			continue ;
		}
		if (state.arena_present)
		{
			arenas++;
			assert(state.arena_count == (size_t)BR_SEATS);
		}
		else
			assert(state.arena_count == 0);
		frames++;
	}
	assert(arenas > 0);
	/* a board pushed on change outruns an arena pushed on a clock */
	assert(arenas < frames);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_the_arena_is_slower_than_the_board (%d arenas in %d "
		"frames)\n", arenas, frames);
}

/*
** The head count cannot be recovered from the arena, because most frames do
** not carry one - a client counting cards would read ALIVE 0/0 between pushes
** and the number the whole mode is played against would flicker. So both
** numbers ride every frame, arena or not.
*/
static void	test_counts_ride_every_frame(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	int				frames;
	int				waited;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	frames = 0;
	waited = 0;
	while (waited < 3000 && frames < 12)
	{
		if (hc_wait_state(&players[0], &state, 250) != 0)
		{
			waited += 250;
			continue ;
		}
		assert(state.players == BR_SEATS);
		assert(state.alive == BR_SEATS);
		frames++;
	}
	assert(frames > 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_counts_ride_every_frame\n");
}

/**
 * @brief Seats everyone, starts the room, and plays the countdown out.
 *
 * @param fx Receives the started fixture.
 * @param players Receives the connections.
 * @param count How many to seat.
 * @param room Receives the room name.
 * @param cap Size of room.
 * @param path Receives the room path.
 * @param path_cap Size of path.
 * @return 0 when the match is running, -1 otherwise.
 */
static int	start_match(t_fixture *fx, t_harness *players, int count,
		char *room, size_t cap, char *path, size_t path_cap)
{
	t_body_state	state;
	int				i;

	if (fx_start(fx) != 0
		|| seat_many(fx, players, count, room, cap) != 0)
		return (-1);
	snprintf(path, path_cap, "/room/%s", room);
	ready_all(players, count, path);
	if (simple(&players[0], "START", path, NULL) != 200)
		return (-1);
	i = 0;
	while (i < count)
	{
		if (hc_lock_in(&players[i], fx, path) != 200)
			return (-1);
		i++;
	}
	/* the room holds every board still for its 3-2-1 before anything moves */
	if (wait_phase(&players[0], &state, BODY_PHASE_ACTIVE,
			TETRISD_MATCH_COUNTDOWN_MS + HC_TIMEOUT_MS) != 0)
		return (-1);
	return (0);
}

/**
 * @brief Waits for a frame that actually carries an arena.
 *
 * @param hc The connection.
 * @param out Receives the frame.
 * @param timeout_ms How long to wait in total.
 * @return 0 when one arrived, -1 on timeout.
 */
static int	wait_arena(t_harness *hc, t_body_state *out, int timeout_ms)
{
	int	waited;

	waited = 0;
	while (waited < timeout_ms)
	{
		if (hc_wait_state(hc, out, 200) != 0)
		{
			waited += 200;
			continue ;
		}
		if (out->arena_present)
			return (0);
		waited += 20;
	}
	return (-1);
}

/**
 * @brief Waits for a frame in a given phase.
 *
 * @param hc The connection.
 * @param out Receives the frame.
 * @param phase The phase being waited for.
 * @param window_ms How long to wait in total.
 * @return 0 when one arrived, -1 on timeout.
 */
static int	wait_phase(t_harness *hc, t_body_state *out, t_body_phase phase,
		int window_ms)
{
	int	waited;

	waited = 0;
	while (waited < window_ms)
	{
		if (hc_wait_state(hc, out, 200) != 0)
		{
			waited += 200;
			continue ;
		}
		if (out->phase == phase)
			return (0);
		waited += 20;
	}
	return (-1);
}

/*
** A placing is a fact at the moment it is taken and an opinion afterwards.
**
** It used to be written for everybody at once when the match ended, out of
** the number of boards still running - so in a twenty-player match all
** nineteen losers were second. Taken at the elimination it is the number of
** players who were still in the match when this one stopped being one, which
** is what "#4 of 5" means and the only moment it can be counted.
**
** So it arrives in the eliminated player's own snapshot with no result beside
** it: the match is not over, they are out of it, and they are watching. The
** verdict comes later and to everybody at once, which is what a verdict is.
*/
static void	test_a_placing_is_taken_when_a_player_goes_out(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(bury(&players[BR_SEATS - 1], room) == 0);
	assert(wait_rank(&players[BR_SEATS - 1], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS);
	/* out of the match, and not yet told how it ended - that is spectating */
	assert(state.result == BODY_RESULT_NONE);
	assert(state.phase == BODY_PHASE_TOP_OUT);
	/* the room is one player lighter on everybody's frame */
	assert(state.alive == BR_SEATS - 1);
	assert(bury(&players[BR_SEATS - 2], room) == 0);
	assert(wait_rank(&players[BR_SEATS - 2], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS - 1);
	assert(state.result == BODY_RESULT_NONE);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_placing_is_taken_when_a_player_goes_out\n");
}

/*
** Two boards can stop inside one authoritative tick, and there is nothing to
** separate them but the order the sweep walks the seats in. Letting that
** decide would mean seat 4 places 4th and seat 5 places 5th for identical
** deaths - an array index deciding a result.
**
** So they share a placing and the next elimination skips the number they took,
** which is how every sport handles a tie.
**
** Both boards are buried in one burst of send-only requests, because that is
** the only way a request/response client can express "at the same time": a
** round trip between the two is long enough for a tick to land in.
*/
static void	test_players_out_together_share_a_placing(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	char			play[96];
	int				i;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(stack_high(&players[BR_SEATS - 1], room) == 0);
	assert(stack_high(&players[BR_SEATS - 2], room) == 0);
	i = 0;
	while (i < 12)
	{
		play_path(play, sizeof(play), room, players[BR_SEATS - 1].player_id);
		assert(hc_send(&players[BR_SEATS - 1], "DROP", play, "HARD\n") == 0);
		play_path(play, sizeof(play), room, players[BR_SEATS - 2].player_id);
		assert(hc_send(&players[BR_SEATS - 2], "DROP", play, "HARD\n") == 0);
		i++;
	}
	assert(wait_rank(&players[BR_SEATS - 1], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS - 1);
	assert(wait_rank(&players[BR_SEATS - 2], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS - 1);
	/* the pair took 4th and 5th between them, so the next one out is 3rd */
	assert(bury(&players[BR_SEATS - 3], room) == 0);
	assert(wait_rank(&players[BR_SEATS - 3], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS - 2);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_players_out_together_share_a_placing\n");
}

/*
** A player who buried themselves is nobody's knockout. Crediting the count to
** whoever happened to have attacked them last would make it a measure of luck
** rather than of aggression, and the feed would name somebody who did nothing.
**
** The elimination is still narrated, because the room has to be able to read
** who is left, and the arena still draws the card dead with its placing on it.
*/
static void	test_a_top_out_nobody_caused_credits_nobody(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	int				card;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(bury(&players[BR_SEATS - 1], room) == 0);
	assert(wait_rank(&players[BR_SEATS - 1], &state, HC_TIMEOUT_MS) == 0);
	assert(wait_chat_line(&players[0], "was knocked out", HC_TIMEOUT_MS) == 0);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	card = card_of(&state, players[BR_SEATS - 1].player_id);
	assert(card >= 0);
	/* dead, placed, and credited to nobody */
	assert((state.arena[card].flags & BODY_ARENA_ALIVE) == 0);
	assert(state.arena[card].rank == BR_SEATS);
	assert(state.arena[card].ko == 0);
	card = card_of(&state, players[0].player_id);
	assert(card >= 0 && state.arena[card].ko == 0);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_top_out_nobody_caused_credits_nobody\n");
}

/*
** Quitting in fourth place records fourth place. The placing is taken when a
** player stops being in the match, and leaving is one of the ways to stop -
** so it is taken here too, before the seat is released and the board is reset,
** which is the last moment there is anything left to say they were playing.
**
** The match plays on. One player leaving a Battle Royale is an ordinary event.
*/
static void	test_a_quitter_keeps_the_placing_they_quit_at(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(simple(&players[BR_SEATS - 1], "LEAVE", path, NULL) == 200);
	/* the room is four players and still playing */
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	assert(state.arena_count == (size_t)BR_SEATS - 1);
	assert(state.alive == BR_SEATS - 1);
	assert(state.result == BODY_RESULT_NONE);
	/* and the next player out takes the placing below the one who left */
	assert(bury(&players[BR_SEATS - 2], room) == 0);
	assert(wait_rank(&players[BR_SEATS - 2], &state, HC_TIMEOUT_MS) == 0);
	assert(state.rank == BR_SEATS - 1);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_quitter_keeps_the_placing_they_quit_at\n");
}

/**
 * @brief Writes the path one player's inputs are addressed to.
 *
 * @param out Receives the path.
 * @param cap Size of out.
 * @param room The room's name.
 * @param pid The player.
 */
static void	play_path(char *out, size_t cap, const char *room, t_player_id pid)
{
	snprintf(out, cap, "/room/%s/player/%llu", room,
		(unsigned long long)pid);
}

/**
 * @brief Hard drops until the board refuses, which is how a top-out reads.
 *
 * @param hc The connection to bury.
 * @param room The room being played in.
 * @return 0 when the board topped out, -1 when it never did.
 */
static int	bury(t_harness *hc, const char *room)
{
	char	play[96];
	int		guard;

	play_path(play, sizeof(play), room, hc->player_id);
	guard = 0;
	while (guard < 400)
	{
		if (simple(hc, "DROP", play, "HARD\n") == 409)
			return (0);
		guard++;
	}
	return (-1);
}

/**
 * @brief Stacks a board to within a few rows of the ceiling and stops there.
 *
 * The point is to leave the board one short burst from topping out, so that a
 * later burst can take two of them out inside one tick. Stopping early is what
 * makes that burst short enough to fit in one.
 *
 * @param hc The connection to stack.
 * @param room The room being played in.
 * @return 0 when the board is near the ceiling, -1 when it never got there.
 */
static int	stack_high(t_harness *hc, const char *room)
{
	t_body_state	state;
	char			play[96];
	int				guard;

	play_path(play, sizeof(play), room, hc->player_id);
	guard = 0;
	while (guard < 400)
	{
		if (simple(hc, "DROP", play, "HARD\n") == 409)
			return (0);
		if (hc_wait_state(hc, &state, 200) == 0 && top_filled_row(&state) <= 5)
			return (0);
		guard++;
	}
	return (-1);
}

/**
 * @brief Finds the highest row of a board that has anything in it.
 *
 * @param snap The snapshot to read.
 * @return The row index, or BODY_BOARD_ROWS for an empty board.
 */
static int	top_filled_row(const t_body_state *snap)
{
	int	row;
	int	col;

	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		col = 0;
		while (col < BODY_BOARD_COLS)
		{
			if (snap->cells[row][col].type != 0)
				return (row);
			col++;
		}
		row++;
	}
	return (BODY_BOARD_ROWS);
}

/**
 * @brief Waits for a frame carrying this player's placing.
 *
 * @param hc The connection.
 * @param out Receives the frame.
 * @param timeout_ms How long to wait in total.
 * @return 0 when one arrived, -1 on timeout.
 */
static int	wait_rank(t_harness *hc, t_body_state *out, int timeout_ms)
{
	int	waited;

	waited = 0;
	while (waited < timeout_ms)
	{
		if (hc_wait_state(hc, out, 200) != 0)
		{
			waited += 200;
			continue ;
		}
		if (out->rank != 0)
			return (0);
		waited += 20;
	}
	return (-1);
}

/**
 * @brief Waits for a feed line containing some text.
 *
 * @param hc The connection.
 * @param needle The text the line has to contain.
 * @param timeout_ms How long to wait in total.
 * @return 0 when such a line arrived, -1 on timeout.
 */
static int	wait_chat_line(t_harness *hc, const char *needle, int timeout_ms)
{
	t_body_chat	chat;
	int			waited;

	waited = 0;
	while (waited < timeout_ms)
	{
		if (hc_wait_chat(hc, &chat, 200) != 0)
		{
			waited += 200;
			continue ;
		}
		if (strstr(chat.text, needle) != NULL)
			return (0);
		waited += 20;
	}
	return (-1);
}

/**
 * @brief Finds one player's card in an arena.
 *
 * @param snap The frame to search.
 * @param pid The player whose card is wanted.
 * @return Its index in the arena, or -1 when it is not there.
 */
static int	card_of(const t_body_state *snap, t_player_id pid)
{
	size_t	index;

	index = 0;
	while (index < snap->arena_count)
	{
		if (snap->arena[index].player_id == pid)
			return ((int)index);
		index++;
	}
	return (-1);
}

/*
** A mode is a preference and travels on its own tiny request: a word, and
** nothing else. There is no seat in it and no player id, because a player
** cannot pick a person - every mode narrows a set and the room draws from it.
**
** The four the client already draws are the four the server takes, and a word
** that is not one of them is a bad request rather than a silent Randoms.
*/
static void	test_a_targeting_mode_is_declared_and_kept(void)
{
	t_fixture	fx;
	t_harness	players[BR_SEATS];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(simple(&players[0], "TARGET", path, "mode ko\n") == 200);
	assert(simple(&players[0], "TARGET", path, "mode attackers\n") == 200);
	assert(simple(&players[0], "TARGET", path, "mode badges\n") == 200);
	assert(simple(&players[0], "TARGET", path, "mode random\n") == 200);
	/* a word that is not a mode, and a body that names nothing */
	assert(simple(&players[0], "TARGET", path, "mode everyone\n") == 400);
	assert(simple(&players[0], "TARGET", path, "ko\n") == 400);
	/* and a room this player is not in */
	assert(simple(&players[0], "TARGET", "/room/BR-99", "mode ko\n") == 404);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_targeting_mode_is_declared_and_kept\n");
}

/*
** Double has one opponent, so a mode would be a choice between one thing and
** itself. Single has none at all. Both refuse rather than accepting a
** preference that could never be acted on.
*/
static void	test_a_target_outside_a_battle_royale_is_refused(void)
{
	t_fixture	fx;
	t_harness	players[2];
	char		room[ROOM_NAME_MAX];
	char		path[64];

	assert(fx_start(&fx) == 0);
	assert(hc_connect(&players[0], &fx) == 0);
	assert(hc_signup(&players[0], "duo0", "hunter2") == 201);
	assert(hc_login(&players[0], "duo0", "hunter2") == 200);
	assert(hc_connect(&players[1], &fx) == 0);
	assert(hc_signup(&players[1], "duo1", "hunter2") == 201);
	assert(hc_login(&players[1], "duo1", "hunter2") == 200);
	assert(hc_join_new(&players[0], "double", room, sizeof(room)) == 201);
	snprintf(path, sizeof(path), "/room/%s", room);
	assert(simple(&players[1], "JOIN", path, NULL) == 200);
	assert(simple(&players[0], "TARGET", path, "mode ko\n") == 409);
	close_all(players, 2);
	fx_stop(&fx);
	printf("PASS test_a_target_outside_a_battle_royale_is_refused\n");
}

/*
** What a card can honestly say is "you are aiming at this kind of rival", not
** "at this one": the Target is drawn per resolution, so a flag naming one
** player would be a promise the next draw breaks.
**
** Badges singles out the players who have knocked somebody out, and at the
** start of a match nobody has - so the set is empty and no card is marked,
** which is also what stops an empty set from silently costing the sender
** their garbage: the draw falls back to every live rival.
**
** Randoms marks nothing either, and for the opposite reason. Every live rival
** is eligible, and outlining all of them says nothing at all.
*/
static void	test_a_mode_marks_the_rivals_it_singles_out(void)
{
	t_body_state	state;
	t_fixture		fx;
	t_harness		players[BR_SEATS];
	char			room[ROOM_NAME_MAX];
	char			path[64];
	size_t			marked;
	size_t			i;

	assert(start_match(&fx, players, BR_SEATS, room, sizeof(room), path,
			sizeof(path)) == 0);
	assert(simple(&players[0], "TARGET", path, "mode badges\n") == 200);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	marked = 0;
	i = 0;
	while (i < state.arena_count)
	{
		if (state.arena[i].flags & BODY_ARENA_TARGETED_BY_YOU)
			marked++;
		i++;
	}
	assert(marked == 0);
	/*
	 * KOs singles out the tallest stack. Every board starts empty and equal,
	 * so there is no tallest and nothing is marked; one player putting three
	 * pieces down makes them the whole of the set.
	 */
	assert(simple(&players[0], "TARGET", path, "mode ko\n") == 200);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	i = 0;
	while (i < state.arena_count)
	{
		assert((state.arena[i].flags & BODY_ARENA_TARGETED_BY_YOU) == 0);
		i++;
	}
	assert(drop_pieces(&players[1], room, 3) == 0);
	assert(wait_arena(&players[0], &state, HC_TIMEOUT_MS) == 0);
	marked = 0;
	i = 0;
	while (i < state.arena_count)
	{
		if (state.arena[i].flags & BODY_ARENA_TARGETED_BY_YOU)
		{
			assert(state.arena[i].player_id == players[1].player_id);
			marked++;
		}
		i++;
	}
	assert(marked == 1);
	close_all(players, BR_SEATS);
	fx_stop(&fx);
	printf("PASS test_a_mode_marks_the_rivals_it_singles_out\n");
}

/**
 * @brief Hard drops a fixed number of pieces, so a board has a stack on it.
 *
 * @param hc The connection to play.
 * @param room The room being played in.
 * @param pieces How many pieces to put down.
 * @return 0 when every drop was accepted, -1 otherwise.
 */
static int	drop_pieces(t_harness *hc, const char *room, int pieces)
{
	char	play[96];

	play_path(play, sizeof(play), room, hc->player_id);
	while (pieces > 0)
	{
		if (simple(hc, "DROP", play, "HARD\n") != 200)
			return (-1);
		pieces--;
	}
	return (0);
}
