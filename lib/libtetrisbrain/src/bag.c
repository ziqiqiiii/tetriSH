#include "tetrisbrain.h"

#define BAG_FALLBACK_SEED 0x6D2B79F5u

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

/* AI-assisted: shuffles caller-owned state so the pure library has no global
 * RNG and tetrisd can later reproduce a room's sequence from its seed. */
static void	refill_bag(t_piece_bag *bag)
{
	int				index;
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
		int swap_index = (int)(next_random(bag) % (uint32_t)(index + 1));

		tmp = bag->pieces[index];
		bag->pieces[index] = bag->pieces[swap_index];
		bag->pieces[swap_index] = tmp;
		index--;
	}
	bag->next = 0;
}

void	piece_bag_init(t_piece_bag *bag, uint32_t seed)
{
	bag->rng_state = seed;
	if (bag->rng_state == 0)
		bag->rng_state = BAG_FALLBACK_SEED;
	refill_bag(bag);
}

t_piece_type	piece_bag_next(t_piece_bag *bag)
{
	t_piece_type	next;

	if (bag->next >= BRAIN_BAG_SIZE)
		refill_bag(bag);
	next = bag->pieces[bag->next];
	bag->next++;
	return (next);
}
