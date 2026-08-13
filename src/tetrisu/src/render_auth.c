#include "tetrisu.h"

/*
 * Below this the form does not fit at all. Every other screen answers that
 * with a resize notice; this one used to answer it by failing, and
 * run_auth_flow turns a failed draw into APP_NAV_QUIT - so a terminal a few
 * columns too narrow ended the application the instant the intro finished,
 * with the terminal restored and nothing printed. Indistinguishable from a
 * crash, and the sign-in screen is the first thing drawn after the intro.
 */
# define AUTH_MIN_ROWS	12
# define AUTH_MIN_COLS	38

typedef struct s_auth_layout
{
	int	field_rows[4];
	int	field_x;
	int	field_width;
	int	title_row;
	int	primary_row;
	int	secondary_row;
	int	status_row;
	int	left_button_x;
	int	preview_button_x;
	int	right_button_x;
	int	button_width;
	int	footer_row;
	bool	art;
}	t_auth_layout;

static bool			auth_art_available(const t_render_ctx *ctx);
static bool			show_too_small(t_render_ctx *ctx);
static t_auth_layout	auth_layout(const t_render_ctx *ctx,
					t_auth_form_mode mode);
static void			set_color(struct ncplane *plane, int red, int green,
						int blue);
static void			fill_line(struct ncplane *plane, int row, int x, int width,
						bool focused);
static void			put_centered(struct ncplane *plane, int row,
						const char *text, bool bold);
static void			put_box_text(struct ncplane *plane, int row, int x,
						int width, const char *text, bool focused,
						bool disabled);
static void			put_left_box_text(struct ncplane *plane, int row, int x,
						int width, const char *text, bool focused,
						bool placeholder);
static void			clip_utf8_bytes(const char *text, size_t limit,
						char *output, size_t capacity);
static void			draw_field(struct ncplane *plane, const t_auth_form *form,
						t_auth_focus focus, int row, int x, int width,
						const char *label, const char *value,
						const char *placeholder, bool password);
static void			draw_status(struct ncplane *plane, const t_auth_form *form,
						const t_auth_layout *layout);
static void			draw_native_frame(struct ncplane *plane, int rows,
						int cols);
static void			draw_form(struct ncplane *plane, const t_auth_form *form,
						const t_auth_layout *layout);
static bool			point_in_row(const ncinput *input, int row, int x,
						int width);

/**
 * @brief Draws a responsive native-text form over the character-free artwork.
 */
bool	render_auth_show(t_render_ctx *ctx, const t_auth_form *form,
	bool rebuild_background)
{
	ncplane_options	options;
	t_auth_layout	layout;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;
	unsigned		plane_rows;
	unsigned		plane_cols;
	bool			pixel_background;

	if (ctx == NULL || ctx->std == NULL || form == NULL)
		return (false);
	if (rebuild_background)
	{
		render_menu_destroy(ctx);
		render_screen_destroy(ctx);
		render_auth_pixel_overlay_destroy(ctx);
		render_auth_pixel_background_reset(ctx);
		if (!auth_art_available(ctx))
			render_background_destroy(ctx);
	}
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < AUTH_MIN_ROWS || cols < AUTH_MIN_COLS)
		return (show_too_small(ctx));
	pixel_background = false;
	if (auth_art_available(ctx))
		pixel_background = render_auth_pixel_background_refresh(ctx, form,
				rebuild_background);
	if (pixel_background
		&& !render_auth_pixel_overlay_refresh(ctx, form))
	{
		render_auth_pixel_overlay_destroy(ctx);
		render_auth_pixel_background_reset(ctx);
		pixel_background = false;
	}
	if (auth_art_available(ctx) && !pixel_background)
	{
		if (render_background_replace(ctx, AUTH_BACKGROUND_PATH, false) < 0)
			return (false);
	}
	if (pixel_background)
	{
		render_screen_destroy(ctx);
		render_compatibility_badge_hide(ctx);
		render_notification_raise(ctx);
		return (notcurses_render(ctx->nc) == 0);
	}
	if (ctx->screen_plane != NULL)
	{
		ncplane_dim_yx(ctx->screen_plane, &plane_rows, &plane_cols);
		if (plane_rows != rows || plane_cols != cols)
			render_screen_destroy(ctx);
	}
	if (ctx->screen_plane == NULL)
	{
		memset(&options, 0, sizeof(options));
		options.rows = (int)rows;
		options.cols = (int)cols;
		ctx->screen_plane = ncplane_create(ctx->std, &options);
	}
	if (ctx->screen_plane == NULL)
		return (false);
	layout = auth_layout(ctx, form->mode);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 255, 236, 249);
	if (layout.art)
		(void)ncchannels_set_bg_alpha(&channels, NCALPHA_TRANSPARENT);
	else
		(void)ncchannels_set_bg_rgb8(&channels, 8, 8, 31);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	if (!layout.art)
		draw_native_frame(ctx->screen_plane, (int)rows, (int)cols);
	draw_form(ctx->screen_plane, form, &layout);
	ncplane_move_top(ctx->screen_plane);
	if (rebuild_background || (render_compatibility_mode(ctx)
			&& ctx->compatibility_plane == NULL))
		render_compatibility_badge_refresh(ctx);
	else if (ctx->compatibility_plane != NULL)
		ncplane_move_top(ctx->compatibility_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Maps a pointer press to the same focus order used by the keyboard.
 */
bool	render_auth_hit_test(const t_render_ctx *ctx, const t_auth_form *form,
	const ncinput *input, t_auth_focus *focus)
{
	t_auth_layout	layout;

	if (ctx == NULL || form == NULL || input == NULL || focus == NULL)
		return (false);
	layout = auth_layout(ctx, form->mode);
	if (point_in_row(input, layout.field_rows[0], layout.field_x,
			layout.field_width))
		*focus = AUTH_FOCUS_USERNAME;
	else if (point_in_row(input, layout.field_rows[1], layout.field_x,
			layout.field_width))
		*focus = AUTH_FOCUS_PASSWORD;
	else if (form->mode == AUTH_FORM_SIGN_UP
		&& point_in_row(input, layout.field_rows[2], layout.field_x,
			layout.field_width))
		*focus = AUTH_FOCUS_CONFIRM;
	else if (point_in_row(input,
			layout.field_rows[form->mode == AUTH_FORM_SIGN_UP ? 3 : 2],
			layout.field_x, layout.field_width))
		*focus = AUTH_FOCUS_DOMAIN;
	else if (point_in_row(input, layout.primary_row, layout.field_x,
			layout.field_width))
		*focus = AUTH_FOCUS_PRIMARY;
	else if (point_in_row(input, layout.secondary_row, layout.left_button_x,
			layout.button_width))
		*focus = AUTH_FOCUS_SECONDARY;
	else if (app_ui_preview_enabled() && form->mode == AUTH_FORM_LOGIN
		&& point_in_row(input, layout.secondary_row,
			layout.preview_button_x, layout.button_width))
		*focus = AUTH_FOCUS_PREVIEW;
	else if (point_in_row(input, layout.secondary_row, layout.right_button_x,
			layout.button_width))
		*focus = AUTH_FOCUS_OFFLINE;
	else
		return (false);
	return (true);
}

/**
 * @brief Removes the auth screen, artwork included.
 *
 * The backdrop goes with it, and that is the whole point rather than tidiness.
 * On a bitmap terminal the auth artwork is a sprixel - render_auth_font.c blits
 * it with NCBLIT_PIXEL - while every screen that follows draws its backdrop
 * through preferred_blitter(), which is deliberately a cell blitter. A cell
 * plane cannot occlude a sprixel however high it sits (render_background.c),
 * so leaving the artwork for the next screen to "replace" only worked where
 * replacing it happened to destroy it.
 *
 * It did not always. render_background_replace() parks an outgoing plane
 * instead of destroying it whenever the backdrop cache is holding it, and
 * parking is a move - which erases the terminal-side image only on a terminal
 * that genuinely implements sprixel movement. Where it does not, the sign-in
 * artwork stayed on the glass and the home screen's cell backdrop was drawn
 * underneath it: signing in appeared to change nothing, and so did every
 * later visit to home.
 *
 * So the artwork is destroyed here, at the one site that knows the screen is
 * being left. render_background_destroy() empties the retained cache with it,
 * so nothing can restack the sign-in art later either. The next screen pays
 * for one backdrop it was already going to build - reflow_home() replaces the
 * background on this path regardless.
 */
void	render_auth_destroy(t_render_ctx *ctx)
{
	render_auth_pixel_overlay_destroy(ctx);
	render_screen_destroy(ctx);
	render_auth_pixel_background_reset(ctx);
	render_background_destroy(ctx);
	render_compatibility_badge_hide(ctx);
}

static bool	auth_art_available(const t_render_ctx *ctx)
{
	unsigned	rows;
	unsigned	cols;

	if (!render_pixels_available(ctx) || ctx->std == NULL)
		return (false);
	ncplane_dim_yx(ctx->std, &rows, &cols);
	return (rows >= 24 && cols >= 64);
}

/**
 * @brief Keeps sign-in alive below its minimum size, showing a resize notice.
 *
 * The artwork and the form are both dropped: what stays is one line naming
 * the size the screen needs, on the same plane the form would have used, so
 * growing the terminal draws the form again on the next resize event.
 */
static bool	show_too_small(t_render_ctx *ctx)
{
	ncplane_options	options;
	uint64_t		channels;
	char			notice[64];
	unsigned		rows;
	unsigned		cols;

	render_auth_pixel_overlay_destroy(ctx);
	render_auth_pixel_background_reset(ctx);
	render_screen_destroy(ctx);
	render_background_destroy(ctx);
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows == 0 || cols == 0)
		return (false);
	memset(&options, 0, sizeof(options));
	options.rows = (int)rows;
	options.cols = (int)cols;
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 255, 236, 249);
	(void)ncchannels_set_bg_rgb8(&channels, 8, 8, 31);
	(void)ncplane_set_base(ctx->screen_plane, " ", 0, channels);
	ncplane_erase(ctx->screen_plane);
	snprintf(notice, sizeof(notice), "SIGN IN NEEDS %dx%d",
		AUTH_MIN_COLS, AUTH_MIN_ROWS);
	set_color(ctx->screen_plane, 255, 206, 104);
	put_centered(ctx->screen_plane, 0, notice, true);
	set_color(ctx->screen_plane, 190, 156, 230);
	put_centered(ctx->screen_plane, 1, "RESIZE THE TERMINAL", false);
	ncplane_move_top(ctx->screen_plane);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

static t_auth_layout	auth_layout(const t_render_ctx *ctx,
	t_auth_form_mode mode)
{
	t_auth_layout	layout;
	unsigned		rows;
	unsigned		cols;
	int				center;

	memset(&layout, 0, sizeof(layout));
	ncplane_dim_yx(ctx->std, &rows, &cols);
	layout.art = auth_art_available(ctx);
	if (layout.art)
	{
		layout.title_row = ctx->bg_row + (ctx->bg_rows * 13) / 100;
		layout.field_rows[0] = ctx->bg_row + (ctx->bg_rows * 21) / 100;
		layout.field_rows[1] = ctx->bg_row + (ctx->bg_rows * 31) / 100;
		layout.field_rows[2] = ctx->bg_row + (ctx->bg_rows * 42) / 100;
		layout.field_rows[3] = ctx->bg_row + (ctx->bg_rows * 52) / 100;
		layout.primary_row = ctx->bg_row + (ctx->bg_rows * 65) / 100;
		layout.status_row = ctx->bg_row + (ctx->bg_rows * 73) / 100;
		layout.secondary_row = ctx->bg_row + (ctx->bg_rows * 82) / 100;
		layout.footer_row = ctx->bg_row + (ctx->bg_rows * 94) / 100;
		layout.field_width = (ctx->bg_cols * 48) / 100;
		layout.button_width = (ctx->bg_cols * 29) / 100;
		center = ctx->bg_col + ctx->bg_cols / 2;
	}
	else
	{
		if (rows >= 22)
		{
			layout.title_row = ((int)rows - 20) / 2;
			layout.field_rows[0] = layout.title_row + 3;
			layout.field_rows[1] = layout.title_row + 6;
			layout.field_rows[2] = layout.title_row + 9;
			layout.field_rows[3] = layout.title_row + 12;
			layout.primary_row = layout.title_row + 15;
			layout.status_row = layout.title_row + 16;
			layout.secondary_row = layout.title_row + 18;
			layout.footer_row = layout.title_row + 19;
		}
		else
		{
			layout.title_row = 1;
			layout.field_rows[0] = 3;
			layout.field_rows[1] = 4;
			layout.field_rows[2] = 5;
			layout.field_rows[3] = 6;
			layout.primary_row = (int)rows - 5;
			layout.status_row = (int)rows - 4;
			layout.secondary_row = (int)rows - 3;
			layout.footer_row = (int)rows - 2;
		}
		layout.field_width = (int)cols - 12;
		if (layout.field_width > 72)
			layout.field_width = 72;
		layout.button_width = (layout.field_width - 4) / 2;
		center = (int)cols / 2;
	}
	if (layout.field_width < 30)
		layout.field_width = 30;
	layout.field_x = center - layout.field_width / 2;
	if (app_ui_preview_enabled() && mode == AUTH_FORM_LOGIN)
	{
		layout.button_width = (layout.field_width - 6) / 3;
		layout.left_button_x = layout.field_x;
		layout.preview_button_x = layout.left_button_x
			+ layout.button_width + 2;
		layout.right_button_x = layout.preview_button_x
			+ layout.button_width + 2;
	}
	else
	{
		layout.left_button_x = center - layout.button_width - 1;
		layout.right_button_x = center + 1;
	}
	return (layout);
}

static void	set_color(struct ncplane *plane, int red, int green, int blue)
{
	(void)ncplane_set_fg_rgb8(plane, red, green, blue);
}

static void	fill_line(struct ncplane *plane, int row, int x, int width,
	bool focused)
{
	int	index;

	if (focused)
		(void)ncplane_set_bg_rgb8(plane, 42, 20, 67);
	else
		(void)ncplane_set_bg_rgb8(plane, 10, 12, 43);
	index = 0;
	while (index < width)
	{
		(void)ncplane_putchar_yx(plane, row, x + index, ' ');
		index++;
	}
}

static void	put_centered(struct ncplane *plane, int row, const char *text,
	bool bold)
{
	unsigned	rows;
	unsigned	cols;

	ncplane_dim_yx(plane, &rows, &cols);
	if (row < 0 || row >= (int)rows)
		return ;
	if (bold)
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_aligned(plane, row, NCALIGN_CENTER, text);
	if (bold)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	put_box_text(struct ncplane *plane, int row, int x, int width,
	const char *text, bool focused, bool disabled)
{
	char	clipped[AUTH_FIELD_MAX + 40];
	int		text_width;
	int		offset;

	fill_line(plane, row, x, width, focused);
	clip_utf8_bytes(text, (size_t)(width - 4), clipped, sizeof(clipped));
	text_width = ncstrwidth(clipped, NULL, NULL);
	if (text_width < 0)
		text_width = (int)strlen(clipped);
	offset = x + (width - text_width) / 2;
	if (offset < x + 1)
		offset = x + 1;
	if (disabled)
		set_color(plane, 116, 111, 132);
	else if (focused)
	{
		set_color(plane, 255, 206, 104);
		(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	}
	else
		set_color(plane, 255, 225, 246);
	(void)ncplane_putstr_yx(plane, row, offset, clipped);
	if (focused && !disabled)
		(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	put_left_box_text(struct ncplane *plane, int row, int x,
	int width, const char *text, bool focused, bool placeholder)
{
	char	clipped[AUTH_FIELD_MAX + 40];

	fill_line(plane, row, x, width, focused);
	clip_utf8_bytes(text, (size_t)(width - 4), clipped, sizeof(clipped));
	if (placeholder)
		set_color(plane, 128, 124, 151);
	else if (focused)
		set_color(plane, 255, 219, 238);
	else
		set_color(plane, 238, 223, 242);
	(void)ncplane_putstr_yx(plane, row, x + 2, clipped);
}

static void	clip_utf8_bytes(const char *text, size_t limit, char *output,
	size_t capacity)
{
	size_t	source;
	size_t	destination;
	size_t	sequence;

	source = 0;
	destination = 0;
	while (text[source] != '\0' && destination < limit
		&& destination + 1 < capacity)
	{
		if (((unsigned char)text[source] & 0x80u) == 0)
			sequence = 1;
		else if (((unsigned char)text[source] & 0xe0u) == 0xc0u)
			sequence = 2;
		else if (((unsigned char)text[source] & 0xf0u) == 0xe0u)
			sequence = 3;
		else
			sequence = 4;
		if (destination + sequence > limit
			|| destination + sequence >= capacity)
			break ;
		memcpy(output + destination, text + source, sequence);
		source += sequence;
		destination += sequence;
	}
	output[destination] = '\0';
}

static void	draw_field(struct ncplane *plane, const t_auth_form *form,
	t_auth_focus focus, int row, int x, int width, const char *label,
	const char *value, const char *placeholder, bool password)
{
	char	display[AUTH_FIELD_MAX];
	char	line[AUTH_FIELD_MAX + 40];
	bool	active;
	bool	show_placeholder;

	active = form->focus == focus;
	if (password)
		(void)auth_form_mask_password(value, display, sizeof(display));
	else
		snprintf(display, sizeof(display), "%s", value);
	show_placeholder = display[0] == '\0' && !active;
	if (show_placeholder)
		snprintf(display, sizeof(display), "%s", placeholder);
	snprintf(line, sizeof(line), "%c %-15s : %s%s",
		active ? '>' : ' ', label, display, active ? "_" : "");
	put_left_box_text(plane, row, x, width, line, active,
		show_placeholder);
}

static void	draw_status(struct ncplane *plane, const t_auth_form *form,
	const t_auth_layout *layout)
{
	if (form->feedback == AUTH_FEEDBACK_ERROR)
		set_color(plane, 255, 111, 142);
	else if (form->feedback == AUTH_FEEDBACK_SUCCESS)
		set_color(plane, 112, 224, 174);
	else if (form->feedback == AUTH_FEEDBACK_LOADING)
		set_color(plane, 255, 206, 104);
	else
		set_color(plane, 190, 156, 230);
	put_centered(plane, layout->status_row, form->status,
		form->feedback != AUTH_FEEDBACK_IDLE);
}

static void	draw_native_frame(struct ncplane *plane, int rows, int cols)
{
	int	x;
	int	y;

	set_color(plane, 255, 104, 184);
	x = 1;
	while (x < cols - 1)
	{
		(void)ncplane_putchar_yx(plane, 1, x, '-');
		(void)ncplane_putchar_yx(plane, rows - 1, x, '-');
		x++;
	}
	y = 1;
	while (y < rows)
	{
		(void)ncplane_putchar_yx(plane, y, 1, '|');
		(void)ncplane_putchar_yx(plane, y, cols - 2, '|');
		y++;
	}
	set_color(plane, 255, 206, 104);
	(void)ncplane_putstr_yx(plane, 2, 3, "* +");
	(void)ncplane_putstr_yx(plane, 2, cols - 7, "+ *");
}

static void	draw_form(struct ncplane *plane, const t_auth_form *form,
	const t_auth_layout *layout)
{
	const char	*primary;
	const char	*secondary;
	char		title[48];
	char		primary_text[48];
	char		secondary_text[48];
	char		preview_text[48];
	char		offline_text[48];

	snprintf(title, sizeof(title), ":: %s ::",
		form->mode == AUTH_FORM_SIGN_UP
		? "CREATE ACCOUNT" : "WELCOME TO TETRISU");
	set_color(plane, 255, 206, 104);
	put_centered(plane, layout->title_row, title, true);
	draw_field(plane, form, AUTH_FOCUS_USERNAME, layout->field_rows[0],
		layout->field_x, layout->field_width, "USERNAME", form->username,
		"your name", false);
	draw_field(plane, form, AUTH_FOCUS_PASSWORD, layout->field_rows[1],
		layout->field_x, layout->field_width, "PASSWORD", form->password,
		"at least 4 characters", true);
	if (form->mode == AUTH_FORM_SIGN_UP)
	{
		draw_field(plane, form, AUTH_FOCUS_CONFIRM, layout->field_rows[2],
			layout->field_x, layout->field_width, "RE-ENTER PASSWORD",
			form->confirm, "type it again", true);
		draw_field(plane, form, AUTH_FOCUS_DOMAIN, layout->field_rows[3],
			layout->field_x, layout->field_width, "DOMAIN / SERVER",
			form->domain, "example.com", false);
	}
	else
	{
		t_auth_layout	status_layout;

		draw_field(plane, form, AUTH_FOCUS_DOMAIN, layout->field_rows[2],
			layout->field_x, layout->field_width, "DOMAIN / SERVER",
			form->domain, "example.com", false);
		fill_line(plane, layout->field_rows[3], layout->field_x,
			layout->field_width, false);
		status_layout = *layout;
		status_layout.status_row = layout->field_rows[3];
		draw_status(plane, form, &status_layout);
	}
	primary = form->mode == AUTH_FORM_SIGN_UP ? "SIGN UP" : "LOGIN";
	secondary = form->mode == AUTH_FORM_SIGN_UP
		? "BACK TO LOGIN" : "SIGN UP";
	snprintf(primary_text, sizeof(primary_text), "%s%s%s",
		form->focus == AUTH_FOCUS_PRIMARY ? "> " : "",
		primary, form->focus == AUTH_FOCUS_PRIMARY ? " <" : "");
	snprintf(secondary_text, sizeof(secondary_text), "%s%s%s",
		form->focus == AUTH_FOCUS_SECONDARY ? "> " : "",
		secondary, form->focus == AUTH_FOCUS_SECONDARY ? " <" : "");
	snprintf(offline_text, sizeof(offline_text), "%sPLAY OFFLINE%s",
		form->focus == AUTH_FOCUS_OFFLINE ? "> " : "",
		form->focus == AUTH_FOCUS_OFFLINE ? " <" : "");
	put_box_text(plane, layout->primary_row, layout->field_x,
		layout->field_width, primary_text,
		form->focus == AUTH_FOCUS_PRIMARY,
		!auth_form_online_enabled(form));
	if (form->mode == AUTH_FORM_SIGN_UP)
		draw_status(plane, form, layout);
	put_box_text(plane, layout->secondary_row, layout->left_button_x,
		layout->button_width, secondary_text,
		form->focus == AUTH_FOCUS_SECONDARY, false);
	if (app_ui_preview_enabled() && form->mode == AUTH_FORM_LOGIN)
	{
		snprintf(preview_text, sizeof(preview_text), "%sPREVIEW%s",
			form->focus == AUTH_FOCUS_PREVIEW ? "> " : "",
			form->focus == AUTH_FOCUS_PREVIEW ? " <" : "");
		put_box_text(plane, layout->secondary_row, layout->preview_button_x,
			layout->button_width, preview_text,
			form->focus == AUTH_FOCUS_PREVIEW, false);
	}
	put_box_text(plane, layout->secondary_row, layout->right_button_x,
		layout->button_width, offline_text,
		form->focus == AUTH_FOCUS_OFFLINE, false);
	set_color(plane, 202, 178, 225);
	put_centered(plane, layout->footer_row,
		app_ui_preview_enabled() && form->mode == AUTH_FORM_LOGIN
		? "P PREVIEW SIGN-IN  |  UP/DOWN/TAB MOVE  ENTER SELECT"
		: "UP/DOWN OR TAB MOVE  ENTER SELECT  ESC BACK", false);
}

static bool	point_in_row(const ncinput *input, int row, int x, int width)
{
	return ((int)input->y >= row - 1 && (int)input->y <= row + 1
		&& (int)input->x >= x && (int)input->x < x + width);
}
