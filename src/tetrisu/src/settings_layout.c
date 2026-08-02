#include "tetrisu.h"

static settings_rect_t	map_rect(int x, int y, int width, int height,
		int pixel_width, int pixel_height);
static bool	point_in_rect(const settings_layout_t *layout,
		const settings_rect_t *rect, int terminal_y, int terminal_x);

static settings_rect_t	map_rect(int x, int y, int width, int height,
	int pixel_width, int pixel_height)
{
	settings_rect_t	mapped;

	mapped.x = x * pixel_width / SETTINGS_REFERENCE_WIDTH;
	mapped.y = y * pixel_height / SETTINGS_REFERENCE_HEIGHT;
	mapped.width = (x + width) * pixel_width / SETTINGS_REFERENCE_WIDTH
		- mapped.x;
	mapped.height = (y + height) * pixel_height / SETTINGS_REFERENCE_HEIGHT
		- mapped.y;
	return (mapped);
}

/**
 * @brief Maps the approved 1448x1086 art contract into fitted pixel space.
 */
void	settings_layout_build(int origin_y, int origin_x, int rows, int cols,
	int cell_px_y, int cell_px_x, settings_layout_t *layout)
{
	int	pixel_width;
	int	pixel_height;

	if (layout == NULL)
		return ;
	memset(layout, 0, sizeof(*layout));
	layout->origin_y = origin_y;
	layout->origin_x = origin_x;
	layout->rows = rows;
	layout->cols = cols;
	layout->cell_px_x = cell_px_x > 0 ? cell_px_x : 1;
	layout->cell_px_y = cell_px_y > 0 ? cell_px_y : 1;
	pixel_width = cols > 0 ? cols * layout->cell_px_x : 1;
	pixel_height = rows > 0 ? rows * layout->cell_px_y : 1;
	layout->pixel_width = pixel_width;
	layout->pixel_height = pixel_height;
	layout->portrait = map_rect(SETTINGS_REF_PORTRAIT_X,
		SETTINGS_REF_PORTRAIT_Y, SETTINGS_REF_PORTRAIT_WIDTH,
		SETTINGS_REF_PORTRAIT_HEIGHT, pixel_width, pixel_height);
	layout->profile = map_rect(SETTINGS_REF_PROFILE_X,
		SETTINGS_REF_PROFILE_Y, SETTINGS_REF_PROFILE_WIDTH,
		SETTINGS_REF_PROFILE_HEIGHT, pixel_width, pixel_height);
	layout->characters = map_rect(SETTINGS_REF_CHARACTERS_X,
		SETTINGS_REF_CHARACTERS_Y, SETTINGS_REF_CHARACTERS_WIDTH,
		SETTINGS_REF_CHARACTERS_HEIGHT, pixel_width, pixel_height);
	layout->themes = map_rect(SETTINGS_REF_THEMES_X, SETTINGS_REF_THEMES_Y,
		SETTINGS_REF_THEMES_WIDTH, SETTINGS_REF_THEMES_HEIGHT, pixel_width,
		pixel_height);
	layout->stats[0] = map_rect(SETTINGS_REF_WALLET_X, SETTINGS_REF_WALLET_Y,
		SETTINGS_REF_WALLET_WIDTH, SETTINGS_REF_WALLET_HEIGHT, pixel_width,
		pixel_height);
	layout->stats[1] = map_rect(SETTINGS_REF_SCORE_X, SETTINGS_REF_SCORE_Y,
		SETTINGS_REF_SCORE_WIDTH, SETTINGS_REF_SCORE_HEIGHT, pixel_width,
		pixel_height);
	layout->stats[2] = map_rect(SETTINGS_REF_RANK_X, SETTINGS_REF_RANK_Y,
		SETTINGS_REF_RANK_WIDTH, SETTINGS_REF_RANK_HEIGHT, pixel_width,
		pixel_height);
	layout->buttons[0] = map_rect(SETTINGS_REF_BUTTON_BACK_X,
		SETTINGS_REF_BUTTON_Y, SETTINGS_REF_BUTTON_WIDTH,
		SETTINGS_REF_BUTTON_HEIGHT, pixel_width, pixel_height);
	layout->buttons[1] = map_rect(SETTINGS_REF_BUTTON_MARKET_X,
		SETTINGS_REF_BUTTON_Y, SETTINGS_REF_BUTTON_WIDTH,
		SETTINGS_REF_BUTTON_HEIGHT, pixel_width, pixel_height);
	layout->buttons[2] = map_rect(SETTINGS_REF_BUTTON_VOLUME_DOWN_X,
		SETTINGS_REF_BUTTON_Y, SETTINGS_REF_BUTTON_WIDTH,
		SETTINGS_REF_BUTTON_HEIGHT, pixel_width, pixel_height);
	layout->buttons[3] = map_rect(SETTINGS_REF_BUTTON_VOLUME_UP_X,
		SETTINGS_REF_BUTTON_Y, SETTINGS_REF_BUTTON_WIDTH,
		SETTINGS_REF_BUTTON_HEIGHT, pixel_width, pixel_height);
	layout->character_arrows[0] = map_rect(SETTINGS_REF_CHARACTER_PREVIOUS_X,
		SETTINGS_REF_CHARACTER_ARROW_Y, SETTINGS_REF_CHARACTER_ARROW_WIDTH,
		SETTINGS_REF_CHARACTER_ARROW_HEIGHT, pixel_width, pixel_height);
	layout->character_arrows[1] = map_rect(SETTINGS_REF_CHARACTER_NEXT_X,
		SETTINGS_REF_CHARACTER_ARROW_Y, SETTINGS_REF_CHARACTER_ARROW_WIDTH,
		SETTINGS_REF_CHARACTER_ARROW_HEIGHT, pixel_width, pixel_height);
}

/**
 * @brief Tests the same cell bounds that contain the rendered bitmap buttons.
 */
bool	settings_layout_hit_test(const settings_layout_t *layout,
	int terminal_y, int terminal_x, int *button_index)
{
	int	index;

	if (layout == NULL || button_index == NULL || layout->rows <= 0
		|| layout->cols <= 0)
		return (false);
	index = 0;
	while (index < SETTINGS_BUTTON_COUNT)
	{
		if (point_in_rect(layout, &layout->buttons[index], terminal_y,
				terminal_x))
		{
			*button_index = index;
			return (true);
		}
		index++;
	}
	index = 0;
	while (index < SETTINGS_CHARACTER_ARROW_COUNT)
	{
		if (point_in_rect(layout, &layout->character_arrows[index],
				terminal_y, terminal_x))
		{
			*button_index = SETTINGS_BUTTON_COUNT + index;
			return (true);
		}
		index++;
	}
	return (false);
}

static bool	point_in_rect(const settings_layout_t *layout,
	const settings_rect_t *rect, int terminal_y, int terminal_x)
{
	int	left;
	int	top;
	int	right;
	int	bottom;

	left = layout->origin_x + rect->x / layout->cell_px_x;
	top = layout->origin_y + rect->y / layout->cell_px_y;
	right = layout->origin_x + (rect->x + rect->width
		+ layout->cell_px_x - 1) / layout->cell_px_x;
	bottom = layout->origin_y + (rect->y + rect->height
		+ layout->cell_px_y - 1) / layout->cell_px_y;
	return (terminal_x >= left && terminal_x < right
		&& terminal_y >= top && terminal_y < bottom);
}
