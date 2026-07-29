#include "tetrisu.h"

# define SIGN_IN_TITLE		"SIGN IN REQUIRED"
# define SIGN_IN_DISMISS	"  BACK  "
# define SIGN_IN_LOGIN_BTN	" SIGN IN "
# define SIGN_IN_HINT		"ESC BACK  |  LEFT/RIGHT SELECT  |  ENTER CONFIRM"
# define SIGN_IN_PIXEL_MAX_COLS	62
# define SIGN_IN_PIXEL_MAX_ROWS	60
# define SIGN_IN_PIXEL_MIN_COLS	52
# define SIGN_IN_PIXEL_MIN_ROWS	16
# define CLR_TITLE_R	255
# define CLR_TITLE_G	112
# define CLR_TITLE_B	190
# define CLR_BODY_R	240
# define CLR_BODY_G	230
# define CLR_BODY_B	245
# define CLR_BTN_FG_R	12
# define CLR_BTN_FG_G	8
# define CLR_BTN_FG_B	24
# define CLR_BTN_BG_R	255
# define CLR_BTN_BG_G	203
# define CLR_BTN_BG_B	102
# define CLR_DIM_R	180
# define CLR_DIM_G	148
# define CLR_DIM_B	205
# define CLR_FRAME_R	216
# define CLR_FRAME_G	120
# define CLR_FRAME_B	224
# define CLR_BG_R	18
# define CLR_BG_G	10
# define CLR_BG_B	28

static bool	show_pixel_modal(render_ctx_t *ctx, sign_in_modal_t *modal);
static bool	show_compat_modal(render_ctx_t *ctx, sign_in_modal_t *modal);
static bool	pixel_layout(const render_ctx_t *ctx, int *rows, int *cols,
				int *y, int *x);
static bool	create_text_plane(render_ctx_t *ctx, sign_in_modal_t *modal,
				int rows, int cols, int y, int x);
static void	make_text_windows_transparent(struct ncvisual *visual,
				const sign_in_modal_t *modal, int rows, int cols,
				int cell_px_y, int cell_px_x);
static void	clear_visual_cell_run(struct ncvisual *visual, int row, int x,
				int width, int cell_px_y, int cell_px_x);
static int	centered_text_x(const char *text, int cols);
static void	draw_modal(render_ctx_t *ctx, sign_in_modal_t *modal);
static void	draw_frame(struct ncplane *plane, int rows, int cols);
static void	draw_content(struct ncplane *plane, sign_in_modal_t *modal,
				int rows, int cols, bool pixel);
static void	draw_button(struct ncplane *plane, int row, int x,
				const char *text, bool focused);
static void	button_geometry(const sign_in_modal_t *modal, int *row,
				int *dismiss_x, int *login_x);
static void	put_centered_clipped(struct ncplane *plane, int row,
				const char *text, unsigned cols);
static bool	button_hit(const ncinput *input, int row, int x, int width,
				int plane_y, int plane_x);

sign_in_result_t	sign_in_modal_handle_mouse(sign_in_modal_t *modal,
	const render_ctx_t *ctx, const ncinput *input, uint32_t key)
{
	int	plane_y;
	int	plane_x;
	int	row;
	int	dismiss_x;
	int	login_x;

	if (modal == NULL || !modal->visible || ctx == NULL || input == NULL
		|| modal->text_plane == NULL)
		return (SIGN_IN_RESULT_NONE);
	ncplane_yx(modal->text_plane, &plane_y, &plane_x);
	button_geometry(modal, &row, &dismiss_x, &login_x);
	if (button_hit(input, row, dismiss_x, (int)strlen(SIGN_IN_DISMISS),
			plane_y, plane_x))
	{
		modal->focus = SIGN_IN_FOCUS_DISMISS;
		if (key == NCKEY_BUTTON1 && (input->evtype == NCTYPE_PRESS
				|| input->evtype == NCTYPE_UNKNOWN))
			return (SIGN_IN_RESULT_DISMISS);
	}
	else if (button_hit(input, row, login_x,
			(int)strlen(SIGN_IN_LOGIN_BTN), plane_y, plane_x))
	{
		modal->focus = SIGN_IN_FOCUS_LOGIN;
		if (key == NCKEY_BUTTON1 && (input->evtype == NCTYPE_PRESS
				|| input->evtype == NCTYPE_UNKNOWN))
			return (SIGN_IN_RESULT_LOGIN);
	}
	return (SIGN_IN_RESULT_NONE);
}

bool	render_sign_in_show(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	if (ctx == NULL || ctx->std == NULL || modal == NULL)
		return (false);
	render_sign_in_destroy(ctx, modal);
	if (render_pixels_available(ctx) && show_pixel_modal(ctx, modal))
		return (true);
	render_sign_in_destroy(ctx, modal);
	return (show_compat_modal(ctx, modal));
}

bool	render_sign_in_refresh(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	if (ctx == NULL || ctx->nc == NULL || modal == NULL
		|| modal->text_plane == NULL)
		return (false);
	draw_modal(ctx, modal);
	ncplane_move_top(modal->text_plane);
	render_notification_raise(ctx);
	modal->visible = true;
	return (notcurses_render(ctx->nc) == 0);
}

void	render_sign_in_destroy(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	(void)ctx;
	if (modal == NULL)
		return ;
	if (modal->text_plane != NULL)
		ncplane_destroy(modal->text_plane);
	if (modal->art_plane != NULL)
		ncplane_destroy(modal->art_plane);
	modal->text_plane = NULL;
	modal->art_plane = NULL;
	modal->visible = false;
}

static bool	show_pixel_modal(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	struct ncvisual			*visual;
	struct ncvisual_options	vopts;
	ncplane_options			art_opts;
	struct ncplane			*art;
	int						rows;
	int						cols;
	int						y;
	int						x;

	if (!pixel_layout(ctx, &rows, &cols, &y, &x))
		return (false);
	visual = ncvisual_from_file(SIGN_IN_ASSET_PATH);
	if (visual == NULL || ncvisual_resize_noninterpolative(visual,
			rows * ctx->cell_px_y, cols * ctx->cell_px_x) != 0)
	{
		if (visual != NULL)
			ncvisual_destroy(visual);
		return (false);
	}
	make_text_windows_transparent(visual, modal, rows, cols,
		ctx->cell_px_y, ctx->cell_px_x);
	memset(&art_opts, 0, sizeof(art_opts));
	art_opts.y = y;
	art_opts.x = x;
	art_opts.rows = rows;
	art_opts.cols = cols;
	art = ncplane_create(ctx->std, &art_opts);
	if (art == NULL)
	{
		ncvisual_destroy(visual);
		return (false);
	}
	memset(&vopts, 0, sizeof(vopts));
	vopts.n = art;
	vopts.scaling = NCSCALE_NONE;
	vopts.blitter = NCBLIT_PIXEL;
	vopts.flags = NCVISUAL_OPTION_NODEGRADE
		| NCVISUAL_OPTION_NOINTERPOLATE;
	if (ncvisual_blit(ctx->nc, visual, &vopts) == NULL)
	{
		ncvisual_destroy(visual);
		ncplane_destroy(art);
		return (false);
	}
	ncvisual_destroy(visual);
	modal->art_plane = art;
	if (!create_text_plane(ctx, modal, rows, cols, y, x))
	{
		ncplane_destroy(art);
		modal->art_plane = NULL;
		return (false);
	}
	return (render_sign_in_refresh(ctx, modal));
}

static bool	show_compat_modal(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	unsigned	std_rows;
	unsigned	std_cols;
	int			rows;
	int			cols;
	int			y;
	int			x;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	rows = SIGN_IN_MODAL_ROWS;
	cols = SIGN_IN_MODAL_COLS;
	if (rows > (int)std_rows - 2)
		rows = (int)std_rows - 2;
	if (cols > (int)std_cols - 2)
		cols = (int)std_cols - 2;
	if (rows < 8 || cols < 28)
		return (false);
	y = ((int)std_rows - rows) / 2;
	x = ((int)std_cols - cols) / 2;
	if (!create_text_plane(ctx, modal, rows, cols, y, x))
		return (false);
	return (render_sign_in_refresh(ctx, modal));
}

static bool	pixel_layout(const render_ctx_t *ctx, int *rows, int *cols,
	int *y, int *x)
{
	unsigned	std_rows;
	unsigned	std_cols;
	int			max_rows;
	int			max_cols;
	int64_t		numerator;
	int64_t		denominator;

	if (ctx->cell_px_y <= 0 || ctx->cell_px_x <= 0)
		return (false);
	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	max_rows = (int)std_rows * SIGN_IN_PIXEL_MAX_ROWS / 100;
	max_cols = (int)std_cols * SIGN_IN_PIXEL_MAX_COLS / 100;
	*cols = max_cols;
	numerator = (int64_t)*cols * ctx->cell_px_x
		* SIGN_IN_SOURCE_PIXELS_Y;
	denominator = (int64_t)SIGN_IN_SOURCE_PIXELS_X * ctx->cell_px_y;
	*rows = (int)((numerator + denominator / 2) / denominator);
	if (*rows > max_rows)
	{
		*rows = max_rows;
		numerator = (int64_t)*rows * ctx->cell_px_y
			* SIGN_IN_SOURCE_PIXELS_X;
		denominator = (int64_t)SIGN_IN_SOURCE_PIXELS_Y * ctx->cell_px_x;
		*cols = (int)((numerator + denominator / 2) / denominator);
	}
	if (*rows > (int)std_rows - 2)
		*rows = (int)std_rows - 2;
	if (*cols > (int)std_cols - 2)
		*cols = (int)std_cols - 2;
	if (*rows < SIGN_IN_PIXEL_MIN_ROWS
		|| *cols < SIGN_IN_PIXEL_MIN_COLS)
		return (false);
	*y = ((int)std_rows - *rows) / 2;
	*x = ((int)std_cols - *cols) / 2;
	return (true);
}

static bool	create_text_plane(render_ctx_t *ctx, sign_in_modal_t *modal,
	int rows, int cols, int y, int x)
{
	ncplane_options	opts;

	memset(&opts, 0, sizeof(opts));
	opts.y = y;
	opts.x = x;
	opts.rows = rows;
	opts.cols = cols;
	modal->text_plane = ncplane_create(ctx->std, &opts);
	return (modal->text_plane != NULL);
}

static void	make_text_windows_transparent(struct ncvisual *visual,
	const sign_in_modal_t *modal, int rows, int cols, int cell_px_y,
	int cell_px_x)
{
	char	line1[80];
	char	line2[80];
	int		title_row;
	int		body_row;
	int		hint_row;
	int		button_row;
	int		dismiss_x;
	int		login_x;

	title_row = rows * 33 / 100;
	body_row = rows * 47 / 100;
	hint_row = rows * 63 / 100;
	home_sign_in_body_line1(modal->label, line1, sizeof(line1));
	home_sign_in_body_line2(line2, sizeof(line2));
	clear_visual_cell_run(visual, title_row,
		centered_text_x(SIGN_IN_TITLE, cols), (int)strlen(SIGN_IN_TITLE),
		cell_px_y, cell_px_x);
	clear_visual_cell_run(visual, body_row, centered_text_x(line1, cols),
		(int)strlen(line1), cell_px_y, cell_px_x);
	clear_visual_cell_run(visual, body_row + 1,
		centered_text_x(line2, cols), (int)strlen(line2),
		cell_px_y, cell_px_x);
	clear_visual_cell_run(visual, hint_row,
		centered_text_x(SIGN_IN_HINT, cols), (int)strlen(SIGN_IN_HINT),
		cell_px_y, cell_px_x);
	button_row = rows * 81 / 100;
	dismiss_x = cols * 31 / 100 - (int)strlen(SIGN_IN_DISMISS) / 2;
	login_x = cols * 69 / 100 - (int)strlen(SIGN_IN_LOGIN_BTN) / 2;
	clear_visual_cell_run(visual, button_row, dismiss_x,
		(int)strlen(SIGN_IN_DISMISS), cell_px_y, cell_px_x);
	clear_visual_cell_run(visual, button_row, login_x,
		(int)strlen(SIGN_IN_LOGIN_BTN), cell_px_y, cell_px_x);
}

static void	clear_visual_cell_run(struct ncvisual *visual, int row, int x,
	int width, int cell_px_y, int cell_px_x)
{
	uint32_t	pixel;
	int			pixel_y;
	int			pixel_x;
	int			end_y;
	int			end_x;

	if (visual == NULL || row < 0 || x < 0 || width <= 0
		|| cell_px_y <= 0 || cell_px_x <= 0)
		return ;
	pixel_y = row * cell_px_y;
	end_y = pixel_y + cell_px_y;
	end_x = (x + width) * cell_px_x;
	while (pixel_y < end_y)
	{
		pixel_x = x * cell_px_x;
		while (pixel_x < end_x)
		{
			if (ncvisual_at_yx(visual, (unsigned)pixel_y,
					(unsigned)pixel_x, &pixel) == 0)
			{
				ncpixel_set_a(&pixel, 0);
				(void)ncvisual_set_yx(visual, (unsigned)pixel_y,
					(unsigned)pixel_x, pixel);
			}
			pixel_x++;
		}
		pixel_y++;
	}
}

static int	centered_text_x(const char *text, int cols)
{
	int	length;
	int	limit;

	if (text == NULL || cols < 4)
		return (1);
	limit = cols - 4;
	length = (int)strlen(text);
	if (length > limit)
		length = limit;
	return ((cols - length) / 2);
}

static void	draw_modal(render_ctx_t *ctx, sign_in_modal_t *modal)
{
	struct ncplane	*plane;
	uint64_t		channels;
	int				rows;
	int				cols;
	bool			pixel;

	plane = modal->text_plane;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	pixel = (modal->art_plane != NULL);
	channels = 0;
	if (pixel)
		(void)ncchannels_set_bg_rgb8(&channels, CLR_BG_R, CLR_BG_G,
			CLR_BG_B);
	else
	{
		(void)ncchannels_set_fg_rgb8(&channels, CLR_BODY_R, CLR_BODY_G,
			CLR_BODY_B);
		(void)ncchannels_set_bg_rgb8(&channels, CLR_BG_R, CLR_BG_G,
			CLR_BG_B);
	}
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	if (!pixel)
		draw_frame(plane, rows, cols);
	draw_content(plane, modal, rows, cols, pixel);
	(void)ctx;
}

static void	draw_frame(struct ncplane *plane, int rows, int cols)
{
	int	index;

	(void)ncplane_set_fg_rgb8(plane, CLR_FRAME_R, CLR_FRAME_G, CLR_FRAME_B);
	(void)ncplane_set_bg_rgb8(plane, CLR_BG_R, CLR_BG_G, CLR_BG_B);
	(void)ncplane_putstr_yx(plane, 0, 0, "+");
	(void)ncplane_putstr_yx(plane, 0, cols - 1, "+");
	(void)ncplane_putstr_yx(plane, rows - 1, 0, "+");
	(void)ncplane_putstr_yx(plane, rows - 1, cols - 1, "+");
	index = 1;
	while (index < cols - 1)
	{
		(void)ncplane_putstr_yx(plane, 0, index, "-");
		(void)ncplane_putstr_yx(plane, rows - 1, index, "-");
		index++;
	}
	index = 1;
	while (index < rows - 1)
	{
		(void)ncplane_putstr_yx(plane, index, 0, "|");
		(void)ncplane_putstr_yx(plane, index, cols - 1, "|");
		index++;
	}
}

static void	draw_content(struct ncplane *plane, sign_in_modal_t *modal,
	int rows, int cols, bool pixel)
{
	char	line1[80];
	char	line2[80];
	int		title_row;
	int		body_row;
	int		hint_row;
	int		button_row;
	int		dismiss_x;
	int		login_x;

	if (pixel)
	{
		title_row = rows * 33 / 100;
		body_row = rows * 47 / 100;
		hint_row = rows * 63 / 100;
	}
	else
	{
		title_row = 2;
		body_row = rows / 2 - 2;
		hint_row = rows - 2;
	}
	(void)ncplane_set_fg_rgb8(plane, CLR_TITLE_R, CLR_TITLE_G, CLR_TITLE_B);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	put_centered_clipped(plane, title_row, SIGN_IN_TITLE, (unsigned)cols);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	home_sign_in_body_line1(modal->label, line1, sizeof(line1));
	home_sign_in_body_line2(line2, sizeof(line2));
	(void)ncplane_set_fg_rgb8(plane, CLR_BODY_R, CLR_BODY_G, CLR_BODY_B);
	put_centered_clipped(plane, body_row, line1, (unsigned)cols);
	put_centered_clipped(plane, body_row + 1, line2, (unsigned)cols);
	button_geometry(modal, &button_row, &dismiss_x, &login_x);
	draw_button(plane, button_row, dismiss_x, SIGN_IN_DISMISS,
		modal->focus == SIGN_IN_FOCUS_DISMISS);
	draw_button(plane, button_row, login_x, SIGN_IN_LOGIN_BTN,
		modal->focus == SIGN_IN_FOCUS_LOGIN);
	(void)ncplane_set_fg_rgb8(plane, CLR_DIM_R, CLR_DIM_G, CLR_DIM_B);
	(void)ncplane_set_bg_rgb8(plane, CLR_BG_R, CLR_BG_G, CLR_BG_B);
	put_centered_clipped(plane, hint_row, SIGN_IN_HINT, (unsigned)cols);
}

static void	draw_button(struct ncplane *plane, int row, int x,
	const char *text, bool focused)
{
	if (focused)
	{
		(void)ncplane_set_fg_rgb8(plane, CLR_BTN_FG_R, CLR_BTN_FG_G,
			CLR_BTN_FG_B);
		(void)ncplane_set_bg_rgb8(plane, CLR_BTN_BG_R, CLR_BTN_BG_G,
			CLR_BTN_BG_B);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, CLR_DIM_R, CLR_DIM_G, CLR_DIM_B);
		(void)ncplane_set_bg_rgb8(plane, CLR_BG_R, CLR_BG_G, CLR_BG_B);
	}
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, text);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	button_geometry(const sign_in_modal_t *modal, int *row,
	int *dismiss_x, int *login_x)
{
	int	rows;
	int	cols;
	int	gap;

	rows = (int)ncplane_dim_y(modal->text_plane);
	cols = (int)ncplane_dim_x(modal->text_plane);
	if (modal->art_plane != NULL)
	{
		*row = rows * 81 / 100;
		*dismiss_x = cols * 31 / 100
			- (int)strlen(SIGN_IN_DISMISS) / 2;
		*login_x = cols * 69 / 100
			- (int)strlen(SIGN_IN_LOGIN_BTN) / 2;
		return ;
	}
	*row = rows - 4;
	gap = 4;
	*dismiss_x = (cols - (int)strlen(SIGN_IN_DISMISS)
			- (int)strlen(SIGN_IN_LOGIN_BTN) - gap) / 2;
	*login_x = *dismiss_x + (int)strlen(SIGN_IN_DISMISS) + gap;
}

static void	put_centered_clipped(struct ncplane *plane, int row,
	const char *text, unsigned cols)
{
	char	clipped[256];
	int		limit;
	int		x;

	if (plane == NULL || text == NULL || cols < 4)
		return ;
	limit = (int)cols - 4;
	snprintf(clipped, sizeof(clipped), "%.*s", limit, text);
	x = ((int)cols - (int)strlen(clipped)) / 2;
	if (x < 1)
		x = 1;
	(void)ncplane_putstr_yx(plane, row, x, clipped);
}

static bool	button_hit(const ncinput *input, int row, int x, int width,
	int plane_y, int plane_x)
{
	return (input->y == plane_y + row && input->x >= plane_x + x
		&& input->x < plane_x + x + width);
}
