#include "tetrisu.h"

static void	test_ability_metadata_and_meter_spacing(void);
static void	test_mirurun_removes_bottom_four_rows(void);
static void	test_activation_rejections_preserve_charge(void);
static void	test_opponent_abilities_are_visual_solo_tests(void);
static void	test_feedback_expires_on_shared_wake_deadline(void);
static void	test_canvas_hitboxes_and_terminal_mapping(void);

/**
 * @brief Runs the local Mirurun ability regression suite.
 *
 * @return 0 after every ability and mouse-mapping invariant passes.
 */
int	main(void)
{
	test_ability_metadata_and_meter_spacing();
	test_mirurun_removes_bottom_four_rows();
	test_activation_rejections_preserve_charge();
	test_opponent_abilities_are_visual_solo_tests();
	test_feedback_expires_on_shared_wake_deadline();
	test_canvas_hitboxes_and_terminal_mapping();
	return (0);
}

/**
 * @brief Exercises costs, labels, and equal authored marker spacing.
 */
static void	test_ability_metadata_and_meter_spacing(void)
{
	int	ability;
	int	previous_y;

	previous_y = -1;
	ability = SOLO_ABILITY_MIRURUN;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		assert(solo_ability_cost((solo_ability_t)ability) == ability * 2);
		assert(solo_ability_name((solo_ability_t)ability)[0] != '\0');
		assert(solo_ability_description((solo_ability_t)ability)[0] != '\0');
		if (previous_y >= 0)
			assert(previous_y
				- solo_ability_center_y((solo_ability_t)ability) == 64);
		previous_y = solo_ability_center_y((solo_ability_t)ability);
		ability++;
	}
	assert(solo_ability_cost(SOLO_ABILITY_NONE) == -1);
	printf("PASS test_ability_metadata_and_meter_spacing\n");
}

/**
 * @brief Exercises the level-one board transform and exact crystal spend.
 */
static void	test_mirurun_removes_bottom_four_rows(void)
{
	solo_game_t	game;
	int			row;
	int			col;

	solo_game_init(&game, 901u);
	row = BOARD_HEIGHT - 4;
	while (row < BOARD_HEIGHT)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			board_set(&game.board, col, row, (t_cell){CELL_GARBAGE, 0});
			col++;
		}
		row++;
	}
	board_set(&game.board, 0, BOARD_HEIGHT - 5,
		(t_cell){CELL_FILLED, PIECE_L});
	game.crystal_charge = 2;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_ACTIVATED);
	assert(game.crystal_charge == 0);
	assert(game.scoring.total == 0 && game.total_lines == 0);
	assert(board_get(&game.board, 0, BOARD_HEIGHT - 1).type == CELL_FILLED);
	assert(board_get(&game.board, 0, BOARD_HEIGHT - 1).color == PIECE_L);
	row = 0;
	while (row < 4)
	{
		col = 0;
		while (col < BOARD_WIDTH)
		{
			assert(board_get(&game.board, col, row).type == CELL_EMPTY);
			col++;
		}
		row++;
	}
	printf("PASS test_mirurun_removes_bottom_four_rows\n");
}

/**
 * @brief Exercises invalid, unaffordable, and non-active rejection paths.
 */
static void	test_activation_rejections_preserve_charge(void)
{
	solo_game_t	game;
	t_board		before;
	int			cols[4];
	int			rows[4];

	solo_game_init(&game, 902u);
	game.crystal_charge = 1;
	before = game.board;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_NO_CHARGE);
	assert(game.crystal_charge == 1);
	assert(memcmp(&game.board, &before, sizeof(before)) == 0);
	game.ability_result = SOLO_ABILITY_RESULT_NONE;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_NONE)
		== SOLO_ABILITY_RESULT_INVALID);
	assert(game.ability_result == SOLO_ABILITY_RESULT_NONE);
	game.active.row = 8;
	assert(piece_cells(&game.active, cols, rows));
	board_set(&game.board, cols[0], rows[0] - 4,
		(t_cell){CELL_FILLED, PIECE_T});
	assert(piece_is_valid(&game.board, &game.active));
	game.crystal_charge = 2;
	before = game.board;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_BLOCKED);
	assert(game.crystal_charge == 2);
	assert(memcmp(&game.board, &before, sizeof(before)) == 0);
	game.crystal_charge = SOLO_CRYSTAL_CAPACITY;
	game.phase = SOLO_CLEARING;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_SIRTET)
		== SOLO_ABILITY_RESULT_UNAVAILABLE);
	assert(game.crystal_charge == SOLO_CRYSTAL_CAPACITY);
	printf("PASS test_activation_rejections_preserve_charge\n");
}

/**
 * @brief Exercises temporary visual-only Solo activation for levels two-four.
 */
static void	test_opponent_abilities_are_visual_solo_tests(void)
{
	solo_game_t	game;
	t_board		before;
	int			ability;

	ability = SOLO_ABILITY_INVERSION;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		solo_game_init(&game, (uint32_t)(910 + ability));
		board_set(&game.board, ability, BOARD_HEIGHT - 1,
			(t_cell){CELL_FILLED, PIECE_T});
		before = game.board;
		game.crystal_charge = SOLO_CRYSTAL_CAPACITY;
		assert(solo_game_activate_ability(&game,
				(solo_ability_t)ability) == SOLO_ABILITY_RESULT_ACTIVATED);
		assert(game.crystal_charge == SOLO_CRYSTAL_CAPACITY - ability * 2);
		assert(memcmp(&game.board, &before, sizeof(before)) == 0);
		ability++;
	}
	printf("PASS test_opponent_abilities_are_visual_solo_tests\n");
}

/**
 * @brief Exercises static feedback timing through the shared poll deadline.
 */
static void	test_feedback_expires_on_shared_wake_deadline(void)
{
	solo_game_t	game;

	solo_game_init(&game, 903u);
	game.crystal_charge = 2;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_MIRURUN)
		== SOLO_ABILITY_RESULT_ACTIVATED);
	assert(solo_game_next_wake_ms(&game) == SOLO_ABILITY_FEEDBACK_MS);
	assert(!solo_game_update(&game, SOLO_ABILITY_FEEDBACK_MS - 1));
	assert(game.ability_result == SOLO_ABILITY_RESULT_ACTIVATED);
	assert(solo_game_next_wake_ms(&game) == 1);
	assert(solo_game_update(&game, 1));
	assert(game.ability_result == SOLO_ABILITY_RESULT_NONE);
	assert(game.last_ability == SOLO_ABILITY_NONE);
	game.phase = SOLO_GAME_OVER;
	assert(solo_game_activate_ability(&game, SOLO_ABILITY_INVERSION)
		== SOLO_ABILITY_RESULT_UNAVAILABLE);
	assert(solo_game_next_wake_ms(&game) == SOLO_ABILITY_FEEDBACK_MS);
	assert(solo_game_update(&game, SOLO_ABILITY_FEEDBACK_MS));
	assert(game.ability_result == SOLO_ABILITY_RESULT_NONE);
	assert(solo_game_next_wake_ms(&game) == -1);
	printf("PASS test_feedback_expires_on_shared_wake_deadline\n");
}

/**
 * @brief Exercises forgiving authored hitboxes and scaled mouse conversion.
 */
static void	test_canvas_hitboxes_and_terminal_mapping(void)
{
	render_ctx_t	ctx;
	solo_render_t	solo;
	ncinput			input;
	int				ability;
	int				center_x;
	int				center_y;
	int				physical_x;
	int				physical_y;
	int				canvas_x;
	int				canvas_y;

	center_x = HUD_METER_X + HUD_METER_WIDTH / 2;
	ability = SOLO_ABILITY_MIRURUN;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		center_y = solo_ability_center_y((solo_ability_t)ability);
		assert(solo_ability_at_canvas(center_x, center_y)
			== (solo_ability_t)ability);
		assert(solo_ability_at_canvas(center_x
				+ SOLO_ABILITY_HITBOX_HALF_WIDTH, center_y)
			== (solo_ability_t)ability);
		ability++;
	}
	assert(solo_ability_at_canvas(center_x, 266) == SOLO_ABILITY_NONE);
	memset(&ctx, 0, sizeof(ctx));
	memset(&solo, 0, sizeof(solo));
	memset(&input, 0, sizeof(input));
	ctx.cell_px_x = 8;
	ctx.cell_px_y = 16;
	solo.layout_valid = true;
	solo.canvas_col = 7;
	solo.canvas_row = 3;
	solo.canvas_cols = 128;
	solo.canvas_rows = 48;
	center_y = solo_ability_center_y(SOLO_ABILITY_MIRURUN);
	physical_x = center_x * solo.canvas_cols * ctx.cell_px_x
		/ SOLO_CANVAS_WIDTH;
	physical_y = center_y * solo.canvas_rows * ctx.cell_px_y
		/ SOLO_CANVAS_HEIGHT;
	input.x = solo.canvas_col + physical_x / ctx.cell_px_x;
	input.y = solo.canvas_row + physical_y / ctx.cell_px_y;
	input.xpx = physical_x % ctx.cell_px_x;
	input.ypx = physical_y % ctx.cell_px_y;
	assert(solo_mouse_canvas_position(&ctx, &solo, &input,
			&canvas_x, &canvas_y));
	assert(canvas_x == center_x && canvas_y == center_y);
	assert(solo_ability_at_canvas(canvas_x, canvas_y)
		== SOLO_ABILITY_MIRURUN);
	input.xpx = -1;
	input.ypx = -1;
	assert(solo_mouse_canvas_position(&ctx, &solo, &input,
			&canvas_x, &canvas_y));
	assert(solo_ability_at_canvas(canvas_x, canvas_y)
		== SOLO_ABILITY_MIRURUN);
	input.x = solo.canvas_col - 1;
	assert(!solo_mouse_canvas_position(&ctx, &solo, &input,
			&canvas_x, &canvas_y));
	printf("PASS test_canvas_hitboxes_and_terminal_mapping\n");
}
