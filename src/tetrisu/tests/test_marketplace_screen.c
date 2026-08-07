#include "tetrisu.h"

static void	test_region_planes_never_overlap(void);
static void	test_marketplace_layout_contract(void);
static void	test_fixture_marketplace_model(void);
static void	test_offline_marketplace_is_refused(void);
static void	test_focus_and_actions(void);
static void	test_inventory_grid_navigation(void);
static void	test_detail_follows_focus(void);
static void	test_buying_debits_the_wallet(void);
static void	test_buying_never_equips(void);
static void	test_equipping_owned_items(void);
static void	test_input_batch_boundaries(void);
static void	test_backwards_focus_reaches_the_last_slot(void);
static void	test_leaving_a_grid_focuses_the_button_below(void);

int	main(void)
{
	test_region_planes_never_overlap();
	test_marketplace_layout_contract();
	test_fixture_marketplace_model();
	test_offline_marketplace_is_refused();
	test_focus_and_actions();
	test_inventory_grid_navigation();
	test_detail_follows_focus();
	test_buying_debits_the_wallet();
	test_buying_never_equips();
	test_equipping_owned_items();
	test_input_batch_boundaries();
	test_backwards_focus_reaches_the_last_slot();
	test_leaving_a_grid_focuses_the_button_below();
	return (0);
}

/**
 * @brief Expands a region to whole cells the way the renderer's planes do.
 */
static void	cell_span(const marketplace_rect_t *rect, int cell_px_x,
	int cell_px_y, int *x0, int *y0, int *x1, int *y1)
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
	marketplace_layout_t	layout;
	marketplace_rect_t		regions[4];
	int						bounds[4][4];
	int						cols;
	int						rows;
	int						cell;
	int						first;
	int						second;

	cols = 44;
	while (cols <= 400)
	{
		rows = 20;
		while (rows <= 120)
		{
			cell = 2;
			while (cell <= 40)
			{
				marketplace_layout_build(0, 0, rows, cols, cell, cell, &layout);
				regions[0] = layout.characters;
				regions[1] = layout.themes;
				regions[2] = layout.detail;
				regions[3] = layout.controls;
				first = 0;
				while (first < 4)
				{
					cell_span(&regions[first], layout.cell_px_x,
						layout.cell_px_y, &bounds[first][0], &bounds[first][1],
						&bounds[first][2], &bounds[first][3]);
					first++;
				}
				first = 0;
				while (first < 4)
				{
					second = first + 1;
					while (second < 4)
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
	printf("PASS test_region_planes_never_overlap\n");
}

static void	test_marketplace_layout_contract(void)
{
	marketplace_layout_t	layout;
	int						index;

	marketplace_layout_build(3, 5, 54, 144, 16, 8, &layout);
	assert(layout.pixel_width == 1152);
	assert(layout.pixel_height == 864);
	assert(layout.origin_y == 3 && layout.origin_x == 5);
	assert(layout.title.width > 0 && layout.title.height > 0);
	assert(layout.characters.x < layout.themes.x);
	assert(layout.characters.y == layout.themes.y);
	assert(layout.detail.y > layout.characters.y + layout.characters.height);
	assert(layout.controls.y > layout.detail.y + layout.detail.height);
	assert(layout.stats[0].x < layout.stats[1].x);
	assert(layout.stats[1].x < layout.stats[2].x);
	/* The four control rectangles share one row and stay left-to-right. */
	index = 0;
	while (index < MARKETPLACE_BUTTON_COUNT)
	{
		assert(layout.buttons[index].width > 0);
		assert(layout.buttons[index].y == layout.buttons[0].y);
		if (index > 0)
			assert(layout.buttons[index - 1].x < layout.buttons[index].x);
		index++;
	}
	/* Buttons live inside the control region, so they must not escape it. */
	index = 0;
	while (index < MARKETPLACE_BUTTON_COUNT)
	{
		assert(layout.buttons[index].x >= layout.controls.x);
		assert(layout.buttons[index].x + layout.buttons[index].width
			<= layout.controls.x + layout.controls.width);
		assert(layout.buttons[index].y >= layout.controls.y);
		index++;
	}
	printf("PASS test_marketplace_layout_contract\n");
}

static void	test_fixture_marketplace_model(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			false, &view) == APP_PROVIDER_OK);
	assert(view.local_preview);
	assert(view.status == APP_DATA_READY);
	assert(view.data.marketplace.signed_in);
	assert(!view.data.marketplace.offline);
	assert(view.data.marketplace.profile.wallet_points == 3200);
	assert(view.data.marketplace.characters.count == 4);
	assert(view.data.marketplace.themes.count == 7);
	assert(view.data.marketplace.characters.items[0].owned);
	assert(!view.data.marketplace.characters.items[2].owned);
	assert(view.data.marketplace.characters.items[2].price == 1400);
	assert(!view.data.marketplace.themes.items[6].owned);
	assert(view.data.marketplace.themes.items[6].price == 1600);
	printf("PASS test_fixture_marketplace_model\n");
}

/**
 * @brief Offline sessions never reach the Marketplace, and never invent data.
 */
static void	test_offline_marketplace_is_refused(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;
	app_navigation_t		navigation;
	marketplace_state_t		state;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			true, &view) == APP_PROVIDER_OK);
	assert(!view.local_preview);
	assert(view.data.marketplace.offline);
	assert(!view.data.marketplace.signed_in);
	assert(view.data.marketplace.characters.count == 0);
	assert(view.data.marketplace.themes.count == 0);
	assert(view.data.marketplace.profile.wallet_points == 0);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	assert(app_navigation_dispatch(&navigation, APP_NAV_PLAY_OFFLINE));
	assert(!app_navigation_dispatch(&navigation, APP_NAV_OPEN_MARKETPLACE));
	/* A signed-out state exposes no slots and cannot spend. */
	marketplace_state_init(&state, false, 4, 7);
	assert(state.character_slots == 0 && state.theme_slots == 0);
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_INVALID);
	printf("PASS test_offline_marketplace_is_refused\n");
}

static void	test_focus_and_actions(void)
{
	marketplace_state_t	state;

	marketplace_state_init(&state, true, 4, 7);
	assert(state.section == MARKETPLACE_SECTION_CONTROLS);
	assert(state.focus == MARKETPLACE_FOCUS_BACK);
	assert(state.preview == MARKETPLACE_SECTION_CHARACTERS);
	assert(marketplace_handle_key(&state, NCKEY_ENTER)
		== MARKETPLACE_ACTION_BACK);
	assert(marketplace_handle_key(&state, NCKEY_RIGHT)
		== MARKETPLACE_ACTION_NONE);
	assert(state.focus == MARKETPLACE_FOCUS_BUY);
	assert(marketplace_handle_key(&state, NCKEY_ENTER)
		== MARKETPLACE_ACTION_BUY);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == MARKETPLACE_FOCUS_VOLUME_DOWN);
	assert(marketplace_handle_key(&state, NCKEY_ENTER)
		== MARKETPLACE_ACTION_VOLUME_DOWN);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == MARKETPLACE_FOCUS_VOLUME_UP);
	assert(marketplace_handle_key(&state, '+') == MARKETPLACE_ACTION_VOLUME_UP);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == MARKETPLACE_FOCUS_BACK);
	assert(marketplace_handle_key(&state, NCKEY_LEFT)
		== MARKETPLACE_ACTION_NONE);
	assert(state.focus == MARKETPLACE_FOCUS_VOLUME_UP);
	assert(marketplace_handle_key(&state, 'b') == MARKETPLACE_ACTION_BUY);
	assert(marketplace_handle_key(&state, 'e') == MARKETPLACE_ACTION_EQUIP);
	assert(marketplace_handle_key(&state, NCKEY_ESC)
		== MARKETPLACE_ACTION_BACK);
	assert(marketplace_handle_key(&state, 'q') == MARKETPLACE_ACTION_QUIT);
	/* Signed-out sessions skip Buy and refuse both purchase shortcuts. */
	marketplace_state_init(&state, false, 4, 7);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == MARKETPLACE_FOCUS_VOLUME_DOWN);
	assert(marketplace_handle_key(&state, 'b') == MARKETPLACE_ACTION_NONE);
	assert(marketplace_handle_key(&state, 'e') == MARKETPLACE_ACTION_NONE);
	printf("PASS test_focus_and_actions\n");
}

/* The fixture uses four columns: one full character row and two theme rows. */
static void	test_inventory_grid_navigation(void)
{
	marketplace_state_t	state;
	int					index;

	marketplace_state_init(&state, true, 4, 7);
	(void)marketplace_handle_key(&state, NCKEY_UP);
	assert(state.section == MARKETPLACE_SECTION_CHARACTERS);
	assert(state.character_slot == 0);
	(void)marketplace_handle_key(&state, NCKEY_UP);
	assert(state.character_slot == 0);
	index = 0;
	while (index < 3)
	{
		(void)marketplace_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.character_slot == 3);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(state.theme_slot == 0);
	(void)marketplace_handle_key(&state, NCKEY_LEFT);
	assert(state.section == MARKETPLACE_SECTION_CHARACTERS);
	assert(state.character_slot == 3);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	index = 0;
	while (index < 3)
	{
		(void)marketplace_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.theme_slot == 3);
	/* Column three entering the partial second row clamps to its final slot. */
	(void)marketplace_handle_key(&state, NCKEY_DOWN);
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(state.theme_slot == 6);
	(void)marketplace_handle_key(&state, NCKEY_DOWN);
	assert(state.section == MARKETPLACE_SECTION_CONTROLS);
	/* Enter inside a grid always acts on the item rather than a button. */
	(void)marketplace_handle_key(&state, NCKEY_UP);
	assert(marketplace_handle_key(&state, NCKEY_ENTER)
		== MARKETPLACE_ACTION_BUY);
	printf("PASS test_inventory_grid_navigation\n");
}

/**
 * @brief The detail card keeps describing the last inspected item.
 *
 * Stepping down to the Buy button must not blank the card or silently retarget
 * the purchase at the other panel.
 */
static void	test_detail_follows_focus(void)
{
	app_data_provider_t						provider;
	app_screen_view_model_t					view;
	marketplace_state_t						state;
	const app_catalogue_item_view_model_t	*item;
	int										index;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			false, &view) == APP_PROVIDER_OK);
	marketplace_state_init(&state, true, 4, 7);
	item = marketplace_focused_item(&view.data.marketplace, &state);
	assert(item != NULL && strcmp(item->name, "Mirurun") == 0);
	(void)marketplace_handle_key(&state, NCKEY_UP);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	item = marketplace_focused_item(&view.data.marketplace, &state);
	assert(item != NULL && strcmp(item->name, "Princess") == 0);
	assert(marketplace_focused_is_character(&state));
	/* Dropping to the controls keeps the same item under the Buy button. */
	(void)marketplace_handle_key(&state, NCKEY_DOWN);
	assert(state.section == MARKETPLACE_SECTION_CONTROLS);
	item = marketplace_focused_item(&view.data.marketplace, &state);
	assert(item != NULL && strcmp(item->name, "Princess") == 0);
	/* Crossing into themes retargets both the card and the purchase. */
	(void)marketplace_handle_key(&state, NCKEY_UP);
	index = 0;
	while (index < 5)
	{
		(void)marketplace_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(!marketplace_focused_is_character(&state));
	item = marketplace_focused_item(&view.data.marketplace, &state);
	assert(item != NULL && strcmp(item->name, "Design AI University") == 0);
	printf("PASS test_detail_follows_focus\n");
}

static void	test_buying_debits_the_wallet(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;
	marketplace_state_t		state;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			false, &view) == APP_PROVIDER_OK);
	marketplace_state_init(&state, true, 4, 7);
	state.section = MARKETPLACE_SECTION_CHARACTERS;
	state.preview = MARKETPLACE_SECTION_CHARACTERS;
	/* Slot zero is owned already, so it reports that rather than charging. */
	state.character_slot = 0;
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_OWNED);
	assert(view.data.marketplace.profile.wallet_points == 3200);
	/* Princess costs 1400 of the 3200 on hand. */
	state.character_slot = 2;
	assert(marketplace_can_afford(&view.data.marketplace,
			marketplace_focused_item(&view.data.marketplace, &state)));
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_BOUGHT);
	assert(view.data.marketplace.profile.wallet_points == 1800);
	assert(view.data.marketplace.characters.items[2].owned);
	/* A second purchase of the same slot is refused and charges nothing. */
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_OWNED);
	assert(view.data.marketplace.profile.wallet_points == 1800);
	/* Wolf-man costs 1800; buying it empties the wallet exactly. */
	state.character_slot = 3;
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_BOUGHT);
	assert(view.data.marketplace.profile.wallet_points == 0);
	/* Clauding costs 1600 with nothing left, so the wallet is untouched. */
	state.section = MARKETPLACE_SECTION_THEMES;
	state.preview = MARKETPLACE_SECTION_THEMES;
	state.theme_slot = 6;
	assert(!marketplace_can_afford(&view.data.marketplace,
			marketplace_focused_item(&view.data.marketplace, &state)));
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_INSUFFICIENT);
	assert(view.data.marketplace.profile.wallet_points == 0);
	assert(!view.data.marketplace.themes.items[6].owned);
	/* Slots past what the panels draw are rejected rather than clamped. */
	state.theme_slot = 99;
	assert(marketplace_focused_item(&view.data.marketplace, &state) == NULL);
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_INVALID);
	printf("PASS test_buying_debits_the_wallet\n");
}

/**
 * @brief A purchase leaves the loadout alone; equipping is a separate step.
 */
static void	test_buying_never_equips(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;
	marketplace_state_t		state;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			false, &view) == APP_PROVIDER_OK);
	marketplace_state_init(&state, true, 4, 7);
	state.section = MARKETPLACE_SECTION_CHARACTERS;
	state.preview = MARKETPLACE_SECTION_CHARACTERS;
	state.character_slot = 2;
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_BOUGHT);
	assert(!view.data.marketplace.characters.items[2].equipped);
	assert(view.data.marketplace.characters.items[0].equipped);
	assert(strcmp(view.data.marketplace.profile.character, "Mirurun") == 0);
	printf("PASS test_buying_never_equips\n");
}

static void	test_equipping_owned_items(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;
	marketplace_state_t		state;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_MARKETPLACE,
			false, &view) == APP_PROVIDER_OK);
	marketplace_state_init(&state, true, 4, 7);
	state.section = MARKETPLACE_SECTION_CHARACTERS;
	state.preview = MARKETPLACE_SECTION_CHARACTERS;
	state.character_slot = 0;
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_UNCHANGED);
	state.character_slot = 1;
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_CHANGED);
	assert(strcmp(view.data.marketplace.profile.character, "Halloween") == 0);
	assert(strcmp(view.data.marketplace.profile.portrait_asset,
			HALLOWEEN_PORTRAIT_PATH) == 0);
	assert(!view.data.marketplace.characters.items[0].equipped);
	/* Locked entries stay inspectable but can never become the loadout. */
	state.character_slot = 3;
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_LOCKED);
	assert(strcmp(view.data.marketplace.profile.character, "Halloween") == 0);
	state.section = MARKETPLACE_SECTION_THEMES;
	state.preview = MARKETPLACE_SECTION_THEMES;
	state.theme_slot = 5;
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_CHANGED);
	assert(strcmp(view.data.marketplace.profile.theme, "Nuclear Ghandi") == 0);
	assert(!view.data.marketplace.themes.items[0].equipped);
	/* Buying a locked theme then equipping it is the full purchase journey. */
	state.theme_slot = 3;
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_LOCKED);
	assert(marketplace_buy_focused(&view.data.marketplace, &state)
		== MARKETPLACE_PURCHASE_BOUGHT);
	assert(marketplace_equip_focused(&view.data.marketplace, &state)
		== SETTINGS_EQUIP_CHANGED);
	assert(strcmp(view.data.marketplace.profile.theme, "Haaland") == 0);
	printf("PASS test_equipping_owned_items\n");
}

/**
 * @brief Held navigation repeats never absorb a different queued command.
 */
static void	test_input_batch_boundaries(void)
{
	assert(marketplace_navigation_keys_coalesce(NCKEY_UP, NCKEY_UP));
	assert(marketplace_navigation_keys_coalesce(NCKEY_DOWN, NCKEY_DOWN));
	assert(marketplace_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_LEFT));
	assert(marketplace_navigation_keys_coalesce(NCKEY_RIGHT, NCKEY_RIGHT));
	assert(marketplace_navigation_keys_coalesce(NCKEY_TAB, NCKEY_TAB));
	assert(!marketplace_navigation_keys_coalesce(NCKEY_UP, NCKEY_DOWN));
	assert(!marketplace_navigation_keys_coalesce(NCKEY_UP, NCKEY_ENTER));
	assert(!marketplace_navigation_keys_coalesce('b', 'b'));
	assert(marketplace_action_leaves_screen(MARKETPLACE_ACTION_BACK));
	assert(marketplace_action_leaves_screen(MARKETPLACE_ACTION_QUIT));
	assert(!marketplace_action_leaves_screen(MARKETPLACE_ACTION_NONE));
	assert(!marketplace_action_leaves_screen(MARKETPLACE_ACTION_BUY));
	assert(!marketplace_action_leaves_screen(MARKETPLACE_ACTION_EQUIP));
	assert(!marketplace_action_leaves_screen(MARKETPLACE_ACTION_VOLUME_UP));
	printf("PASS test_input_batch_boundaries\n");
}

/**
 * @brief Backwards focus must land on the final drawn slot, not the first row.
 */
static void	test_backwards_focus_reaches_the_last_slot(void)
{
	marketplace_state_t	state;

	marketplace_state_init(&state, true, 4, 7);
	marketplace_state_focus_previous(&state);
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(state.theme_slot == 6);
	while (state.theme_slot > 0)
		marketplace_state_focus_previous(&state);
	marketplace_state_focus_previous(&state);
	assert(state.section == MARKETPLACE_SECTION_CHARACTERS);
	assert(state.character_slot == 3);
	/* A themes-only panel still reaches its final slot. */
	marketplace_state_init(&state, true, 0, 7);
	assert(state.preview == MARKETPLACE_SECTION_THEMES);
	marketplace_state_focus_previous(&state);
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(state.theme_slot == 6);
	/* Tab forwards out of the controls skips the empty characters panel. */
	marketplace_state_init(&state, true, 0, 7);
	marketplace_state_focus_next(&state);
	marketplace_state_focus_next(&state);
	marketplace_state_focus_next(&state);
	marketplace_state_focus_next(&state);
	assert(state.section == MARKETPLACE_SECTION_THEMES);
	assert(state.theme_slot == 0);
	printf("PASS test_backwards_focus_reaches_the_last_slot\n");
}

/**
 * @brief Dropping out of a grid focuses the button under the column left.
 */
static void	test_leaving_a_grid_focuses_the_button_below(void)
{
	marketplace_state_t	state;

	marketplace_state_init(&state, true, 4, 7);
	(void)marketplace_handle_key(&state, NCKEY_UP);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	(void)marketplace_handle_key(&state, NCKEY_RIGHT);
	assert(state.character_slot == 2);
	(void)marketplace_handle_key(&state, NCKEY_DOWN);
	assert(state.section == MARKETPLACE_SECTION_CONTROLS);
	assert(state.focus == MARKETPLACE_FOCUS_VOLUME_DOWN);
	/* Column one is the Buy button, which signed-out users skip. */
	marketplace_state_init(&state, false, 4, 7);
	state.character_slots = 4;
	state.section = MARKETPLACE_SECTION_CHARACTERS;
	state.character_slot = 1;
	(void)marketplace_handle_key(&state, NCKEY_DOWN);
	assert(state.section == MARKETPLACE_SECTION_CONTROLS);
	assert(state.focus == MARKETPLACE_FOCUS_BACK);
	printf("PASS test_leaving_a_grid_focuses_the_button_below\n");
}
