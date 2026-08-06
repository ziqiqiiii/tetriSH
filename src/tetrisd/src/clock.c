#include "tetrisd.h"

/**
 * @brief Returns wall-clock milliseconds, for log record timestamps.
 *
 * @return Milliseconds since the epoch.
 */
uint64_t	clock_now_ms(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return (0);
	return ((uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000));
}

/**
 * @brief Measures elapsed monotonic milliseconds and rearms the marker.
 *
 * Gravity accumulates real elapsed time rather than assuming the ticker slept
 * exactly its period, so a descheduled thread does not slow the game down.
 *
 * @param last Marker holding the previous reading; updated to now.
 * @return Milliseconds since the previous reading, never negative.
 */
int	clock_elapsed_ms(struct timespec *last)
{
	struct timespec	now;
	long			ms;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return (0);
	ms = (now.tv_sec - last->tv_sec) * 1000
		+ (now.tv_nsec - last->tv_nsec) / 1000000;
	*last = now;
	if (ms < 0)
		return (0);
	return ((int)ms);
}
