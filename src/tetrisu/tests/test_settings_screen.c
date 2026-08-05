#include "tetrisu.h"

static void	test_fixture_settings_model(void);
static void	test_offline_settings_are_account_free(void);
static void	test_settings_focus_and_actions(void);
static void	test_preview_gate_and_navigation(void);
static void	test_settings_layout_contract(void);
static void	test_character_selection(void);

int	main(void)
{
	test_fixture_settings_model();
	test_offline_settings_are_account_free();
	test_settings_focus_and_actions();
	test_preview_gate_and_navigation();
	test_settings_layout_contract();
	test_character_selection();
	return (0);
}

static void	test_fixture_settings_model(void)
{
	app_data_provider_t	provider;
	app_screen_view_model_t	view;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	assert(view.local_preview);
	assert(view.status == APP_DATA_READY);
	assert(view.data.settings.signed_in);
	assert(!view.data.settings.offline);
	assert(strcmp(view.data.settings.profile.username, "PreviewPlayer") == 0);
	assert(strcmp(view.data.settings.profile.character, "Mirurun") == 0);
	assert(strcmp(view.data.settings.profile.theme, "Classic Temple") == 0);
	assert(strcmp(view.data.settings.profile.portrait_asset,
		DEFAULT_MIRURUN_PATH) == 0);
	assert(view.data.settings.characters.count == 4);
	assert(view.data.settings.themes.count == 6);
	assert(view.data.settings.characters.items[0].owned);
	assert(view.data.settings.characters.items[0].equipped);
	assert(view.data.settings.themes.items[1].owned);
	assert(strcmp(view.data.settings.themes.items[5].name,
		"Build a Snowman") == 0);
	assert(view.data.settings.themes.items[5].owned);
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

	settings_state_init(&state, true);
	assert(state.focus == SETTINGS_FOCUS_BACK);
	assert(settings_handle_key(&state, NCKEY_ENTER) == SETTINGS_ACTION_BACK);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_MARKETPLACE);
	assert(settings_handle_key(&state, 'm') == SETTINGS_ACTION_MARKETPLACE);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_DOWN);
	assert(settings_handle_key(&state, NCKEY_ENTER)
		== SETTINGS_ACTION_VOLUME_DOWN);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_UP);
	assert(settings_handle_key(&state, '+') == SETTINGS_ACTION_VOLUME_UP);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_CHARACTER_PREVIOUS);
	assert(settings_handle_key(&state, NCKEY_ENTER)
		== SETTINGS_ACTION_CHARACTER_PREVIOUS);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_CHARACTER_NEXT);
	assert(settings_handle_key(&state, ']')
		== SETTINGS_ACTION_CHARACTER_NEXT);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_BACK);

	settings_state_init(&state, false);
	settings_state_focus_next(&state);
	assert(state.focus == SETTINGS_FOCUS_VOLUME_DOWN);
	assert(settings_handle_key(&state, 'm') == SETTINGS_ACTION_NONE);
	/* Signed-out traversal must skip Marketplace in both directions. */
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
	assert(layout.character_arrows[0].x < layout.character_arrows[1].x);
	assert(layout.character_arrows[0].height > 0);
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
