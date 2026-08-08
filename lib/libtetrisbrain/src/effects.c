#include "tetrisbrain.h"

// Piece-counted status effects from the UC-14 Gaiden catalogue. The brain
// owns the canonical durations and the lock-time countdown so tetrisd's
// input checks and tetrisu's prediction agree on exactly which piece an
// effect expires on. Who gets targeted, when the server ends Dark/Pals/
// Mirror, and forwarding Fry's burned rows are tetrisd's business.

static void	effect_set(t_effect_state *state, t_status_effect effect, int v);

/**
 * @brief Initialises a status effect state to all-inactive.
 *
 * @param state Effect state to initialise.
 */
void	effect_state_init(t_effect_state *state)
{
	memset(state, 0, sizeof(*state));
}

// Shared by effect_apply/effect_clear: routes to the field an effect owns.
// v is an int, not a bool, since it's a piece-count for counter effects and
// a 0/1 flag (any nonzero = true) for the server-cleared ones.
static void	effect_set(t_effect_state *state, t_status_effect effect, int v)
{
	switch (effect)
	{
	case EFFECT_PARALYSIS:
		state->no_rotate_pieces = v;
		break;
	case EFFECT_INVERSION:
		state->inverted_pieces = v;
		break;
	case EFFECT_NUE:
		state->no_fastdrop_pieces = v;
		break;
	case EFFECT_THWACK:
		state->thwack_pieces = v;
		break;
	case EFFECT_FRY:
		state->fry_rows = v;
		break;
	case EFFECT_DARK:
		state->blackout = v;
		break;
	case EFFECT_PALS:
		state->pals = v;
		break;
	case EFFECT_MIRROR:
		state->mirror_armed = v;
		break;
	}
}

/**
 * @brief Activates a status effect, (re)starting its full duration.
 *
 * Paralysis/Inversion last 3 pieces, Nue/Thwack last 4, Fry burns 3 rows on
 * the next lock. Dark/Pals/Mirror stay on until effect_clear - the server
 * decides when they end.
 *
 * @param state Effect state to modify.
 * @param effect Status effect to activate.
 */
void	effect_apply(t_effect_state *state, t_status_effect effect)
{
	static const int	duration[] = {3, 3, 4, 4, 3, 1, 1, 1};

	effect_set(state, effect, duration[effect]);
}

/**
 * @brief Deactivates a status effect immediately.
 *
 * @param state Effect state to modify.
 * @param effect Status effect to deactivate.
 */
void	effect_clear(t_effect_state *state, t_status_effect effect)
{
	effect_set(state, effect, 0);
}

/**
 * @brief Advances every active piece-counted effect by one piece lock.
 *
 * Decrements Paralysis/Inversion/Nue/Thwack (never below zero) and consumes
 * Fry's pending burn. Read effect_fry_rows before calling this if the burn
 * still needs forwarding. Dark/Pals/Mirror are untouched - the server ends
 * those, not the lock counter.
 *
 * @param state Effect state to advance.
 */
void	effect_on_piece_lock(t_effect_state *state)
{
	if (state->no_rotate_pieces > 0)
		state->no_rotate_pieces--;
	if (state->inverted_pieces > 0)
		state->inverted_pieces--;
	if (state->no_fastdrop_pieces > 0)
		state->no_fastdrop_pieces--;
	if (state->thwack_pieces > 0)
		state->thwack_pieces--;
	state->fry_rows = 0;
}

/**
 * @brief Reports whether Paralysis is currently blocking rotation.
 *
 * @param state Effect state to query.
 * @return true if a Paralysis counter is still active, false otherwise.
 */
bool	effect_rotation_blocked(const t_effect_state *state)
{
	return (state->no_rotate_pieces > 0);
}

/**
 * @brief Reports whether Nue is currently blocking soft/hard drop.
 *
 * @param state Effect state to query.
 * @return true if a Nue counter is still active, false otherwise.
 */
bool	effect_fastdrop_blocked(const t_effect_state *state)
{
	return (state->no_fastdrop_pieces > 0);
}

/**
 * @brief Reports whether Inversion is currently active.
 *
 * @param state Effect state to query.
 * @return true if an Inversion counter is still active, false otherwise.
 */
bool	effect_controls_inverted(const t_effect_state *state)
{
	return (state->inverted_pieces > 0);
}

/**
 * @brief Reports whether Thwack is currently active.
 *
 * @param state Effect state to query.
 * @return true if a Thwack counter is still active, false otherwise.
 */
bool	effect_thwack_active(const t_effect_state *state)
{
	return (state->thwack_pieces > 0);
}

/**
 * @brief Reports how many rows Fry will burn on the next piece lock.
 *
 * @param state Effect state to query.
 * @return Number of pending Fry rows, or 0 if Fry is not active.
 */
int	effect_fry_rows(const t_effect_state *state)
{
	return (state->fry_rows);
}
