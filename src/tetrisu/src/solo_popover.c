#include "tetrisu.h"

static void	start_fade(solo_render_t *solo, solo_popover_phase_t phase);
static int	fade_opacity(const solo_render_t *solo, int duration_ms);

/**
 * @brief Changes the pointer-selected ability and starts the matching fade.
 *
 * Active activation/rejection feedback owns the popover until its one-second
 * lifetime expires. Hover changes are still remembered so the correct help
 * can take over afterward.
 *
 * @param solo Solo renderer containing popover presentation state.
 * @param ability Newly hovered ability, or SOLO_ABILITY_NONE on pointer exit.
 * @return true when visible or pending presentation state changed.
 */
bool	solo_popover_set_hover(solo_render_t *solo, solo_ability_t ability)
{
	if (solo == NULL)
		return (false);
	if (ability < SOLO_ABILITY_NONE || ability > SOLO_ABILITY_SIRTET)
		ability = SOLO_ABILITY_NONE;
	if (solo->hovered_ability == ability)
		return (false);
	solo->hovered_ability = ability;
	if (solo->popover_feedback_active)
		return (true);
	if (ability != SOLO_ABILITY_NONE)
	{
		solo->popover_ability = ability;
		if (solo->popover_opacity >= 255)
			solo->popover_phase = SOLO_POPOVER_VISIBLE;
		else
			start_fade(solo, SOLO_POPOVER_FADING_IN);
	}
	else if (solo->popover_opacity > 0)
		start_fade(solo, SOLO_POPOVER_FADING_OUT);
	else
	{
		solo->popover_phase = SOLO_POPOVER_HIDDEN;
		solo->popover_ability = SOLO_ABILITY_NONE;
	}
	return (true);
}

/**
 * @brief Advances hover fades and synchronizes priority feedback.
 *
 * @param solo Solo renderer containing popover presentation state.
 * @param game Current Solo game state.
 * @param elapsed_ms Elapsed monotonic milliseconds.
 * @return true when a redraw is required.
 */
bool	solo_popover_update(solo_render_t *solo, const solo_game_t *game,
	int elapsed_ms)
{
	int		before;
	bool	feedback;
	bool	changed;

	if (solo == NULL || game == NULL || elapsed_ms < 0)
		return (false);
	feedback = game->ability_result != SOLO_ABILITY_RESULT_NONE;
	if (feedback)
	{
		before = solo->popover_opacity;
		changed = !solo->popover_feedback_active
			|| solo->popover_ability != game->last_ability
			|| before != (int)solo_game_ability_result_opacity(game);
		solo->popover_feedback_active = true;
		solo->popover_ability = game->last_ability;
		solo->popover_phase = SOLO_POPOVER_VISIBLE;
		solo->popover_opacity
			= (int)solo_game_ability_result_opacity(game);
		solo->popover_fade_start_opacity = solo->popover_opacity;
		solo->popover_fade_elapsed_ms = 0;
		return (changed);
	}
	if (solo->popover_feedback_active)
	{
		solo->popover_feedback_active = false;
		solo->popover_ability = solo->hovered_ability;
		if (solo->hovered_ability != SOLO_ABILITY_NONE)
		{
			if (solo->popover_opacity >= 255)
				solo->popover_phase = SOLO_POPOVER_VISIBLE;
			else
				start_fade(solo, SOLO_POPOVER_FADING_IN);
		}
		else if (solo->popover_opacity > 0)
			start_fade(solo, SOLO_POPOVER_FADING_OUT);
		else
		{
			solo->popover_phase = SOLO_POPOVER_HIDDEN;
			solo->popover_ability = SOLO_ABILITY_NONE;
		}
		return (true);
	}
	if (solo->popover_phase != SOLO_POPOVER_FADING_IN
		&& solo->popover_phase != SOLO_POPOVER_FADING_OUT)
		return (false);
	before = solo->popover_opacity;
	solo->popover_fade_elapsed_ms += elapsed_ms;
	if (solo->popover_phase == SOLO_POPOVER_FADING_IN)
	{
		solo->popover_opacity = fade_opacity(solo,
				SOLO_POPOVER_FADE_IN_MS);
		if (solo->popover_fade_elapsed_ms >= SOLO_POPOVER_FADE_IN_MS)
			solo->popover_phase = SOLO_POPOVER_VISIBLE;
	}
	else
	{
		solo->popover_opacity = fade_opacity(solo,
				SOLO_POPOVER_FADE_OUT_MS);
		if (solo->popover_fade_elapsed_ms >= SOLO_POPOVER_FADE_OUT_MS)
		{
			solo->popover_phase = SOLO_POPOVER_HIDDEN;
			solo->popover_opacity = 0;
			solo->popover_ability = SOLO_ABILITY_NONE;
		}
	}
	return (before != solo->popover_opacity);
}

/**
 * @brief Returns the next animation-frame deadline for an active fade.
 */
int	solo_popover_next_wake_ms(const solo_render_t *solo)
{
	int	duration_ms;
	int	remaining_ms;

	if (solo == NULL || (solo->popover_phase != SOLO_POPOVER_FADING_IN
			&& solo->popover_phase != SOLO_POPOVER_FADING_OUT))
		return (-1);
	duration_ms = SOLO_POPOVER_FADE_OUT_MS;
	if (solo->popover_phase == SOLO_POPOVER_FADING_IN)
		duration_ms = SOLO_POPOVER_FADE_IN_MS;
	remaining_ms = duration_ms - solo->popover_fade_elapsed_ms;
	if (remaining_ms < 0)
		remaining_ms = 0;
	if (remaining_ms < SOLO_RENDER_INTERVAL_MS)
		return (remaining_ms);
	return (SOLO_RENDER_INTERVAL_MS);
}

/**
 * @brief Resolves the content owner, giving game feedback absolute priority.
 */
solo_ability_t	solo_popover_displayed_ability(const solo_render_t *solo,
	const solo_game_t *game)
{
	if (game != NULL
		&& game->ability_result != SOLO_ABILITY_RESULT_NONE)
		return (game->last_ability);
	if (solo == NULL || solo->popover_opacity <= 0)
		return (SOLO_ABILITY_NONE);
	return (solo->popover_ability);
}

/**
 * @brief Resolves the visual opacity, keeping feedback fully legible.
 */
int	solo_popover_displayed_opacity(const solo_render_t *solo,
	const solo_game_t *game)
{
	if (game != NULL
		&& game->ability_result != SOLO_ABILITY_RESULT_NONE)
		return ((int)solo_game_ability_result_opacity(game));
	if (solo == NULL)
		return (0);
	return (solo->popover_opacity);
}

/**
 * @brief Starts one linear fade from the current opacity.
 */
static void	start_fade(solo_render_t *solo, solo_popover_phase_t phase)
{
	solo->popover_phase = phase;
	solo->popover_fade_start_opacity = solo->popover_opacity;
	solo->popover_fade_elapsed_ms = 0;
}

/**
 * @brief Calculates linear opacity for the current fade direction.
 */
static int	fade_opacity(const solo_render_t *solo, int duration_ms)
{
	int	elapsed_ms;

	elapsed_ms = solo->popover_fade_elapsed_ms;
	if (elapsed_ms > duration_ms)
		elapsed_ms = duration_ms;
	if (solo->popover_phase == SOLO_POPOVER_FADING_IN)
		return (solo->popover_fade_start_opacity
			+ (255 - solo->popover_fade_start_opacity)
			* elapsed_ms / duration_ms);
	return (solo->popover_fade_start_opacity
		- solo->popover_fade_start_opacity * elapsed_ms / duration_ms);
}
