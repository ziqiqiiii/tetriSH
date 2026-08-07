#include "tetrisu.h"

static void	test_region_planes_never_overlap(void);
static void	test_layout_contract(void);
static void	test_unused_rectangles_stay_zero(void);
static void	test_mode_picker_focus_and_actions(void);
static void	test_mode_picker_copy_is_bounded(void);
static void	test_create_room_focus_and_actions(void);
static void	test_mode_input_batch_boundaries(void);
static void	region_list(const mp_layout_t *layout, app_screen_t screen,
				const mp_rect_t **regions, int *count);

int	main(void)
{
	test_region_planes_never_overlap();
	test_layout_contract();
	test_unused_rectangles_stay_zero();
	test_mode_picker_focus_and_actions();
	test_mode_picker_copy_is_bounded();
	test_create_room_focus_and_actions();
	test_mode_input_batch_boundaries();
	return (0);
}

/**
 * @brief Collects the rectangles one screen actually turns into planes.
 *
 * The layout struct also holds plates and control rows, which are drawn into
 * other surfaces rather than given planes of their own. Only the entries below
 * become sprixels, so only these have to be disjoint.
 */
static void	region_list(const mp_layout_t *layout, app_screen_t screen,
	const mp_rect_t **regions, int *count)
{
	*count = 0;
	if (screen == APP_SCREEN_MULTIPLAYER_MODE)
		regions[(*count)++] = &layout->cards;
	else if (screen == APP_SCREEN_LOBBY)
	{
		regions[(*count)++] = &layout->list;
		regions[(*count)++] = &layout->field;
		regions[(*count)++] = &layout->status;
	}
	else if (screen == APP_SCREEN_CREATE_ROOM_MODAL)
		regions[(*count)++] = &layout->options;
	else if (screen == APP_SCREEN_WAITING_ROOM)
	{
		regions[(*count)++] = &layout->slots;
		regions[(*count)++] = &layout->status;
		regions[(*count)++] = &layout->chat;
	}
}

/**
 * @brief Expands a region to whole cells the way the renderer's planes do.
 */
static void	cell_span(const mp_rect_t *rect, int cell_px_x, int cell_px_y,
	int *x0, int *y0, int *x1, int *y1)
{
	*x0 = rect->x / cell_px_x;
	*y0 = rect->y / cell_px_y;
	*x1 = (rect->x + rect->width + cell_px_x - 1) / cell_px_x;
	*y1 = (rect->y + rect->height + cell_px_y - 1) / cell_px_y;
}

/**
 * @brief Region planes must never share a cell on any supported geometry.
 *
 * Each region becomes its own bitmap plane. A stationary protocol cannot stack
 * those: re-emitting one blanks whatever it overlaps. Because every plane is
 * expanded out to whole cells, two rectangles that merely sit close together in
 * reference pixels can still collide once the terminal's cell size is coarse
 * enough, so the check sweeps geometries rather than trusting one.
 */
static void	test_region_planes_never_overlap(void)
{
	static const app_screen_t	screens[] = {
		APP_SCREEN_MULTIPLAYER_MODE,
		APP_SCREEN_LOBBY,
		APP_SCREEN_CREATE_ROOM_MODAL,
		APP_SCREEN_WAITING_ROOM
	};
	mp_layout_t					layout;
	const mp_rect_t				*regions[4];
	int							bounds[4][4];
	size_t						screen;
	int							count;
	int							cols;
	int							rows;
	int							cell;
	int							first;
	int							second;

	screen = 0;
	while (screen < sizeof(screens) / sizeof(screens[0]))
	{
		cols = 44;
		while (cols <= 400)
		{
			rows = 20;
			while (rows <= 120)
			{
				cell = 2;
				while (cell <= 40)
				{
					mp_layout_build(screens[screen], 0, 0, rows, cols, cell,
						cell, &layout);
					region_list(&layout, screens[screen], regions, &count);
					first = 0;
					while (first < count)
					{
						cell_span(regions[first], layout.cell_px_x,
							layout.cell_px_y, &bounds[first][0],
							&bounds[first][1], &bounds[first][2],
							&bounds[first][3]);
						first++;
					}
					first = 0;
					while (first < count)
					{
						second = first + 1;
						while (second < count)
						{
							assert(bounds[first][2] <= bounds[second][0]
								|| bounds[second][2] <= bounds[first][0]
								|| bounds[first][3] <= bounds[second][1]
								|| bounds[second][3] <= bounds[first][1]);
							second++;
						}
						first++;
					}
					cell += 2;
				}
				rows += 4;
			}
			cols += 13;
		}
		screen++;
	}
	printf("PASS test_region_planes_never_overlap\n");
}

/**
 * @brief The mapped rectangles must keep the relationships the art assumes.
 */
static void	test_layout_contract(void)
{
	mp_layout_t	layout;
	int			index;

	mp_layout_build(APP_SCREEN_MULTIPLAYER_MODE, 3, 5, 54, 144, 16, 8,
		&layout);
	assert(layout.pixel_width == 1152 && layout.pixel_height == 864);
	assert(layout.origin_y == 3 && layout.origin_x == 5);
	assert(layout.cards.width > 0 && layout.cards.height > 0);
	/* The card strip's plane must contain the control legend it draws. */
	assert(layout.controls.y + layout.controls.height
		<= layout.cards.y + layout.cards.height);
	index = 0;
	while (index + 1 < MP_MODE_CARD_COUNT)
	{
		assert(layout.card_slots[index].x < layout.card_slots[index + 1].x);
		assert(layout.card_slots[index].x + layout.card_slots[index].width
			< layout.card_slots[index + 1].x);
		index++;
	}
	mp_layout_build(APP_SCREEN_LOBBY, 0, 0, 54, 144, 16, 8, &layout);
	assert(layout.list.x + layout.list.width < layout.field.x);
	assert(layout.list.y + layout.list.height < layout.status.y);
	assert(layout.list.x >= layout.rooms_plate.x);
	assert(layout.field.x >= layout.join_plate.x);
	mp_layout_build(APP_SCREEN_WAITING_ROOM, 0, 0, 54, 144, 16, 8, &layout);
	assert(layout.slots.y + layout.slots.height < layout.status.y);
	assert(layout.slots.x + layout.slots.width < layout.chat.x);
	assert(layout.status.x + layout.status.width < layout.chat.x);
	assert(layout.chat.y >= layout.chat_plate.y);
	printf("PASS test_layout_contract\n");
}

/**
 * @brief A screen must not leave another screen's rectangles populated.
 *
 * The four screens share one layout struct and one set of planes, so a stale
 * non-zero rectangle would let a region be cropped for the wrong screen.
 */
static void	test_unused_rectangles_stay_zero(void)
{
	mp_layout_t	layout;

	mp_layout_build(APP_SCREEN_MULTIPLAYER_MODE, 0, 0, 54, 144, 16, 8, &layout);
	assert(layout.list.width == 0 && layout.chat.width == 0);
	assert(layout.slots.width == 0 && layout.options.width == 0);
	mp_layout_build(APP_SCREEN_LOBBY, 0, 0, 54, 144, 16, 8, &layout);
	assert(layout.cards.width == 0 && layout.chat.width == 0);
	assert(layout.options.width == 0 && layout.slots.width == 0);
	mp_layout_build(APP_SCREEN_CREATE_ROOM_MODAL, 0, 0, 54, 144, 16, 8,
		&layout);
	assert(layout.list.width == 0 && layout.status.width == 0);
	mp_layout_build(APP_SCREEN_WAITING_ROOM, 0, 0, 54, 144, 16, 8, &layout);
	assert(layout.cards.width == 0 && layout.list.width == 0);
	assert(layout.options.width == 0 && layout.field.width == 0);
	printf("PASS test_unused_rectangles_stay_zero\n");
}

/**
 * @brief The picker moves on both axes and reports the mode it settled on.
 */
static void	test_mode_picker_focus_and_actions(void)
{
	mp_mode_state_t	state;

	mp_mode_state_init(&state);
	assert(state.focus == MP_MODE_FOCUS_DOUBLE);
	assert(mp_mode_focused_mode(&state) == APP_GAME_MODE_DOUBLE);
	assert(mp_mode_handle_key(&state, NCKEY_RIGHT) == MP_MODE_ACTION_NONE);
	assert(state.focus == MP_MODE_FOCUS_BATTLE_ROYALE);
	assert(mp_mode_focused_mode(&state) == APP_GAME_MODE_BATTLE_ROYALE);
	assert(mp_mode_handle_key(&state, NCKEY_DOWN) == MP_MODE_ACTION_NONE);
	assert(state.focus == MP_MODE_FOCUS_BATTLE_ROYALE);
	assert(mp_mode_handle_key(&state, NCKEY_UP) == MP_MODE_ACTION_NONE);
	assert(state.focus == MP_MODE_FOCUS_DOUBLE);
	/* The digits pick a card and confirm it in the same keystroke. */
	assert(mp_mode_handle_key(&state, '2') == MP_MODE_ACTION_SELECT);
	assert(state.focus == MP_MODE_FOCUS_BATTLE_ROYALE);
	assert(mp_mode_handle_key(&state, NCKEY_ENTER) == MP_MODE_ACTION_SELECT);
	assert(mp_mode_handle_key(&state, NCKEY_ESC) == MP_MODE_ACTION_BACK);
	assert(mp_mode_handle_key(&state, 'b') == MP_MODE_ACTION_BACK);
	assert(mp_mode_handle_key(&state, 'q') == MP_MODE_ACTION_QUIT);
	assert(mp_mode_handle_key(&state, '+') == MP_MODE_ACTION_VOLUME_UP);
	assert(mp_mode_handle_key(&state, '-') == MP_MODE_ACTION_VOLUME_DOWN);
	assert(mp_mode_action_leaves_screen(MP_MODE_ACTION_SELECT));
	assert(!mp_mode_action_leaves_screen(MP_MODE_ACTION_VOLUME_UP));
	/* Moving retires the last result, which described the old cursor. */
	state.feedback = MP_MODE_FEEDBACK_VOLUME;
	(void)mp_mode_handle_key(&state, NCKEY_LEFT);
	assert(state.feedback == MP_MODE_FEEDBACK_NONE);
	printf("PASS test_mode_picker_focus_and_actions\n");
}

/**
 * @brief Every caption the cards draw must exist and stay inside its bounds.
 */
static void	test_mode_picker_copy_is_bounded(void)
{
	char	line[APP_TEXT_MAX];
	int		index;

	index = 0;
	while (index < MP_MODE_CARD_COUNT)
	{
		assert(mp_mode_card_name(index)[0] != '\0');
		assert(mp_mode_card_players(index)[0] != '\0');
		assert(mp_mode_card_line(index, 0)[0] != '\0');
		assert(mp_mode_card_line(index, 1)[0] != '\0');
		assert(strlen(mp_mode_card_name(index)) < 24);
		index++;
	}
	assert(mp_mode_card_name(-1)[0] == '\0');
	assert(mp_mode_card_name(MP_MODE_CARD_COUNT)[0] == '\0');
	assert(mp_mode_card_line(0, 2)[0] == '\0');
	assert(mp_feedback_text(MP_MODE_FEEDBACK_NONE, 0, line,
			sizeof(line))[0] == '\0');
	assert(strstr(mp_feedback_text(MP_MODE_FEEDBACK_VOLUME, 40, line,
				sizeof(line)), "40%") != NULL);
	printf("PASS test_mode_picker_copy_is_bounded\n");
}

/**
 * @brief The create-room panel carries the lobby's mode in and reports it out.
 */
static void	test_create_room_focus_and_actions(void)
{
	create_room_state_t	state;

	create_room_state_init(&state, APP_GAME_MODE_BATTLE_ROYALE);
	assert(state.mode == APP_GAME_MODE_BATTLE_ROYALE);
	assert(create_room_focused_index(&state) == 1);
	/* An unset filter must not leave the panel on an invalid mode. */
	create_room_state_init(&state, APP_GAME_MODE_NONE);
	assert(state.mode == APP_GAME_MODE_DOUBLE);
	assert(create_room_focused_index(&state) == 0);
	assert(create_room_handle_key(&state, NCKEY_DOWN)
		== CREATE_ROOM_ACTION_NONE);
	assert(state.mode == APP_GAME_MODE_BATTLE_ROYALE);
	assert(create_room_handle_key(&state, NCKEY_UP)
		== CREATE_ROOM_ACTION_NONE);
	assert(state.mode == APP_GAME_MODE_DOUBLE);
	assert(create_room_handle_key(&state, '2') == CREATE_ROOM_ACTION_CREATE);
	assert(state.mode == APP_GAME_MODE_BATTLE_ROYALE);
	assert(create_room_handle_key(&state, NCKEY_ENTER)
		== CREATE_ROOM_ACTION_CREATE);
	assert(create_room_handle_key(&state, NCKEY_ESC)
		== CREATE_ROOM_ACTION_CANCEL);
	assert(create_room_handle_key(&state, 'q') == CREATE_ROOM_ACTION_QUIT);
	assert(create_room_action_leaves_screen(CREATE_ROOM_ACTION_CREATE));
	assert(!create_room_action_leaves_screen(CREATE_ROOM_ACTION_NONE));
	printf("PASS test_create_room_focus_and_actions\n");
}

/**
 * @brief Only identical movement keys may be folded into one repaint.
 */
static void	test_mode_input_batch_boundaries(void)
{
	assert(mp_mode_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_LEFT));
	assert(mp_mode_navigation_keys_coalesce(NCKEY_DOWN, NCKEY_DOWN));
	assert(!mp_mode_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_RIGHT));
	assert(!mp_mode_navigation_keys_coalesce(NCKEY_ENTER, NCKEY_ENTER));
	assert(!mp_mode_navigation_keys_coalesce('1', '1'));
	printf("PASS test_mode_input_batch_boundaries\n");
}
