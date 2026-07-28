#include "tetrisbrain.h"

static void		refill_bag(t_piece_bag *bag);
static uint32_t	next_random(t_piece_bag *bag);

/**
 * @brief Initialises a 7-bag piece randomiser with a seed.
 *
 * @param bag Caller-owned bag state to initialise.
 * @param seed RNG seed; 0 is replaced with BAG_FALLBACK_SEED.
 */
void	piece_bag_init(t_piece_bag *bag, uint32_t seed)
{
	bag->rng_state = seed;
	if (bag->rng_state == 0)
		bag->rng_state = BAG_FALLBACK_SEED;
	refill_bag(bag);
}

/**
 * @brief Returns the next piece type drawn from the bag, refilling it first
 * if it's empty.
 *
 * @param bag Caller-owned bag state to draw from.
 * @return The next t_piece_type in the bag's current sequence.
 */
t_piece_type	piece_bag_next(t_piece_bag *bag)
{
	t_piece_type	next;

	if (bag->next >= BRAIN_BAG_SIZE)
		refill_bag(bag);
	next = bag->pieces[bag->next];
	bag->next++;
	return (next);
}

static void	refill_bag(t_piece_bag *bag)
{
	int				index;
	int				swap_index;
	t_piece_type	tmp;

	index = 0;
	while (index < BRAIN_BAG_SIZE)
	{
		bag->pieces[index] = (t_piece_type)index;
		index++;
	}
	index = BRAIN_BAG_SIZE - 1;
	while (index > 0)
	{
		swap_index = (int)(next_random(bag) % (uint32_t)(index + 1));
		tmp = bag->pieces[index];
		bag->pieces[index] = bag->pieces[swap_index];
		bag->pieces[swap_index] = tmp;
		index--;
	}
	bag->next = 0;
}

/* xorshift32 step; advances and returns the bag's caller-owned RNG state. */
static uint32_t	next_random(t_piece_bag *bag)
{
	uint32_t	x;

	x = bag->rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	bag->rng_state = x;
	return (x);
}
