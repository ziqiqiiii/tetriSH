/* ************************************************************************** */
/*                                                                            */
/*   recovery.c — boot / recovery (§6)                                        */
/*                                                                            */
/*   Replays the log front->back into the hash map (LWW: latest per username  */
/*   wins), then iterates the map once to seed the skip list. Also derives    */
/*   next_id as the highest player_id seen plus one.                          */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: static helper — replay_cb: hashmap_put + track max player_id
// TODO: static helper — seed_skiplist: hashmap_foreach -> skiplist_insert
// TODO: recovery_run (log_replay, then seed, then set *out_next_id)
