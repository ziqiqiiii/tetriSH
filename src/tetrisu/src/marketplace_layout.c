#include "tetrisu.h"

static marketplace_rect_t	map_rect(int x, int y, int width, int height,
				int pixel_width, int pixel_height);

static marketplace_rect_t	map_rect(int x, int y, int width, int height,
	int pixel_width, int pixel_height)
{
	marketplace_rect_t	mapped;

	mapped.x = x * pixel_width / MARKETPLACE_REFERENCE_WIDTH;
	mapped.y = y * pixel_height / MARKETPLACE_REFERENCE_HEIGHT;
	mapped.width = (x + width) * pixel_width / MARKETPLACE_REFERENCE_WIDTH
		- mapped.x;
	mapped.height = (y + height) * pixel_height / MARKETPLACE_REFERENCE_HEIGHT
		- mapped.y;
	return (mapped);
}

/**
 * @brief Maps the 1448x1086 Marketplace art contract into fitted pixel space.
 *
 * Every rectangle the renderer and the tests use comes from here, so a change
 * to the reference contract cannot leave the two disagreeing about where a
 * region plane lands.
 */
void	marketplace_layout_build(int origin_y, int origin_x, int rows, int cols,
	int cell_px_y, int cell_px_x, marketplace_layout_t *layout)
{
	int	pixel_width;
	int	pixel_height;
	int	index;

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
	layout->title = map_rect(MARKETPLACE_REF_CONTENT_X, MARKETPLACE_REF_TITLE_Y,
			MARKETPLACE_REF_CONTENT_WIDTH, MARKETPLACE_REF_TITLE_HEIGHT,
			pixel_width, pixel_height);
	layout->wallet_card = map_rect(MARKETPLACE_REF_STATS_X,
			MARKETPLACE_REF_STATS_Y, MARKETPLACE_REF_STAT_WIDTH,
			MARKETPLACE_REF_STATS_HEIGHT, pixel_width, pixel_height);
	index = 0;
	while (index < 3)
	{
		layout->stats[index] = map_rect(MARKETPLACE_REF_STATS_X
				+ index * MARKETPLACE_REF_STAT_STEP_X, MARKETPLACE_REF_STATS_Y,
				MARKETPLACE_REF_STAT_WIDTH, MARKETPLACE_REF_STATS_HEIGHT,
				pixel_width, pixel_height);
		index++;
	}
	layout->characters = map_rect(MARKETPLACE_REF_CHARACTERS_X,
			MARKETPLACE_REF_CHARACTERS_Y, MARKETPLACE_REF_CHARACTERS_WIDTH,
			MARKETPLACE_REF_CHARACTERS_HEIGHT, pixel_width, pixel_height);
	layout->themes = map_rect(MARKETPLACE_REF_THEMES_X,
			MARKETPLACE_REF_THEMES_Y, MARKETPLACE_REF_THEMES_WIDTH,
			MARKETPLACE_REF_THEMES_HEIGHT, pixel_width, pixel_height);
	layout->detail = map_rect(MARKETPLACE_REF_DETAIL_X,
			MARKETPLACE_REF_DETAIL_Y, MARKETPLACE_REF_DETAIL_WIDTH,
			MARKETPLACE_REF_DETAIL_HEIGHT, pixel_width, pixel_height);
	layout->controls = map_rect(MARKETPLACE_REF_CONTROLS_X,
			MARKETPLACE_REF_CONTROLS_Y, MARKETPLACE_REF_CONTROLS_WIDTH,
			MARKETPLACE_REF_CONTROLS_HEIGHT, pixel_width, pixel_height);
	index = 0;
	while (index < MARKETPLACE_BUTTON_COUNT)
	{
		layout->buttons[index] = map_rect(MARKETPLACE_REF_BUTTON_FIRST_X
				+ index * MARKETPLACE_REF_BUTTON_STEP_X,
				MARKETPLACE_REF_BUTTON_Y, MARKETPLACE_REF_BUTTON_WIDTH,
				MARKETPLACE_REF_BUTTON_HEIGHT, pixel_width, pixel_height);
		index++;
	}
}
