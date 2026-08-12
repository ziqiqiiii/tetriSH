/* ************************************************************************** */
/*                                                                            */
/*   stress.h - the load generator's own types                                */
/*                                                                            */
/*   Companion header to stress_client.c, in the shape src/tetrisd/tests/     */
/*   harness.h already uses: a test binary that needs types of its own gets   */
/*   a header of its own rather than putting them in the application's.       */
/*                                                                            */
/*   Everything here is per-worker bookkeeping. A worker is a process, so the */
/*   report it fills has to survive the trip home down a pipe: it is plain    */
/*   old data, sized under PIPE_BUF, and written in one call - which makes    */
/*   the write atomic against the other workers sharing that pipe.            */
/*                                                                            */
/* ************************************************************************** */

# ifndef STRESS_H
# define STRESS_H

# include <sys/wait.h>
# include <time.h>

# include "tetrisu.h"

/*
** Every bot signs up rather than being seeded, so the store is exercised too.
** One password for all of them: what is under test is how many sessions the
** server can hold, not how well it keeps a secret.
*/
# define STRESS_PASSWORD		"stress0"
# define STRESS_MAX_SEATS		2
# define STRESS_DEFAULT_PLAYERS	20
# define STRESS_DEFAULT_SECONDS	20
# define STRESS_DEFAULT_RATE	20
/*
** Round-trip times are bucketed at 100 us, which covers 0 - 25.6 ms exactly.
** Anything slower lands in the last bucket and is reported as an overflow,
** because a percentile that silently saturates is worse than no percentile:
** the maximum is tracked separately and in full.
*/
# define STRESS_BUCKET_US		100
# define STRESS_BUCKETS			256
# define STRESS_ACTION_COUNT	8
/*
** How often a bot spends charge instead of moving. Abilities are refused
** without charge, and a refusal costs the server the same lookup an
** activation does, so this exercises the path either way.
*/
# define STRESS_ABILITY_EVERY	32
# define STRESS_NOTE_MAX		64

/*
** Double seats two bots per worker process and is what the server does under
** a real load; single seats one, and is the only way to get N genuinely
** simultaneous handshakes, because a worker's two connects are sequential.
*/
typedef enum e_stress_mode
{
	STRESS_MODE_DOUBLE,
	STRESS_MODE_SINGLE
}	t_stress_mode;

typedef struct s_stress_plan
{
	t_stress_mode	mode;
	int				players;
	int				seconds;
	int				actions_per_second;
	int				ramp_ms;
}	t_stress_plan;

/*
** One worker's account of its run, and after merge() the whole fleet's. The
** counters are separated by what they mean to the server: `accepted` is work
** it did, `throttled` is work it refused on purpose, `refused` is work it
** declined for a game reason, and `errors` is the only one that is a fault.
*/
typedef struct s_stress_report
{
	uint32_t	bots;
	uint32_t	connected;
	uint32_t	authed;
	uint32_t	seated;
	uint32_t	matches;
	uint32_t	actions;
	uint32_t	abilities;
	uint32_t	accepted;
	uint32_t	throttled;
	uint32_t	refused;
	uint32_t	errors;
	uint32_t	frames;
	uint32_t	connect_ms_max;
	uint32_t	latency_us_max;
	uint64_t	connect_ms_total;
	uint64_t	latency_us_total;
	uint32_t	latency[STRESS_BUCKETS];
	char		note[STRESS_NOTE_MAX];
	/*
	** The `reason` word on the first refusal. A run with a large refused
	** count is not necessarily a run that went wrong - a board that has
	** topped out refuses every input until it is dealt again - so the number
	** on its own cannot be read, and this is what tells the two apart.
	*/
	char		refusal[STRESS_NOTE_MAX];
}	t_stress_report;

# endif
