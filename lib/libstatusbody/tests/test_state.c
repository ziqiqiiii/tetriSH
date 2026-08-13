#include "statusbody.h"

#include <assert.h>
#include <stdio.h>

// Static Variables
// every header line of a valid body, in encode order, up to "board"
static const char	*g_head = \
	"seq 42\n" \
	"phase active\n" \
	"piece 3 1 4 0\n" \
	"next 0 1 2\n" \
	"hold 5 1\n" \
	"score 1200\n" \
	"lines 14\n" \
	"level 2\n" \
	"combo 3\n" \
	"b2b 1\n" \
	"charge 7\n" \
	"ability 2 1\n" \
	"clear tetris\n" \
	"clearing 2 350 18 19\n" \
	"countdown 0\n" \
	"pending 0\n" \
	"effects 0 0 0 0 0 0 0 0\n" \
	"result none 0\n";

// Static Functions
static void	make_state(t_body_state *st);
static void	build_body(char *dst, const char *head, int zero_rows,
				const char *tail);
static void	fill_opponent(t_body_opponent *opponent);
void		test_state_build_body_is_valid(void);
void		test_state_absent_arena_is_not_an_empty_one(void);
void		test_state_arena_round_trips_every_card(void);
void		test_state_arena_cells_survive_the_wire(void);
void		test_state_arena_cells_are_optional_per_card(void);
void		test_state_decode_rejects_arena_count_mismatch(void);
void		test_state_arena_fits_the_derived_cap(void);
void		test_state_arena_seats_are_numbered_from_one(void);
static void	fill_card(t_body_arena_slot *card, int slot, bool with_cells);
static int	card_cell(const t_body_arena_slot *card, int row, int col);
static void	set_card_cell(t_body_arena_slot *card, int row, int col,
				int code);

// a full, valid frame matching g_head with an all-empty board
static void	make_state(t_body_state *st)
{
	memset(st, 0, sizeof(*st));
	st->seq = 42;
	st->phase = BODY_PHASE_ACTIVE;
	st->piece.type = 3;
	st->piece.rotation = 1;
	st->piece.col = 4;
	st->piece.row = 0;
	st->next[0] = 0;
	st->next[1] = 1;
	st->next[2] = 2;
	st->hold = 5;
	st->hold_used = true;
	st->score = 1200;
	st->lines = 14;
	st->level = 2;
	st->combo = 3;
	st->back_to_back = true;
	st->charge = 7;
	st->last_ability.level = 2;
	st->last_ability.accepted = true;
	st->clearing_count = 2;
	st->clearing_ms = 350;
	st->clearing_rows[0] = 18;
	st->clearing_rows[1] = 19;
	st->last_clear = BODY_CLEAR_TETRIS;
}

// head + "board\n" + zero_rows all-empty rows + "opponents 0\n" + a tail
static void	build_body(char *dst, const char *head, int zero_rows,
				const char *tail)
{
	int	i;

	strcpy(dst, head);
	strcat(dst, "board\n");
	i = 0;
	while (i < zero_rows)
	{
		strcat(dst, "00000000000000000000\n");
		i++;
	}
	strcat(dst, "opponents 0\n");
	/*
	 * The counts and the arena close every body, so they belong in the helper
	 * rather than in each caller. Leaving them out would not have shown up as
	 * a failure: every test built this way is a negative one, so the bodies
	 * would still have been rejected - for the missing lines rather than for
	 * the defect the test is named after. test_state_build_body_is_valid is
	 * what stops that happening again.
	 */
	strcat(dst, "counts 1 1\n");
	strcat(dst, "arena absent 0\n");
	if (tail)
		strcat(dst, tail);
}

// one opponent whose board is distinguishable from an empty one
static void	fill_opponent(t_body_opponent *opponent)
{
	memset(opponent, 0, sizeof(*opponent));
	opponent->slot = 2;
	opponent->player_id = 7;
	opponent->alive = true;
	opponent->phase = BODY_PHASE_CLEARING;
	opponent->score = 4200;
	opponent->lines = 9;
	opponent->pending = 3;
	opponent->piece.type = 4;
	opponent->piece.rotation = 2;
	opponent->piece.col = 6;
	opponent->piece.row = 3;
	opponent->charge = 8;
	opponent->character = 3;
	strcpy(opponent->username, "rival");
	opponent->cells[19][0].type = 1;
	opponent->cells[19][0].color = 4;
	opponent->cells[18][9].type = 2;
	opponent->cells[18][9].color = 11;
}

void	test_state_encode_contains_all_required_keys(void)
{
	t_body_state	st;
	char		out[4096];

	make_state(&st);
	assert(body_state_encode(&st, out, sizeof(out)) > 0);
	assert(strstr(out, "seq ") != NULL);
	assert(strstr(out, "phase ") != NULL);
	assert(strstr(out, "piece ") != NULL);
	assert(strstr(out, "next ") != NULL);
	assert(strstr(out, "hold ") != NULL);
	assert(strstr(out, "score ") != NULL);
	assert(strstr(out, "lines ") != NULL);
	assert(strstr(out, "level ") != NULL);
	assert(strstr(out, "combo ") != NULL);
	assert(strstr(out, "b2b ") != NULL);
	assert(strstr(out, "charge ") != NULL);
	assert(strstr(out, "board\n") != NULL);
	printf("PASS test_state_encode_contains_all_required_keys\n");
}

void	test_state_round_trip_full_frame(void)
{
	t_body_state	in;
	t_body_state	back;
	char		out[4096];
	int			n;

	make_state(&in);
	in.cells[19][0].type = 1;
	in.cells[19][0].color = 3;
	in.cells[10][9].type = 2;
	in.cells[10][9].color = 7;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	memset(&back, 0, sizeof(back));
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(&in, &back, sizeof(in)) == 0);
	printf("PASS test_state_round_trip_full_frame\n");
}

void	test_state_board_cells_preserve_type_and_color(void)
{
	t_body_state	in;
	t_body_state	back;
	char		out[4096];
	char		*row0;
	int			n;
	int			col;

	make_state(&in);
	col = 0;
	while (col < BODY_BOARD_COLS)
	{
		in.cells[0][col].type = (uint8_t)(col % 3);
		in.cells[0][col].color = (uint8_t)(col + 6); // colors 6..15
		col++;
	}
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	row0 = strstr(out, "board\n");
	assert(row0 != NULL);
	row0 += 6;
	assert(strchr(row0, '\n') - row0 == BODY_BOARD_COLS * 2); // 20 hex chars
	memset(&back, 0, sizeof(back));
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(in.cells, back.cells, sizeof(in.cells)) == 0);
	printf("PASS test_state_board_cells_preserve_type_and_color\n");
}

void	test_state_encode_rejects_out_of_range_fields(void)
{
	t_body_state	st;
	char		out[4096];

	make_state(&st);
	st.charge = BODY_CHARGE_MAX + 1;
	assert(body_state_encode(&st, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	make_state(&st);
	st.clearing_count = BODY_CLEARING_MAX + 1;
	assert(body_state_encode(&st, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	make_state(&st);
	st.phase = (t_body_phase)99;
	assert(body_state_encode(&st, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_encode_rejects_out_of_range_fields\n");
}

void	test_state_encode_small_cap_returns_erange(void)
{
	t_body_state	st;
	char		out[64];

	make_state(&st);
	memset(out, 0x7f, sizeof(out));
	assert(body_state_encode(&st, out, 32) == -1);
	assert(errno == ERANGE);
	assert(out[32] == 0x7f); // never writes past cap
	assert(out[63] == 0x7f);
	printf("PASS test_state_encode_small_cap_returns_erange\n");
}

void	test_state_decode_rejects_missing_seq(void)
{
	t_body_state	st;
	char		body[4096];

	build_body(body, g_head + 7, BODY_BOARD_ROWS, NULL); // skip "seq 42\n"
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_missing_seq\n");
}

void	test_state_decode_rejects_malformed_board_row(void)
{
	t_body_state	st;
	char		body[4096];

	// last row one cell short (18 hex chars)
	build_body(body, g_head, BODY_BOARD_ROWS - 1, "000000000000000000\n");
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	// last row is not hex
	build_body(body, g_head, BODY_BOARD_ROWS - 1, "zzzzzzzzzzzzzzzzzzzz\n");
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	// only 19 rows
	build_body(body, g_head, BODY_BOARD_ROWS - 1, NULL);
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_malformed_board_row\n");
}

void	test_state_decode_rejects_unknown_key_and_trailing_junk(void)
{
	t_body_state	st;
	char		body[4096];
	char		head[2048];

	strcpy(head, "foo bar\n"); // unknown key ahead of the real body
	strcat(head, g_head);
	build_body(body, head, BODY_BOARD_ROWS, NULL);
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	build_body(body, g_head, BODY_BOARD_ROWS, "extra\n"); // junk after board
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_unknown_key_and_trailing_junk\n");
}

void	test_state_decode_rejects_numeric_overflow(void)
{
	t_body_state	st;
	char		body[4096];
	char		head[2048];

	strcpy(head, "seq 99999999999999999999999999\n"); // > UINT64_MAX
	strcat(head, g_head + 7);
	build_body(body, head, BODY_BOARD_ROWS, NULL);
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	strcpy(head, g_head);
	head[strlen(g_head) - 60] = '\0'; // rebuild with a negative lines value
	strcpy(head, "seq 42\nphase active\npiece 3 1 4 0\nnext 0 1 2\n"
		"score 1200\nlines -5\nlevel 2\ncombo 3\nb2b 1\ncharge 7\n"
		"ability 2 1\nclear tetris\nclearing 2 350 18 19\n");
	build_body(body, head, BODY_BOARD_ROWS, NULL);
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_numeric_overflow\n");
}

void	test_state_large_seq_round_trips_exactly(void)
{
	t_body_state	in;
	t_body_state	back;
	char		out[4096];
	int			n;

	make_state(&in);
	in.seq = UINT64_MAX;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	memset(&back, 0, sizeof(back));
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.seq == UINT64_MAX);
	printf("PASS test_state_large_seq_round_trips_exactly\n");
}

/*
** An empty hold slot is a value the wire has to carry, not the absence of a
** line: a client that saw no "hold" key could not tell "holding nothing" from
** a truncated body. BODY_HOLD_EMPTY is the one negative number the field
** accepts, and anything below it is a malformed frame.
*/
void	test_state_hold_round_trips_empty_and_held(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[4096];
	int				n;

	make_state(&in);
	in.hold = BODY_HOLD_EMPTY;
	in.hold_used = false;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "hold -1 0\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.hold == BODY_HOLD_EMPTY && !back.hold_used);
	make_state(&in);
	in.hold = 6;
	in.hold_used = true;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "hold 6 1\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.hold == 6 && back.hold_used);
	in.hold = BODY_HOLD_EMPTY - 1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_hold_round_trips_empty_and_held\n");
}

/*
** A Single frame is the same frame it always was, plus the two zero-valued
** lines that keep the line order fixed. This is the check that says the Solo
** path did not quietly change shape when Double was added to the body.
*/
void	test_state_solo_frame_carries_zero_countdown_and_opponents(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	assert(strstr(out, "countdown 0\n") != NULL);
	assert(strstr(out, "pending 0\n") != NULL);
	assert(strstr(out, "effects 0 0 0 0 0 0 0 0\n") != NULL);
	assert(strstr(out, "opponents 0\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.opponent_count == 0 && back.countdown_ms == 0);
	assert(memcmp(&in, &back, sizeof(in)) == 0);
	printf("PASS test_state_solo_frame_carries_zero_countdown_and_opponents\n");
}

/*
** The countdown is a phase of its own rather than a paused game, because a
** paused game is one player's clock stopped and a countdown is everybody's.
*/
void	test_state_countdown_phase_round_trips(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.phase = BODY_PHASE_COUNTDOWN;
	in.countdown_ms = 2400;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	assert(strstr(out, "phase countdown\n") != NULL);
	assert(strstr(out, "countdown 2400\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.phase == BODY_PHASE_COUNTDOWN && back.countdown_ms == 2400);
	in.countdown_ms = -1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_countdown_phase_round_trips\n");
}

/*
** The winner's board is indistinguishable from a board still being played -
** active, with a piece on it - so the outcome cannot be read off the phase
** and has to survive as a field of its own.
*/
void	test_state_result_round_trips(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.result = BODY_RESULT_WON;
	in.rank = 1;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "result won 1\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.result == BODY_RESULT_WON && back.rank == 1);
	assert(back.phase == BODY_PHASE_ACTIVE);
	make_state(&in);
	in.result = BODY_RESULT_LOST;
	in.rank = 7;
	in.phase = BODY_PHASE_TOP_OUT;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "result lost 7\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.result == BODY_RESULT_LOST && back.rank == 7);
	in.rank = -1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_result_round_trips\n");
}

/*
** Garbage queued against the recipient, and against the other player, are two
** different numbers on two different lines. They are asserted together
** because conflating them is the mistake worth catching: a client that drew
** the opponent's incoming rows as its own would warn the wrong player.
*/
void	test_state_pending_garbage_round_trips(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.pending = 4;
	in.opponent_count = 1;
	in.opponents[0].slot = 2;
	in.opponents[0].alive = true;
	in.opponents[0].pending = 2;
	snprintf(in.opponents[0].username, sizeof(in.opponents[0].username),
		"rival");
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "pending 4\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(back.pending == 4 && back.opponents[0].pending == 2);
	in.pending = -1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1 && errno == EINVAL);
	printf("PASS test_state_pending_garbage_round_trips\n");
}

/*
** The effect counts. They are on the wire because an effect nobody can see is
** indistinguishable from a bug: Paralysis worked perfectly and looked exactly
** like a rotate key that had stopped responding, and Dark could not be drawn
** at all, since blacking out a field is something only a renderer can do.
*/
void	test_state_effects_round_trip(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.effect_paralysis = 3;
	in.effect_inversion = 2;
	in.effect_nue = 4;
	in.effect_thwack = 1;
	in.effect_fry = 3;
	in.effect_dark = 4;
	in.effect_pals = 1;
	in.effect_mirror = 1;
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0 && strstr(out, "effects 3 2 4 1 3 4 1 1\n") != NULL);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(&in, &back, sizeof(in)) == 0);
	in.effect_dark = -1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1 && errno == EINVAL);
	printf("PASS test_state_effects_round_trip\n");
}

void	test_state_opponent_round_trips_whole(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.opponent_count = 1;
	fill_opponent(&in.opponents[0]);
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	assert(strstr(out, "opponents 1\n") != NULL);
	assert(strstr(out, "opp 2 7 1 clearing 4200 9 3 4 2 6 3 8 3 rival\n")
		!= NULL);
	memset(&back, 0, sizeof(back));
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(&in, &back, sizeof(in)) == 0);
	/*
	 * The charge is bounded exactly as the frame's own is. It is the one new
	 * field with a range, and a rival drawn with eleven segments of a
	 * ten-segment meter is a body the encoder should never have written.
	 */
	in.opponents[0].charge = BODY_CHARGE_MAX + 1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1 && errno == EINVAL);
	printf("PASS test_state_opponent_round_trips_whole\n");
}

/*
** The two boards must stay distinct through a round trip. They are the same
** block in the same encoding read back to back, so a decoder that reused one
** cursor position for both would produce two copies of whichever it read
** first - and the player would watch their own stack on the opponent's side.
*/
void	test_state_opponent_board_is_not_the_local_board(void)
{
	t_body_state	in;
	t_body_state	back;
	char			out[8192];
	int				n;

	make_state(&in);
	in.cells[0][0].type = 1;
	in.cells[0][0].color = 2;
	in.opponent_count = 1;
	fill_opponent(&in.opponents[0]);
	n = body_state_encode(&in, out, sizeof(out));
	assert(n > 0);
	assert(body_state_decode(out, (size_t)n, &back) == 0);
	assert(memcmp(back.cells, in.cells, sizeof(in.cells)) == 0);
	assert(memcmp(back.opponents[0].cells, in.opponents[0].cells,
			sizeof(in.opponents[0].cells)) == 0);
	assert(memcmp(back.cells, back.opponents[0].cells,
			sizeof(back.cells)) != 0);
	printf("PASS test_state_opponent_board_is_not_the_local_board\n");
}

/*
** The count bounds the read, so a body that claims an opponent it does not
** carry has to run out of lines rather than leave the previous frame's
** opponent in the array.
*/
void	test_state_decode_rejects_opponent_count_mismatch(void)
{
	t_body_state	st;
	char			body[8192];

	build_body(body, g_head, BODY_BOARD_ROWS, NULL);
	strcpy(strstr(body, "opponents 0\n"), "opponents 1\n");
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	build_body(body, g_head, BODY_BOARD_ROWS, NULL);
	strcpy(strstr(body, "opponents 0\n"), "opponents 9\n");
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_opponent_count_mismatch\n");
}

/*
** Every field on an opp line is positional and the name is last, so a space
** inside the name shifts all of them. It is refused at the encoder rather
** than written out and misread at the far end - the failure the leaderboard
** decoder already suffered once.
*/
void	test_state_encode_rejects_unusable_opponent_username(void)
{
	t_body_state	in;
	char			out[8192];

	make_state(&in);
	in.opponent_count = 1;
	fill_opponent(&in.opponents[0]);
	strcpy(in.opponents[0].username, "amber lee");
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	fill_opponent(&in.opponents[0]);
	in.opponents[0].username[0] = '\0';
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	fill_opponent(&in.opponents[0]);
	in.opponent_count = BODY_OPPONENTS_MAX + 1;
	assert(body_state_encode(&in, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_encode_rejects_unusable_opponent_username\n");
}

int	main(void)
{
	test_state_encode_contains_all_required_keys();
	test_state_round_trip_full_frame();
	test_state_board_cells_preserve_type_and_color();
	test_state_encode_rejects_out_of_range_fields();
	test_state_encode_small_cap_returns_erange();
	test_state_decode_rejects_missing_seq();
	test_state_decode_rejects_malformed_board_row();
	test_state_decode_rejects_unknown_key_and_trailing_junk();
	test_state_decode_rejects_numeric_overflow();
	test_state_large_seq_round_trips_exactly();
	test_state_hold_round_trips_empty_and_held();
	test_state_solo_frame_carries_zero_countdown_and_opponents();
	test_state_countdown_phase_round_trips();
	test_state_result_round_trips();
	test_state_pending_garbage_round_trips();
	test_state_effects_round_trip();
	test_state_opponent_round_trips_whole();
	test_state_opponent_board_is_not_the_local_board();
	test_state_decode_rejects_opponent_count_mismatch();
	test_state_encode_rejects_unusable_opponent_username();
	test_state_build_body_is_valid();
	test_state_absent_arena_is_not_an_empty_one();
	test_state_arena_seats_are_numbered_from_one();
	test_state_arena_round_trips_every_card();
	test_state_arena_cells_survive_the_wire();
	test_state_arena_cells_are_optional_per_card();
	test_state_decode_rejects_arena_count_mismatch();
	test_state_arena_fits_the_derived_cap();
	return (0);
}

/*
** The helper every negative decode test is built on, checked once.
**
** Without this, build_body producing an incomplete body is invisible: every
** test that uses it expects a rejection, so an omission there turns those
** tests into assertions that a truncated body is rejected - which is true,
** uninteresting, and not what any of them is named after. This is the only
** test that asks the helper to succeed.
*/
void	test_state_build_body_is_valid(void)
{
	t_body_state	st;
	char			body[8192];

	build_body(body, g_head, BODY_BOARD_ROWS, NULL);
	assert(body_state_decode(body, strlen(body), &st) == 0);
	assert(st.seq == 42);
	assert(st.opponent_count == 0);
	assert(!st.arena_present);
	printf("PASS test_state_build_body_is_valid\n");
}

/*
** `arena absent` and `arena full 0` are different answers.
**
** Absent means the frame says nothing about the arena, which is what almost
** every frame says, because the arena rides a slower clock than the board.
** Full 0 means the room is empty. A decoder that collapsed them would have
** every client wipe a screen full of live cards between pushes.
*/
void	test_state_absent_arena_is_not_an_empty_one(void)
{
	t_body_state	st;
	char			body[16384];
	int				len;

	make_state(&st);
	st.arena_present = false;
	st.arena_count = 0;
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	assert(strstr(body, "arena absent 0\n") != NULL);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	assert(!st.arena_present);
	make_state(&st);
	st.arena_present = true;
	st.arena_count = 0;
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	assert(strstr(body, "arena full 0\n") != NULL);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	assert(st.arena_present && st.arena_count == 0);
	printf("PASS test_state_absent_arena_is_not_an_empty_one\n");
}

/*
** A full arena is the complete roster, so every card has to survive intact:
** the card list *is* the roster (there is no separate one), and a field that
** did not round-trip would be a rival misdescribed rather than a line missing.
*/
void	test_state_arena_round_trips_every_card(void)
{
	t_body_arena_slot	expect;
	t_body_state		st;
	char				body[16384];
	int					len;
	int					i;

	make_state(&st);
	st.players = 40;
	st.alive = 12;
	st.arena_present = true;
	st.arena_count = 3;
	i = 0;
	while (i < 3)
	{
		fill_card(&st.arena[i], i * 7 + 1, true);
		i++;
	}
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	assert(st.players == 40 && st.alive == 12);
	assert(st.arena_present && st.arena_count == 3);
	i = 0;
	while (i < 3)
	{
		/*
		 * Compared against a freshly built card rather than against the
		 * numbers fill_card happens to produce: restating its arithmetic here
		 * is how an assertion ends up agreeing with itself instead of with the
		 * codec.
		 */
		fill_card(&expect, i * 7 + 1, true);
		assert(st.arena[i].slot == expect.slot);
		assert(st.arena[i].player_id == expect.player_id);
		assert(st.arena[i].lines == expect.lines);
		assert(st.arena[i].pending == expect.pending);
		assert(st.arena[i].ko == expect.ko);
		assert(st.arena[i].rank == expect.rank);
		assert(st.arena[i].flags == expect.flags);
		assert(st.arena[i].cells_valid);
		assert(memcmp(st.arena[i].cells, expect.cells,
				sizeof(expect.cells)) == 0);
		i++;
	}
	printf("PASS test_state_arena_round_trips_every_card\n");
}

/*
** The cells are the whole point of the card, and they are the one field packed
** rather than printed: 200 cells into 50 hex characters, four cells to a
** character, crossing row boundaries. An off-by-one in either direction would
** shear every rival's stack sideways, so the exact cells are asserted rather
** than a count of set bits.
*/
void	test_state_arena_cells_survive_the_wire(void)
{
	t_body_state	st;
	char			body[16384];
	int				len;

	make_state(&st);
	st.players = 2;
	st.alive = 2;
	st.arena_present = true;
	st.arena_count = 1;
	fill_card(&st.arena[0], 1, true);
	memset(st.arena[0].cells, 0, sizeof(st.arena[0].cells));
	set_card_cell(&st.arena[0], 0, 0, 2);
	set_card_cell(&st.arena[0], 0, 9, 8);
	set_card_cell(&st.arena[0], 19, 0, 1);
	set_card_cell(&st.arena[0], 19, 9, 1);
	set_card_cell(&st.arena[0], 7, 4, 5);
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	/* the code, not merely that something was there: a piece's type and a
	** row of garbage have to come off the wire as different things */
	assert(card_cell(&st.arena[0], 0, 0) == 2);
	assert(card_cell(&st.arena[0], 0, 9) == 8);
	assert(card_cell(&st.arena[0], 19, 0) == 1);
	assert(card_cell(&st.arena[0], 19, 9) == 1);
	assert(card_cell(&st.arena[0], 7, 4) == 5);
	assert(card_cell(&st.arena[0], 0, 1) == 0);
	assert(card_cell(&st.arena[0], 7, 5) == 0);
	assert(card_cell(&st.arena[0], 10, 0) == 0);
	printf("PASS test_state_arena_cells_survive_the_wire\n");
}

/*
** A dead board never changes again, so its card leaves the cells off and the
** client keeps the one it holds. The flags say which, per card, on the same
** line - so one card omitting them must not shift the next card's fields,
** which is the failure this shape risks.
*/
void	test_state_arena_cells_are_optional_per_card(void)
{
	t_body_state	st;
	char			body[16384];
	int				len;

	make_state(&st);
	st.players = 3;
	st.alive = 1;
	st.arena_present = true;
	st.arena_count = 3;
	fill_card(&st.arena[0], 1, false);
	fill_card(&st.arena[1], 2, true);
	fill_card(&st.arena[2], 3, false);
	st.arena[0].flags = 0;
	st.arena[2].flags = 0;
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	assert(st.arena_count == 3);
	assert(!st.arena[0].cells_valid && !st.arena[2].cells_valid);
	assert(st.arena[1].cells_valid);
	/* the card after a cell-less one is still read whole */
	assert(st.arena[1].slot == 2 && st.arena[1].ko == 3);
	assert(st.arena[2].slot == 3 && st.arena[2].lines == 6);
	printf("PASS test_state_arena_cells_are_optional_per_card\n");
}

/*
** The count says how many cards follow, and a body claiming more than it
** carries has to be refused rather than half-read - the same rule the
** opponents section already answers to.
*/
void	test_state_decode_rejects_arena_count_mismatch(void)
{
	t_body_state	st;
	char			body[16384];
	int				len;

	make_state(&st);
	st.arena_present = true;
	st.arena_count = 1;
	fill_card(&st.arena[0], 1, true);
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	strcpy(strstr(body, "arena full 1\n"), "arena full 2\n");
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_arena_count_mismatch\n");
}

/*
** BODY_STATE_MAX_BYTES is derived from the constants that produce it so that
** a field added to a card cannot silently overrun the buffer carrying it.
** This is the arithmetic being checked against a real encode of the widest
** arena there can be, rather than against the table it was computed from.
*/
void	test_state_arena_fits_the_derived_cap(void)
{
	static t_body_state	st;
	static char			body[BODY_STATE_MAX_BYTES];
	int					len;
	int					i;

	make_state(&st);
	st.players = BODY_ARENA_MAX;
	st.alive = BODY_ARENA_MAX;
	st.arena_present = true;
	st.arena_count = BODY_ARENA_MAX;
	i = 0;
	while (i < BODY_ARENA_MAX)
	{
		fill_card(&st.arena[i], i + 1, true);
		st.arena[i].player_id = UINT64_MAX;
		st.arena[i].lines = 9999;
		st.arena[i].pending = 999;
		st.arena[i].ko = 98;
		st.arena[i].rank = 99;
		st.arena[i].flags = BODY_ARENA_FLAGS_MAX;
		i++;
	}
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	assert((size_t)len <= sizeof(body));
	printf("PASS test_state_arena_fits_the_derived_cap (%d of %d bytes)\n",
		len, (int)sizeof(body));
}

// one card, distinguishable from its neighbours by slot
static void	fill_card(t_body_arena_slot *card, int slot, bool with_cells)
{
	int	row;

	memset(card, 0, sizeof(*card));
	card->slot = slot;
	card->player_id = (uint64_t)slot + 1000;
	card->flags = BODY_ARENA_ALIVE;
	card->lines = slot + 3;
	card->pending = slot % 3;
	card->ko = slot % 3 + 1;
	card->rank = slot * 2 % 7;
	if (!with_cells)
		return ;
	card->flags |= BODY_ARENA_MASK_PRESENT;
	row = 0;
	while (row < BODY_BOARD_ROWS)
	{
		if ((row + slot) % 3 == 0)
			set_card_cell(card, row, (row + slot) % BODY_BOARD_COLS,
				2 + (row + slot) % 7);
		row++;
	}
}

static int	card_cell(const t_body_arena_slot *card, int row, int col)
{
	return ((int)card->cells[row][col]);
}

static void	set_card_cell(t_body_arena_slot *card, int row, int col, int code)
{
	card->cells[row][col] = (unsigned char)code;
}

/*
** A slot is the seat number a room hands out, and a room numbers its seats
** from 1 - so the last seat of a full room is BODY_ARENA_MAX itself, and
** there is no seat 0.
**
** Read as a 0-based index into an array of that size, seat 99 failed to
** validate; and because one bad card fails the whole body, a full
** ninety-nine player room encoded no arena at all. Nothing said so: the
** encoder returned -1, the push was dropped, and every client in the biggest
** room the mode supports drew an empty grid for the whole match. It is the
** last seat that is the assertion here, because it is the only one that was
** ever wrong.
*/
void	test_state_arena_seats_are_numbered_from_one(void)
{
	t_body_state	st;
	char			body[16384];
	int				len;

	make_state(&st);
	st.players = BODY_ARENA_MAX;
	st.alive = BODY_ARENA_MAX;
	st.arena_present = true;
	st.arena_count = 1;
	fill_card(&st.arena[0], BODY_ARENA_MAX, true);
	len = body_state_encode(&st, body, sizeof(body));
	assert(len > 0);
	memset(&st, 0, sizeof(st));
	assert(body_state_decode(body, (size_t)len, &st) == 0);
	assert(st.arena_count == 1 && st.arena[0].slot == BODY_ARENA_MAX);
	/* and neither end of the range beyond it is a seat */
	make_state(&st);
	st.players = 1;
	st.alive = 1;
	st.arena_present = true;
	st.arena_count = 1;
	fill_card(&st.arena[0], 0, true);
	assert(body_state_encode(&st, body, sizeof(body)) == -1);
	assert(errno == EINVAL);
	fill_card(&st.arena[0], BODY_ARENA_MAX + 1, true);
	assert(body_state_encode(&st, body, sizeof(body)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_arena_seats_are_numbered_from_one\n");
}
