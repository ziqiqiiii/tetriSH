#include "tetrisu.h"

// Static Functions
static mp_rect_t	map_rect(int x, int y, int width, int height,
						int pixel_width, int pixel_height);
static void	build_mode(mp_layout_t *layout);
static void	build_lobby(mp_layout_t *layout);
static void	build_create_room(mp_layout_t *layout);
static void	build_waiting_room(mp_layout_t *layout);

static mp_rect_t	map_rect(int x, int y, int width, int height,
	int pixel_width, int pixel_height)
{
	mp_rect_t	mapped;

	mapped.x = x * pixel_width / MULTIPLAYER_REFERENCE_WIDTH;
	mapped.y = y * pixel_height / MULTIPLAYER_REFERENCE_HEIGHT;
	mapped.width = (x + width) * pixel_width / MULTIPLAYER_REFERENCE_WIDTH
		- mapped.x;
	mapped.height = (y + height) * pixel_height / MULTIPLAYER_REFERENCE_HEIGHT
		- mapped.y;
	return (mapped);
}

/**
 * @brief Maps one multiplayer screen's art contract into fitted pixel space.
 *
 * All four screens share this function so the renderer and the tests read the
 * same rectangles; a change to the reference contract cannot leave the two
 * disagreeing about where a region plane lands. Rectangles a screen does not
 * use stay zeroed, which is what the disjointness sweep skips on.
 *
 * @param screen Which multiplayer surface to lay out.
 * @param origin_y Backdrop origin row.
 * @param origin_x Backdrop origin column.
 * @param rows Backdrop height in cells.
 * @param cols Backdrop width in cells.
 * @param cell_px_y Cell height in pixels.
 * @param cell_px_x Cell width in pixels.
 * @param layout Destination geometry.
 */
void	mp_layout_build(app_screen_t screen, int origin_y, int origin_x,
	int rows, int cols, int cell_px_y, int cell_px_x, mp_layout_t *layout)
{
	if (layout == NULL)
		return ;
	memset(layout, 0, sizeof(*layout));
	layout->origin_y = origin_y;
	layout->origin_x = origin_x;
	layout->rows = rows;
	layout->cols = cols;
	layout->cell_px_x = cell_px_x > 0 ? cell_px_x : 1;
	layout->cell_px_y = cell_px_y > 0 ? cell_px_y : 1;
	layout->pixel_width = cols > 0 ? cols * layout->cell_px_x : 1;
	layout->pixel_height = rows > 0 ? rows * layout->cell_px_y : 1;
	if (screen == APP_SCREEN_MULTIPLAYER_MODE)
		build_mode(layout);
	else if (screen == APP_SCREEN_LOBBY)
		build_lobby(layout);
	else if (screen == APP_SCREEN_CREATE_ROOM_MODAL)
		build_create_room(layout);
	else if (screen == APP_SCREEN_WAITING_ROOM)
		build_waiting_room(layout);
}

/**
 * @brief Lays out the mode picker: one opaque panel over the home artwork.
 *
 * The home backdrop is busy across its whole upper half, so the panel is drawn
 * opaque and sits low where the art is plain. Only the card strip is a region;
 * everything else on the screen is settled before it opens.
 */
static void	build_mode(mp_layout_t *layout)
{
	int	index;

	layout->panel = map_rect(MP_MODE_REF_PANEL_X, MP_MODE_REF_PANEL_Y,
			MP_MODE_REF_PANEL_WIDTH, MP_MODE_REF_PANEL_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->cards = map_rect(MP_MODE_REF_CARDS_X, MP_MODE_REF_CARDS_Y,
			MP_MODE_REF_CARDS_WIDTH, MP_MODE_REF_REGION_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	index = 0;
	while (index < MP_MODE_CARD_COUNT)
	{
		layout->card_slots[index] = map_rect(MP_MODE_REF_CARDS_X
				+ index * MP_MODE_REF_CARD_STEP_X, MP_MODE_REF_CARDS_Y,
				MP_MODE_REF_CARD_WIDTH, MP_MODE_REF_CARDS_HEIGHT,
				layout->pixel_width, layout->pixel_height);
		index++;
	}
	layout->controls = map_rect(MP_MODE_REF_PANEL_X, MP_MODE_REF_CONTROLS_Y,
			MP_MODE_REF_PANEL_WIDTH, MP_MODE_REF_CONTROLS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
}

/**
 * @brief Lays out the lobby: room table, join field, and a status line.
 *
 * Each dynamic panel owns its complete plate. Bitmap region crops expand to
 * terminal-cell boundaries, so keeping a border on a separate plane lets the
 * crop overwrite it at coarse cell sizes. Full-panel ownership prevents that.
 */
static void	build_lobby(mp_layout_t *layout)
{
	layout->rooms_plate = map_rect(LOBBY_REF_ROOMS_X, LOBBY_REF_ROOMS_Y,
			LOBBY_REF_ROOMS_WIDTH, LOBBY_REF_ROOMS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->list = layout->rooms_plate;
	layout->join_plate = map_rect(LOBBY_REF_JOIN_X, LOBBY_REF_JOIN_Y,
			LOBBY_REF_JOIN_WIDTH, LOBBY_REF_JOIN_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->field = layout->join_plate;
	layout->status = map_rect(LOBBY_REF_STATUS_X, LOBBY_REF_STATUS_Y,
			LOBBY_REF_STATUS_WIDTH, LOBBY_REF_STATUS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->controls = map_rect(LOBBY_REF_CONTENT_X, LOBBY_REF_CONTROLS_Y,
			LOBBY_REF_CONTENT_WIDTH, LOBBY_REF_CONTROLS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
}

/**
 * @brief Lays out the create-room panel: one plate, one region of options.
 */
static void	build_create_room(mp_layout_t *layout)
{
	layout->panel = map_rect(CREATE_REF_PANEL_X, CREATE_REF_PANEL_Y,
			CREATE_REF_PANEL_WIDTH, CREATE_REF_PANEL_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->options = map_rect(CREATE_REF_OPTIONS_X, CREATE_REF_OPTIONS_Y,
			CREATE_REF_OPTIONS_WIDTH, CREATE_REF_OPTIONS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->controls = map_rect(CREATE_REF_PANEL_X, CREATE_REF_CONTROLS_Y,
			CREATE_REF_PANEL_WIDTH, CREATE_REF_CONTROLS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
}

/**
 * @brief Lays out the waiting room: seats and status left, chat column right.
 *
 * Three regions. Seats and chat own their complete plates so cell-expanded
 * bitmap crops cannot erase the frame drawn by a different plane.
 */
static void	build_waiting_room(mp_layout_t *layout)
{
	layout->slots_plate = map_rect(ROOM_REF_SLOTS_PLATE_X,
			ROOM_REF_SLOTS_PLATE_Y, ROOM_REF_SLOTS_PLATE_WIDTH,
			ROOM_REF_SLOTS_PLATE_HEIGHT, layout->pixel_width,
			layout->pixel_height);
	layout->slots = layout->slots_plate;
	layout->status = map_rect(ROOM_REF_STATUS_X, ROOM_REF_STATUS_Y,
			ROOM_REF_STATUS_WIDTH, ROOM_REF_STATUS_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->chat_plate = map_rect(ROOM_REF_CHAT_PLATE_X, ROOM_REF_CHAT_PLATE_Y,
			ROOM_REF_CHAT_PLATE_WIDTH, ROOM_REF_CHAT_PLATE_HEIGHT,
			layout->pixel_width, layout->pixel_height);
	layout->chat = layout->chat_plate;
	layout->controls = map_rect(ROOM_REF_CONTENT_X, ROOM_REF_CONTROLS_Y,
			MULTIPLAYER_REFERENCE_WIDTH - 2 * ROOM_REF_CONTENT_X,
			ROOM_REF_CONTROLS_HEIGHT, layout->pixel_width,
			layout->pixel_height);
}
