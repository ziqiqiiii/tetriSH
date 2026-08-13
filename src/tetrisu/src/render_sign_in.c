#include "tetrisu.h"

# define SIGN_IN_TITLE		"SIGN IN REQUIRED"
# define SIGN_IN_DISMISS	"  BACK  "
# define SIGN_IN_LOGIN_BTN	" SIGN IN "
# define SIGN_IN_HINT		"ESC BACK  |  LEFT/RIGHT SELECT  |  ENTER CONFIRM"
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

static bool	create_modal_plane(t_render_ctx *ctx, t_sign_in_modal *modal);
static void	draw_modal(t_sign_in_modal *modal);
static void	draw_frame(struct ncplane *plane, int rows, int cols);
static void	draw_content(struct ncplane *plane, t_sign_in_modal *modal,
				int rows, int cols);
static void	draw_button(struct ncplane *plane, int row, int x,
				const char *text, bool focused);
static void	button_geometry(const t_sign_in_modal *modal, int *row,
				int *dismiss_x, int *login_x);
static void	put_centered_clipped(struct ncplane *plane, int row,
				const char *text, unsigned cols);
static bool	button_hit(const ncinput *input, int row, int x, int width,
				int plane_y, int plane_x);

t_sign_in_result	sign_in_modal_handle_mouse(t_sign_in_modal *modal,
	const t_render_ctx *ctx, const ncinput *input, uint32_t key)
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

bool	render_sign_in_show(t_render_ctx *ctx, t_sign_in_modal *modal)
{
	if (ctx == NULL || ctx->std == NULL || modal == NULL)
		return (false);
	render_sign_in_destroy(ctx, modal);
	if (!create_modal_plane(ctx, modal))
		return (false);
	return (render_sign_in_refresh(ctx, modal));
}

bool	render_sign_in_refresh(t_render_ctx *ctx, t_sign_in_modal *modal)
{
	if (ctx == NULL || ctx->nc == NULL || modal == NULL
		|| modal->text_plane == NULL)
		return (false);
	draw_modal(modal);
	ncplane_move_top(modal->text_plane);
	render_notification_raise(ctx);
	modal->visible = true;
	return (notcurses_render(ctx->nc) == 0);
}

void	render_sign_in_destroy(t_render_ctx *ctx, t_sign_in_modal *modal)
{
	(void)ctx;
	if (modal == NULL)
		return ;
	if (modal->text_plane != NULL)
		ncplane_destroy(modal->text_plane);
	modal->text_plane = NULL;
	modal->visible = false;
}

static bool	create_modal_plane(t_render_ctx *ctx, t_sign_in_modal *modal)
{
	unsigned		std_rows;
	unsigned		std_cols;
	int				rows;
	int				cols;
	ncplane_options	opts;

	ncplane_dim_yx(ctx->std, &std_rows, &std_cols);
	rows = SIGN_IN_MODAL_ROWS;
	cols = SIGN_IN_MODAL_COLS;
	if (rows > (int)std_rows - 2)
		rows = (int)std_rows - 2;
	if (cols > (int)std_cols - 2)
		cols = (int)std_cols - 2;
	if (rows < 10 || cols < 34)
		return (false);
	memset(&opts, 0, sizeof(opts));
	opts.y = ((int)std_rows - rows) / 2;
	opts.x = ((int)std_cols - cols) / 2;
	opts.rows = rows;
	opts.cols = cols;
	modal->text_plane = ncplane_create(ctx->std, &opts);
	return (modal->text_plane != NULL);
}

static void	draw_modal(t_sign_in_modal *modal)
{
	struct ncplane	*plane;
	uint64_t		channels;
	int				rows;
	int				cols;

	plane = modal->text_plane;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, CLR_BODY_R, CLR_BODY_G,
		CLR_BODY_B);
	(void)ncchannels_set_bg_rgb8(&channels, CLR_BG_R, CLR_BG_G, CLR_BG_B);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	draw_frame(plane, rows, cols);
	draw_content(plane, modal, rows, cols);
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

static void	draw_content(struct ncplane *plane, t_sign_in_modal *modal,
	int rows, int cols)
{
	char	line1[80];
	char	line2[80];
	int		body_row;
	int		button_row;
	int		dismiss_x;
	int		login_x;

	(void)ncplane_set_fg_rgb8(plane, CLR_TITLE_R, CLR_TITLE_G, CLR_TITLE_B);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	put_centered_clipped(plane, 2, SIGN_IN_TITLE, (unsigned)cols);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	home_sign_in_body_line1(modal->label, line1, sizeof(line1));
	home_sign_in_body_line2(line2, sizeof(line2));
	body_row = rows / 2 - 2;
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
	put_centered_clipped(plane, rows - 2, SIGN_IN_HINT, (unsigned)cols);
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

static void	button_geometry(const t_sign_in_modal *modal, int *row,
	int *dismiss_x, int *login_x)
{
	int	rows;
	int	cols;
	int	gap;

	rows = (int)ncplane_dim_y(modal->text_plane);
	cols = (int)ncplane_dim_x(modal->text_plane);
	*row = rows - 4;
	gap = 6;
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
