#include "tetrisu.h"

# define LEADERBOARD_PANEL_MAX_COLS	100
# define LEADERBOARD_PANEL_MAX_ROWS	34
# define LEADERBOARD_PANEL_MIN_COLS	44
# define LEADERBOARD_PANEL_MIN_ROWS	20
# define LEADERBOARD_PANEL_SIDE_MARGIN	8
# define LEADERBOARD_PANEL_ROW_MARGIN	2
# define LEADERBOARD_REF_WIDTH		1448
# define LEADERBOARD_REF_HEIGHT		1086
# define LEADERBOARD_REF_BACK_X		494
# define LEADERBOARD_REF_REFRESH_X	744
# define LEADERBOARD_REF_BUTTON_Y	872
# define LEADERBOARD_REF_BUTTON_WIDTH	210
# define LEADERBOARD_REF_BUTTON_HEIGHT	58
/*
 * Bounding box of everything focus can change: both buttons plus the control
 * hint above them. Kept as one rectangle so a focus move rewrites a single
 * region plane rather than two overlapping ones.
 */
# define LEADERBOARD_REF_CONTROLS_X	320
# define LEADERBOARD_REF_CONTROLS_Y	830
# define LEADERBOARD_REF_CONTROLS_WIDTH	810
# define LEADERBOARD_REF_CONTROLS_HEIGHT	108

static int	min_int(int first, int second);
static leaderboard_pixel_rect_t	map_pixel_rect(int x, int y, int width,
				int height, int pixel_width, int pixel_height);

/**
 * @brief Chooses the leaderboard backdrop operation for one redraw.
 *
 * Cell sessions never depend on artwork. Bitmap sessions replace the backdrop
 * only at lifecycle boundaries (entry and resize), then reuse it for focus and
 * data refresh draws so stationary protocols do not churn a full-screen plane.
 */
leaderboard_background_action_t	leaderboard_background_action_for(
	tetrisu_pixel_policy_t pixels, bool rebuild_requested)
{
	if (pixels == TETRISU_PIXELS_NONE)
		return (LEADERBOARD_BACKGROUND_CELL);
	if (rebuild_requested)
		return (LEADERBOARD_BACKGROUND_REPLACE_EXACT);
	return (LEADERBOARD_BACKGROUND_REUSE);
}

/**
 * @brief Resolves a complete leaderboard panel inside terminal geometry.
 *
 * Small-but-supported bitmap terminals surrender the decorative margin rather
 * than shrinking below the 20-row layout required to show all ten ranks.
 */
bool	leaderboard_layout_resolve(int terminal_rows, int terminal_cols,
	bool compatibility, leaderboard_layout_t *layout)
{
	int	margin;

	if (layout == NULL || terminal_rows < LEADERBOARD_PANEL_MIN_ROWS
		|| terminal_cols < LEADERBOARD_PANEL_MIN_COLS)
		return (false);
	memset(layout, 0, sizeof(*layout));
	if (compatibility)
	{
		layout->y = terminal_rows > LEADERBOARD_PANEL_MIN_ROWS ? 1 : 0;
		layout->rows = terminal_rows - layout->y;
		layout->cols = terminal_cols;
		layout->compact = true;
		return (true);
	}
	margin = min_int(LEADERBOARD_PANEL_ROW_MARGIN,
			(terminal_rows - LEADERBOARD_PANEL_MIN_ROWS) / 2);
	layout->rows = min_int(terminal_rows - margin * 2,
			LEADERBOARD_PANEL_MAX_ROWS);
	margin = min_int(LEADERBOARD_PANEL_SIDE_MARGIN,
			(terminal_cols - LEADERBOARD_PANEL_MIN_COLS) / 2);
	layout->cols = min_int(terminal_cols - margin * 2,
			LEADERBOARD_PANEL_MAX_COLS);
	layout->y = (terminal_rows - layout->rows) / 2;
	layout->x = (terminal_cols - layout->cols) / 2;
	layout->compact = layout->rows < 27 || layout->cols < 66;
	return (true);
}

/**
 * @brief Maps the authored 1448x1086 leaderboard contract into fitted pixels.
 */
void	leaderboard_pixel_layout_build(int origin_y, int origin_x,
	int rows, int cols, int cell_px_y, int cell_px_x,
	leaderboard_pixel_layout_t *layout)
{
	if (layout == NULL)
		return ;
	memset(layout, 0, sizeof(*layout));
	layout->origin_y = origin_y;
	layout->origin_x = origin_x;
	layout->rows = rows;
	layout->cols = cols;
	layout->cell_px_y = cell_px_y > 0 ? cell_px_y : 1;
	layout->cell_px_x = cell_px_x > 0 ? cell_px_x : 1;
	if (cols > 0 && cols <= INT_MAX / layout->cell_px_x)
		layout->pixel_width = cols * layout->cell_px_x;
	if (rows > 0 && rows <= INT_MAX / layout->cell_px_y)
		layout->pixel_height = rows * layout->cell_px_y;
	layout->buttons[LEADERBOARD_FOCUS_BACK] = map_pixel_rect(
			LEADERBOARD_REF_BACK_X, LEADERBOARD_REF_BUTTON_Y,
			LEADERBOARD_REF_BUTTON_WIDTH, LEADERBOARD_REF_BUTTON_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->buttons[LEADERBOARD_FOCUS_REFRESH] = map_pixel_rect(
			LEADERBOARD_REF_REFRESH_X, LEADERBOARD_REF_BUTTON_Y,
			LEADERBOARD_REF_BUTTON_WIDTH, LEADERBOARD_REF_BUTTON_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->controls = map_pixel_rect(LEADERBOARD_REF_CONTROLS_X,
			LEADERBOARD_REF_CONTROLS_Y, LEADERBOARD_REF_CONTROLS_WIDTH,
			LEADERBOARD_REF_CONTROLS_HEIGHT, layout->pixel_width,
			layout->pixel_height);
}

/**
 * @brief Resolves a terminal pointer position against pixel-rendered controls.
 */
bool	leaderboard_pixel_hit_test(const leaderboard_pixel_layout_t *layout,
	int input_y, int input_x, leaderboard_focus_t *focus)
{
	const leaderboard_pixel_rect_t	*rect;
	int64_t							pixel_y;
	int64_t							pixel_x;
	int								index;

	if (layout == NULL || focus == NULL || layout->cell_px_y <= 0
		|| layout->cell_px_x <= 0 || layout->pixel_width <= 0
		|| layout->pixel_height <= 0 || input_y < layout->origin_y
		|| input_x < layout->origin_x
		|| (int64_t)input_y >= (int64_t)layout->origin_y + layout->rows
		|| (int64_t)input_x >= (int64_t)layout->origin_x + layout->cols)
		return (false);
	pixel_y = (int64_t)(input_y - layout->origin_y) * layout->cell_px_y
		+ layout->cell_px_y / 2;
	pixel_x = (int64_t)(input_x - layout->origin_x) * layout->cell_px_x
		+ layout->cell_px_x / 2;
	index = 0;
	while (index < LEADERBOARD_PIXEL_BUTTON_COUNT)
	{
		rect = &layout->buttons[index];
		if (pixel_x >= rect->x && pixel_x < rect->x + rect->width
			&& pixel_y >= rect->y && pixel_y < rect->y + rect->height)
		{
			*focus = (leaderboard_focus_t)index;
			return (true);
		}
		index++;
	}
	return (false);
}

/**
 * @brief Finds a rank by its explicit position, independent of provider order.
 */
const app_leaderboard_entry_view_model_t	*leaderboard_entry_for_position(
	const app_leaderboard_view_model_t *leaderboard, int position)
{
	int	index;

	if (position < 1 || position > APP_LEADERBOARD_MAX_ENTRIES)
		return (NULL);
	index = 0;
	while (leaderboard != NULL && index < leaderboard->count
		&& index < APP_LEADERBOARD_MAX_ENTRIES)
	{
		if (leaderboard->entries[index].position == position)
			return (&leaderboard->entries[index]);
		index++;
	}
	return (NULL);
}

static int	min_int(int first, int second)
{
	if (first < second)
		return (first);
	return (second);
}

static leaderboard_pixel_rect_t	map_pixel_rect(int x, int y, int width,
	int height, int pixel_width, int pixel_height)
{
	leaderboard_pixel_rect_t	mapped;

	mapped.x = (int)((int64_t)x * pixel_width / LEADERBOARD_REF_WIDTH);
	mapped.y = (int)((int64_t)y * pixel_height / LEADERBOARD_REF_HEIGHT);
	mapped.width = (int)((int64_t)(x + width) * pixel_width
		/ LEADERBOARD_REF_WIDTH)
		- mapped.x;
	mapped.height = (int)((int64_t)(y + height) * pixel_height
		/ LEADERBOARD_REF_HEIGHT)
		- mapped.y;
	return (mapped);
}
