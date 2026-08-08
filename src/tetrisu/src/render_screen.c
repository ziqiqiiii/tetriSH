#include "tetrisu.h"

// Static Functions
static bool	put_centered(struct ncplane *plane, int row, const char *text,
				bool bold);
static void	draw_border(struct ncplane *plane, int rows, int cols);
static void	screen_summary(const t_app_screen_view_model *view,
				char *summary, size_t size);
static const char	*screen_controls(t_app_screen screen);

/**
 * @brief Draws the shared native-terminal shell for scaffolded screens.
 *
 * Item 15 intentionally supplies navigation and typed data rather than final
 * screen artwork. Every fixture-backed screen is visibly labelled so preview
 * data cannot be mistaken for a live server response.
 */
bool	render_screen_show(t_render_ctx *ctx,
	const t_app_screen_view_model *view)
{
	ncplane_options	options;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;
	char			title[APP_TEXT_MAX + 8];
	char			status[APP_TEXT_MAX + 16];
	char			summary[APP_TEXT_MAX * 2];
	int				detail_row;

	if (ctx == NULL || ctx->std == NULL || view == NULL)
		return (false);
	render_menu_destroy(ctx);
	render_background_destroy(ctx);
	render_screen_destroy(ctx);
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < 6 || cols < 24)
		return (false);
	memset(&options, 0, sizeof(options));
	options.rows = (int)rows;
	options.cols = (int)cols;
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 250, 245, 250);
	(void)ncchannels_set_bg_rgb8(&channels, 12, 8, 24);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	draw_border(ctx->screen_plane, (int)rows, (int)cols);
	snprintf(title, sizeof(title), ":: %s ::", view->title);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 255, 112, 190);
	if (!put_centered(ctx->screen_plane, 1, title, true))
		return (false);
	if (view->local_preview)
	{
		(void)ncplane_set_fg_rgb8(ctx->screen_plane, 255, 203, 102);
		if (!put_centered(ctx->screen_plane, 2,
				":: LOCAL UI PREVIEW ::", true))
			return (false);
	}
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 220, 198, 235);
	(void)put_centered(ctx->screen_plane, 4, view->subtitle, false);
	screen_summary(view, summary, sizeof(summary));
	detail_row = (int)rows / 2;
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 250, 245, 250);
	(void)put_centered(ctx->screen_plane, detail_row, summary, true);
	snprintf(status, sizeof(status), "DATA: %s",
		app_data_status_name(view->status));
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 112, 214, 174);
	(void)put_centered(ctx->screen_plane, detail_row + 2, status, false);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 255, 203, 102);
	(void)put_centered(ctx->screen_plane, (int)rows - 3,
		screen_controls(view->screen), true);
	(void)ncplane_set_fg_rgb8(ctx->screen_plane, 180, 148, 205);
	(void)put_centered(ctx->screen_plane, (int)rows - 2, "Q QUIT", false);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Destroys the shared scaffold plane.
 */
void	render_screen_destroy(t_render_ctx *ctx)
{
	if (ctx != NULL && ctx->settings_portrait_plane != NULL)
	{
		ncplane_destroy(ctx->settings_portrait_plane);
		ctx->settings_portrait_plane = NULL;
	}
	if (ctx != NULL && ctx->settings_controls_plane != NULL)
	{
		ncplane_destroy(ctx->settings_controls_plane);
		ctx->settings_controls_plane = NULL;
	}
	if (ctx != NULL && ctx->settings_characters_plane != NULL)
	{
		ncplane_destroy(ctx->settings_characters_plane);
		ctx->settings_characters_plane = NULL;
	}
	if (ctx != NULL && ctx->settings_themes_plane != NULL)
	{
		ncplane_destroy(ctx->settings_themes_plane);
		ctx->settings_themes_plane = NULL;
	}
	if (ctx != NULL && ctx->settings_volume_plane != NULL)
	{
		ncplane_destroy(ctx->settings_volume_plane);
		ctx->settings_volume_plane = NULL;
	}
	if (ctx != NULL && ctx->settings_ability_plane != NULL)
	{
		ncplane_destroy(ctx->settings_ability_plane);
		ctx->settings_ability_plane = NULL;
	}
	if (ctx != NULL && ctx->screen_plane != NULL)
	{
		ncplane_destroy(ctx->screen_plane);
		ctx->screen_plane = NULL;
	}
}

/**
 * @brief Centers and clips one native-terminal line.
 */
static bool	put_centered(struct ncplane *plane, int row, const char *text,
	bool bold)
{
	char		clipped[APP_TEXT_MAX * 2 + 1];
	unsigned	rows;
	unsigned	cols;
	int			limit;
	int			x;
	int			result;

	ncplane_dim_yx(plane, &rows, &cols);
	if (row < 0 || row >= (int)rows || cols < 3)
		return (true);
	limit = (int)cols - 4;
	snprintf(clipped, sizeof(clipped), "%.*s", limit, text);
	x = ((int)cols - (int)strlen(clipped)) / 2;
	if (bold)
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	result = ncplane_putstr_yx(plane, row, x, clipped);
	if (bold)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	return (result >= 0);
}

/**
 * @brief Draws a stationary one-cell frame with no glow or bitmap dependency.
 */
static void	draw_border(struct ncplane *plane, int rows, int cols)
{
	int	x;
	int	y;

	(void)ncplane_set_fg_rgb8(plane, 112, 62, 145);
	x = 0;
	while (x < cols)
	{
		(void)ncplane_putchar_yx(plane, 0, x, x == 0 || x == cols - 1
			? '+' : '-');
		(void)ncplane_putchar_yx(plane, rows - 1, x,
			x == 0 || x == cols - 1 ? '+' : '-');
		x++;
	}
	y = 1;
	while (y < rows - 1)
	{
		(void)ncplane_putchar_yx(plane, y, 0, '|');
		(void)ncplane_putchar_yx(plane, y, cols - 1, '|');
		y++;
	}
}

/**
 * @brief Formats one useful line from the screen-specific typed model.
 */
static void	screen_summary(const t_app_screen_view_model *view,
	char *summary, size_t size)
{
	if (view->screen == APP_SCREEN_HOME
		|| view->screen == APP_SCREEN_SETTINGS)
		snprintf(summary, size, "%s | %" PRIu64 " SCORE | RANK %d",
			view->data.profile.username, view->data.profile.score,
			view->data.profile.rank);
	else if (view->screen == APP_SCREEN_MARKETPLACE
		&& view->data.catalogue.count > 0)
		snprintf(summary, size, "%d ITEMS | FEATURED: %s",
			view->data.catalogue.count, view->data.catalogue.items[0].name);
	else if (view->screen == APP_SCREEN_LEADERBOARD
		&& view->data.leaderboard.count > 0)
		snprintf(summary, size, "#1 %s | %" PRIu64,
			view->data.leaderboard.entries[0].username,
			view->data.leaderboard.entries[0].score);
	else if (view->screen == APP_SCREEN_LOBBY)
		snprintf(summary, size, "%d PREVIEW ROOMS",
			view->data.lobby.count);
	else if ((view->screen == APP_SCREEN_CREATE_ROOM_MODAL
			|| view->screen == APP_SCREEN_WAITING_ROOM)
		&& view->data.room.id[0] != '\0')
		snprintf(summary, size, "%s | %s | %d/%d PLAYERS",
			view->data.room.id, app_game_mode_name(view->data.room.mode),
			view->data.room.player_count, view->data.room.required_players);
	else if (view->screen == APP_SCREEN_DOUBLE
		|| view->screen == APP_SCREEN_BATTLE_ROYALE)
		snprintf(summary, size, "%s | %d PLAYERS | %s",
			app_game_mode_name(view->data.match.mode),
			view->data.match.player_count, view->data.match.status);
	else if (view->screen == APP_SCREEN_ENTRY)
		snprintf(summary, size, "LOGIN | SIGN UP | PLAY OFFLINE");
	else if (view->screen == APP_SCREEN_LOGIN
		|| view->screen == APP_SCREEN_SIGN_UP)
		snprintf(summary, size, "%s", view->data.auth.message);
	else
		snprintf(summary, size, "%s", view->subtitle);
}

/**
 * @brief Returns the temporary navigation hints for each scaffold.
 */
static const char	*screen_controls(t_app_screen screen)
{
	if (screen == APP_SCREEN_ENTRY)
		return ("L LOGIN  S SIGN UP  O PLAY OFFLINE");
	if (screen == APP_SCREEN_LOGIN || screen == APP_SCREEN_SIGN_UP)
		return ("ENTER CONTINUE  ESC BACK");
	if (screen == APP_SCREEN_LOBBY)
		return ("ENTER CREATE ROOM  ESC BACK");
	if (screen == APP_SCREEN_CREATE_ROOM_MODAL)
		return ("ENTER CREATE  ESC CANCEL");
	if (screen == APP_SCREEN_WAITING_ROOM)
		return ("D DOUBLE  B BATTLE ROYALE  ESC LEAVE");
	if (screen == APP_SCREEN_DOUBLE
		|| screen == APP_SCREEN_BATTLE_ROYALE)
		return ("ESC WAITING ROOM");
	return ("ESC BACK");
}
