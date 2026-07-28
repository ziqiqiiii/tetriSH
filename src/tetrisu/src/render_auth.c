#include "tetrisu.h"

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
	int	right_button_x;
	int	button_width;
	int	footer_row;
	bool	art;
}	auth_layout_t;

static bool			auth_art_available(const render_ctx_t *ctx);
static auth_layout_t	auth_layout(const render_ctx_t *ctx);
static void			set_color(struct ncplane *plane, int red, int green,
						int blue);
static void			fill_line(struct ncplane *plane, int row, int x, int width,
						bool focused);
static void			put_centered(struct ncplane *plane, int row,
						const char *text, bool bold);
static void			put_box_text(struct ncplane *plane, int row, int x,
						int width, const char *text, bool focused,
						bool disabled);
static void			clip_utf8_bytes(const char *text, size_t limit,
						char *output, size_t capacity);
static void			draw_field(struct ncplane *plane, const auth_form_t *form,
						auth_focus_t focus, int row, int x, int width,
						const char *label, const char *value,
						const char *placeholder, bool password);
static void			draw_status(struct ncplane *plane, const auth_form_t *form,
						const auth_layout_t *layout);
static void			draw_native_frame(struct ncplane *plane, int rows,
						int cols);
static void			draw_form(struct ncplane *plane, const auth_form_t *form,
						const auth_layout_t *layout);
static void			draw_supported_values(struct ncplane *plane,
						const auth_form_t *form,
						const auth_layout_t *layout);
static void			draw_supported_value(struct ncplane *plane,
						const auth_form_t *form, auth_focus_t focus,
						int row, int x, int width, const char *value,
						const char *placeholder, bool password);
static bool			point_in_row(const ncinput *input, int row, int x,
						int width);

/**
 * @brief Draws a responsive native-text form over the character-free artwork.
 */
bool	render_auth_show(render_ctx_t *ctx, const auth_form_t *form,
	bool rebuild_background)
{
	ncplane_options	options;
	auth_layout_t	layout;
	uint64_t		channels;
	unsigned		rows;
	unsigned		cols;
	bool			pixel_labels;

	if (ctx == NULL || ctx->std == NULL || form == NULL)
		return (false);
	render_menu_destroy(ctx);
	if (rebuild_background)
	{
		render_screen_destroy(ctx);
		render_auth_pixel_labels_destroy(ctx);
		if (auth_art_available(ctx))
		{
			if (render_background_replace(ctx, AUTH_BACKGROUND_PATH, false) < 0)
				return (false);
		}
		else
			render_background_destroy(ctx);
	}
	else
		render_screen_destroy(ctx);
	ncplane_dim_yx(ctx->std, &rows, &cols);
	if (rows < 12 || cols < 38)
		return (false);
	memset(&options, 0, sizeof(options));
	options.rows = (int)rows;
	options.cols = (int)cols;
	ctx->screen_plane = ncplane_create(ctx->std, &options);
	if (ctx->screen_plane == NULL)
		return (false);
	layout = auth_layout(ctx);
	pixel_labels = false;
	if (layout.art)
		pixel_labels = render_auth_pixel_labels_refresh(ctx, form,
				rebuild_background);
	if (!pixel_labels)
		render_auth_pixel_labels_destroy(ctx);
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
	if (pixel_labels)
		draw_supported_values(ctx->screen_plane, form, &layout);
	else
		draw_form(ctx->screen_plane, form, &layout);
	ncplane_move_top(ctx->screen_plane);
	render_compatibility_badge_refresh(ctx);
	render_notification_raise(ctx);
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Maps a pointer press to the same focus order used by the keyboard.
 */
bool	render_auth_hit_test(const render_ctx_t *ctx, const auth_form_t *form,
	const ncinput *input, auth_focus_t *focus)
{
	auth_layout_t	layout;

	if (ctx == NULL || form == NULL || input == NULL || focus == NULL)
		return (false);
	layout = auth_layout(ctx);
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
	else if (point_in_row(input, layout.secondary_row, layout.right_button_x,
			layout.button_width))
		*focus = AUTH_FOCUS_OFFLINE;
	else
		return (false);
	return (true);
}

/**
 * @brief Removes the auth overlay without touching the next screen.
 */
void	render_auth_destroy(render_ctx_t *ctx)
{
	render_screen_destroy(ctx);
	render_auth_pixel_labels_destroy(ctx);
	render_compatibility_badge_hide(ctx);
}

static bool	auth_art_available(const render_ctx_t *ctx)
{
	unsigned	rows;
	unsigned	cols;

	if (!render_pixels_available(ctx) || ctx->std == NULL)
		return (false);
	ncplane_dim_yx(ctx->std, &rows, &cols);
	return (rows >= 24 && cols >= 64);
}

static auth_layout_t	auth_layout(const render_ctx_t *ctx)
{
	auth_layout_t	layout;
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
		layout.button_width = (ctx->bg_cols * 24) / 100;
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
	layout.left_button_x = center - layout.button_width - 1;
	layout.right_button_x = center + 1;
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

static void	draw_field(struct ncplane *plane, const auth_form_t *form,
	auth_focus_t focus, int row, int x, int width, const char *label,
	const char *value, const char *placeholder, bool password)
{
	char	display[AUTH_FIELD_MAX];
	char	line[AUTH_FIELD_MAX + 40];
	bool	active;

	active = form->focus == focus;
	if (password)
		(void)auth_form_mask_password(value, display, sizeof(display));
	else
		snprintf(display, sizeof(display), "%s", value);
	if (display[0] == '\0')
		snprintf(display, sizeof(display), "%s", placeholder);
	snprintf(line, sizeof(line), "%c %-15s : %s%s",
		active ? '>' : ' ', label, display, active ? "_" : "");
	put_box_text(plane, row, x, width, line, active, false);
}

static void	draw_status(struct ncplane *plane, const auth_form_t *form,
	const auth_layout_t *layout)
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

static void	draw_form(struct ncplane *plane, const auth_form_t *form,
	const auth_layout_t *layout)
{
	const char	*primary;
	const char	*secondary;
	char		title[48];
	char		primary_text[48];
	char		secondary_text[48];
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
		auth_layout_t	status_layout;

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
	put_box_text(plane, layout->secondary_row, layout->right_button_x,
		layout->button_width, offline_text,
		form->focus == AUTH_FOCUS_OFFLINE, false);
	set_color(plane, 202, 178, 225);
	put_centered(plane, layout->footer_row,
		"UP/DOWN OR TAB MOVE  ENTER SELECT  ESC BACK", false);
}

static void	draw_supported_values(struct ncplane *plane,
	const auth_form_t *form, const auth_layout_t *layout)
{
	int	value_x;
	int	value_width;

	value_x = layout->field_x + (layout->field_width * 56) / 100;
	value_width = layout->field_x + layout->field_width - value_x - 2;
	if (value_width < 8)
		value_width = 8;
	draw_supported_value(plane, form, AUTH_FOCUS_USERNAME,
		layout->field_rows[0], value_x, value_width, form->username,
		"your name", false);
	draw_supported_value(plane, form, AUTH_FOCUS_PASSWORD,
		layout->field_rows[1], value_x, value_width, form->password,
		"4+ characters", true);
	if (form->mode == AUTH_FORM_SIGN_UP)
	{
		draw_supported_value(plane, form, AUTH_FOCUS_CONFIRM,
			layout->field_rows[2], value_x, value_width, form->confirm,
			"type again", true);
		draw_supported_value(plane, form, AUTH_FOCUS_DOMAIN,
			layout->field_rows[3], value_x, value_width, form->domain,
			"server id", false);
	}
	else
		draw_supported_value(plane, form, AUTH_FOCUS_DOMAIN,
			layout->field_rows[2], value_x, value_width, form->domain,
			"server id", false);
}

static void	draw_supported_value(struct ncplane *plane,
	const auth_form_t *form, auth_focus_t focus, int row, int x, int width,
	const char *value, const char *placeholder, bool password)
{
	char	display[AUTH_FIELD_MAX];
	char	line[AUTH_FIELD_MAX + 4];
	bool	active;

	active = form->focus == focus;
	if (password)
		(void)auth_form_mask_password(value, display, sizeof(display));
	else
		snprintf(display, sizeof(display), "%s", value);
	if (display[0] == '\0')
		snprintf(display, sizeof(display), "%s", placeholder);
	snprintf(line, sizeof(line), "%s%s", display, active ? "_" : "");
	put_box_text(plane, row, x, width, line, active, false);
}

static bool	point_in_row(const ncinput *input, int row, int x, int width)
{
	return ((int)input->y >= row - 1 && (int)input->y <= row + 1
		&& (int)input->x >= x && (int)input->x < x + width);
}
