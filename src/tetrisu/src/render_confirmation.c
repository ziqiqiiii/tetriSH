#include "tetrisu.h"

# define CONFIRM_NO_BUTTON	"   NO   "
# define CONFIRM_YES_BUTTON	"  YES   "
# define CONFIRM_HINT		"ESC/N CANCEL  |  LEFT/RIGHT SELECT  |  ENTER CONFIRM"

static bool	create_dialog_plane(t_render_ctx *ctx,
				t_confirmation_dialog *dialog);
static void	draw_dialog(t_confirmation_dialog *dialog);
static void	draw_frame(struct ncplane *plane, int rows, int cols);
static void	draw_button(struct ncplane *plane, int row, int x,
				const char *text, bool focused);
static void	put_centered(struct ncplane *plane, int row, int cols,
				const char *text);
static bool	show_dialog(t_render_ctx *ctx, t_confirmation_dialog *dialog);
static void	paint_dialog(t_render_ctx *ctx, t_confirmation_dialog *dialog);
static void	destroy_dialog(t_render_ctx *ctx,
				t_confirmation_dialog *dialog);

/**
 * @brief Runs a persistent, safe-default Yes/No prompt over the active screen.
 */
bool	confirmation_prompt_run(t_render_ctx *ctx, t_audio_ctx *audio,
	t_confirmation_kind kind)
{
	t_confirmation_dialog	dialog;
	t_confirmation_result	result;
	t_confirmation_focus	previous_focus;
	ncinput					input;
	uint32_t				key;
	bool					accepted;

	if (ctx == NULL || ctx->nc == NULL || ctx->std == NULL)
		return (false);
	confirmation_dialog_init(&dialog, kind);
	if (!show_dialog(ctx, &dialog))
	{
		destroy_dialog(ctx, &dialog);
		return (false);
	}
	result = CONFIRM_RESULT_NONE;
	while (result == CONFIRM_RESULT_NONE)
	{
		key = render_wait_input(ctx, &input);
		if (key == (uint32_t)-1)
		{
			result = CONFIRM_RESULT_NO;
			break ;
		}
		if (input.evtype == NCTYPE_RELEASE && !nckey_mouse_p(key))
			continue ;
		if (key == NCKEY_RESIZE || key == 12u)
		{
			/* Return the resize to the owning screen indirectly. We deliberately
			 * leave standard-plane geometry untouched, so its normal geometry
			 * poll observes the mismatch and runs the renderer-specific reflow. */
			result = CONFIRM_RESULT_NO;
			continue ;
		}
		if (nckey_mouse_p(key))
			continue ;
		previous_focus = dialog.focus;
		result = confirmation_dialog_handle_key(&dialog, key);
		if (dialog.focus != previous_focus)
		{
			audio_play_menu_move(audio);
			paint_dialog(ctx, &dialog);
			ncplane_move_top(dialog.plane);
			(void)notcurses_render(ctx->nc);
		}
	}
	accepted = result == CONFIRM_RESULT_YES;
	audio_play_menu_select(audio);
	destroy_dialog(ctx, &dialog);
	render_notification_raise(ctx);
	(void)notcurses_render(ctx->nc);
	return (accepted);
}

static bool	show_dialog(t_render_ctx *ctx, t_confirmation_dialog *dialog)
{
	destroy_dialog(ctx, dialog);
	if (!create_dialog_plane(ctx, dialog))
		return (false);
	paint_dialog(ctx, dialog);
	render_notification_raise(ctx);
	ncplane_move_top(dialog->plane);
	dialog->visible = true;
	return (notcurses_render(ctx->nc) == 0);
}

/**
 * @brief Draws the dialog as a bitmap where one is available, else as cells.
 *
 * A cell plane cannot occlude a sprixel however high it sits in the pile, so
 * over a bitmap screen such as Solo the cell dialog is drawn through by the
 * planes it is supposed to cover. Where bitmaps exist the dialog becomes one
 * itself; the cell drawing stays for terminals that have no other option.
 */
static void	paint_dialog(t_render_ctx *ctx, t_confirmation_dialog *dialog)
{
	if (render_confirmation_pixel_show(ctx, dialog))
		return ;
	draw_dialog(dialog);
}

static bool	create_dialog_plane(t_render_ctx *ctx,
	t_confirmation_dialog *dialog)
{
	ncplane_options	options;
	unsigned		rows;
	unsigned		cols;
	int				height;
	int				width;

	ncplane_dim_yx(ctx->std, &rows, &cols);
	height = CONFIRMATION_MODAL_ROWS;
	width = CONFIRMATION_MODAL_COLS;
	if (height > (int)rows - 2)
		height = (int)rows - 2;
	if (width > (int)cols - 2)
		width = (int)cols - 2;
	if (height < 9 || width < 34)
		return (false);
	memset(&options, 0, sizeof(options));
	options.y = ((int)rows - height) / 2;
	options.x = ((int)cols - width) / 2;
	options.rows = height;
	options.cols = width;
	dialog->plane = ncplane_create(ctx->std, &options);
	return (dialog->plane != NULL);
}

static void	draw_dialog(t_confirmation_dialog *dialog)
{
	struct ncplane	*plane;
	uint64_t		channels;
	int				rows;
	int				cols;
	int				button_row;
	int				no_x;
	int				yes_x;

	plane = dialog->plane;
	rows = (int)ncplane_dim_y(plane);
	cols = (int)ncplane_dim_x(plane);
	channels = 0;
	(void)ncchannels_set_fg_rgb8(&channels, 250, 242, 221);
	(void)ncchannels_set_bg_rgb8(&channels, 18, 10, 28);
	(void)ncplane_set_base(plane, " ", 0, channels);
	ncplane_erase(plane);
	draw_frame(plane, rows, cols);
	(void)ncplane_set_fg_rgb8(plane, 255, 112, 190);
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	put_centered(plane, 2, cols, confirmation_title(dialog->kind));
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_set_fg_rgb8(plane, 250, 242, 221);
	put_centered(plane, rows / 2 - 1, cols,
		confirmation_body(dialog->kind));
	button_row = rows - 4;
	no_x = (cols - (int)strlen(CONFIRM_NO_BUTTON)
			- (int)strlen(CONFIRM_YES_BUTTON) - 6) / 2;
	yes_x = no_x + (int)strlen(CONFIRM_NO_BUTTON) + 6;
	draw_button(plane, button_row, no_x, CONFIRM_NO_BUTTON,
		dialog->focus == CONFIRM_FOCUS_NO);
	draw_button(plane, button_row, yes_x, CONFIRM_YES_BUTTON,
		dialog->focus == CONFIRM_FOCUS_YES);
	(void)ncplane_set_fg_rgb8(plane, 180, 148, 205);
	(void)ncplane_set_bg_rgb8(plane, 18, 10, 28);
	put_centered(plane, rows - 2, cols, CONFIRM_HINT);
}

static void	draw_frame(struct ncplane *plane, int rows, int cols)
{
	int	index;

	(void)ncplane_set_fg_rgb8(plane, 216, 120, 224);
	(void)ncplane_set_bg_rgb8(plane, 18, 10, 28);
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

static void	draw_button(struct ncplane *plane, int row, int x,
	const char *text, bool focused)
{
	if (focused)
	{
		(void)ncplane_set_fg_rgb8(plane, 12, 8, 24);
		(void)ncplane_set_bg_rgb8(plane, 255, 203, 102);
	}
	else
	{
		(void)ncplane_set_fg_rgb8(plane, 180, 148, 205);
		(void)ncplane_set_bg_rgb8(plane, 18, 10, 28);
	}
	(void)ncplane_on_styles(plane, NCSTYLE_BOLD);
	(void)ncplane_putstr_yx(plane, row, x, text);
	(void)ncplane_off_styles(plane, NCSTYLE_BOLD);
}

static void	put_centered(struct ncplane *plane, int row, int cols,
	const char *text)
{
	char	clipped[160];
	int		limit;
	int		x;

	limit = cols - 4;
	if (limit <= 0)
		return ;
	snprintf(clipped, sizeof(clipped), "%.*s", limit, text);
	x = (cols - (int)strlen(clipped)) / 2;
	if (x < 1)
		x = 1;
	(void)ncplane_putstr_yx(plane, row, x, clipped);
}

static void	destroy_dialog(t_render_ctx *ctx,
	t_confirmation_dialog *dialog)
{
	(void)ctx;
	if (dialog == NULL)
		return ;
	if (dialog->plane != NULL)
		ncplane_destroy(dialog->plane);
	dialog->plane = NULL;
	dialog->visible = false;
}
