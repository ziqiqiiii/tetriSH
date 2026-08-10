/* ************************************************************************** */
/*                                                                            */
/*   test_actions.c - HOLD, PAUSE, RESTART and ABILITY                        */
/*                                                                            */
/*   The four requests that act on a game rather than on the board under it.  */
/*   Everything asserted here is something a client can see: the status it    */
/*   is answered with, the reason a refusal carries, and the STATE that       */
/*   follows. Nothing reaches into the server for it.                         */
/*                                                                            */
/* ************************************************************************** */

#include "harness.h"

#include <assert.h>

// Static Functions
static void	test_hold_swaps_the_piece_and_spends_itself(void);
static void	test_hold_is_given_back_by_a_lock(void);
static void	test_pause_stops_gravity_and_resume_restarts_it(void);
static void	test_a_paused_game_refuses_inputs(void);
static void	test_restart_deals_a_clean_board(void);
static void	test_an_ability_without_charge_is_refused_and_costs_nothing(void);
static void	test_a_targeted_ability_is_refused_in_single(void);
static void	test_a_paid_ability_spends_exactly_its_cost(void);
static void	test_an_ability_is_the_character_and_the_level(void);
static void	test_ability_numbers_are_strictly_parsed(void);

static int		player(t_fixture *fx, t_harness *hc, const char *name);
static int		simple(t_harness *hc, const char *method, const char *path, const char *body);
static int		refusal(t_harness *hc, const char *method, const char *path, const char *body, char *reason, size_t cap);
static int		start_single(t_harness *hc, char *play_path, size_t cap);
static int		latest_state(t_harness *hc, t_body_state *out, int window_ms);
static int		fill_bottom_rows(t_harness *hc, const char *play, int rows);
static void		nap(int ms);

int	main(void)
{
	test_hold_swaps_the_piece_and_spends_itself();
	test_hold_is_given_back_by_a_lock();
	test_pause_stops_gravity_and_resume_restarts_it();
	test_a_paused_game_refuses_inputs();
	test_restart_deals_a_clean_board();
	test_an_ability_without_charge_is_refused_and_costs_nothing();
	test_a_targeted_ability_is_refused_in_single();
	test_a_paid_ability_spends_exactly_its_cost();
	test_an_ability_is_the_character_and_the_level();
	test_ability_numbers_are_strictly_parsed();
	return (0);
}

/*
** The first hold has nothing to swap with, so it takes the head of the queue:
** the piece that was falling goes into the slot, and the one that arrives is
** the one the client was already being shown as next. A second hold on the
** same piece is refused - that is what makes hold a swap and not a shuffle.
*/
static void	test_hold_swaps_the_piece_and_spends_itself(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_body_state	before;
	t_body_state	after;
	char			play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "holder") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(latest_state(&hc, &before, 400) == 0);
	assert(before.hold == BODY_HOLD_EMPTY && !before.hold_used);
	assert(simple(&hc, "HOLD", play, NULL) == 200);
	assert(latest_state(&hc, &after, 400) == 0);
	assert(after.hold == before.piece.type);
	assert(after.piece.type == before.next[0]);
	assert(after.hold_used);
	assert(simple(&hc, "HOLD", play, NULL) == 409);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_hold_swaps_the_piece_and_spends_itself\n");
}

/*
** Landing a piece is what gives the hold slot back. The swap that follows is
** a real swap this time: the held piece comes out and the one that was
** falling goes in.
*/
static void	test_hold_is_given_back_by_a_lock(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_body_state	held;
	t_body_state	after;
	char			play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "swapper") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "HOLD", play, NULL) == 200);
	assert(latest_state(&hc, &held, 400) == 0);
	assert(simple(&hc, "DROP", play, "HARD") == 200);
	assert(latest_state(&hc, &after, 400) == 0);
	assert(!after.hold_used);
	assert(simple(&hc, "HOLD", play, NULL) == 200);
	assert(latest_state(&hc, &after, 400) == 0);
	assert(after.piece.type == held.hold);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_hold_is_given_back_by_a_lock\n");
}

/*
** A paused game says so in its phase and stops falling, and it is still the
** player's game while it does: resuming finds the piece where it was left,
** not four rows further down for the time that passed.
*/
static void	test_pause_stops_gravity_and_resume_restarts_it(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_body_state	paused;
	t_body_state	later;
	char			play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "pauser") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "PAUSE", play, "PAUSE") == 200);
	assert(latest_state(&hc, &paused, 400) == 0);
	assert(paused.phase == BODY_PHASE_PAUSED);
	nap(600);
	assert(latest_state(&hc, &later, 200) == 0);
	assert(later.piece.row == paused.piece.row);
	assert(simple(&hc, "PAUSE", play, "PAUSE") == 409);
	assert(simple(&hc, "PAUSE", play, "RESUME") == 200);
	assert(latest_state(&hc, &later, 400) == 0);
	assert(later.phase == BODY_PHASE_ACTIVE);
	assert(later.piece.row == paused.piece.row);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_pause_stops_gravity_and_resume_restarts_it\n");
}

/*
** Pause is not a way to think with the piece still under your hands: while it
** is on, every input that would move the board is refused, and so is an
** ability.
*/
static void	test_a_paused_game_refuses_inputs(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	char		reason[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "frozen") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "PAUSE", play, "PAUSE") == 200);
	assert(simple(&hc, "MOVE", play, "LEFT") == 409);
	assert(simple(&hc, "ROTATE", play, "CW") == 409);
	assert(simple(&hc, "DROP", play, "HARD") == 409);
	assert(simple(&hc, "HOLD", play, NULL) == 409);
	assert(refusal(&hc, "ABILITY", play, "level 1\n", reason,
			sizeof(reason)) == 409);
	assert(simple(&hc, "PAUSE", play, "RESUME") == 200);
	assert(simple(&hc, "MOVE", play, "LEFT") == 200);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_paused_game_refuses_inputs\n");
}

/*
** Restart is the player deciding the last game did not happen: the board
** comes back empty, the score comes back to zero, and the hold slot is empty
** again. It is Single mode only - in a room with anybody else in it, one
** player rewinding their own game is an advantage over everyone who cannot.
*/
static void	test_restart_deals_a_clean_board(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_body_state	fresh;
	char			play[96];
	char			reason[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "again") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "HOLD", play, NULL) == 200);
	assert(fill_bottom_rows(&hc, play, 6) == 0);
	assert(simple(&hc, "RESTART", play, NULL) == 200);
	assert(latest_state(&hc, &fresh, 400) == 0);
	assert(fresh.score == 0 && fresh.lines == 0);
	assert(fresh.hold == BODY_HOLD_EMPTY && !fresh.hold_used);
	assert(fresh.phase == BODY_PHASE_ACTIVE);
	assert(fresh.cells[BODY_BOARD_ROWS - 1][0].type == 0);
	(void)reason;
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_restart_deals_a_clean_board\n");
}

/*
** Charge is banked two cleared lines at a time, so a game that has just
** started cannot afford anything. The refusal says which wall it hit, and it
** costs nothing: a player who asks for an ability they cannot pay for still
** has whatever charge they had.
*/
static void	test_an_ability_without_charge_is_refused_and_costs_nothing(void)
{
	t_fixture		fx;
	t_harness		hc;
	t_body_state	snap;
	char			play[96];
	char			reason[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "broke") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(refusal(&hc, "ABILITY", play, "level 1\n", reason,
			sizeof(reason)) == 409);
	assert(strcmp(reason, "no-charge") == 0);
	assert(latest_state(&hc, &snap, 400) == 0);
	assert(snap.charge == 0);
	assert(snap.last_ability.level == 1 && !snap.last_ability.accepted);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_an_ability_without_charge_is_refused_and_costs_nothing\n");
}

/*
** Single mode has no Target (docs/CONTEXT.md), so the twelve abilities that
** land on somebody else have nobody to land on. They are refused by name
** rather than quietly redirected at the player who asked for one - and the
** refusal comes before charge is even looked at, so a player is never told
** they cannot afford something they could not have used.
*/
static void	test_a_targeted_ability_is_refused_in_single(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		play[96];
	char		reason[64];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "lonely") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(refusal(&hc, "ABILITY", play, "level 3\n", reason,
			sizeof(reason)) == 409);
	assert(strcmp(reason, "no-target") == 0);
	assert(refusal(&hc, "ABILITY", play, "level 4\n", reason,
			sizeof(reason)) == 409);
	assert(strcmp(reason, "no-target") == 0);
	assert(simple(&hc, "ABILITY", play, "level 9\n") == 400);
	assert(simple(&hc, "ABILITY", play, "nothing\n") == 400);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_a_targeted_ability_is_refused_in_single\n");
}

/*
** What an activation costs and what it does to the board, driven in process.
**
** Playing a real game well enough to bank charge through the socket is not
** something a blind test client can be relied on to do - it tops out first -
** and a test that skips itself when it cannot is not a test. The transform
** and its price are the part worth pinning down, so charge is banked directly
** and the same game_ability the handler calls is asked to spend it.
*/
static void	test_a_paid_ability_spends_exactly_its_cost(void)
{
	const t_ability_def	*def;
	t_game				g;
	t_cell				block;

	game_start(&g, 7, 12345u);
	charge_on_clear(&g.charge, 4);
	assert(g.charge.charges == 2);
	block.type = CELL_FILLED;
	block.color = 3;
	board_set(&g.board, 0, BOARD_HEIGHT - 1, block);
	def = ability_lookup(2, 1);
	assert(def != NULL && ability_is_playable_solo(def));
	assert(game_ability(&g, def, 0) == ABILITY_ACTIVATED);
	assert(g.charge.charges == 0);
	assert(board_get(&g.board, 0, BOARD_HEIGHT - 1).type == CELL_EMPTY);
	assert(g.last_ability.level == 1 && g.last_ability.accepted);
	assert(game_ability(&g, def, 0) == ABILITY_NO_CHARGE);
	assert(!g.last_ability.accepted);
	printf("PASS test_a_paid_ability_spends_exactly_its_cost\n");
}

/*
** The catalogue is (character, level), not level: the same 1 a client sends
** is Fry for Halloween and Cut for Wolf-man, and reading it as one ability
** would let a player use a power they have not equipped. Every character
** offers one a room with no Target can serve, which is the least a default
** account should be able to do with the meter it is filling.
*/
static void	test_an_ability_is_the_character_and_the_level(void)
{
	t_item_id	character;
	int			level;

	assert(strcmp(ability_lookup(1, 1)->name, "Fry") == 0);
	assert(strcmp(ability_lookup(4, 1)->name, "Cut") == 0);
	assert(ability_lookup(2, 5) == NULL);
	assert(ability_lookup(9, 1) == NULL);
	assert(ability_lookup(0, 0) == NULL);
	character = 1;
	while (character <= 4)
	{
		assert(ability_is_playable_solo(ability_lookup(character, 1)));
		level = 2;
		while (level <= 3)
		{
			assert(!ability_is_playable_solo(ability_lookup(character, level)));
			level++;
		}
		character++;
	}
	assert(ability_is_playable_solo(ability_lookup(4, 4)));
	assert(!ability_is_playable_solo(ability_lookup(3, 4)));
	printf("PASS test_an_ability_is_the_character_and_the_level\n");
}

/*
** ABILITY's numbers are protocol fields, not numeric prefixes. Trailing text
** and non-numbers must be rejected, as must an aimed column off the board.
*/
static void	test_ability_numbers_are_strictly_parsed(void)
{
	t_fixture	fx;
	t_harness	hc;
	char		play[96];

	assert(fx_start(&fx) == 0);
	assert(player(&fx, &hc, "strict") == 0);
	assert(start_single(&hc, play, sizeof(play)) == 0);
	assert(simple(&hc, "ABILITY", play, "level 1junk\n") == 400);
	assert(simple(&hc, "ABILITY", play, "level nope\n") == 400);
	assert(simple(&hc, "ABILITY", play, "level 1\ncolumn 2junk\n") == 400);
	assert(simple(&hc, "ABILITY", play, "level 1\ncolumn -1\n") == 400);
	assert(simple(&hc, "ABILITY", play, "level 1\ncolumn 10\n") == 400);
	hc_close(&hc);
	fx_stop(&fx);
	printf("PASS test_ability_numbers_are_strictly_parsed\n");
}

/**
 * @brief Connects, registers, and logs in one player.
 *
 * @param fx Running fixture.
 * @param hc Client to bring up.
 * @param name Username to register.
 * @return 0 on success, -1 otherwise.
 */
static int	player(t_fixture *fx, t_harness *hc, const char *name)
{
	if (hc_connect(hc, fx) != 0)
		return (-1);
	if (hc_signup(hc, name, "hunter2") != 201)
		return (-1);
	if (hc_login(hc, name, "hunter2") != 200)
		return (-1);
	return (0);
}

/**
 * @brief Sends a request and returns only its status code.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body, or NULL.
 * @return The status code, or -1 when the exchange failed.
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
 * @brief Sends a request and reads back the `reason` a refusal carries.
 *
 * A bare status would leave a player unable to tell "you cannot afford that"
 * from "that ability has nothing to act on here", which is exactly the
 * distinction the refusal body exists to make.
 *
 * @param hc Connected client.
 * @param method HTTTP method.
 * @param path Resource path.
 * @param body Request body, or NULL.
 * @param reason Receives the reason word, empty when the body carried none.
 * @param cap Size of reason.
 * @return The status code, or -1 when the exchange failed.
 */
static int	refusal(t_harness *hc, const char *method, const char *path,
			const char *body, char *reason, size_t cap)
{
	t_htttp_message	resp;
	const char		*found;
	int				status;
	size_t			len;

	reason[0] = '\0';
	if (hc_request(hc, method, path, body, &resp) != 0)
		return (-1);
	status = (int)resp.status_code;
	if (resp.body != NULL && resp.body_len > strlen("reason ")
		&& memcmp(resp.body, "reason ", strlen("reason ")) == 0)
	{
		found = (const char *)resp.body + strlen("reason ");
		len = resp.body_len - strlen("reason ");
		while (len > 0 && (found[len - 1] == '\n' || found[len - 1] == '\r'))
			len--;
		if (len < cap)
		{
			memcpy(reason, found, len);
			reason[len] = '\0';
		}
	}
	htttp_message_free(&resp);
	return (status);
}

/**
 * @brief Takes a Single room, starts it, and builds the input path to use.
 *
 * @param hc Logged-in client.
 * @param play_path Receives /room/<name>/player/<pid>.
 * @param cap Size of play_path.
 * @return 0 on success, -1 otherwise.
 */
static int	start_single(t_harness *hc, char *play_path, size_t cap)
{
	char	room[ROOM_NAME_MAX];
	char	path[64];

	if (hc_join_new(hc, "single", room, sizeof(room)) != 201)
		return (-1);
	snprintf(path, sizeof(path), "/room/%s", room);
	if (simple(hc, "START", path, NULL) != 200)
		return (-1);
	snprintf(play_path, cap, "/room/%s/player/%llu", room,
		(unsigned long long)hc->player_id);
	return (0);
}

/**
 * @brief Collects STATE pushes for a while and keeps the last one.
 *
 * @param hc Connected client.
 * @param out Receives the most recent snapshot seen.
 * @param window_ms How long to keep collecting.
 * @return 0 when at least one snapshot arrived, -1 otherwise.
 */
static int	latest_state(t_harness *hc, t_body_state *out, int window_ms)
{
	t_body_state	snap;
	int				seen;

	seen = 0;
	while (hc_wait_state(hc, &snap, window_ms) == 0)
	{
		*out = snap;
		seen = 1;
		window_ms = 60;
	}
	if (seen == 0 && hc->has_state)
	{
		*out = hc->last_state;
		seen = 1;
	}
	if (seen == 0)
		return (-1);
	return (0);
}

/**
 * @brief Hard-drops pieces down one side to build a stack worth clearing.
 *
 * Dropping into a column rather than at spawn is what eventually completes
 * rows, which is what banks charge - an ability test needs a game that has
 * actually been played, not one that has only been started.
 *
 * @param hc Connected client.
 * @param play Input path for this player.
 * @param rows How many pieces to place.
 * @return 0 while the game was still running, -1 once it refused a drop.
 */
static int	fill_bottom_rows(t_harness *hc, const char *play, int pieces)
{
	int	i;
	int	step;

	i = 0;
	while (i < pieces)
	{
		step = 0;
		while (step < BODY_BOARD_COLS)
		{
			if (simple(hc, "MOVE", play, "LEFT") != 200)
				break ;
			step++;
		}
		step = 0;
		while (step < i % BODY_BOARD_COLS)
		{
			if (simple(hc, "MOVE", play, "RIGHT") != 200)
				break ;
			step++;
		}
		if (simple(hc, "DROP", play, "HARD") != 200)
			return (0);
		i++;
	}
	return (0);
}

/**
 * @brief Sleeps for a number of milliseconds.
 *
 * @param ms Milliseconds to sleep.
 */
static void	nap(int ms)
{
	struct timespec	ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
