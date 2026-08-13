#include "tetrisbrain.h"

// UC-14 ability-charge meter: 2 cleared lines = 1 charge; levels 1-4 cost
// 2/4/6/8. Ownership/targeting/HTTTP mapping live in tetrisd/ability_ctrl.c.

/**
 * @brief Resets a charge meter to zero charges and zero banked lines.
 *
 * @param state The charge state to initialise.
 */
void	charge_state_init(t_charge_state *state)
{
	memset(state, 0, sizeof(*state));
}

/**
 * @brief Banks cleared lines into charge, carrying odd lines as remainder.
 *
 * @param state The charge state to update.
 * @param lines_cleared Number of lines cleared by the last piece lock.
 */
void	charge_on_clear(t_charge_state *state, int lines_cleared)
{
	if (lines_cleared <= 0)
		return ;
	state->line_remainder += lines_cleared;
	state->charges += state->line_remainder / LINES_PER_CHARGE;
	state->line_remainder %= LINES_PER_CHARGE;
}

/**
 * @brief Computes the charge cost for an ability level (2/4/6/8 for 1-4).
 *
 * @param level Ability level to price.
 * @return The charge cost for a valid level, or -1 if out of range.
 */
int	ability_cost(int level)
{
	if (level < 1 || level > 4)
		return (-1);
	return (level * 2);
}

/**
 * @brief Checks whether a charge state can afford an ability level.
 *
 * @param state The charge state to check.
 * @param level Ability level being considered.
 * @return true if level is valid and state holds enough charge.
 */
bool	charge_can_afford(const t_charge_state *state, int level)
{
	int	cost;

	cost = ability_cost(level);
	return (cost >= 0 && state->charges >= cost);
}

/**
 * @brief Deducts an ability's cost if affordable; else consumes nothing
 * (UC-14 ext 4a).
 *
 * @param state The charge state to deduct from.
 * @param level Ability level being activated.
 * @return true if the cost was deducted, false otherwise.
 */
bool	charge_deduct(t_charge_state *state, int level)
{
	int	cost;

	cost = ability_cost(level);
	if (cost < 0 || state->charges < cost)
		return (false);
	state->charges -= cost;
	return (true);
}

/**
 * @brief Moves all whole charges from one state to another (Halloween
 * "Vampire"); each side keeps its own line remainder.
 *
 * @param from The charge state losing its charges (zeroed on return).
 * @param to The charge state receiving the transferred charges.
 */
void	charge_transfer(t_charge_state *from, t_charge_state *to)
{
	to->charges += from->charges;
	from->charges = 0;
}
