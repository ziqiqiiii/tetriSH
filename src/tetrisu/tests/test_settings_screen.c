#include "tetrisu.h"

static void	test_fixture_settings_model(void);
static void	test_offline_settings_are_account_free(void);
static void	test_settings_focus_and_actions(void);
static void	test_preview_gate_and_navigation(void);
static void	test_settings_layout_contract(void);
static void	test_character_selection(void);
static void	test_inventory_grid_navigation(void);
static void	test_slot_equipping(void);
static void	test_powers_card_follows_focus(void);
static void	test_settings_input_batch_boundaries(void);
static void	test_backwards_focus_reaches_the_last_slot(void);
static void	test_leaving_a_grid_focuses_the_button_below(void);
static void	test_region_planes_never_overlap(void);

int	main(void)
{
	test_region_planes_never_overlap();
	test_fixture_settings_model();
	test_offline_settings_are_account_free();
	test_settings_focus_and_actions();
	test_preview_gate_and_navigation();
	test_settings_layout_contract();
	test_character_selection();
	test_inventory_grid_navigation();
	test_slot_equipping();
	test_powers_card_follows_focus();
	test_settings_input_batch_boundaries();
	test_backwards_focus_reaches_the_last_slot();
	test_leaving_a_grid_focuses_the_button_below();
	return (0);
}

/**
 * @brief Expands a region to whole cells the way the renderer's planes do.
 */
static void	cell_span(const settings_rect_t *rect, int cell_px_x,
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
	settings_layout_t	layout;
	settings_rect_t		regions[5];
	int					bounds[5][4];
	int					cols;
	int					rows;
	int					cell;
	int					first;
	int					second;
	int					count;

	cols = 44;
	while (cols <= 400)
	{
		rows = 20;
		while (rows <= 120)
		{
			cell = 2;
			while (cell <= 40)
			{
				settings_layout_build(0, 0, rows, cols, cell, cell, &layout);
				/*
				 * volume and card are mutually exclusive at runtime: the
				 * signed-in readout sits inside the card, so the renderer only
				 * ever keeps one of them alive. Every other pair must be
				 * disjoint, and the offline readout must clear the card too.
				 */
				regions[0] = layout.controls;
				regions[1] = layout.characters;
				regions[2] = layout.themes;
				regions[3] = layout.card;
				regions[4] = layout.volume_offline;
				count = 5;
				first = 0;
				while (first < count)
				{
					cell_span(&regions[first], layout.cell_px_x,
						layout.cell_px_y, &bounds[first][0], &bounds[first][1],
						&bounds[first][2], &bounds[first][3]);
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
	printf("PASS test_region_planes_never_overlap\n");
}

/**
 * @brief Backwards focus must land on the final drawn slot, not the first row.
 *
 * Themes fill two rows, so entering at row zero left every slot on the second
 * row unreachable by backwards tabbing.
 */
static void	test_backwards_focus_reaches_the_last_slot(void)
{
	settings_state_t	state;

	settings_state_init(&state, true, 4, 7);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	settings_state_focus_previous(&state);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(state.theme_slot == 6);
	/* Walking back off theme slot zero enters characters at its last slot. */
	while (state.theme_slot > 0)
		settings_state_focus_previous(&state);
	settings_state_focus_previous(&state);
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(state.character_slot == 3);
	/* A themes-only panel still reaches its final slot. */
	settings_state_init(&state, true, 0, 7);
	settings_state_focus_previous(&state);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(state.theme_slot == 6);
	printf("PASS test_backwards_focus_reaches_the_last_slot\n");
}

/**
 * @brief Dropping out of a grid focuses the button under the column left.
 *
 * The grid and the control strip are both four wide, so the landing button is
 * a property of where the cursor was, not of what was focused beforehand.
 */
static void	test_leaving_a_grid_focuses_the_button_below(void)
{
	settings_state_t	state;

	settings_state_init(&state, true, 4, 7);
	(void)settings_handle_key(&state, NCKEY_UP);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(state.character_slot == 2);
	(void)settings_handle_key(&state, NCKEY_DOWN);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_DOWN);
	/* Column one is the Marketplace button, which signed-out users skip. */
	settings_state_init(&state, false, 4, 7);
	state.character_slots = 4;
	state.section = SETTINGS_SECTION_CHARACTERS;
	state.character_slot = 1;
	(void)settings_handle_key(&state, NCKEY_DOWN);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	printf("PASS test_leaving_a_grid_focuses_the_button_below\n");
}

static void	test_fixture_settings_model(void)
{
	static const char	*character_names[] = {
		"Mirurun", "Halloween", "Princess", "Wolf-man"
	};
	static const char	*theme_names[] = {
		"Classic", "Design AI University",
		"Do You Wanna Build a Snowman", "Haaland",
		"Al Merqaedes F1 Team", "Nuclear Ghandi", "Clauding"
	};
	static const char	*theme_preview_paths[] = {
		SETTINGS_THEME_CLASSIC_PREVIEW_PATH,
		SETTINGS_THEME_DESIGN_AI_UNIVERSITY_PREVIEW_PATH,
		SETTINGS_THEME_SNOWMAN_PREVIEW_PATH,
		SETTINGS_THEME_HAALAND_PREVIEW_PATH,
		SETTINGS_THEME_AL_MERQAEDES_PREVIEW_PATH,
		SETTINGS_THEME_NUCLEAR_GHANDI_PREVIEW_PATH,
		SETTINGS_THEME_CLAUDING_PREVIEW_PATH
	};
	static const bool	character_owned[] = {true, true, false, false};
	static const bool	theme_owned[] = {
		true, true, true, false, true, true, false
	};
	app_data_provider_t	provider;
	app_screen_view_model_t	view;
	int				index;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	assert(view.local_preview);
	assert(view.status == APP_DATA_READY);
	assert(view.data.settings.signed_in);
	assert(!view.data.settings.offline);
	assert(strcmp(view.data.settings.profile.username, "PreviewPlayer") == 0);
	assert(strcmp(view.data.settings.profile.character, "Mirurun") == 0);
	assert(strcmp(view.data.settings.profile.theme, "Classic") == 0);
	assert(strcmp(view.data.settings.profile.portrait_asset,
		DEFAULT_MIRURUN_PATH) == 0);
	assert(view.data.settings.characters.count == 4);
	assert(view.data.settings.themes.count == 7);
	index = 0;
	while (index < view.data.settings.characters.count)
	{
		assert(strcmp(view.data.settings.characters.items[index].name,
			character_names[index]) == 0);
		assert(view.data.settings.characters.items[index].owned
			== character_owned[index]);
		index++;
	}
	index = 0;
	while (index < view.data.settings.themes.count)
	{
		assert(strcmp(view.data.settings.themes.items[index].name,
			theme_names[index]) == 0);
		/* Theme thumbnails use the authored canonical preview paths. */
		assert(view.data.settings.themes.items[index].portrait_asset[0] != '\0');
		assert(strcmp(view.data.settings.themes.items[index].portrait_asset,
			theme_preview_paths[index]) == 0);
		assert(strstr(view.data.settings.themes.items[index].portrait_asset,
			"settings_previews/") != NULL);
		assert(view.data.settings.themes.items[index].owned
			== theme_owned[index]);
		index++;
	}
	assert(view.data.settings.characters.items[0].owned);
	assert(view.data.settings.characters.items[0].equipped);
	assert(view.data.settings.themes.items[1].owned);
	assert(!view.data.settings.characters.items[2].owned);
	assert(view.data.settings.characters.items[2].price == 1400);
	assert(!view.data.settings.themes.items[3].owned);
	assert(view.data.settings.themes.items[3].price == 1200);
	assert(strcmp(view.data.settings.characters.items[1].name,
		"Halloween") == 0);
	assert(strcmp(view.data.settings.characters.items[1].portrait_asset,
		HALLOWEEN_PORTRAIT_PATH) == 0);
	assert(strcmp(view.data.settings.characters.items[1].abilities[0].name,
		"Fry") == 0);
	assert(strstr(view.data.settings.characters.items[3]
		.abilities[3].description, "cascade") != NULL);
	assert(view.data.settings.profile.wallet_points == 3200);
	assert(view.data.settings.profile.score == 125400);
	assert(view.data.settings.profile.rank == 7);
	app_settings_apply_local_controls(&view.data.settings, 40,
		TETRISU_RENDERER_CELL);
	assert(view.data.settings.music_volume == 40);
	assert(view.data.settings.renderer_mode == TETRISU_RENDERER_CELL);
	printf("PASS test_fixture_settings_model\n");
}

static void	test_offline_settings_are_account_free(void)
{
	app_data_provider_t	provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		true, &view) == APP_PROVIDER_OK);
	assert(!view.local_preview);
	assert(view.status == APP_DATA_READY);
	assert(view.data.settings.offline);
	assert(!view.data.settings.signed_in);
	assert(view.data.settings.profile.username[0] == '\0');
	assert(view.data.settings.profile.character[0] == '\0');
	assert(view.data.settings.profile.theme[0] == '\0');
	assert(view.data.settings.profile.portrait_asset[0] == '\0');
	assert(view.data.settings.profile.wallet_points == 0);
	assert(view.data.settings.profile.score == 0);
	assert(view.data.settings.profile.rank == 0);
	assert(view.data.settings.characters.count == 0);
	assert(view.data.settings.themes.count == 0);
	assert(strstr(view.data.settings.local_status, "NO ACCOUNT DATA") != NULL);
	printf("PASS test_offline_settings_are_account_free\n");
}

static void	test_settings_focus_and_actions(void)
{
	settings_state_t	state;
	app_navigation_t	navigation;

	settings_state_init(&state, true, 4, 7);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	assert(settings_handle_key(&state, NCKEY_ENTER) == SETTINGS_ACTION_BACK);
	/* Right walks the control strip and wraps back to Back. */
	assert(settings_handle_key(&state, NCKEY_RIGHT) == SETTINGS_ACTION_NONE);
	assert(state.focus == SETTINGS_FOCUS_MARKETPLACE);
	assert(settings_handle_key(&state, 'm') == SETTINGS_ACTION_MARKETPLACE);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_DOWN);
	assert(settings_handle_key(&state, NCKEY_ENTER)
		== SETTINGS_ACTION_VOLUME_DOWN);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_UP);
	assert(settings_handle_key(&state, '+') == SETTINGS_ACTION_VOLUME_UP);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	assert(settings_handle_key(&state, NCKEY_LEFT) == SETTINGS_ACTION_NONE);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_UP);

	settings_state_init(&state, false, 4, 6);
	/* Offline sessions have no inventory and must skip Marketplace. */
	assert(state.character_slots == 0 && state.theme_slots == 0);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_DOWN);
	assert(settings_handle_key(&state, 'm') == SETTINGS_ACTION_NONE);
	(void)settings_handle_key(&state, NCKEY_UP);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	settings_state_focus_next(&state);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	settings_state_focus_previous(&state);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_UP);
	app_navigation_init(&navigation, APP_SCREEN_HOME);
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_SETTINGS));
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_MARKETPLACE));
	assert(navigation.current == APP_SCREEN_MARKETPLACE);
	assert(app_navigation_dispatch(&navigation, APP_NAV_BACK));
	assert(navigation.current == APP_SCREEN_HOME);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	assert(app_navigation_dispatch(&navigation, APP_NAV_PLAY_OFFLINE));
	assert(app_navigation_dispatch(&navigation, APP_NAV_OPEN_SETTINGS));
	assert(!app_navigation_dispatch(&navigation, APP_NAV_OPEN_MARKETPLACE));
	printf("PASS test_settings_focus_and_actions\n");
}

static void	test_preview_gate_and_navigation(void)
{
	app_auth_view_model_t	view;
	app_data_provider_t	provider;
	auth_form_t		form;
	app_navigation_t	navigation;

	app_fixture_provider_init(&provider);
	(void)unsetenv("TETRISU_UI_PREVIEW");
	assert(!app_ui_preview_enabled());
	assert(app_provider_preview_sign_in(&provider, &view)
		== APP_PROVIDER_UNAVAILABLE);
	auth_form_init(&form, AUTH_FORM_LOGIN);
	form.focus = AUTH_FOCUS_SECONDARY;
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_OFFLINE);

	assert(setenv("TETRISU_UI_PREVIEW", "1", 1) == 0);
	assert(app_ui_preview_enabled());
	assert(app_provider_preview_sign_in(&provider, &view) == APP_PROVIDER_OK);
	assert(view.signed_in);
	assert(strcmp(view.username, "PreviewPlayer") == 0);
	auth_form_init(&form, AUTH_FORM_LOGIN);
	form.focus = AUTH_FOCUS_SECONDARY;
	auth_form_focus_next(&form);
	assert(form.focus == AUTH_FOCUS_PREVIEW);
	assert(auth_form_handle_key(&form, NCKEY_ENTER)
		== AUTH_ACTION_PREVIEW_LOGIN);
	form.focus = AUTH_FOCUS_PRIMARY;
	assert(auth_form_handle_key(&form, 'P') == AUTH_ACTION_PREVIEW_LOGIN);
	app_navigation_init(&navigation, APP_SCREEN_LOGIN);
	assert(app_navigation_dispatch(&navigation, APP_NAV_AUTHENTICATED));
	assert(navigation.current == APP_SCREEN_HOME);
	assert(!navigation.offline);
	(void)unsetenv("TETRISU_UI_PREVIEW");
	printf("PASS test_preview_gate_and_navigation\n");
}

static void	test_settings_layout_contract(void)
{
	settings_layout_t	layout;
	int				index;

	settings_layout_build(3, 5, 54, 144, 16, 8, &layout);
	assert(layout.pixel_width == 1152);
	assert(layout.pixel_height == 864);
	assert(layout.origin_y == 3 && layout.origin_x == 5);
	assert(layout.profile.x > 0);
	assert(layout.characters.x < layout.themes.x);
	assert(layout.stats[0].x < layout.stats[1].x);
	assert(layout.stats[1].x < layout.stats[2].x);
	assert(layout.portrait.width > 0 && layout.portrait.height > 0);
	/* The four control rectangles share one row and stay left-to-right. */
	index = 0;
	while (index < SETTINGS_BUTTON_COUNT)
	{
		assert(layout.buttons[index].width > 0);
		assert(layout.buttons[index].y == layout.buttons[0].y);
		if (index > 0)
			assert(layout.buttons[index - 1].x < layout.buttons[index].x);
		index++;
	}
	assert(layout.characters.width > 0 && layout.themes.width > 0);
	printf("PASS test_settings_layout_contract\n");
}

static void	test_character_selection(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	assert(settings_select_character(&view.data.settings, 1));
	assert(strcmp(view.data.settings.profile.character, "Halloween") == 0);
	assert(strcmp(view.data.settings.profile.portrait_asset,
		HALLOWEEN_PORTRAIT_PATH) == 0);
	assert(view.data.settings.characters.items[1].equipped);
	assert(settings_select_character(&view.data.settings, -1));
	assert(strcmp(view.data.settings.profile.character, "Mirurun") == 0);
	printf("PASS test_character_selection\n");
}

/* The fixture uses four columns: one full character row and a partial theme row. */
static void	test_inventory_grid_navigation(void)
{
	settings_state_t	state;
	int				index;

	settings_state_init(&state, true, 4, 7);
	assert(settings_slot_rows(4) == 1 && settings_slot_rows(7) == 2);
	/* Up from the control row lands in the characters panel. */
	(void)settings_handle_key(&state, NCKEY_UP);
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(state.character_slot == 0);
	(void)settings_handle_key(&state, NCKEY_UP);
	assert(state.character_slot == 0);
	/* Walk across the four-character row, then cross into themes. */
	index = 0;
	while (index < 3)
	{
		(void)settings_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(state.character_slot == 3);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(state.theme_slot == 0);
	/* Left from theme column zero returns to the rightmost character. */
	(void)settings_handle_key(&state, NCKEY_LEFT);
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(state.character_slot == 3);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(state.theme_slot == 0);
	/* Column three entering the partial second row clamps to its final slot. */
	index = 0;
	while (index < 3)
	{
		(void)settings_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.theme_slot == 3);
	(void)settings_handle_key(&state, NCKEY_DOWN);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(state.theme_slot == 6);
	/* Down from the partial final row exits to the control strip. */
	(void)settings_handle_key(&state, NCKEY_DOWN);
	assert(state.section == SETTINGS_SECTION_CONTROLS);
	/* Enter reports an equip only while an inventory panel holds focus. */
	(void)settings_handle_key(&state, NCKEY_UP);
	assert(settings_handle_key(&state, NCKEY_ENTER)
		== SETTINGS_ACTION_EQUIP_CHARACTER);
	index = 0;
	while (index < 4)
	{
		(void)settings_handle_key(&state, NCKEY_RIGHT);
		index++;
	}
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(settings_handle_key(&state, NCKEY_ENTER)
		== SETTINGS_ACTION_EQUIP_THEME);
	printf("PASS test_inventory_grid_navigation\n");
}

static void	test_slot_equipping(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	assert(settings_catalogue_count(&view.data.settings.characters,
			SETTINGS_CHARACTER_SLOTS) == 4);
	assert(settings_catalogue_count(&view.data.settings.themes,
			SETTINGS_THEME_SLOTS) == 7);
	assert(settings_owned_count(&view.data.settings.characters,
			SETTINGS_CHARACTER_SLOTS) == 2);
	assert(settings_owned_count(&view.data.settings.themes,
			SETTINGS_THEME_SLOTS) == 5);
	/* Re-equipping the current slot reports no change, so nothing repaints. */
	assert(settings_equip_character_slot(&view.data.settings, 0)
		== SETTINGS_EQUIP_UNCHANGED);
	assert(settings_equip_character_slot(&view.data.settings, 1)
		== SETTINGS_EQUIP_CHANGED);
	assert(strcmp(view.data.settings.profile.character, "Halloween") == 0);
	assert(view.data.settings.characters.items[1].equipped);
	assert(!view.data.settings.characters.items[0].equipped);
	assert(settings_equip_theme_slot(&view.data.settings, 5)
		== SETTINGS_EQUIP_CHANGED);
	assert(strcmp(view.data.settings.profile.theme, "Nuclear Ghandi") == 0);
	assert(view.data.settings.themes.items[5].equipped);
	assert(!view.data.settings.themes.items[0].equipped);
	/* Locked entries remain visible but never mutate equipped profile state. */
	assert(settings_equip_character_slot(&view.data.settings, 2)
		== SETTINGS_EQUIP_LOCKED);
	assert(strcmp(view.data.settings.profile.character, "Halloween") == 0);
	assert(settings_equip_theme_slot(&view.data.settings, 6)
		== SETTINGS_EQUIP_LOCKED);
	assert(strcmp(view.data.settings.profile.theme, "Nuclear Ghandi") == 0);
	/* Slots past what the panels draw are rejected rather than clamped. */
	assert(settings_equip_theme_slot(&view.data.settings, 99)
		== SETTINGS_EQUIP_INVALID);
	printf("PASS test_slot_equipping\n");
}

/*
 * The powers card follows the cursor through the characters panel and falls
 * back to the equipped character everywhere else.
 */
static void	test_powers_card_follows_focus(void)
{
	app_data_provider_t		provider;
	app_screen_view_model_t	view;
	settings_state_t		state;
	const app_catalogue_item_view_model_t	*card;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	settings_state_init(&state, true, 4, 7);
	/* Controls focused: hidden until I asks for it, and shows the equipped. */
	assert(!settings_card_visible(&state));
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Mirurun") == 0);
	(void)settings_handle_key(&state, 'i');
	assert(settings_card_visible(&state));
	(void)settings_handle_key(&state, 'i');
	/* Entering the characters panel shows it without pressing anything. */
	(void)settings_handle_key(&state, NCKEY_UP);
	assert(state.section == SETTINGS_SECTION_CHARACTERS);
	assert(settings_card_visible(&state));
	assert(state.character_slot == 0);
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Mirurun") == 0);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Halloween") == 0);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Princess") == 0);
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Wolf-man") == 0);
	/* The themes panel has no powers to show, so it falls back to equipped. */
	(void)settings_handle_key(&state, NCKEY_RIGHT);
	assert(state.section == SETTINGS_SECTION_THEMES);
	assert(!settings_card_visible(&state));
	card = settings_card_character(&view.data.settings, &state);
	assert(card != NULL && strcmp(card->name, "Mirurun") == 0);
	printf("PASS test_powers_card_follows_focus\n");
}

/**
 * @brief Held navigation repeats never absorb a different queued command.
 */
static void	test_settings_input_batch_boundaries(void)
{
	assert(settings_navigation_keys_coalesce(NCKEY_UP, NCKEY_UP));
	assert(settings_navigation_keys_coalesce(NCKEY_DOWN, NCKEY_DOWN));
	assert(settings_navigation_keys_coalesce(NCKEY_LEFT, NCKEY_LEFT));
	assert(settings_navigation_keys_coalesce(NCKEY_RIGHT, NCKEY_RIGHT));
	assert(settings_navigation_keys_coalesce(NCKEY_TAB, NCKEY_TAB));
	assert(!settings_navigation_keys_coalesce(NCKEY_UP, NCKEY_DOWN));
	assert(!settings_navigation_keys_coalesce(NCKEY_UP, NCKEY_ENTER));
	assert(!settings_navigation_keys_coalesce('+', '+'));
	assert(settings_action_leaves_screen(SETTINGS_ACTION_BACK));
	assert(settings_action_leaves_screen(SETTINGS_ACTION_MARKETPLACE));
	assert(settings_action_leaves_screen(SETTINGS_ACTION_QUIT));
	assert(!settings_action_leaves_screen(SETTINGS_ACTION_NONE));
	assert(!settings_action_leaves_screen(SETTINGS_ACTION_VOLUME_UP));
	assert(!settings_action_leaves_screen(SETTINGS_ACTION_EQUIP_CHARACTER));
	printf("PASS test_settings_input_batch_boundaries\n");
}
