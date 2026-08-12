#include "tetrisu.h"

/*
 * The confirmation dialog exists twice: as terminal cells, and here as a
 * bitmap. Cells are the honest choice on a text terminal, but a cell plane
 * cannot occlude a sprixel however high it sits in the pile - the rule
 * render_intro.c works around when it removes the home artwork before playing
 * cell-blitted video. Raised over Solo, whose HUD is a stack of bitmap planes,
 * the cell dialog came out shredded: the score panel and the ability rail drew
 * straight through its middle. A bitmap dialog is itself a sprixel, so it
 * covers them the way any other bitmap does.
 */

# define CONFIRM_PANEL_R		18
# define CONFIRM_PANEL_G		10
# define CONFIRM_PANEL_B		28
# define CONFIRM_BORDER_R		216
# define CONFIRM_BORDER_G		120
# define CONFIRM_BORDER_B		224

typedef struct s_confirm_box
{
	int	x;
	int	y;
	int	width;
	int	height;
}	t_confirm_box;

static const t_color	g_confirm_title = {255, 112, 190};
static const t_color	g_confirm_body = {250, 242, 221};
static const t_color	g_confirm_hint = {180, 148, 205};
static const t_color	g_confirm_idle = {180, 148, 205};
static const t_color	g_confirm_focus_ink = {12, 8, 24};
static const t_color	g_confirm_focus_fill = {255, 203, 102};
static const t_color	g_confirm_panel = {CONFIRM_PANEL_R, CONFIRM_PANEL_G,
	CONFIRM_PANEL_B};
static const t_color	g_confirm_border = {CONFIRM_BORDER_R,
	CONFIRM_BORDER_G, CONFIRM_BORDER_B};

static uint32_t	*build_dialog_canvas(t_render_ctx *ctx,
					const t_confirmation_dialog *dialog, int width,
					int height);
static void		draw_panel(uint32_t *canvas, int width, int height);
static void		draw_dialog_text(t_render_ctx *ctx, uint32_t *canvas,
					int width, int height,
					const t_confirmation_dialog *dialog);
static void		draw_buttons(t_render_ctx *ctx, uint32_t *canvas, int width,
					int height, const t_confirmation_dialog *dialog);
static void		draw_button(t_render_ctx *ctx, uint32_t *canvas, int width,
					int height, const t_confirm_box *box, const char *label,
					bool focused);
static void		draw_centered(t_render_ctx *ctx, uint32_t *canvas, int width,
					int height, const char *text, int row, int glyph_size,
					t_color tint);
static void		draw_atlas_text(uint32_t *canvas, int width, int height,
					const t_pixel_asset *font, const char *text,
					const t_confirm_box *at, t_color tint);
static void		draw_atlas_glyph(uint32_t *canvas, int width, int height,
					const t_pixel_asset *font, int glyph, int x, int y,
					int glyph_size, t_color tint);
static void		fill_rect(uint32_t *canvas, int width, int height,
					const t_confirm_box *box, t_color colour, unsigned alpha);
static void		blend_over(uint32_t *dest, unsigned red, unsigned green,
					unsigned blue, unsigned alpha);
static int		ink_span(int units, int glyph_size);
static int		glyph_spacing(int glyph_size);
static int		text_pixels(const char *text, int glyph_size);
static int		fit_glyph(const char *text, int box_width, int glyph_size);
static const t_pixel_asset	*confirmation_font(t_render_ctx *ctx);

/**
 * @brief Draws the dialog as a bitmap plane over whatever the screen holds.
 *
 * @param ctx Active render context.
 * @param dialog Dialog whose plane is replaced with a freshly drawn bitmap.
 * @return true when the bitmap dialog is on screen.
 */
bool	render_confirmation_pixel_show(t_render_ctx *ctx,
	t_confirmation_dialog *dialog)
{
	uint32_t	*canvas;
	int			width;
	int			height;

	if (ctx == NULL || dialog == NULL || dialog->plane == NULL
		|| !render_pixels_available(ctx) || ctx->cell_px_x <= 0
		|| ctx->cell_px_y <= 0 || confirmation_font(ctx) == NULL)
		return (false);
	width = (int)ncplane_dim_x(dialog->plane) * ctx->cell_px_x;
	height = (int)ncplane_dim_y(dialog->plane) * ctx->cell_px_y;
	canvas = build_dialog_canvas(ctx, dialog, width, height);
	if (canvas == NULL)
		return (false);
	if (!render_plane_blit_rgba(ctx, dialog->plane, canvas, width, height,
			width))
	{
		free(canvas);
		return (false);
	}
	free(canvas);
	return (true);
}

/**
 * @brief Releases the glyph sheet the bitmap dialog samples.
 */
void	render_confirmation_font_release(t_render_ctx *ctx)
{
	if (ctx == NULL)
		return ;
	render_font_mask_free(&ctx->confirmation_font);
}

/**
 * @brief Composes panel, lettering and buttons into one opaque surface.
 *
 * The canvas is deliberately opaque everywhere. A dialog that asks a question
 * has to be readable over whatever it lands on, and an opaque sprixel is also
 * what stops the planes underneath showing through it.
 */
static uint32_t	*build_dialog_canvas(t_render_ctx *ctx,
	const t_confirmation_dialog *dialog, int width, int height)
{
	uint32_t	*canvas;

	if (width <= 0 || height <= 0
		|| (size_t)width > SIZE_MAX / (size_t)height / sizeof(*canvas))
		return (NULL);
	canvas = calloc((size_t)width * (size_t)height, sizeof(*canvas));
	if (canvas == NULL)
		return (NULL);
	draw_panel(canvas, width, height);
	draw_dialog_text(ctx, canvas, width, height, dialog);
	draw_buttons(ctx, canvas, width, height, dialog);
	return (canvas);
}

/**
 * @brief Fills the panel and traces its two-pixel border.
 */
static void	draw_panel(uint32_t *canvas, int width, int height)
{
	t_confirm_box	box;
	int				edge;

	box.x = 0;
	box.y = 0;
	box.width = width;
	box.height = height;
	fill_rect(canvas, width, height, &box, g_confirm_panel, 255u);
	edge = height / 40;
	if (edge < 2)
		edge = 2;
	box.height = edge;
	fill_rect(canvas, width, height, &box, g_confirm_border, 255u);
	box.y = height - edge;
	fill_rect(canvas, width, height, &box, g_confirm_border, 255u);
	box.y = 0;
	box.width = edge;
	box.height = height;
	fill_rect(canvas, width, height, &box, g_confirm_border, 255u);
	box.x = width - edge;
	fill_rect(canvas, width, height, &box, g_confirm_border, 255u);
}

/**
 * @brief Places the title, the question and the control hint.
 */
static void	draw_dialog_text(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, const t_confirmation_dialog *dialog)
{
	int	glyph;

	glyph = ctx->cell_px_y;
	if (glyph < 4)
		glyph = 4;
	draw_centered(ctx, canvas, width, height, confirmation_title(dialog->kind),
		height / 8, glyph, g_confirm_title);
	draw_centered(ctx, canvas, width, height, confirmation_body(dialog->kind),
		height * 34 / 100, glyph * 3 / 4, g_confirm_body);
	draw_centered(ctx, canvas, width, height, confirmation_hint(dialog->kind),
		height * 82 / 100, glyph / 2, g_confirm_hint);
}

/**
 * @brief Lays the two answers out side by side, safe answer first.
 */
static void	draw_buttons(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, const t_confirmation_dialog *dialog)
{
	t_confirm_box	box;
	size_t			longest;
	int				gap;

	/*
	 * A word answer needs the room a word takes. YES and NO fit a quarter of
	 * the dialog with space to spare; PLAY OFFLINE at the same width is
	 * shrunk by fit_glyph until it is the small unreadable text this whole
	 * bitmap tier exists to avoid.
	 */
	longest = strlen(confirmation_no_label(dialog->kind));
	if (strlen(confirmation_yes_label(dialog->kind)) > longest)
		longest = strlen(confirmation_yes_label(dialog->kind));
	gap = width / 10;
	box.width = width / 4;
	if (longest > 4)
		box.width = (width - gap) / 2 - gap / 2;
	box.height = height / 6;
	box.y = height * 55 / 100;
	box.x = width / 2 - box.width - gap / 2;
	draw_button(ctx, canvas, width, height, &box,
		confirmation_no_label(dialog->kind),
		dialog->focus == CONFIRM_FOCUS_NO);
	box.x = width / 2 + gap / 2;
	draw_button(ctx, canvas, width, height, &box,
		confirmation_yes_label(dialog->kind),
		dialog->focus == CONFIRM_FOCUS_YES);
}

/**
 * @brief Draws one answer, filled when it holds focus.
 */
static void	draw_button(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, const t_confirm_box *box, const char *label, bool focused)
{
	t_confirm_box	at;
	t_color			ink;
	int				glyph;

	if (focused)
		fill_rect(canvas, width, height, box, g_confirm_focus_fill, 255u);
	else
		fill_rect(canvas, width, height, box, g_confirm_border, 60u);
	ink = g_confirm_idle;
	if (focused)
		ink = g_confirm_focus_ink;
	glyph = box->height * 55 / 100;
	if (glyph < 4)
		glyph = 4;
	glyph = fit_glyph(label, box->width * 80 / 100, glyph);
	at.height = glyph;
	at.width = text_pixels(label, glyph);
	at.x = box->x + (box->width - at.width) / 2;
	at.y = box->y + (box->height - glyph) / 2;
	draw_atlas_text(canvas, width, height, confirmation_font(ctx), label,
		&at, ink);
}

/**
 * @brief Centres one line horizontally, shrinking it until it fits.
 */
static void	draw_centered(t_render_ctx *ctx, uint32_t *canvas, int width,
	int height, const char *text, int row, int glyph_size, t_color tint)
{
	t_confirm_box	at;

	if (glyph_size < 3)
		glyph_size = 3;
	glyph_size = fit_glyph(text, width * 88 / 100, glyph_size);
	at.height = glyph_size;
	at.width = text_pixels(text, glyph_size);
	at.x = (width - at.width) / 2;
	at.y = row;
	draw_atlas_text(canvas, width, height, confirmation_font(ctx), text,
		&at, tint);
}

static void	draw_atlas_text(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, const char *text, const t_confirm_box *at,
	t_color tint)
{
	unsigned	codepoint;
	int			index;

	if (font == NULL)
		return ;
	index = 0;
	while (text[index] != '\0')
	{
		codepoint = (unsigned char)text[index];
		if (codepoint < 32 || codepoint >= 32 + FONT_COLUMNS * FONT_ROWS)
			codepoint = '?';
		draw_atlas_glyph(canvas, width, height, font, (int)codepoint - 32,
			at->x + index * (at->height + glyph_spacing(at->height)),
			at->y, at->height, tint);
		index++;
	}
}

static void	draw_atlas_glyph(uint32_t *canvas, int width, int height,
	const t_pixel_asset *font, int glyph, int x, int y, int glyph_size,
	t_color tint)
{
	t_confirm_box	cell;
	unsigned		alpha;
	int				source_y;
	int				source_x;

	source_y = FONT_INK_TOP;
	while (source_y < FONT_INK_BOTTOM)
	{
		source_x = 0;
		while (source_x < FONT_GLYPH_WIDTH)
		{
			cell.x = x + source_x * glyph_size / FONT_GLYPH_WIDTH;
			cell.width = (source_x + 1) * glyph_size / FONT_GLYPH_WIDTH
				- source_x * glyph_size / FONT_GLYPH_WIDTH;
			cell.y = y + ink_span(source_y - FONT_INK_Y, glyph_size);
			cell.height = ink_span(source_y + 1 - FONT_INK_Y, glyph_size)
				- ink_span(source_y - FONT_INK_Y, glyph_size);
			alpha = ncpixel_a(font->pixels[(size_t)((glyph / FONT_COLUMNS)
						* FONT_GLYPH_HEIGHT + source_y) * font->width
					+ (glyph % FONT_COLUMNS) * FONT_GLYPH_WIDTH + source_x]);
			if (alpha != 0)
				fill_rect(canvas, width, height, &cell, tint, alpha);
			source_x++;
		}
		source_y++;
	}
}

static void	fill_rect(uint32_t *canvas, int width, int height,
	const t_confirm_box *box, t_color colour, unsigned alpha)
{
	int	y;
	int	x;

	if (alpha == 0)
		return ;
	y = box->y;
	while (y < box->y + box->height)
	{
		if (y >= 0 && y < height)
		{
			x = box->x;
			while (x < box->x + box->width)
			{
				if (x >= 0 && x < width)
					blend_over(&canvas[(size_t)y * width + x], colour.r,
						colour.g, colour.b, alpha);
				x++;
			}
		}
		y++;
	}
}

/**
 * @brief Blends one colour over a canvas pixel, leaving it fully opaque.
 */
static void	blend_over(uint32_t *dest, unsigned red, unsigned green,
	unsigned blue, unsigned alpha)
{
	unsigned	base_r;
	unsigned	base_g;
	unsigned	base_b;

	base_r = ncpixel_r(*dest);
	base_g = ncpixel_g(*dest);
	base_b = ncpixel_b(*dest);
	*dest = ncpixel((base_r * (255u - alpha) + red * alpha) / 255u,
			(base_g * (255u - alpha) + green * alpha) / 255u,
			(base_b * (255u - alpha) + blue * alpha) / 255u);
	ncpixel_set_a(dest, 255u);
}

static int	ink_span(int units, int glyph_size)
{
	if (units >= 0)
		return (units * glyph_size / FONT_INK_HEIGHT);
	return (-((-units * glyph_size + FONT_INK_HEIGHT - 1) / FONT_INK_HEIGHT));
}

static int	glyph_spacing(int glyph_size)
{
	if (glyph_size / 4 < 1)
		return (1);
	return (glyph_size / 4);
}

static int	text_pixels(const char *text, int glyph_size)
{
	int	length;

	length = (int)strlen(text);
	if (length <= 0)
		return (0);
	return (length * glyph_size + (length - 1) * glyph_spacing(glyph_size));
}

/**
 * @brief Shrinks a glyph size until the whole line fits its box.
 */
static int	fit_glyph(const char *text, int box_width, int glyph_size)
{
	while (glyph_size > 3 && text_pixels(text, glyph_size) > box_width)
		glyph_size--;
	return (glyph_size);
}

/**
 * @brief Lazily decodes the shared glyph sheet the dialog letters with.
 */
static const t_pixel_asset	*confirmation_font(t_render_ctx *ctx)
{
	if (ctx->confirmation_font.pixels != NULL)
		return (&ctx->confirmation_font);
	if (!render_font_mask_load(&ctx->confirmation_font))
		return (NULL);
	return (&ctx->confirmation_font);
}
