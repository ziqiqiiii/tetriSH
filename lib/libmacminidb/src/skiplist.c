/* ************************************************************************** */
/*                                                                            */
/*   skiplist.c — leaderboard index, ordered by (leaderboard_score, id)       */
/*                                                                            */
/*   §7.4: chosen over B+/AVL for simplest implementation and no rotations to */
/*   coordinate under the rwlock. Only db_record_game mutates it. Nodes point */
/*   at the same player_t objects owned by the hash map.                      */
/*                                                                            */
/* ************************************************************************** */

#include "internal.h"

// TODO: define struct s_skiplist (head tower + level) and the node type
// TODO: skiplist_create / skiplist_destroy
// TODO: skiplist_insert / skiplist_remove / skiplist_update
// TODO: skiplist_topn / skiplist_rank
// TODO: static helpers — random_level, node_cmp (score desc, id asc), up top
