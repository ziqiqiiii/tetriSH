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
	"score 1200\n" \
	"lines 14\n" \
	"level 2\n" \
	"combo 3\n" \
	"b2b 1\n" \
	"charge 7\n" \
	"ability 2 1\n" \
	"clear tetris\n" \
	"clearing 2 350 18 19\n";

// Static Functions
static void	make_state(t_sb_state *st);
static void	build_body(char *dst, const char *head, int zero_rows,
				const char *tail);

// a full, valid frame matching g_head with an all-empty board
static void	make_state(t_sb_state *st)
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

// head + "board\n" + zero_rows all-empty rows + an optional tail
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
	if (tail)
		strcat(dst, tail);
}

void	test_state_encode_contains_all_required_keys(void)
{
	t_sb_state	st;
	char		out[4096];

	make_state(&st);
	assert(body_state_encode(&st, out, sizeof(out)) > 0);
	assert(strstr(out, "seq ") != NULL);
	assert(strstr(out, "phase ") != NULL);
	assert(strstr(out, "piece ") != NULL);
	assert(strstr(out, "next ") != NULL);
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
	t_sb_state	in;
	t_sb_state	back;
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
	t_sb_state	in;
	t_sb_state	back;
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
	t_sb_state	st;
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
	st.phase = (t_sb_phase)99;
	assert(body_state_encode(&st, out, sizeof(out)) == -1);
	assert(errno == EINVAL);
	printf("PASS test_state_encode_rejects_out_of_range_fields\n");
}

void	test_state_encode_small_cap_returns_erange(void)
{
	t_sb_state	st;
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
	t_sb_state	st;
	char		body[4096];

	build_body(body, g_head + 7, BODY_BOARD_ROWS, NULL); // skip "seq 42\n"
	assert(body_state_decode(body, strlen(body), &st) == -1);
	assert(errno == EBADMSG);
	printf("PASS test_state_decode_rejects_missing_seq\n");
}

void	test_state_decode_rejects_malformed_board_row(void)
{
	t_sb_state	st;
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
	t_sb_state	st;
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
	t_sb_state	st;
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
	t_sb_state	in;
	t_sb_state	back;
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
	return (0);
}
