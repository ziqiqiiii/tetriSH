#include "tetrisu.h"

static void	test_theme_path_contract(void);
static void	test_visual_selection_sync(void);
static void	test_theme_cycle_stress(void);
static void	test_account_loadout_lands_before_the_home_screen(void);
static t_app_provider_result	stub_equip(void *userdata,
					t_app_catalogue_kind kind, uint32_t item_id,
					t_app_settings_view_model *settings);
static void	assert_file(const char *path);

int	main(void)
{
	test_theme_path_contract();
	test_visual_selection_sync();
	test_theme_cycle_stress();
	test_account_loadout_lands_before_the_home_screen();
	return (0);
}

static void	test_theme_path_contract(void)
{
	static const char	*names[] = {
		"Classic", "Design AI University", "Do You Wanna Build a Snowman",
		"Haaland", "Al Merqaedes F1 Team", "Nuclear Ghandi", "Clauding"
	};
	static const char	*directories[] = {
		"default_theme", "design_ai_university_theme",
		"do_you_want_a_snowman_theme", "haaland_theme",
		"al_merqaedes_f1_team_theme", "nuclear_ghandi_theme",
		"clauding_theme"
	};
	t_theme_assets	assets;
	const char		*path;
	int				index;

	index = 0;
	while (index < (int)(sizeof(names) / sizeof(names[0])))
	{
		tetrisu_theme_assets_build(&assets, names[index]);
		assert(strcmp(assets.name, names[index]) == 0);
		assert(strstr(assets.homepage, directories[index]) != NULL);
		assert(strstr(assets.solo_background, directories[index]) != NULL);
		assert(strstr(assets.settings_background, directories[index]) != NULL);
		assert(strstr(assets.marketplace_background, directories[index]) != NULL);
		assert(strstr(assets.leaderboard_background, directories[index]) != NULL);
		assert(strstr(assets.multiplayer_background, directories[index]) != NULL);
		if (index == 0)
		{
			assert(strcmp(assets.music, HOME_BGM_PATH) == 0);
			assert(strcmp(assets.danger_music, DANGER_BGM_PATH) == 0);
		}
		else
		{
			assert(strstr(assets.music, directories[index]) != NULL);
			assert(strstr(assets.danger_music, directories[index]) != NULL);
		}
		assert_file(assets.homepage);
		assert_file(assets.solo_background);
		assert_file(assets.settings_background);
		assert_file(assets.marketplace_background);
		assert_file(assets.leaderboard_background);
		assert_file(assets.multiplayer_background);
		assert_file(assets.music);
		assert_file(assets.danger_music);
		path = tetrisu_theme_character_path(&assets, "Mirurun");
		assert(path != NULL);
		assert_file(path);
		path = tetrisu_theme_character_path(&assets, "Halloween");
		assert(path != NULL);
		assert_file(path);
		index++;
	}
	tetrisu_theme_assets_build(&assets, "not-a-theme");
	assert(strcmp(assets.name, "Classic") == 0);
	printf("PASS test_theme_path_contract\n");
}

static void	test_visual_selection_sync(void)
{
	t_app_data_provider		provider;
	t_app_screen_view_model	view;
	t_render_ctx			ctx;
	const char				*path;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	memset(&ctx, 0, sizeof(ctx));
	tetrisu_visual_selection_sync(&ctx, &view.data.settings);
	assert(strcmp(ctx.theme_assets.name, "Classic") == 0);
	assert(strcmp(view.data.settings.profile.character, "Mirurun") == 0);
	assert(strcmp(view.data.settings.profile.portrait_asset,
		DEFAULT_MIRURUN_PATH) == 0);

	tetrisu_theme_apply(&ctx, "Nuclear Ghandi");
	tetrisu_character_apply(&ctx, "Halloween");
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
		false, &view) == APP_PROVIDER_OK);
	tetrisu_visual_selection_sync(&ctx, &view.data.settings);
	assert(strcmp(view.data.settings.profile.theme, "Nuclear Ghandi") == 0);
	assert(strcmp(view.data.settings.profile.character, "Halloween") == 0);
	path = view.data.settings.profile.portrait_asset;
	assert(strstr(path, "nuclear_ghandi_theme/halloween.png") != NULL);
	assert(strcmp(view.data.settings.characters.items[1].portrait_asset, path)
		== 0);
	assert(view.data.settings.characters.items[1].equipped);
	assert(view.data.settings.themes.items[5].equipped);
	printf("PASS test_visual_selection_sync\n");
}

/*
** What signing in has to leave behind. The renderer is born wearing Classic,
** and the account's own loadout only ever arrived through Settings or the
** Marketplace - so a returning player landed on home in the default theme and
** had to re-equip a theme they already owned to get it back.
**
** This is the mechanism that fixes it, asserted at the seam main.c now calls:
** an account-backed Settings model (equip_item present, so the loadout is the
** server's rather than the fixture's) has to move theme_assets.homepage off
** Classic on its own, before anything draws.
*/
static void	test_account_loadout_lands_before_the_home_screen(void)
{
	t_app_data_provider		provider;
	t_app_screen_view_model	view;
	t_render_ctx			ctx;
	int						index;

	app_fixture_provider_init(&provider);
	assert(app_screen_view_load_for_session(&provider, APP_SCREEN_SETTINGS,
			false, &view) == APP_PROVIDER_OK);
	/*
	 * An equip_item is what tells the bind that the loadout belongs to an
	 * account rather than to the stateless fixture, so the stub is the whole
	 * difference between the two paths and is never called.
	 */
	provider.equip_item = stub_equip;
	/*
	 * The fixture equips nothing, so the account this stands in for is built
	 * here: one owned theme, equipped, and it is not the one render_init left
	 * standing.
	 */
	index = 0;
	while (index < view.data.settings.themes.count)
	{
		view.data.settings.themes.items[index].equipped = false;
		index++;
	}
	assert(view.data.settings.themes.count > 1);
	index = 1;
	view.data.settings.themes.items[index].owned = true;
	view.data.settings.themes.items[index].equipped = true;
	/*
	 * The id is what the equipped theme travels as - tetrisd's label for the
	 * item, and the only key tetrisu_theme_apply_by_id resolves artwork from.
	 * 8 is Nuclear Ghandi in catalogue_art.c, whose store spelling this client
	 * does not share, so it is also the id that proves the resolution is by
	 * number and not by name.
	 */
	view.data.settings.themes.items[index].item_id = 8U;
	memset(&ctx, 0, sizeof(ctx));
	tetrisu_theme_apply(&ctx, "Classic");
	tetrisu_character_apply(&ctx, "Mirurun");
	ctx.visual_selection_initialized = false;
	assert(strstr(ctx.theme_assets.homepage, "default_theme") != NULL);
	tetrisu_visual_selection_bind(&ctx, &provider, &view.data.settings);
	assert(strcmp(ctx.theme_assets.name,
			view.data.settings.themes.items[index].name) == 0);
	assert(strstr(ctx.theme_assets.homepage, "nuclear_ghandi_theme") != NULL);
	assert(strstr(ctx.theme_assets.homepage, "/default_theme/") == NULL);
	assert_file(ctx.theme_assets.homepage);
	assert_file(ctx.theme_assets.music);
	printf("PASS test_account_loadout_lands_before_the_home_screen\n");
}

/**
 * @brief Stands in for an account-backed equip; the bind only tests for it.
 */
static t_app_provider_result	stub_equip(void *userdata,
		t_app_catalogue_kind kind, uint32_t item_id,
		t_app_settings_view_model *settings)
{
	(void)userdata;
	(void)kind;
	(void)item_id;
	(void)settings;
	return (APP_PROVIDER_UNAVAILABLE);
}

static void	test_theme_cycle_stress(void)
{
	static const char	*names[] = {
		"Classic", "Design AI University", "Do You Wanna Build a Snowman",
		"Haaland", "Al Merqaedes F1 Team", "Nuclear Ghandi", "Clauding"
	};
	t_app_data_provider		provider;
	t_app_screen_view_model	view;
	t_render_ctx			ctx;
	int					cycle;
	int					index;

	app_fixture_provider_init(&provider);
	memset(&ctx, 0, sizeof(ctx));
	cycle = 0;
	while (cycle < 10)
	{
		index = 0;
		while (index < (int)(sizeof(names) / sizeof(names[0])))
		{
			tetrisu_theme_apply(&ctx, names[index]);
			assert(app_screen_view_load_for_session(&provider,
				APP_SCREEN_SETTINGS, false, &view) == APP_PROVIDER_OK);
			tetrisu_visual_selection_sync(&ctx, &view.data.settings);
			assert(strcmp(ctx.theme_assets.name, names[index]) == 0);
			assert(strcmp(view.data.settings.profile.theme, names[index]) == 0);
			assert_file(ctx.theme_assets.homepage);
			assert_file(ctx.theme_assets.settings_background);
			assert_file(ctx.theme_assets.music);
			assert_file(ctx.theme_assets.danger_music);
			if (strcmp(names[index], "Nuclear Ghandi") == 0)
			{
				assert(strstr(ctx.theme_assets.homepage,
					"nuclear_ghandi_theme/default_homepage.png") != NULL);
				assert(strstr(ctx.theme_assets.solo_background,
					"nuclear_ghandi_theme/default_background_4x3.png")
					!= NULL);
				assert(strstr(ctx.theme_assets.settings_background,
					"nuclear_ghandi_theme/settings_profile_background_v2.png")
					!= NULL);
				assert(strstr(ctx.theme_assets.music,
					"nuclear_ghandi_theme/music.mp3") != NULL);
				assert(strstr(ctx.theme_assets.danger_music,
					"nuclear_ghandi_theme/danger.mp3") != NULL);
			}
			index++;
		}
		cycle++;
	}
	printf("PASS test_theme_cycle_stress\n");
}

static void	assert_file(const char *path)
{
	FILE	*file;

	file = fopen(path, "rb");
	assert(file != NULL);
	assert(fclose(file) == 0);
}
