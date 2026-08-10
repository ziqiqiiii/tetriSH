/* ************************************************************************** */
/*                                                                            */
/*   test_rate_limit.c - bounded refill arithmetic                            */
/*                                                                            */
/* ************************************************************************** */

#include "tetrisd.h"

#include <assert.h>

int	main(void)
{
	assert(rate_limit_refill_level(1000, 25, 120000, 24) == 1600);
	assert(rate_limit_refill_level(119990, 1, 120000, 24) == 120000);
	assert(rate_limit_refill_level(0, UINT64_MAX, 120000, 24) == 120000);
	assert(rate_limit_refill_level(-4000, UINT64_MAX, 120000, 24)
		== 120000);
	assert(rate_limit_refill_level(7000, 100, 120000, 0) == 7000);
	printf("PASS test_refill_saturates_before_large_elapsed_time_overflows\n");
	return (0);
}
