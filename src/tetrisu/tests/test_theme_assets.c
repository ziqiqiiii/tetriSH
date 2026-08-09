#include "tetrisu.h"

static void	test_theme_path_contract(void);
static void	test_visual_selection_sync(void);
static void	test_theme_cycle_stress(void);
static void	assert_file(const char *path);

int	main(void)
{
	test_theme_path_contract();
	test_visual_selection_sync();
	test_theme_cycle_stress();
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
