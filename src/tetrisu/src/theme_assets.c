#include "tetrisu.h"

typedef struct s_theme_definition
{
	const char	*name;
	const char	*directory;
	bool		classic;
} t_theme_definition;

static const t_theme_definition	g_theme_definitions[] = {
	{"Classic", "default_theme", true},
	{"Design AI University", "design_ai_university_theme", false},
	{"Do You Wanna Build a Snowman", "do_you_want_a_snowman_theme", false},
	{"Haaland", "haaland_theme", false},
	{"Al Merqaedes F1 Team", "al_merqaedes_f1_team_theme", false},
	{"Nuclear Ghandi", "nuclear_ghandi_theme", false},
	{"Clauding", "clauding_theme", false}
};

static const t_theme_definition	*find_theme(const char *name);
static void					build_from(t_theme_assets *assets,
						const t_theme_definition *definition,
						const char *display_name);
static bool					name_is(const char *value, const char *first,
						const char *second);
static void					set_character_path(char *out, size_t size,
						const t_theme_definition *definition,
						const char *filename);
static bool					character_is(const char *value, const char *id,
						const char *name);
static void					invalidate_theme_caches(t_render_ctx *ctx);

/**
 * @brief Resolves one catalogue label to the directory-backed artwork set.
 *
 * Unknown labels deliberately fall back to Classic so a server-side catalogue
 * addition cannot make the client lose all of its visual defaults.
 */
void	tetrisu_theme_assets_build(t_theme_assets *assets, const char *theme_name)
{
	if (assets == NULL)
		return ;
	build_from(assets, find_theme(theme_name), NULL);
}

/**
 * @brief Resolves the artwork set for a theme by its catalogue id.
 *
 * The id is what tetrisd equips, and it is the only key that survives the trip:
 * matching on the display name meant carrying an alias for every spelling the
 * store used, and the one that was missing - "Nuclear Gandhi" against this
 * file's "Nuclear Ghandi" - silently fell through to Classic, so a player who
 * bought that theme got the default artwork and no error anywhere.
 *
 * @param assets Receives the resolved paths.
 * @param theme_id The catalogue id of the equipped theme.
 * @param display_name The store's name for it, used as the label; may be NULL.
 */
void	tetrisu_theme_assets_build_by_id(t_theme_assets *assets,
	uint32_t theme_id, const char *display_name)
{
	t_theme_definition	resolved;
	const char			*directory;

	if (assets == NULL)
		return ;
	directory = catalogue_theme_directory(theme_id);
	if (directory == NULL)
	{
		build_from(assets, find_theme(NULL), display_name);
		return ;
	}
	resolved.name = catalogue_theme_slug(theme_id);
	resolved.directory = directory;
	/* Theme 1 is the one that ships inside default_theme/, where the files
	** carry the default_ prefix and the music is the bundled Tetris theme. */
	resolved.classic = (theme_id == 1);
	build_from(assets, &resolved, display_name);
}

/**
 * @brief Fills one artwork set from a resolved theme definition.
 *
 * @param assets Receives the resolved paths.
 * @param definition The theme whose directory the paths are built from.
 * @param display_name Label to record, or NULL to use the definition's name.
 */
static void	build_from(t_theme_assets *assets,
	const t_theme_definition *definition, const char *display_name)
{
	memset(assets, 0, sizeof(*assets));
	if (display_name != NULL && display_name[0] != '\0')
		snprintf(assets->name, sizeof(assets->name), "%s", display_name);
	else
		snprintf(assets->name, sizeof(assets->name), "%s", definition->name);
	snprintf(assets->homepage, sizeof(assets->homepage), "%s/%s/%s",
		ASSET_DIR, definition->directory, "default_homepage.png");
	snprintf(assets->solo_background, sizeof(assets->solo_background),
		"%s/%s/%s", ASSET_DIR, definition->directory,
		"default_background_4x3.png");
	snprintf(assets->settings_background,
		sizeof(assets->settings_background), "%s/%s/%s", ASSET_DIR,
		definition->directory, "settings_profile_background_v2.png");
	snprintf(assets->marketplace_background,
		sizeof(assets->marketplace_background), "%s/%s/%s", ASSET_DIR,
		definition->directory, "marketplace_background.png");
	snprintf(assets->leaderboard_background,
		sizeof(assets->leaderboard_background), "%s/%s/%s", ASSET_DIR,
		definition->directory, "leaderboard_background.png");
	snprintf(assets->multiplayer_background,
		sizeof(assets->multiplayer_background), "%s/%s/%s", ASSET_DIR,
		definition->directory, "multiplayer_background.png");
	if (definition->classic)
	{
		snprintf(assets->music, sizeof(assets->music), "%s", HOME_BGM_PATH);
		snprintf(assets->danger_music, sizeof(assets->danger_music), "%s",
			DANGER_BGM_PATH);
		snprintf(assets->mirurun, sizeof(assets->mirurun), "%s/%s/%s",
			ASSET_DIR, definition->directory, "default_mirurun.png");
		snprintf(assets->halloween, sizeof(assets->halloween), "%s/%s/%s",
			ASSET_DIR, definition->directory, "character_halloween.png");
		snprintf(assets->princess, sizeof(assets->princess), "%s/%s/%s",
			ASSET_DIR, definition->directory, "character_princess.png");
		snprintf(assets->wolfman, sizeof(assets->wolfman), "%s/%s/%s",
			ASSET_DIR, definition->directory, "character_wolfman.png");
	}
	else
	{
		snprintf(assets->music, sizeof(assets->music), "%s/%s/%s",
			ASSET_DIR, definition->directory, "music.mp3");
		snprintf(assets->danger_music, sizeof(assets->danger_music), "%s/%s/%s",
			ASSET_DIR, definition->directory, "danger.mp3");
		set_character_path(assets->mirurun, sizeof(assets->mirurun),
			definition, "mirurun.png");
		set_character_path(assets->halloween, sizeof(assets->halloween),
			definition, "halloween.png");
		set_character_path(assets->princess, sizeof(assets->princess),
			definition, "princess.png");
		set_character_path(assets->wolfman, sizeof(assets->wolfman),
			definition, "wolfman.png");
	}
}

/**
 * @brief Returns the selected theme's portrait for one known character.
 */
const char	*tetrisu_theme_character_path(const t_theme_assets *assets,
	const char *character_name)
{
	if (assets == NULL || character_name == NULL)
		return (NULL);
	if (character_is(character_name, "mirurun", "Mirurun"))
		return (assets->mirurun);
	if (character_is(character_name, "halloween", "Halloween"))
		return (assets->halloween);
	if (character_is(character_name, "princess", "Princess"))
		return (assets->princess);
	if (character_is(character_name, "wolfman", "Wolf-man"))
		return (assets->wolfman);
	return (NULL);
}

/**
 * @brief Applies a theme and invalidates only caches that contain its artwork.
 */
void	tetrisu_theme_apply(t_render_ctx *ctx, const char *theme_name)
{
	t_theme_assets	assets;

	if (ctx == NULL)
		return ;
	tetrisu_theme_assets_build(&assets, theme_name);
	if (strcmp(ctx->theme_assets.name, assets.name) != 0)
	{
		ctx->theme_assets = assets;
		invalidate_theme_caches(ctx);
	}
	ctx->visual_selection_initialized = true;
}

/**
 * @brief Applies the theme tetrisd says is equipped, by its catalogue id.
 *
 * The caches are keyed on the label rather than the id because that is what
 * the renderers compare, but the artwork behind it is resolved from the id -
 * so a theme whose store name this file spells differently still gets its own
 * backdrops instead of quietly falling back to Classic.
 *
 * @param ctx Render context whose theme is being set.
 * @param theme_id The catalogue id of the equipped theme.
 * @param display_name The store's name for it; may be NULL.
 */
void	tetrisu_theme_apply_by_id(t_render_ctx *ctx, uint32_t theme_id,
	const char *display_name)
{
	t_theme_assets	assets;

	if (ctx == NULL)
		return ;
	tetrisu_theme_assets_build_by_id(&assets, theme_id, display_name);
	if (strcmp(ctx->theme_assets.name, assets.name) != 0)
	{
		ctx->theme_assets = assets;
		invalidate_theme_caches(ctx);
	}
	ctx->visual_selection_initialized = true;
}

/**
 * @brief Records the equipped character used by new visual surfaces.
 */
void	tetrisu_character_apply(t_render_ctx *ctx, const char *character_name)
{
	if (ctx == NULL || character_name == NULL || character_name[0] == '\0')
		return ;
	snprintf(ctx->active_character, sizeof(ctx->active_character), "%s",
		character_name);
	ctx->visual_selection_initialized = true;
}

/**
 * @brief Points the renderer and the music at whatever loadout is in force.
 *
 * Which way that resolves is the whole difference this makes. Against a
 * server, the model already carries the equipped ids tetrisd persisted, so
 * this only reads them and writes nothing back - the account decides what is
 * on screen. Against the stateless fixture provider there is nobody to ask,
 * so the older sync still carries the choice between screens.
 *
 * @param ctx Render context whose theme and character are being set.
 * @param provider Data provider; a NULL equip_item means fixtures.
 * @param settings The loaded Settings or Marketplace model.
 */
void	tetrisu_visual_selection_bind(t_render_ctx *ctx,
	const t_app_data_provider *provider, t_app_settings_view_model *settings)
{
	const t_app_catalogue_item_view_model	*equipped;

	if (ctx == NULL || settings == NULL || !settings->signed_in
		|| settings->offline)
		return ;
	if (provider == NULL || provider->equip_item == NULL)
	{
		tetrisu_visual_selection_sync(ctx, settings);
		return ;
	}
	equipped = app_catalogue_equipped(&settings->themes);
	if (equipped != NULL)
		tetrisu_theme_apply_by_id(ctx, equipped->item_id, equipped->name);
	equipped = app_catalogue_equipped(&settings->characters);
	if (equipped != NULL)
		tetrisu_character_apply(ctx, equipped->id);
	ctx->visual_selection_initialized = true;
}

/**
 * @brief Keeps loaded Settings/Marketplace models aligned with visual state.
 *
 * The fixture provider is intentionally stateless. This small bridge preserves
 * an equip choice while navigating between screens without giving the preview
 * build a server. It is the fixture path only - against tetrisd the loadout
 * comes back from the account, and writing the renderer's idea of it back into
 * the model would overwrite the server's answer with the client's.
 */
void	tetrisu_visual_selection_sync(t_render_ctx *ctx,
	t_app_settings_view_model *settings)
{
	const char	*path;
	int			index;

	if (ctx == NULL || settings == NULL || !settings->signed_in
		|| settings->offline)
		return ;
	if (!ctx->visual_selection_initialized)
	{
		tetrisu_theme_apply(ctx, settings->profile.theme);
		tetrisu_character_apply(ctx, settings->profile.character);
		if (ctx->theme_assets.name[0] == '\0')
			tetrisu_theme_apply(ctx, "Classic");
		if (ctx->active_character[0] == '\0')
			tetrisu_character_apply(ctx, "Mirurun");
	}
	snprintf(settings->profile.theme, sizeof(settings->profile.theme), "%s",
		ctx->theme_assets.name);
	snprintf(settings->profile.character,
		sizeof(settings->profile.character), "%s", ctx->active_character);
	path = tetrisu_theme_character_path(&ctx->theme_assets,
		ctx->active_character);
	if (path != NULL)
		snprintf(settings->profile.portrait_asset,
			sizeof(settings->profile.portrait_asset), "%s", path);
	index = 0;
	while (index < settings->characters.count
		&& index < APP_CATALOGUE_MAX_ITEMS)
	{
		path = tetrisu_theme_character_path(&ctx->theme_assets,
			settings->characters.items[index].id);
		if (path != NULL)
			snprintf(settings->characters.items[index].portrait_asset,
				sizeof(settings->characters.items[index].portrait_asset), "%s",
				path);
		settings->characters.items[index].equipped =
			strcmp(settings->characters.items[index].name,
				ctx->active_character) == 0;
		index++;
	}
	index = 0;
	while (index < settings->themes.count
		&& index < APP_CATALOGUE_MAX_ITEMS)
	{
		settings->themes.items[index].equipped =
			strcmp(settings->themes.items[index].name,
				ctx->theme_assets.name) == 0;
		index++;
	}
}

static const t_theme_definition	*find_theme(const char *name)
{
	static const t_theme_definition	classic = {
		"Classic", "default_theme", true
	};
	int							index;

	if (name == NULL || name[0] == '\0')
		return (&classic);
	index = 0;
	while (index < (int)(sizeof(g_theme_definitions)
		/ sizeof(g_theme_definitions[0])))
	{
		if (name_is(name, g_theme_definitions[index].name, NULL))
			return (&g_theme_definitions[index]);
		index++;
	}
	if (name_is(name, "Default", NULL))
		return (&classic);
	if (name_is(name, "Design and AI", NULL))
		return (&g_theme_definitions[1]);
	if (name_is(name, "Do u wanna build a snowman?", NULL))
		return (&g_theme_definitions[2]);
	if (name_is(name, "Al-Merqaedes", NULL))
		return (&g_theme_definitions[4]);
	if (name_is(name, "Claude-ing", NULL))
		return (&g_theme_definitions[6]);
	/* themes.cfg spells it "Gandhi"; this file's table spells it "Ghandi". */
	if (name_is(name, "Nuclear Gandhi", NULL))
		return (&g_theme_definitions[5]);
	return (&classic);
}

static bool	name_is(const char *value, const char *first, const char *second)
{
	return (value != NULL && first != NULL && strcmp(value, first) == 0)
		|| (value != NULL && second != NULL && strcmp(value, second) == 0);
}

static void	set_character_path(char *out, size_t size,
	const t_theme_definition *definition, const char *filename)
{
	snprintf(out, size, "%s/%s/%s", ASSET_DIR, definition->directory,
		filename);
}

static bool	character_is(const char *value, const char *id, const char *name)
{
	return (strcmp(value, id) == 0 || strcmp(value, name) == 0);
}

static void	invalidate_theme_caches(t_render_ctx *ctx)
{
	ctx->settings_background_ready = false;
	ctx->settings_static_signature = 0;
	ctx->settings_controls_signature = 0;
	ctx->settings_characters_signature = 0;
	ctx->settings_themes_signature = 0;
	ctx->settings_volume_signature = 0;
	ctx->settings_ability_signature = 0;
	ctx->marketplace_background_ready = false;
	ctx->marketplace_static_signature = 0;
	ctx->marketplace_stats_signature = 0;
	ctx->marketplace_characters_signature = 0;
	ctx->marketplace_themes_signature = 0;
	ctx->marketplace_detail_signature = 0;
	ctx->marketplace_controls_signature = 0;
	ctx->mp_background_ready = false;
	ctx->mp_background_source[0] = '\0';
	ctx->mp_static_signature = 0;
	ctx->mp_cards_signature = 0;
	ctx->mp_list_signature = 0;
	ctx->mp_field_signature = 0;
	ctx->mp_status_signature = 0;
	ctx->mp_options_signature = 0;
	ctx->mp_slots_signature = 0;
	ctx->mp_chat_signature = 0;
	ctx->leaderboard_static_signature = 0;
	ctx->leaderboard_controls_signature = 0;
}
