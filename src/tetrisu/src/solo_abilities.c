#include "tetrisu.h"

typedef struct s_solo_ability_definition
{
	const char	*name;
	const char	*description;
	int			cost;
}	solo_ability_definition_t;

static const solo_ability_definition_t	g_abilities[SOLO_ABILITY_COUNT] = {
	{"MIRURUN", "REMOVE BOTTOM FOUR ROWS", 2},
	{"INVERSION", "INVERT NEXT 3 CONTROLS", 4},
	{"PENTARIS", "SEND 5 GARBAGE LINES", 6},
	{"SIRTET", "INVERT OCCUPIED ROWS", 8}
};

static bool	ability_is_valid(solo_ability_t ability);
static solo_ability_result_t	remember_ability_result(solo_game_t *game,
									solo_ability_t ability,
									solo_ability_result_t result);
static int	mouse_subpixel(int event_pixel, int cell_pixels);

/**
 * @brief Returns the crystal cost of one Mirurun ability.
 *
 * @param ability Ability level to inspect.
 * @return Crystal cost, or -1 for an invalid ability.
 */
int	solo_ability_cost(solo_ability_t ability)
{
	if (!ability_is_valid(ability))
		return (-1);
	return (g_abilities[ability - 1].cost);
}

/**
 * @brief Returns the display name of one Mirurun ability.
 *
 * @param ability Ability level to inspect.
 * @return Static display name, or an empty string for an invalid ability.
 */
const char	*solo_ability_name(solo_ability_t ability)
{
	if (!ability_is_valid(ability))
		return ("");
	return (g_abilities[ability - 1].name);
}

/**
 * @brief Returns the short hover description of one Mirurun ability.
 *
 * @param ability Ability level to inspect.
 * @return Static description, or an empty string for an invalid ability.
 */
const char	*solo_ability_description(solo_ability_t ability)
{
	if (!ability_is_valid(ability))
		return ("");
	return (g_abilities[ability - 1].description);
}

/**
 * @brief Maps an ability cost to its evenly spaced meter marker.
 *
 * Cost thresholds 2, 4, 6, and 8 divide the ten-charge meter into equal
 * intervals while preserving one interval of margin at each end.
 *
 * @param ability Ability level to position.
 * @return Authored canvas y coordinate, or -1 for an invalid ability.
 */
int	solo_ability_center_y(solo_ability_t ability)
{
	int	cost;

	cost = solo_ability_cost(ability);
	if (cost < 0)
		return (-1);
	return (HUD_METER_Y + HUD_METER_HEIGHT
		- cost * HUD_METER_HEIGHT / SOLO_CRYSTAL_CAPACITY);
}

/**
 * @brief Hit-tests one authored canvas point against generous ability targets.
 *
 * AI-assisted: hitboxes are deliberately larger than the 24-pixel circles so
 * scaled terminal mouse coordinates remain forgiving without overlapping.
 *
 * @param x Authored 512x384 canvas x coordinate.
 * @param y Authored 512x384 canvas y coordinate.
 * @return Hit ability, or SOLO_ABILITY_NONE outside every target.
 */
solo_ability_t	solo_ability_at_canvas(int x, int y)
{
	solo_ability_t	ability;
	int				center_x;
	int				center_y;

	center_x = HUD_METER_X + HUD_METER_WIDTH / 2;
	ability = SOLO_ABILITY_MIRURUN;
	while (ability <= SOLO_ABILITY_SIRTET)
	{
		center_y = solo_ability_center_y(ability);
		if (abs(x - center_x) <= SOLO_ABILITY_HITBOX_HALF_WIDTH
			&& abs(y - center_y) <= SOLO_ABILITY_HITBOX_HALF_HEIGHT)
			return (ability);
		ability++;
	}
	return (SOLO_ABILITY_NONE);
}

/**
 * @brief Converts an absolute terminal mouse event into authored coordinates.
 *
 * Cell coordinates are combined with Notcurses' optional within-cell pixel
 * offsets. Missing offsets use the cell centre, keeping hit-testing stable on
 * terminals that only report cell-granular mouse positions.
 *
 * @param ctx Render context containing physical terminal-cell dimensions.
 * @param solo Current fitted Solo canvas geometry.
 * @param input Notcurses mouse event.
 * @param canvas_x Output authored x coordinate.
 * @param canvas_y Output authored y coordinate.
 * @return true inside the fitted canvas, otherwise false.
 */
bool	solo_mouse_canvas_position(const render_ctx_t *ctx,
	const solo_render_t *solo, const ncinput *input,
	int *canvas_x, int *canvas_y)
{
	int64_t	physical_x;
	int64_t	physical_y;
	int		cell_x;
	int		cell_y;

	if (ctx == NULL || solo == NULL || input == NULL
		|| canvas_x == NULL || canvas_y == NULL || !solo->layout_valid
		|| ctx->cell_px_x <= 0 || ctx->cell_px_y <= 0
		|| solo->canvas_cols <= 0 || solo->content_rows <= 0)
		return (false);
	cell_x = input->x - solo->canvas_col;
	cell_y = input->y - solo->canvas_row;
	if (cell_x < 0 || cell_y < 0 || cell_x >= solo->canvas_cols
		|| cell_y >= solo->content_rows)
		return (false);
	physical_x = (int64_t)cell_x * ctx->cell_px_x
		+ mouse_subpixel(input->xpx, ctx->cell_px_x);
	physical_y = (int64_t)cell_y * ctx->cell_px_y
		+ mouse_subpixel(input->ypx, ctx->cell_px_y);
	*canvas_x = (int)(physical_x * SOLO_CANVAS_WIDTH
		/ ((int64_t)solo->canvas_cols * ctx->cell_px_x));
	*canvas_y = (int)(physical_y * SOLO_CONTENT_HEIGHT
		/ ((int64_t)solo->content_rows * ctx->cell_px_y));
	return (true);
}

/**
 * @brief Activates one Mirurun ability in temporary local Solo authority.
 *
 * AI-assisted: level one applies the existing pure board transform to a copy
 * first, so an active-piece collision rejects atomically. Opponent-targeted
 * levels spend charge and report a test activation without mutating Solo.
 *
 * @param game Solo game state to mutate.
 * @param ability Ability level selected by key or mouse.
 * @return Activation or rejection result also stored for HUD feedback.
 */
solo_ability_result_t	solo_game_activate_ability(solo_game_t *game,
	solo_ability_t ability)
{
	t_board	transformed;
	int		cost;

	if (game == NULL || !ability_is_valid(ability))
		return (SOLO_ABILITY_RESULT_INVALID);
	if (game->paused || game->countdown_active
		|| game->phase != SOLO_ACTIVE)
		return (remember_ability_result(game, ability,
				SOLO_ABILITY_RESULT_UNAVAILABLE));
	cost = solo_ability_cost(ability);
	if (game->crystal_charge < cost)
		return (remember_ability_result(game, ability,
				SOLO_ABILITY_RESULT_NO_CHARGE));
	if (ability == SOLO_ABILITY_MIRURUN)
	{
		transformed = game->board;
		board_cut_bottom(&transformed, 4);
		if (!piece_is_valid(&transformed, &game->active))
			return (remember_ability_result(game, ability,
					SOLO_ABILITY_RESULT_BLOCKED));
		game->board = transformed;
	}
	game->crystal_charge -= cost;
	return (remember_ability_result(game, ability,
			SOLO_ABILITY_RESULT_ACTIVATED));
}

/**
 * @brief Validates an ability before indexing the fixed metadata table.
 *
 * @param ability Ability identifier to validate.
 * @return true only for levels one through four.
 */
static bool	ability_is_valid(solo_ability_t ability)
{
	return (ability >= SOLO_ABILITY_MIRURUN
		&& ability <= SOLO_ABILITY_SIRTET);
}

/**
 * @brief Stores one bounded-time activation response for the HUD.
 *
 * @param game Solo game receiving feedback state.
 * @param ability Ability associated with the result.
 * @param result Activation result to display.
 * @return The supplied result.
 */
static solo_ability_result_t	remember_ability_result(solo_game_t *game,
	solo_ability_t ability, solo_ability_result_t result)
{
	game->last_ability = ability;
	game->ability_result = result;
	game->ability_feedback_elapsed_ms = 0;
	if (result == SOLO_ABILITY_RESULT_ACTIVATED)
		game->pending_events |= SOLO_EVENT_ABILITY_ACTIVATED;
	else if (result != SOLO_ABILITY_RESULT_INVALID)
		game->pending_events |= SOLO_EVENT_ABILITY_REJECTED;
	return (result);
}

/**
 * @brief Normalizes optional mouse pixel precision to a cell-local offset.
 *
 * @param event_pixel Reported offset, or a negative value when unavailable.
 * @param cell_pixels Physical size of the terminal cell on this axis.
 * @return Safe offset within the cell.
 */
static int	mouse_subpixel(int event_pixel, int cell_pixels)
{
	if (event_pixel < 0 || event_pixel >= cell_pixels)
		return (cell_pixels / 2);
	return (event_pixel);
}
